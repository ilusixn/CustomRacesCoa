// DLSS Super Resolution in CoA64's render pipeline (stage 3): the world renders at Streamline's render size, then every
// (continued: docs/2026-10-29-render-pipeline-notes.md, DlssClient.cpp #1)
#include "CoA/Source/Runtime/Dlss.h"

#ifndef COA_STREAMLINE
// Built without the Streamline SDK (x86): DLSS stays off.
bool CoA::DlssClient::Wanted() {
    return false;
}

bool CoA::DlssClient::Jitters() {
    return false;
}

bool CoA::DlssClient::WantedFor(int /* modeValue */) {
    return false;
}

bool CoA::DlssClient::Begin(CGxDevice* /* device */, uint32_t /* displayW */, uint32_t /* displayH */, uint32_t& /* renderW */, uint32_t& /* renderH */) {
    return false;
}

bool CoA::DlssClient::Active() {
    return false;
}

void CoA::DlssClient::NotActive(const char* /* why */) {
}

void CoA::DlssClient::Skip() {
}

void* CoA::DlssClient::Evaluate(CGxDevice* /* device */, const CoA::RenderPipeline::Frame& /* frame */, bool /* jittered */, uint32_t /* displayW */, uint32_t /* displayH */, int /* debug */) {
    return nullptr;
}

void CoA::DlssClient::Release() {
}
#else
#include "CoA/Shaders/DepthRemapPS.h"
#include "CoA/Shaders/FullscreenVS.h"
#include "CoA/Source/Runtime/DlssFrame.h"
#include "CoA/Source/Runtime/SlFrame.h"
#include "CoA/Source/Runtime/FrameGen.h"
#include "CoA/Source/Runtime/Log.h"
#include "CoA/Source/Runtime/RenderPipeline.h"
#include "CoA/Source/Runtime/StreamlineClient.h"
#include "Engine/Source/ConsoleBase/ConsoleVar.h"
#include "Engine/Source/Gx/CGxDevice/CGxDevice.h"
#include "ThirdParty/dxvk/include/coa_dxvk_streamline.h"
#include <d3d9.h>
#include <atomic>
#include <cstdio>
#include <string>

namespace {
    using namespace CoA::RenderPipeline;

    // The one DLSS viewport.
    constexpr uint32_t kViewport = 0;
    // DepthRemapPS / FullscreenVS write c0 only (ExternalStateRestore re-sends that many).
    constexpr uint32_t kVsConstantsUsed = 1;
    constexpr uint32_t kPsConstantsUsed = 1;
    // VkImageUsageFlags of DXVK's render-target textures (D3D9CommonTexture: transfer src/dst, sampled, colour
    // attachment) and of the output (plus storage), reported to Streamline with each tag.
    constexpr uint32_t kTargetUsage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    constexpr uint32_t kOutputUsage = kTargetUsage | VK_IMAGE_USAGE_STORAGE_BIT;

    // RecordCallback's image order.
    enum Image : uint32_t { kColor, kDepth, kMotion, kOutput, kMask, kImageCount };

    // The CS thread's outcome of the latest callback: stage << 16 | detail, 0 = evaluated. Read by the main thread.
    enum Stage : uint32_t { kStageOk, kStageNoApi, kStageNoView, kStageToken, kStageConstants, kStageTags, kStageEvaluate };
    std::atomic<uint32_t> s_csOutcome { 0 };
    // Streamline frame index of the latest evaluate that returned eOk (or a warning), 0 before the first.
    std::atomic<uint32_t> s_csOkFrame { 0 };
    // The latest evaluate's warning result (eWarnOutOfVRAM), 0 for none; logged once.
    std::atomic<uint32_t> s_csWarning { 0 };
    // The first callback's images as DXVK reported them (written once by the CS thread before s_csImagesReady, logged
    // once by the main thread): format, layout, extent and whether a view was made.
    struct ImageInfo {
        uint32_t format, layout, width, height;
        bool view;
    };
    ImageInfo s_csImages[kImageCount];
    std::atomic<bool> s_csImagesReady { false };
    bool s_csImagesTaken;
    bool s_loggedImages;
    // Bumped by Release: a callback recorded before it does nothing.
    std::atomic<uint32_t> s_generation { 1 };

    ID3D9VkInteropDevice1* s_interop;
    IDirect3DDevice9* s_interopDevice;
    bool s_interopLogged;

    // The remapped depth (R32F render target, render size) and DLSS's output (display size, storage image).
    IDirect3DTexture9* s_remap;
    uint32_t s_remapWidth, s_remapHeight;
    IDirect3DTexture9* s_output;
    uint32_t s_outputWidth, s_outputHeight;
    IDirect3DVertexShader9* s_vs;
    IDirect3DPixelShader9* s_ps;
    IDirect3DVertexDeclaration9* s_decl;
    // Creation failed (logged): no DLSS until Release (device reset / pipeline off) tries again.
    bool s_objectsFailed;

    // The options Streamline has (slDLSSSetOptions) for the viewport: sl::DLSSMode value and display size, and the
    // render size chosen for them.
    bool s_optionsSet;
    uint32_t s_optMode, s_optWidth, s_optHeight;
    uint32_t s_renderWidth, s_renderHeight;
    // slDLSSSetOptions failed for this mode and size: not retried until either changes or Release.
    bool s_optionsFailed;
    uint32_t s_failedMode, s_failedWidth, s_failedHeight;
    // Evaluates reset DLSS's history (new options, or frames that did not evaluate) until one recorded with the reset at
    // or after s_resetFrame has come back eOk from the CS thread.
    bool s_resetPending;
    uint32_t s_resetFrame;
    // One-time logs: the first good evaluate, a warning result.
    bool s_loggedFirstOk;
    bool s_loggedWarning;
    // Begin decided this world render evaluates.
    bool s_active;
    // This world render's plan (Begin): DLSS evaluate, jitter, Frame Generation tags.
    CoA::FrameGen::FramePlan s_plan;
    // Streamline's frame index (one per evaluate).
    uint32_t s_slFrame;
    // A fallback streak is logged once, and its end.
    bool s_failing;
    // The last state line (logged on change).
    std::string s_logged;

    void LogState(const std::string& line) {
        if (line != s_logged) {
            s_logged = line;
            CoA::Log::Printf("%s", line.c_str());
        }
    }

    int ModeValue() {
        CVar* var = *CoA::RenderPipelineClient::DlssModeSlot();
        return var ? var->m_intValue : 0;
    }

    float Sign(CVar** slot) {
        CVar* var = *slot;
        return var && var->m_floatValue < 0.0f ? -1.0f : 1.0f;
    }

    const char* ModeName(CoA::Dlss::Mode m) {
        switch (m) {
            case CoA::Dlss::Mode::Dlaa: return "DLAA";
            case CoA::Dlss::Mode::Quality: return "Quality";
            case CoA::Dlss::Mode::Balanced: return "Balanced";
            case CoA::Dlss::Mode::Performance: return "Performance";
            case CoA::Dlss::Mode::UltraPerformance: return "Ultra Performance";
            default: return "off";
        }
    }

    const char* StageName(uint32_t stage) {
        switch (stage) {
            case kStageNoApi: return "Streamline not ready";
            case kStageNoView: return "no Vulkan view for image";
            case kStageToken: return "slGetNewFrameToken";
            case kStageConstants: return "slSetConstants";
            case kStageTags: return "slSetTagForFrame";
            case kStageEvaluate: return "slEvaluateFeature";
            default: return "?";
        }
    }

    // DXVK's interop interface of the D3D9 device (one reference, dropped by Release).
    ID3D9VkInteropDevice1* Interop(IDirect3DDevice9* device) {
        if (s_interop && s_interopDevice == device) {
            return s_interop;
        }

        if (s_interop) {
            s_interop->Release();
            s_interop = nullptr;
        }

        void* interop = nullptr;

        if (FAILED(device->QueryInterface(__uuidof(ID3D9VkInteropDevice1), &interop)) || !interop) {
            if (!s_interopLogged) {
                s_interopLogged = true;
                CoA::Log::Printf("DLSS: the D3D9 device has no ID3D9VkInteropDevice1 (not CoA64's DXVK)");
            }

            return nullptr;
        }

        s_interop = static_cast<ID3D9VkInteropDevice1*>(interop);
        s_interopDevice = device;
        return s_interop;
    }

    // No DLSS work of DXVK's command stream is left: every recorded callback has run (CS thread synchronised) and the GPU
    // is done with the last evaluate's output.
    void Drain() {
        if (s_interop) {
            s_interop->FlushRenderingCommands();

            if (s_output) {
                s_interop->WaitForResource(s_output, 0);
            }
        }
    }

    void FreeSl() {
        if (!s_optionsSet) {
            return;
        }

        s_optionsSet = false;

        if (const CoA::StreamlineClient::Api* api = CoA::StreamlineClient::GetApi()) {
            const sl::Result r = api->freeResources(sl::kFeatureDLSS, sl::ViewportHandle(kViewport));
            CoA::Log::Printf("DLSS: slFreeResources %s", CoA::StreamlineClient::ResultName(r));
        }
    }

    void ReleaseTextures() {
        IUnknown* objects[] = { s_remap, s_output };

        for (IUnknown* object : objects) {
            if (object) {
                object->Release();
            }
        }

        s_remap = nullptr;
        s_output = nullptr;
        s_remapWidth = s_remapHeight = 0;
        s_outputWidth = s_outputHeight = 0;
    }

    void ReleaseObjects() {
        IUnknown* objects[] = { s_vs, s_ps, s_decl };

        for (IUnknown* object : objects) {
            if (object) {
                object->Release();
            }
        }

        s_vs = nullptr;
        s_ps = nullptr;
        s_decl = nullptr;
    }

    // The mode or display size changed (or Streamline has no options yet): drain, free the old feature, ask for the
    // render size and set the new options. False: DLSS stays off for this mode and size (logged).
    bool ApplyOptions(const CoA::StreamlineClient::Api* api, CoA::Dlss::Mode mode, uint32_t displayW, uint32_t displayH) {
        const uint32_t slMode = CoA::Dlss::SlMode(mode);

        if (s_optionsFailed && s_failedMode == slMode && s_failedWidth == displayW && s_failedHeight == displayH) {
            return false;
        }

        Drain();
        FreeSl();

        sl::DLSSOptions options {};
        options.mode = static_cast<sl::DLSSMode>(slMode);
        options.outputWidth = displayW;
        options.outputHeight = displayH;
        // The world colour is the LDR back-buffer format.
        options.colorBuffersHDR = sl::Boolean::eFalse;

        sl::DLSSOptimalSettings settings {};
        sl::Result r = api->dlssGetOptimalSettings(options, settings);
        uint32_t w = settings.optimalRenderWidth;
        uint32_t h = settings.optimalRenderHeight;
        const bool optimal = r == sl::Result::eOk && w && h && w <= displayW && h <= displayH;

        if (!optimal) {
            CoA::Dlss::FallbackRenderSize(mode, displayW, displayH, w, h);
        }

        CoA::Log::Printf("DLSS: slDLSSGetOptimalSettings(%s, %ux%u) %s: render %ux%u%s", ModeName(mode), displayW, displayH,
            CoA::StreamlineClient::ResultName(r), w, h, optimal ? "" : " (fallback)");

        r = api->dlssSetOptions(sl::ViewportHandle(kViewport), options);
        CoA::Log::Printf("DLSS: slDLSSSetOptions %s", CoA::StreamlineClient::ResultName(r));

        if (r != sl::Result::eOk) {
            s_optionsFailed = true;
            s_failedMode = slMode;
            s_failedWidth = displayW;
            s_failedHeight = displayH;
            LogState("DLSS: inactive: slDLSSSetOptions failed for this mode and size");
            return false;
        }

        s_optionsFailed = false;
        s_optionsSet = true;
        s_optMode = slMode;
        s_optWidth = displayW;
        s_optHeight = displayH;
        s_renderWidth = w;
        s_renderHeight = h;
        s_resetPending = true;
        s_resetFrame = 0;
        return true;
    }

    // The remap target (render size) and the output (display size, created through DXVK with storage usage). The output
    // (continued: docs/2026-10-29-render-pipeline-notes.md, DlssClient.cpp #2)
    bool EnsureTextures(IDirect3DDevice9* device, uint32_t renderW, uint32_t renderH, uint32_t displayW, uint32_t displayH, bool& fresh) {
        fresh = false;

        if (s_remap && (s_remapWidth != renderW || s_remapHeight != renderH)) {
            s_remap->Release();
            s_remap = nullptr;
        }

        if (!s_remap) {
            if (FAILED(device->CreateTexture(renderW, renderH, 1, D3DUSAGE_RENDERTARGET, D3DFMT_R32F, D3DPOOL_DEFAULT, &s_remap, nullptr))) {
                s_remap = nullptr;
                CoA::Log::Printf("DLSS: R32F depth remap target creation failed (%ux%u)", renderW, renderH);
                return false;
            }

            s_remapWidth = renderW;
            s_remapHeight = renderH;
        }

        if (s_output && (s_outputWidth != displayW || s_outputHeight != displayH)) {
            s_output->Release();
            s_output = nullptr;
        }

        if (!s_output) {
            const D3DFORMAT formats[] = { D3DFMT_A8R8G8B8, D3DFMT_A16B16G16R16F };

            for (D3DFORMAT format : formats) {
                D3D9VkExtImageDesc desc = {};
                desc.Type = D3DRTYPE_TEXTURE;
                desc.Width = displayW;
                desc.Height = displayH;
                desc.Depth = 1;
                desc.MipLevels = 1;
                desc.Usage = D3DUSAGE_RENDERTARGET;
                desc.Format = format;
                desc.Pool = D3DPOOL_DEFAULT;
                desc.MultiSample = D3DMULTISAMPLE_NONE;
                desc.ImageUsage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

                IDirect3DResource9* resource = nullptr;
                const HRESULT hr = s_interop->CreateImage(&desc, &resource);
                void* texture = nullptr;

                if (SUCCEEDED(hr) && resource && SUCCEEDED(resource->QueryInterface(__uuidof(IDirect3DTexture9), &texture)) && texture) {
                    s_output = static_cast<IDirect3DTexture9*>(texture);
                }

                if (resource) {
                    resource->Release();
                }

                CoA::Log::Printf("DLSS: output %ux%u %s %s (0x%08lX)", displayW, displayH,
                    format == D3DFMT_A8R8G8B8 ? "A8R8G8B8" : "A16B16G16R16F", s_output ? "created" : "failed",
                    static_cast<unsigned long>(hr));

                if (s_output) {
                    if (format != D3DFMT_A8R8G8B8) {
                        CoA::Log::Printf("DLSS: warning: A8R8G8B8 was refused as a storage image, the output is A16B16G16R16F "
                            "(copied to the back buffer through DXVK's blit path)");
                    }

                    break;
                }
            }

            if (!s_output) {
                return false;
            }

            s_outputWidth = displayW;
            s_outputHeight = displayH;
            fresh = true;
        }

        return true;
    }

    bool CreateObjects(IDirect3DDevice9* device) {
        if (s_decl) {
            return true;
        }

        if (s_objectsFailed) {
            return false;
        }

        const D3DVERTEXELEMENT9 quad[] = {
            { 0, 0, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
            { 0, 16, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
            D3DDECL_END()
        };

        if (FAILED(device->CreateVertexShader(reinterpret_cast<const DWORD*>(kFullscreenVS), &s_vs))
            || FAILED(device->CreatePixelShader(reinterpret_cast<const DWORD*>(kDepthRemapPS), &s_ps))
            || FAILED(device->CreateVertexDeclaration(quad, &s_decl))) {
            ReleaseObjects();
            s_objectsFailed = true;
            CoA::Log::Printf("DLSS: depth remap pass unavailable (D3D9 object creation failed)");
            return false;
        }

        return true;
    }

    // The depth remap pass (render size): INTZ depth -> saturate(stored * scale + offset) in the R32F target. Raw D3D9 as
    // (continued: docs/2026-10-29-render-pipeline-notes.md, DlssClient.cpp #3)
    bool RemapPass(CGxDevice* gx, IDirect3DDevice9* device, IDirect3DBaseTexture9* depth, const Frame& frame) {
        IDirect3DSurface9* savedTarget = nullptr;
        IDirect3DSurface9* savedDepth = nullptr;
        IDirect3DSurface9* target = nullptr;
        const bool haveTarget = SUCCEEDED(device->GetRenderTarget(0, &savedTarget)) && savedTarget;
        const bool haveDepth = SUCCEEDED(device->GetDepthStencilSurface(&savedDepth)) && savedDepth;
        bool drawn = false;

        if (haveTarget && haveDepth && SUCCEEDED(s_remap->GetSurfaceLevel(0, &target)) && target) {
            device->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
            device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
            device->SetRenderState(D3DRS_DEPTHBIAS, 0);
            device->SetRenderState(D3DRS_SLOPESCALEDEPTHBIAS, 0);
            device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
            device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
            device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
            device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
            device->SetRenderState(D3DRS_COLORWRITEENABLE, 0xF);
            device->SetRenderState(D3DRS_CLIPPLANEENABLE, 0);
            device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
            device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
            device->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
            device->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
            device->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);

            // No depth surface bound while INTZ is sampled.
            device->SetDepthStencilSurface(nullptr);

            if (SUCCEEDED(device->SetRenderTarget(0, target))) {
                D3DVIEWPORT9 viewport = { 0, 0, s_remapWidth, s_remapHeight, 0.0f, 1.0f };
                device->SetViewport(&viewport);

                const CoA::Dlss::DepthRemap remap = CoA::Dlss::DepthRemapFor(frame);
                const float c0[4] = { remap.scale, remap.offset, 0.0f, 0.0f };
                const float w = static_cast<float>(s_remapWidth);
                const float h = static_cast<float>(s_remapHeight);
                // Clip-space strip with the D3D9 half-pixel offset applied by the vertex shader (c0).
                const float offset[4] = { -1.0f / w, 1.0f / h, 0.0f, 0.0f };
                const float quad[4][6] = {
                    { -1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f },
                    { 1.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f },
                    { -1.0f, -1.0f, 0.0f, 1.0f, 0.0f, 1.0f },
                    { 1.0f, -1.0f, 0.0f, 1.0f, 1.0f, 1.0f }
                };

                device->SetTexture(0, depth);
                device->SetPixelShader(s_ps);
                device->SetPixelShaderConstantF(0, c0, 1);
                device->SetVertexDeclaration(s_decl);
                device->SetVertexShader(s_vs);
                device->SetVertexShaderConstantF(0, offset, 1);
                drawn = SUCCEEDED(device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, quad, sizeof(quad[0])));
                device->SetTexture(0, nullptr);
            }

            device->SetRenderTarget(0, savedTarget);
            device->SetDepthStencilSurface(savedDepth);
        }

        IDirect3DSurface9* surfaces[] = { savedTarget, savedDepth, target };

        for (IDirect3DSurface9* surface : surfaces) {
            if (surface) {
                surface->Release();
            }
        }

        gx->ExternalStateRestore(kVsConstantsUsed, kPsConstantsUsed);
        return drawn;
    }

    // The output starts as the stretched world (stage 1's copy-back), so it is never undefined on screen.
    void SeedOutput(IDirect3DDevice9* device, IDirect3DTexture9* color) {
        IDirect3DSurface9* source = nullptr;
        IDirect3DSurface9* dest = nullptr;

        if (SUCCEEDED(color->GetSurfaceLevel(0, &source)) && source && SUCCEEDED(s_output->GetSurfaceLevel(0, &dest)) && dest) {
            device->StretchRect(source, nullptr, dest, nullptr, D3DTEXF_LINEAR);
        }

        if (source) {
            source->Release();
        }

        if (dest) {
            dest->Release();
        }
    }

    void Store(uint32_t stage, uint32_t detail) {
        s_csOutcome.store(stage << 16 | (detail & 0xFFFFu), std::memory_order_release);
    }

    sl::Resource Describe(const D3D9VkImageAccess& a, uint32_t usage) {
        sl::Resource r(sl::ResourceType::eTex2d, reinterpret_cast<void*>(a.image), nullptr, reinterpret_cast<void*>(a.view), static_cast<uint32_t>(a.layout));
        r.width = a.extent.width;
        r.height = a.extent.height;
        r.nativeFormat = static_cast<uint32_t>(a.format);
        r.mipLevels = 1;
        r.arrayLayers = 1;
        r.usage = usage;
        // DXVK makes the 8-bit colour formats with an sRGB twin mutable.
        r.flags = (a.format == VK_FORMAT_B8G8R8A8_UNORM || a.format == VK_FORMAT_R8G8B8A8_UNORM) ? VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT : 0;
        return r;
    }

    sl::Extent Rect(uint32_t w, uint32_t h) {
        sl::Extent e;
        e.top = 0;
        e.left = 0;
        e.width = w;
        e.height = h;
        return e;
    }

    // DXVK's CS thread, inside its command buffer: the whole per-frame Streamline sequence. Every result lands in
    // s_csOutcome for the main thread.
    void __stdcall EvaluateOnCs(VkCommandBuffer cmd, D3D9VkImageAccess* images, UINT count, void* user) {
        const auto* b = static_cast<const CoA::Dlss::FrameBlob*>(user);

        if (!b || count != kImageCount || b->generation != s_generation.load(std::memory_order_acquire)) {
            return;
        }

        const CoA::StreamlineClient::Api* api = CoA::StreamlineClient::GetApi();

        if (!api) {
            Store(kStageNoApi, 0);
            return;
        }

        if (!s_csImagesTaken) {
            s_csImagesTaken = true;

            for (UINT i = 0; i < count; ++i) {
                s_csImages[i] = { static_cast<uint32_t>(images[i].format), static_cast<uint32_t>(images[i].layout),
                    images[i].extent.width, images[i].extent.height, images[i].view != VK_NULL_HANDLE };
            }

            s_csImagesReady.store(true, std::memory_order_release);
        }

        for (UINT i = 0; i < count; ++i) {
            if (!images[i].image || !images[i].view) {
                Store(kStageNoView, i);
                return;
            }
        }

        sl::FrameToken* token = nullptr;
        sl::Result r = api->getNewFrameToken(token, &b->frameIndex);

        if (r != sl::Result::eOk || !token) {
            Store(kStageToken, static_cast<uint32_t>(r));
            return;
        }

        const sl::ViewportHandle viewport(b->viewport);
        r = api->setConstants(b->constants, *token, viewport);

        if (r != sl::Result::eOk) {
            Store(kStageConstants, static_cast<uint32_t>(r));
            return;
        }

        sl::Resource color = Describe(images[kColor], kTargetUsage);
        sl::Resource depth = Describe(images[kDepth], kTargetUsage);
        sl::Resource motion = Describe(images[kMotion], kTargetUsage);
        sl::Resource output = Describe(images[kOutput], kOutputUsage);
        sl::Resource mask = Describe(images[kMask], kTargetUsage);
        const sl::Extent render = Rect(b->renderWidth, b->renderHeight);
        const sl::Extent display = Rect(b->displayWidth, b->displayHeight);
        const bool evaluate = (b->mode & CoA::Dlss::kBlobEvaluate) != 0;
        const bool frameGen = (b->mode & CoA::Dlss::kBlobFrameGen) != 0;
        // Frame Generation reads its inputs at present (DLSS_G guide 5.2: start with eValidUntilPresent).
        const sl::ResourceLifecycle shared = frameGen ? sl::ResourceLifecycle::eValidUntilPresent : sl::ResourceLifecycle::eOnlyValidNow;
        sl::ResourceTag tags[6];
        uint32_t tagCount = 0;
        tags[tagCount++] = sl::ResourceTag(&depth, sl::kBufferTypeDepth, shared, &render);
        tags[tagCount++] = sl::ResourceTag(&motion, sl::kBufferTypeMotionVectors, shared, &render);

        if (evaluate) {
            tags[tagCount++] = sl::ResourceTag(&color, sl::kBufferTypeScalingInputColor, sl::ResourceLifecycle::eOnlyValidNow, &render);
            tags[tagCount++] = sl::ResourceTag(&output, sl::kBufferTypeScalingOutputColor, shared, &display);
            // Translucency drawn over the opaque scene has no motion vectors: trust the current colour there.
            tags[tagCount++] = sl::ResourceTag(&mask, sl::kBufferTypeBiasCurrentColorHint, sl::ResourceLifecycle::eOnlyValidNow, &render);
        }

        if (frameGen) {
            // The world before the UI: DLSS's output, or the display-size world colour without DLSS.
            tags[tagCount++] = sl::ResourceTag(evaluate ? &output : &color, sl::kBufferTypeHUDLessColor, sl::ResourceLifecycle::eValidUntilPresent, &display);
        }

        sl::CommandBuffer* commands = reinterpret_cast<sl::CommandBuffer*>(cmd);
        r = api->setTagForFrame(*token, viewport, tags, tagCount, commands);

        if (r != sl::Result::eOk) {
            Store(kStageTags, static_cast<uint32_t>(r));
            return;
        }

        if (!evaluate) {
            Store(kStageOk, 0);
            return;
        }

        const sl::BaseStructure* inputs[] = { &viewport };
        r = api->evaluateFeature(sl::kFeatureDLSS, *token, inputs, 1, commands);

        // eWarnOutOfVRAM still evaluated (Streamline does not stomp the result of a successful dispatch with it).
        if (r == sl::Result::eOk || r == sl::Result::eWarnOutOfVRAM) {
            s_csWarning.store(r == sl::Result::eOk ? 0u : static_cast<uint32_t>(r), std::memory_order_relaxed);
            s_csOkFrame.store(b->frameIndex, std::memory_order_relaxed);
            Store(kStageOk, 0);
        } else {
            Store(kStageEvaluate, static_cast<uint32_t>(r));
        }
    }

    // This frame shows the stretched world: the first frame of a streak is logged, and DLSS's history restarts at the
    // next evaluate that goes through.
    void* Fallback(const char* why) {
        if (!s_failing) {
            s_failing = true;
            CoA::Log::Printf("DLSS: falling back to the stretched world: %s", why);
        }

        s_resetPending = true;
        s_resetFrame = 0;
        return nullptr;
    }

    // Frames without an evaluate break DLSS's history: the next evaluate resets it.
    void HistoryBroken() {
        if (s_optionsSet) {
            s_resetPending = true;
            s_resetFrame = 0;
        }
    }

    // coaRenderDebug 8 / 9: the output filled with magenta (8: before the evaluate, so magenta on screen means
    // (continued: docs/2026-10-29-render-pipeline-notes.md, DlssClient.cpp #4)
    bool s_loggedFill;

    void ProbeFill(IDirect3DDevice9* device) {
        IDirect3DSurface9* surface = nullptr;
        const HRESULT level = s_output->GetSurfaceLevel(0, &surface);
        HRESULT fill = E_FAIL;
        HRESULT descResult = E_FAIL;
        D3DSURFACE_DESC desc = {};

        if (SUCCEEDED(level) && surface) {
            descResult = surface->GetDesc(&desc);
            fill = device->ColorFill(surface, nullptr, D3DCOLOR_XRGB(255, 0, 255));
            surface->Release();
        }

        if (!s_loggedFill) {
            s_loggedFill = true;
            CoA::Log::Printf("DLSS: probe fill: GetSurfaceLevel 0x%08lX, GetDesc 0x%08lX (format %u, usage 0x%lX, pool %u, "
                "%ux%u, type %u), ColorFill 0x%08lX", static_cast<unsigned long>(level), static_cast<unsigned long>(descResult),
                static_cast<unsigned>(desc.Format), static_cast<unsigned long>(desc.Usage), static_cast<unsigned>(desc.Pool),
                desc.Width, desc.Height, static_cast<unsigned>(desc.Type), static_cast<unsigned long>(fill));
        }
    }
}

bool CoA::DlssClient::Wanted() {
    return WantedFor(ModeValue());
}

// DLSS super resolution for this coaDlssMode value; Streamline may be up for Reflex or Frame Generation alone.
static bool DlssWantedFor(int modeValue) {
    return CoA::StreamlineClient::Ready() && CoA::Streamline::Supported().dlss && CoA::Dlss::ModeFromCVar(modeValue) != CoA::Dlss::Mode::Off;
}

bool CoA::DlssClient::WantedFor(int modeValue) {
    return CoA::FrameGen::PlanFrame(DlssWantedFor(modeValue), CoA::FrameGenClient::Wanted()).pipeline;
}

bool CoA::DlssClient::Jitters() {
    return s_active && s_plan.jitter;
}

bool CoA::DlssClient::Begin(CGxDevice* device, uint32_t displayW, uint32_t displayH, uint32_t& renderW, uint32_t& renderH) {
    // The last Begin was not followed by an evaluate.
    if (s_active) {
        HistoryBroken();
    }

    s_active = false;

    if (!Wanted()) {
        // Mode 0 (live): Streamline's feature, the textures and the interop reference go, the world renders as before.
        if (s_optionsSet || s_output || s_remap || s_interop) {
            Release();
            LogState("DLSS: inactive: coaDlssMode 0");
        }

        return false;
    }

    const CoA::StreamlineClient::Api* api = CoA::StreamlineClient::GetApi();
    IDirect3DDevice9* d3d = static_cast<IDirect3DDevice9*>(device->D3dDevice());

    if (!api || !d3d || !displayW || !displayH) {
        return false;
    }

    if (!Interop(d3d)) {
        LogState("DLSS: inactive: no DXVK interop device");
        return false;
    }

    if (!device->WorldMotionSupported()) {
        LogState("DLSS: inactive: no motion targets (G16R16F / A8R8G8B8 render targets)");
        return false;
    }

    const CoA::Dlss::Mode mode = CoA::Dlss::ModeFromCVar(ModeValue());
    s_plan = CoA::FrameGen::PlanFrame(DlssWantedFor(ModeValue()), CoA::FrameGenClient::Wanted());

    if (s_plan.evaluateDlss) {
        if ((!s_optionsSet || s_optMode != CoA::Dlss::SlMode(mode) || s_optWidth != displayW || s_optHeight != displayH)
            && !ApplyOptions(api, mode, displayW, displayH)) {
            return false;
        }
    } else {
        // Frame Generation alone: the world at display size, no DLSS feature.
        if (s_optionsSet) {
            Drain();
            FreeSl();
        }

        s_renderWidth = displayW;
        s_renderHeight = displayH;
    }

    char line[128];
    snprintf(line, sizeof(line), "DLSS: active (%s%s, %ux%u from %ux%u)", s_plan.evaluateDlss ? ModeName(mode) : "tags only",
        s_plan.tagFrameGen ? " + Frame Generation" : "", displayW, displayH, s_renderWidth, s_renderHeight);
    LogState(line);
    renderW = s_renderWidth;
    renderH = s_renderHeight;
    s_active = true;
    return true;
}

bool CoA::DlssClient::Active() {
    return s_active;
}

void CoA::DlssClient::NotActive(const char* why) {
    s_active = false;
    HistoryBroken();

    if (Wanted()) {
        LogState(std::string("DLSS: inactive: ") + why);
    }
}

void CoA::DlssClient::Skip() {
    if (s_active) {
        s_active = false;
        HistoryBroken();
    }
}

void* CoA::DlssClient::Evaluate(CGxDevice* gx, const Frame& frame, bool jittered, uint32_t displayW, uint32_t displayH, int debug) {
    const bool probe = debug == 8;

    if (!s_active) {
        return nullptr;
    }

    s_active = false;
    IDirect3DDevice9* device = static_cast<IDirect3DDevice9*>(gx->D3dDevice());
    IDirect3DTexture9* color = static_cast<IDirect3DTexture9*>(gx->WorldColorTexture());
    IDirect3DTexture9* depth = static_cast<IDirect3DTexture9*>(gx->WorldDepthTexture());
    IDirect3DTexture9* motion = static_cast<IDirect3DTexture9*>(gx->WorldMotionTexture());
    IDirect3DTexture9* mask = static_cast<IDirect3DTexture9*>(gx->WorldMaskTexture());

    if (!device || !s_interop || !color || !depth || !motion || !mask) {
        return Fallback("a world, depth, motion or mask texture is missing");
    }

    // The world target and the motion target are this world render's size.
    D3DSURFACE_DESC colorDesc;
    D3DSURFACE_DESC motionDesc;
    D3DSURFACE_DESC maskDesc;

    if (FAILED(color->GetLevelDesc(0, &colorDesc)) || FAILED(motion->GetLevelDesc(0, &motionDesc)) || FAILED(mask->GetLevelDesc(0, &maskDesc))
        || colorDesc.Width != s_renderWidth || colorDesc.Height != s_renderHeight
        || motionDesc.Width != colorDesc.Width || motionDesc.Height != colorDesc.Height
        || maskDesc.Width != colorDesc.Width || maskDesc.Height != colorDesc.Height) {
        return Fallback("the world, motion or mask target is not at the render size");
    }

    bool fresh = false;

    if (!CreateObjects(device) || !EnsureTextures(device, s_renderWidth, s_renderHeight, displayW, displayH, fresh)) {
        return Fallback("the depth remap target, output or shaders could not be created");
    }

    // coaRenderDebug 9: magenta straight to the back buffer, no evaluate (the copy path alone).
    if (debug == 9) {
        ProbeFill(device);
        HistoryBroken();
        return s_output;
    }

    if (probe) {
        ProbeFill(device);
    } else if (fresh && s_plan.evaluateDlss) {
        SeedOutput(device, color);
    }

    if (!RemapPass(gx, device, depth, frame)) {
        return Fallback("the depth remap pass failed");
    }

    CoA::Dlss::FrameSetup setup {};
    // The game frame's Streamline index (SlFrame) once frames run; DLSS alone (no Reflex / PCL) counts its own.
    const uint64_t gameFrame = CoA::SlFrame::Index();
    setup.frameIndex = gameFrame ? static_cast<uint32_t>(gameFrame) : ++s_slFrame;
    setup.viewport = kViewport;
    setup.mode = s_optMode;
    setup.renderWidth = s_renderWidth;
    setup.renderHeight = s_renderHeight;
    setup.displayWidth = displayW;
    setup.displayHeight = displayH;
    setup.generation = s_generation.load(std::memory_order_relaxed);
    setup.mvecSign = Sign(CoA::RenderPipelineClient::DlssMvecSignSlot());
    setup.jitterSign = Sign(CoA::RenderPipelineClient::DlssJitterSignSlot());
    setup.jittered = jittered;
    setup.reset = s_resetPending;
    setup.evaluate = s_plan.evaluateDlss;
    setup.frameGen = s_plan.tagFrameGen;

    CoA::Dlss::FrameBlob blob;
    CoA::Dlss::FillFrameBlob(blob, frame, setup);

    // Inputs read by compute shaders, the output written as a storage image.
    const D3D9VkImageAccess images[kImageCount] = {
        { color, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0 },
        { s_remap, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0 },
        { motion, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0 },
        { s_output, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_USAGE_STORAGE_BIT },
        { mask, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0 }
    };
    const HRESULT hr = s_interop->RecordCallback(EvaluateOnCs, nullptr, images, kImageCount, &blob, sizeof(blob));

    if (FAILED(hr)) {
        char why[64];
        snprintf(why, sizeof(why), "RecordCallback 0x%08lX", static_cast<unsigned long>(hr));
        return Fallback(why);
    }

    if (s_plan.tagFrameGen) {
        CoA::FrameGenClient::OnTagged(displayW, displayH);
    }

    // Frame Generation alone: nothing to copy from, the world target is already at display size.
    if (!s_plan.evaluateDlss) {
        return nullptr;
    }

    // The reset stays on until an evaluate recorded with it has come back good.
    if (s_resetPending && !s_resetFrame) {
        s_resetFrame = setup.frameIndex;
    }

    // The CS thread reports a callback after the main thread moved on: the latest outcome (usually last frame's) decides
    // whether the output is trusted. A failed evaluate leaves the output at its last good image for at most that frame.
    const uint32_t outcome = s_csOutcome.load(std::memory_order_acquire);
    const uint32_t okFrame = s_csOkFrame.load(std::memory_order_relaxed);

    if (!s_loggedImages && s_csImagesReady.load(std::memory_order_acquire)) {
        s_loggedImages = true;
        static const char* const kNames[] = { "colour", "depth", "motion", "output", "mask" };
        static_assert(sizeof(kNames) / sizeof(kNames[0]) == kImageCount);

        for (uint32_t i = 0; i < kImageCount; ++i) {
            const ImageInfo& info = s_csImages[i];
            CoA::Log::Printf("DLSS: %s image VkFormat %u, layout %u, %ux%u, %s", kNames[i], info.format, info.layout,
                info.width, info.height, info.view ? "view ok" : "NO VIEW");
        }
    }

    if (okFrame && !s_loggedFirstOk) {
        s_loggedFirstOk = true;
        CoA::Log::Printf("DLSS: first evaluate ok (Streamline frame %u)", okFrame);
    }

    if (const uint32_t warning = s_csWarning.load(std::memory_order_relaxed); warning && !s_loggedWarning) {
        s_loggedWarning = true;
        CoA::Log::Printf("DLSS: slEvaluateFeature %s (still evaluated)", CoA::StreamlineClient::ResultName(static_cast<sl::Result>(warning)));
    }

    if (s_resetPending && s_resetFrame && okFrame >= s_resetFrame) {
        s_resetPending = false;
        s_resetFrame = 0;
    }

    // coaRenderDebug 8: the output as it is (magenta where Streamline wrote nothing), whatever the outcome.
    if (probe) {
        return s_output;
    }

    if (outcome != 0) {
        const uint32_t stage = outcome >> 16;
        const uint32_t detail = outcome & 0xFFFFu;
        char why[128];

        if (stage == kStageNoView) {
            snprintf(why, sizeof(why), "%s %u", StageName(stage), detail);
        } else {
            snprintf(why, sizeof(why), "%s %s", StageName(stage), CoA::StreamlineClient::ResultName(static_cast<sl::Result>(detail)));
        }

        return Fallback(why);
    }

    if (s_failing) {
        s_failing = false;
        CoA::Log::Printf("DLSS: evaluating again");
    }

    return s_output;
}

void CoA::DlssClient::Release() {
    Drain();
    FreeSl();
    s_generation.fetch_add(1, std::memory_order_acq_rel);
    s_csOutcome.store(0, std::memory_order_release);
    s_csOkFrame.store(0, std::memory_order_relaxed);
    s_csWarning.store(0, std::memory_order_relaxed);
    s_resetPending = false;
    s_resetFrame = 0;
    ReleaseTextures();
    ReleaseObjects();

    if (s_interop) {
        s_interop->Release();
        s_interop = nullptr;
    }

    s_interopDevice = nullptr;
    s_objectsFailed = false;
    s_optionsFailed = false;
    s_active = false;
    s_failing = false;
    s_logged.clear();
}
#endif
