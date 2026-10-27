// RenderMotion's engine half: the motion passes of CoA64's render pipeline (stage 2), run by RenderPipelineClient while
// the world target is begun. OpaqueDone snapshots the opaque world; BeforeCopyBack draws the static (camera) motion,
// the skinned-model motion and the translucency mask into the device's motion targets, then records this frame's bone
// palettes for the next frame. Raw D3D9 behind a D3DSBT_ALL state block, so the engine's state and its caches are as
// they were afterwards. Nothing here runs unless coaRenderMotion is 1. Not part of the original (CoA64 addition).
#include "CoA/Shaders/BiasMaskPS.h"
#include "CoA/Shaders/FullscreenVS.h"
#include "CoA/Shaders/MotionPS.h"
#include "CoA/Shaders/MotionViewPS.h"
#include "CoA/Shaders/SkinnedMotionVS.h"
#include "CoA/Shaders/StaticMotionPS.h"
#include "CoA/Source/Runtime/Log.h"
#include "CoA/Source/Runtime/MotionCache.h"
#include "CoA/Source/Runtime/RenderMotion.h"
#include "CoA/Source/Runtime/RenderPipeline.h"
#include "Engine/Source/ConsoleBase/ConsoleVar.h"
#include "Engine/Source/Gx/CGxBuf.h"
#include "Engine/Source/Gx/CGxDevice/CGxDevice.h"
#include "Engine/Source/Gx/CGxPool.h"
#include "Engine/Source/Model2/M2Data.h"
#include "Engine/Source/Model2/M2Model.h"
#include "Engine/Source/Model2/M2Scene.h"
#include "Engine/Source/Model2/M2Shared.h"
#include "Engine/Source/Model2/Types.h"
#include "Engine/Source/Tempest/C44Matrix.h"
#include "WoW/Source/WorldClient/World.h"
#include <d3d9.h>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <unordered_map>

namespace {
    using namespace CoA::RenderPipeline;
    namespace MotionCache = CoA::MotionCache;

    // The model pass's depth test against the world's INTZ (read-only, LESSEQUAL): the re-drawn batches land on the
    // depth the world wrote for them, pulled slightly towards the eye so equal depths pass. D3DRS_DEPTHBIAS and
    // D3DRS_SLOPESCALEDEPTHBIAS take the float's bits.
    constexpr float kDepthBias = -0.00002f;
    constexpr float kSlopeScaleDepthBias = -1.0f;

    // M2Model.cpp / CM2SceneRender.cpp flags: m_fFlags 0x1000 = animated with AnimateMTSimple (static model), 0x2000 =
    // culled (not drawn); M2Element::m_flags 4 = optimized geometry (the model's own CPU-skinned buffers).
    constexpr uint32_t kModelStaticSimple = 0x1000;
    constexpr uint32_t kModelCulled = 0x2000;
    constexpr uint32_t kElementOptimized = 4;

    // The palette texture: A32B32G32R32F, 256 x 2 (row 0 this frame, row 1 last frame), 3 texels per bone slot.
    constexpr uint32_t kPaletteWidth = 256;

    static_assert(sizeof(C44Matrix) == sizeof(Mat4), "C44Matrix and Mat4 must share a layout");

    IDirect3DVertexShader9* s_fullscreenVS;
    IDirect3DVertexShader9* s_skinnedVS;
    IDirect3DPixelShader9* s_staticPS;
    IDirect3DPixelShader9* s_motionPS;
    IDirect3DPixelShader9* s_maskPS;
    IDirect3DPixelShader9* s_viewPS;
    IDirect3DVertexDeclaration9* s_quadDecl;
    IDirect3DVertexDeclaration9* s_modelDecl;
    IDirect3DStateBlock9* s_block;
    // Created lazily with the model pass; nullptr when the device has no A32B32G32R32F vertex textures.
    IDirect3DTexture9* s_palette;
    // Creation failed (logged): the passes stay off until Release (device reset / pipeline off) tries again.
    bool s_failed;
    // -1 unknown, 0 no A32B32G32R32F vertex textures (model pass off), 1 yes. Asked again after Release.
    int s_vertexTextures = -1;
    // OpaqueDone copied the opaque world this world render (cleared by BeforeCopyBack).
    bool s_snapshotTaken;
    // Frame::index of the last BeforeCopyBack: a world render without a new camera record repeats it.
    bool s_haveLastIndex;
    uint32_t s_lastIndex;

    // One-time logs (per process).
    bool s_loggedOptimized;
    bool s_loggedBones;
    bool s_loggedProjection;
    bool s_loggedViewport;
    bool s_loggedVertexTextures;

    void LogOnce(bool& logged, const char* message) {
        if (!logged) {
            logged = true;
            CoA::Log::Printf("%s", message);
        }
    }

    Mat4 Mul(const Mat4& a, const Mat4& b) {
        Mat4 r{};

        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                r.m[i][j] = a.m[i][0] * b.m[0][j] + a.m[i][1] * b.m[1][j] + a.m[i][2] * b.m[2][j] + a.m[i][3] * b.m[3][j];
            }
        }

        return r;
    }

    // The device's native projection for a GL-style one, exactly as CGxDeviceD3d::XformSetProjection derives
    // m_projNative (c3 normalised to 1, z remapped to D3D's [0, 1]). The 0.2 scale it applies without
    // GxMasterEnable_NormalProjection is not reproduced: the passes do not run in that state.
    Mat4 NativeProjection(const Mat4& matrix) {
        Mat4 p = matrix;
        float& c2 = p.m[2][2];
        float& c3 = p.m[2][3];
        float& d2 = p.m[3][2];
        float& d3 = p.m[3][3];

        if (!(std::fabs(c3 - 1.0f) < 0.00000023841858f) && !(std::fabs(c3) < 0.00000023841858f)) {
            const float scale = 1.0f / c3;

            for (auto& row : p.m) {
                for (float& v : row) {
                    v *= scale;
                }
            }
        }

        if (d3 == 0.0f) {
            const float minZ = -(d2 / (c2 + 1.0f));
            const float maxZ = -(d2 / (c2 - 1.0f));
            c2 = maxZ / (maxZ - minZ);
            d2 = maxZ * minZ / (minZ - maxZ);
        } else {
            const float invC2 = 1.0f / c2;
            const float minZ = (-1.0f - d2) * invC2;
            const float maxZ = (1.0f - d2) * invC2;
            c2 = 1.0f / (maxZ - minZ);
            d2 = minZ / (minZ - maxZ);
        }

        return p;
    }

    DWORD FloatBits(float value) {
        DWORD bits;
        memcpy(&bits, &value, sizeof(bits));
        return bits;
    }

    void ReleaseObjects() {
        IUnknown* objects[] = { s_fullscreenVS, s_skinnedVS, s_staticPS, s_motionPS, s_maskPS, s_viewPS, s_quadDecl, s_modelDecl, s_block, s_palette };

        for (IUnknown* object : objects) {
            if (object) {
                object->Release();
            }
        }

        s_fullscreenVS = nullptr;
        s_skinnedVS = nullptr;
        s_staticPS = nullptr;
        s_motionPS = nullptr;
        s_maskPS = nullptr;
        s_viewPS = nullptr;
        s_quadDecl = nullptr;
        s_modelDecl = nullptr;
        s_block = nullptr;
        s_palette = nullptr;
    }

    // Shaders, declarations and the state block; false (logged once until Release) when any creation fails.
    bool CreateObjects(IDirect3DDevice9* device) {
        if (s_block) {
            return true;
        }

        if (s_failed) {
            return false;
        }

        const D3DVERTEXELEMENT9 quad[] = {
            { 0, 0, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
            { 0, 16, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
            D3DDECL_END()
        };
        // M2Vertex (the shared vertex buffer of CM2Shared::SetVertices with shaders): position, weights, indices.
        const D3DVERTEXELEMENT9 model[] = {
            { 0, offsetof(M2Vertex, m_position), D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
            { 0, offsetof(M2Vertex, m_weights), D3DDECLTYPE_UBYTE4N, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_BLENDWEIGHT, 0 },
            { 0, offsetof(M2Vertex, m_indices), D3DDECLTYPE_UBYTE4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_BLENDINDICES, 0 },
            D3DDECL_END()
        };

        if (FAILED(device->CreateVertexShader(reinterpret_cast<const DWORD*>(kFullscreenVS), &s_fullscreenVS))
            || FAILED(device->CreateVertexShader(reinterpret_cast<const DWORD*>(kSkinnedMotionVS), &s_skinnedVS))
            || FAILED(device->CreatePixelShader(reinterpret_cast<const DWORD*>(kStaticMotionPS), &s_staticPS))
            || FAILED(device->CreatePixelShader(reinterpret_cast<const DWORD*>(kMotionPS), &s_motionPS))
            || FAILED(device->CreatePixelShader(reinterpret_cast<const DWORD*>(kBiasMaskPS), &s_maskPS))
            || FAILED(device->CreatePixelShader(reinterpret_cast<const DWORD*>(kMotionViewPS), &s_viewPS))
            || FAILED(device->CreateVertexDeclaration(quad, &s_quadDecl))
            || FAILED(device->CreateVertexDeclaration(model, &s_modelDecl))
            || FAILED(device->CreateStateBlock(D3DSBT_ALL, &s_block))) {
            ReleaseObjects();
            s_failed = true;
            CoA::Log::Printf("RenderMotion: motion passes unavailable (D3D9 object creation failed)");
            return false;
        }

        return true;
    }

    // The palette texture, when the device can sample A32B32G32R32F in the vertex shader. Nullptr: no model pass.
    IDirect3DTexture9* Palette(IDirect3DDevice9* device) {
        if (s_palette || s_vertexTextures == 0) {
            return s_palette;
        }

        if (s_vertexTextures < 0) {
            s_vertexTextures = 0;
            IDirect3D9* d3d = nullptr;
            D3DDEVICE_CREATION_PARAMETERS creation;
            D3DDISPLAYMODE mode;

            if (SUCCEEDED(device->GetDirect3D(&d3d)) && d3d) {
                if (SUCCEEDED(device->GetCreationParameters(&creation)) && SUCCEEDED(device->GetDisplayMode(0, &mode))
                    && SUCCEEDED(d3d->CheckDeviceFormat(creation.AdapterOrdinal, creation.DeviceType, mode.Format, D3DUSAGE_QUERY_VERTEXTEXTURE, D3DRTYPE_TEXTURE, D3DFMT_A32B32G32R32F))) {
                    s_vertexTextures = 1;
                }

                d3d->Release();
            }

            if (!s_vertexTextures) {
                LogOnce(s_loggedVertexTextures, "RenderMotion: no A32B32G32R32F vertex textures, the model pass stays off (camera motion only)");
                return nullptr;
            }
        }

        if (FAILED(device->CreateTexture(kPaletteWidth, 2, 1, D3DUSAGE_DYNAMIC, D3DFMT_A32B32G32R32F, D3DPOOL_DEFAULT, &s_palette, nullptr))) {
            s_palette = nullptr;
            s_vertexTextures = 0;
            LogOnce(s_loggedVertexTextures, "RenderMotion: palette texture creation failed, the model pass stays off (camera motion only)");
        }

        return s_palette;
    }

    void PointClamp(IDirect3DDevice9* device, DWORD sampler) {
        device->SetSamplerState(sampler, D3DSAMP_MINFILTER, D3DTEXF_POINT);
        device->SetSamplerState(sampler, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
        device->SetSamplerState(sampler, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
        device->SetSamplerState(sampler, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
        device->SetSamplerState(sampler, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
        device->SetSamplerState(sampler, D3DSAMP_ADDRESSW, D3DTADDRESS_CLAMP);
        device->SetSamplerState(sampler, D3DSAMP_SRGBTEXTURE, FALSE);
        // Explicitly not fetch4 (D3DSAMP_MIPMAPLODBIAS == 'GET4' enables it in DXVK), whatever the engine left there.
        device->SetSamplerState(sampler, D3DSAMP_MIPMAPLODBIAS, 0);
        device->SetSamplerState(sampler, D3DSAMP_MAXMIPLEVEL, 0);
    }

    // The fixed-function states every pass shares (the state block restores the engine's afterwards).
    void CommonStates(IDirect3DDevice9* device) {
        device->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
        device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
        device->SetRenderState(D3DRS_DEPTHBIAS, 0);
        device->SetRenderState(D3DRS_SLOPESCALEDEPTHBIAS, 0);
        device->SetRenderState(D3DRS_STENCILENABLE, FALSE);
        device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
        device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
        device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
        device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
        device->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
        device->SetRenderState(D3DRS_COLORWRITEENABLE, 0xF);
        device->SetRenderState(D3DRS_CLIPPLANEENABLE, 0);
        device->SetRenderState(D3DRS_SRGBWRITEENABLE, FALSE);
        device->SetRenderState(D3DRS_FOGENABLE, FALSE);
        device->SetRenderState(D3DRS_WRAP0, 0);
    }

    // A quad over the bound render target (width x height) with FullscreenVS; uv 0..1 over it. The caller sets the
    // pixel shader, its constants and textures.
    void DrawQuad(IDirect3DDevice9* device, uint32_t width, uint32_t height) {
        const float w = static_cast<float>(width);
        const float h = static_cast<float>(height);
        // Clip-space strip with the D3D9 half-pixel offset applied by the vertex shader (c0).
        const float offset[4] = { -1.0f / w, 1.0f / h, 0.0f, 0.0f };
        const float quad[4][6] = {
            { -1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f },
            { 1.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f },
            { -1.0f, -1.0f, 0.0f, 1.0f, 0.0f, 1.0f },
            { 1.0f, -1.0f, 0.0f, 1.0f, 1.0f, 1.0f }
        };

        device->SetVertexDeclaration(s_quadDecl);
        device->SetVertexShader(s_fullscreenVS);
        device->SetVertexShaderConstantF(0, offset, 1);
        device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, quad, sizeof(quad[0]));
    }

    // Sets render target 0 (and resets the viewport to it: full size, depth 0..1). Nullptr surfaces are skipped.
    bool BindTarget(IDirect3DDevice9* device, IDirect3DTexture9* texture) {
        IDirect3DSurface9* surface = nullptr;

        if (FAILED(texture->GetSurfaceLevel(0, &surface)) || !surface) {
            return false;
        }

        const HRESULT result = device->SetRenderTarget(0, surface);
        surface->Release();

        return SUCCEEDED(result);
    }

    void SetViewport(IDirect3DDevice9* device, uint32_t width, uint32_t height, float minZ, float maxZ) {
        D3DVIEWPORT9 viewport = { 0, 0, width, height, minZ, maxZ };
        device->SetViewport(&viewport);
    }

    // Applies the captured state and makes sure none of the given textures stays bound to a pixel sampler or the first
    // vertex sampler (a held D3DPOOL_DEFAULT texture fails the next Reset).
    void RestoreState(IDirect3DDevice9* device, IDirect3DBaseTexture9* const* ours, size_t count) {
        s_block->Apply();

        const DWORD samplers[] = { 0, 1, D3DVERTEXTEXTURESAMPLER0 };

        for (DWORD sampler : samplers) {
            IDirect3DBaseTexture9* bound = nullptr;

            if (FAILED(device->GetTexture(sampler, &bound)) || !bound) {
                continue;
            }

            for (size_t i = 0; i < count; ++i) {
                if (ours[i] && bound == ours[i]) {
                    device->SetTexture(sampler, nullptr);
                    break;
                }
            }

            bound->Release();
        }
    }

    bool MotionOn(CGxDevice* gx) {
        CVar* var = *CoA::RenderPipelineClient::MotionSlot();
        return var && var->m_intValue != 0 && gx && gx->D3dDevice() && gx->WorldMotionSupported();
    }

    // 1. Static pass: camera motion of every pixel from the INTZ depth, into the bound motion target.
    void StaticPass(IDirect3DDevice9* device, IDirect3DBaseTexture9* depth, const Frame& frame, uint32_t width, uint32_t height) {
        const Mat4 invCur = CoA::RenderMotion::Inverse(Mul(frame.cur.view, frame.cur.proj));
        const Mat4 prevViewProj = Mul(frame.prev.view, frame.prev.proj);
        // c0..c3 inverse(cur.view * cur.proj) rows, c4..c7 prev.view * prev.proj rows, c8 = cur.pos - prev.pos,
        // c9 = (depthMin, 1 / (depthMax - depthMin), depthMax, 0).
        float constants[10][4];
        memcpy(constants[0], invCur.m, sizeof(invCur.m));
        memcpy(constants[4], prevViewProj.m, sizeof(prevViewProj.m));
        constants[8][0] = frame.cur.pos[0] - frame.prev.pos[0];
        constants[8][1] = frame.cur.pos[1] - frame.prev.pos[1];
        constants[8][2] = frame.cur.pos[2] - frame.prev.pos[2];
        constants[8][3] = 0.0f;
        constants[9][0] = frame.depthMin;
        constants[9][1] = 1.0f / (frame.depthMax - frame.depthMin);
        constants[9][2] = frame.depthMax;
        constants[9][3] = 0.0f;

        PointClamp(device, 0);
        device->SetTexture(0, depth);
        device->SetPixelShader(s_staticPS);
        device->SetPixelShaderConstantF(0, constants[0], 10);
        DrawQuad(device, width, height);
        device->SetTexture(0, nullptr);
    }

    struct ModelPrevious {
        const Mat4* bones;      // nullptr: not drawn in the model pass
        uint32_t count;
    };

    // Last frame's bones of a model the model pass draws (ModelNeedsPass), else bones = nullptr.
    ModelPrevious PreviousFor(const CM2Model* model, const CM2Scene* scene, const Frame& frame) {
        ModelPrevious result = { nullptr, 0 };

        if (!model->m_boneMatrices || !model->m_shared || !model->m_shared->m_data) {
            return result;
        }

        const MotionCache::Key key = { model, model->m_handle };
        uint32_t count = 0;
        const Mat4* bones = MotionCache::Previous(key, frame.index, count);
        const Mat4* world = MotionCache::PreviousWorld(key, frame.index);
        const bool worldChanged = world && memcmp(world, &model->m_worldTransform, sizeof(Mat4)) != 0;
        // A model whose bone count changed (reloaded under the same pointer and handle) has no usable history.
        const bool havePrevious = bones && count == model->m_shared->m_data->m_bones.m_count;

        if (CoA::RenderMotion::ModelNeedsPass(model->m_frameStamp == scene->m_frameStamp, (model->m_fFlags & kModelCulled) != 0,
                                              (model->m_fFlags & kModelStaticSimple) != 0, worldChanged, havePrevious)) {
            result.bones = bones;
            result.count = count;
        }

        return result;
    }

    // Packs one row of the palette: slot i = model bone m_boneCombos[boneComboIndex + i], 3 texels holding the
    // engine's c31+ registers for it (texel k = column k of the matrix: a_k, b_k, c_k, d_k; CM2SceneRender::DrawBatch).
    // False when a slot names a bone outside the array.
    bool PackRow(float* row, const M2Data* data, const M2SkinSection* section, const Mat4* bones, uint32_t boneCount) {
        for (uint32_t i = 0; i < section->m_boneCount; ++i) {
            const uint32_t bone = data->m_boneCombos[section->m_boneComboIndex + i];

            if (bone >= boneCount) {
                return false;
            }

            const Mat4& m = bones[bone];

            for (uint32_t k = 0; k < 3; ++k) {
                float* texel = row + (3 * i + k) * 4;
                texel[0] = m.m[0][k];
                texel[1] = m.m[1][k];
                texel[2] = m.m[2][k];
                texel[3] = m.m[3][k];
            }
        }

        return true;
    }

    // 2. Model pass: the opaque batches of every model that moved on its own (skinned, or a static model whose world
    // transform changed) re-drawn with SkinnedMotionVS / MotionPS, depth-tested against the world's INTZ.
    void ModelPass(CGxDevice* gx, IDirect3DDevice9* device, IDirect3DTexture9* palette, IDirect3DSurface9* depthSurface, const Frame& frame, uint32_t width, uint32_t height) {
        CM2Scene* scene = World::s_m2Scene;

        if (!scene) {
            return;
        }

        // c0..c3: the projection the world rasterised with (the jittered cur.proj in the device's native form), so the
        // depth test lines up; c4..c7 / c8..c11: the unjittered GL-style cur.proj / prev.proj (the motion uses .xyw,
        // which the native form leaves the same up to a common factor of w).
        const Mat4 renderProj = NativeProjection(ApplyJitter(frame.cur.proj, frame.jitterX, frame.jitterY, width, height));
        float constants[12][4];
        memcpy(constants[0], renderProj.m, sizeof(renderProj.m));
        memcpy(constants[4], frame.cur.proj.m, sizeof(frame.cur.proj.m));
        memcpy(constants[8], frame.prev.proj.m, sizeof(frame.prev.proj.m));

        device->SetDepthStencilSurface(depthSurface);
        SetViewport(device, width, height, frame.depthMin, frame.depthMax);
        device->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
        device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
        device->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
        device->SetRenderState(D3DRS_DEPTHBIAS, FloatBits(kDepthBias));
        device->SetRenderState(D3DRS_SLOPESCALEDEPTHBIAS, FloatBits(kSlopeScaleDepthBias));
        device->SetVertexDeclaration(s_modelDecl);
        device->SetVertexShader(s_skinnedVS);
        device->SetPixelShader(s_motionPS);
        device->SetVertexShaderConstantF(0, constants[0], 12);
        PointClamp(device, D3DVERTEXTEXTURESAMPLER0);
        device->SetTexture(D3DVERTEXTEXTURESAMPLER0, palette);

        // Stream 0 and the index buffer are put back by hand as well as by the state block: the device caches them
        // (ISetVertexBuffer, IStateSync) and skips re-binding a buffer it believes is still bound.
        IDirect3DVertexBuffer9* savedStream = nullptr;
        UINT savedOffset = 0;
        UINT savedStride = 0;
        IDirect3DIndexBuffer9* savedIndices = nullptr;
        device->GetStreamSource(0, &savedStream, &savedOffset, &savedStride);
        device->GetIndices(&savedIndices);

        std::unordered_map<const CM2Model*, ModelPrevious> previous;
        const TSGrowableArray<uint32_t>& list = scene->m_passElements[0];

        for (uint32_t i = 0; i < list.m_count; ++i) {
            const M2Element* element = &scene->m_elements.m_data[list.m_data[i]];

            if (element->m_type != 0 || !element->m_model || !element->m_batch || !element->m_skinSection) {
                continue;
            }

            if (element->m_flags & kElementOptimized) {
                LogOnce(s_loggedOptimized, "RenderMotion: optimized-geometry M2 batches are not in the model pass (camera motion only)");
                continue;
            }

            const CM2Model* model = element->m_model;
            auto found = previous.find(model);

            if (found == previous.end()) {
                found = previous.emplace(model, PreviousFor(model, scene, frame)).first;
            }

            const ModelPrevious prev = found->second;

            if (!prev.bones) {
                continue;
            }

            const CM2Shared* shared = model->m_shared;
            const M2Data* data = shared->m_data;
            const M2SkinSection* section = element->m_skinSection;

            // Opaque materials only: alpha-keyed batches would write model motion over their cut-out texels (the pass
            // has no alpha test), so those keep the camera motion.
            if (data->m_materials[element->m_batch->m_materialIndex].m_blendMode != M2BLEND_OPAQUE || section->m_boneCount == 0 || section->m_indexCount < 3) {
                continue;
            }

            if (section->m_boneCount > MotionCache::kMaxBones) {
                LogOnce(s_loggedBones, "RenderMotion: an M2 batch has more bones than the palette holds; it keeps the camera motion");
                continue;
            }

            // The geometry the world drew for this batch: CM2Shared's index and vertex buffers (SetIndices /
            // SetVertices(0), both already uploaded for this frame's draw), the range of CM2SceneRender::DrawBatch.
            const CGxBuf* vertexBuf = shared->m_vertexBuf;
            const CGxBuf* indexBuf = shared->m_indexBuf;

            if (!vertexBuf || !indexBuf || !vertexBuf->m_pool || !indexBuf->m_pool || !vertexBuf->m_pool->m_apiSpecific || !indexBuf->m_pool->m_apiSpecific
                || !vertexBuf->m_hasData || !vertexBuf->m_valid || !indexBuf->m_hasData || !indexBuf->m_valid || vertexBuf->m_itemSize != sizeof(M2Vertex)) {
                continue;
            }

            D3DLOCKED_RECT locked;

            if (FAILED(palette->LockRect(0, &locked, nullptr, D3DLOCK_DISCARD))) {
                continue;
            }

            float* row0 = static_cast<float*>(locked.pBits);
            float* row1 = reinterpret_cast<float*>(static_cast<uint8_t*>(locked.pBits) + locked.Pitch);
            const uint32_t boneCount = data->m_bones.m_count;
            const bool packed = PackRow(row0, data, section, reinterpret_cast<const Mat4*>(model->m_boneMatrices), boneCount)
                && PackRow(row1, data, section, prev.bones, prev.count);
            palette->UnlockRect(0);

            if (!packed) {
                continue;
            }

            // As CGxDeviceD3d::Draw / ISetVertexBuffer: a byte offset into the stream, or a base vertex without stream
            // offsets.
            uint32_t offset = vertexBuf->m_index;
            INT baseVertex = 0;

            if (!gx->m_caps.m_streamOffset) {
                offset = 0;
                baseVertex = static_cast<INT>(vertexBuf->m_index / vertexBuf->m_itemSize);
            }

            device->SetStreamSource(0, static_cast<IDirect3DVertexBuffer9*>(vertexBuf->m_pool->m_apiSpecific), offset, vertexBuf->m_itemSize);
            device->SetIndices(static_cast<IDirect3DIndexBuffer9*>(indexBuf->m_pool->m_apiSpecific));
            device->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, baseVertex, section->m_vertexStart, section->m_vertexCount,
                                         section->m_indexStart + (indexBuf->m_index >> 1), section->m_indexCount / 3);
        }

        device->SetTexture(D3DVERTEXTEXTURESAMPLER0, nullptr);
        device->SetStreamSource(0, savedStream, savedOffset, savedStride);
        device->SetIndices(savedIndices);

        if (savedStream) {
            savedStream->Release();
        }

        if (savedIndices) {
            savedIndices->Release();
        }
    }

    // 3. Mask pass: white where the final world colour differs from the opaque snapshot (translucency drawn over it).
    void MaskPass(IDirect3DDevice9* device, IDirect3DBaseTexture9* color, IDirect3DBaseTexture9* snapshot, uint32_t width, uint32_t height) {
        PointClamp(device, 0);
        PointClamp(device, 1);
        device->SetTexture(0, color);
        device->SetTexture(1, snapshot);
        device->SetPixelShader(s_maskPS);
        DrawQuad(device, width, height);
        device->SetTexture(0, nullptr);
        device->SetTexture(1, nullptr);
    }

    // 4. This frame's bones and world transform of every model animated this frame and not culled (drawn in the model
    // pass or not), for next frame's model pass; models above kMaxBones are not stored.
    void StoreBones(const Frame& frame) {
        if (frame.reset) {
            MotionCache::Clear();
        }

        if (const CM2Scene* scene = World::s_m2Scene) {
            for (const CM2Model* model = scene->m_modelList; model; model = model->m_sceneNext) {
                if (model->m_frameStamp != scene->m_frameStamp || (model->m_fFlags & kModelCulled) || !model->m_boneMatrices || !model->m_shared || !model->m_shared->m_data) {
                    continue;
                }

                const MotionCache::Key key = { model, model->m_handle };
                MotionCache::Store(key, frame.index, *reinterpret_cast<const Mat4*>(&model->m_worldTransform),
                                   reinterpret_cast<const Mat4*>(model->m_boneMatrices), model->m_shared->m_data->m_bones.m_count);
            }
        }

        MotionCache::EndFrame(frame.index);
    }

    uint32_t TextureWidth(IDirect3DTexture9* texture, uint32_t& height) {
        D3DSURFACE_DESC desc;

        if (FAILED(texture->GetLevelDesc(0, &desc))) {
            height = 0;
            return 0;
        }

        height = desc.Height;
        return desc.Width;
    }
}

void CoA::RenderMotionClient::OpaqueDone() {
    CGxDevice* gx = g_theGxDevicePtr;
    s_snapshotTaken = false;

    if (!MotionOn(gx)) {
        return;
    }

    // Ensure first: a Begin at a new size leaves the old-size motion targets in place until then.
    s_snapshotTaken = gx->WorldMotionTargetsEnsure() && gx->WorldSnapshotOpaque();
}

void CoA::RenderMotionClient::BeforeCopyBack() {
    CGxDevice* gx = g_theGxDevicePtr;
    const bool snapshotTaken = s_snapshotTaken;
    s_snapshotTaken = false;

    if (!MotionOn(gx) || !gx->WorldMotionTargetsEnsure()) {
        return;
    }

    IDirect3DDevice9* device = static_cast<IDirect3DDevice9*>(gx->D3dDevice());
    IDirect3DTexture9* motion = static_cast<IDirect3DTexture9*>(gx->WorldMotionTexture());
    IDirect3DTexture9* mask = static_cast<IDirect3DTexture9*>(gx->WorldMaskTexture());
    IDirect3DTexture9* snapshot = static_cast<IDirect3DTexture9*>(gx->WorldSnapshotTexture());
    IDirect3DTexture9* color = static_cast<IDirect3DTexture9*>(gx->WorldColorTexture());
    IDirect3DTexture9* depth = static_cast<IDirect3DTexture9*>(gx->WorldDepthTexture());

    if (!motion || !mask || !snapshot || !color || !depth || !CreateObjects(device)) {
        return;
    }

    uint32_t height = 0;
    const uint32_t width = TextureWidth(motion, height);

    if (!width || !height) {
        return;
    }

    const Frame& frame = Current();
    // A world render without a new camera record (BeginFrame did not run) repeats the last index: nothing new to
    // reproject, and storing would overwrite this index's bones.
    const bool repeated = s_haveLastIndex && frame.index == s_lastIndex;
    s_haveLastIndex = true;
    s_lastIndex = frame.index;

    // Without GxMasterEnable_NormalProjection the device scales m_projNative by 0.2 and the world's depth no longer
    // follows Frame.proj.
    const bool normalProjection = gx->MasterEnable(GxMasterEnable_NormalProjection) != 0;

    if (!normalProjection) {
        LogOnce(s_loggedProjection, "RenderMotion: GxMasterEnable_NormalProjection is off, motion is cleared for such frames");
    }

    // The motion convention is in UV of the whole world target; a world viewport that covers only part of it (the
    // projection maps onto the viewport) is not handled.
    const CBoundingBox& vp = gx->m_viewport;
    const bool fullViewport = std::fabs(vp.m_x.m_l) < 1e-4f && std::fabs(vp.m_x.m_h - 1.0f) < 1e-4f && std::fabs(vp.m_y.m_l) < 1e-4f && std::fabs(vp.m_y.m_h - 1.0f) < 1e-4f;

    if (!fullViewport) {
        LogOnce(s_loggedViewport, "RenderMotion: the world viewport does not cover the window, motion is cleared for such frames");
    }

    // Reset (history restarts) or an empty depth range (c9.y would be 1 / 0): motion is cleared to (0, 0).
    const bool reproject = !repeated && normalProjection && fullViewport && !frame.reset && frame.depthMax != frame.depthMin;

    IDirect3DSurface9* savedTarget = nullptr;
    IDirect3DSurface9* savedDepth = nullptr;
    IDirect3DSurface9* depthSurface = nullptr;

    if (FAILED(s_block->Capture())) {
        return;
    }

    device->GetRenderTarget(0, &savedTarget);
    device->GetDepthStencilSurface(&savedDepth);
    CommonStates(device);

    // 1. Static pass (or the reset clear), no depth surface bound while INTZ is sampled.
    device->SetDepthStencilSurface(nullptr);

    if (BindTarget(device, motion)) {
        SetViewport(device, width, height, 0.0f, 1.0f);

        if (reproject) {
            StaticPass(device, depth, frame, width, height);
        } else {
            device->Clear(0, nullptr, D3DCLEAR_TARGET, 0, 1.0f, 0);
        }

        // 2. Model pass, over the static result.
        IDirect3DTexture9* palette = reproject ? Palette(device) : nullptr;

        if (palette && SUCCEEDED(depth->GetSurfaceLevel(0, &depthSurface)) && depthSurface) {
            ModelPass(gx, device, palette, depthSurface, frame, width, height);
            device->SetDepthStencilSurface(nullptr);
            CommonStates(device);
        }
    }

    // 3. Mask pass (cleared when OpaqueDone did not snapshot this world render, e.g. no M2 scene).
    if (BindTarget(device, mask)) {
        SetViewport(device, width, height, 0.0f, 1.0f);

        if (snapshotTaken) {
            MaskPass(device, color, snapshot, width, height);
        } else {
            device->Clear(0, nullptr, D3DCLEAR_TARGET, 0, 1.0f, 0);
        }
    }

    // Render targets are not part of the state block: put the world target's surfaces back, then the rest.
    device->SetRenderTarget(0, savedTarget);
    device->SetDepthStencilSurface(savedDepth);

    IDirect3DSurface9* surfaces[] = { savedTarget, savedDepth, depthSurface };

    for (IDirect3DSurface9* surface : surfaces) {
        if (surface) {
            surface->Release();
        }
    }

    IDirect3DBaseTexture9* ours[] = { motion, mask, snapshot, color, depth, s_palette };
    RestoreState(device, ours, sizeof(ours) / sizeof(ours[0]));

    // 4. Next frame's history.
    if (!repeated) {
        StoreBones(frame);
    }
}

bool CoA::RenderMotionClient::DrawDebugView(int mode) {
    CGxDevice* gx = g_theGxDevicePtr;

    if (!MotionOn(gx)) {
        return false;
    }

    IDirect3DDevice9* device = static_cast<IDirect3DDevice9*>(gx->D3dDevice());
    IDirect3DBaseTexture9* source = static_cast<IDirect3DBaseTexture9*>(mode == 4 ? gx->WorldMaskTexture() : gx->WorldMotionTexture());

    if (!source || !CreateObjects(device) || FAILED(s_block->Capture())) {
        return false;
    }

    // The quad covers the bound render target (the back buffer, WorldTargetEnd has run), as the depth view does.
    uint32_t width = 0;
    uint32_t height = 0;
    IDirect3DSurface9* target = nullptr;

    if (SUCCEEDED(device->GetRenderTarget(0, &target)) && target) {
        D3DSURFACE_DESC desc;

        if (SUCCEEDED(target->GetDesc(&desc))) {
            width = desc.Width;
            height = desc.Height;
        }

        target->Release();
    }

    if (!width || !height) {
        return false;
    }

    // c0.x: 0 = motion hue view (coaRenderDebug 3), 1 = grey mask (coaRenderDebug 4).
    const float view[4] = { mode == 4 ? 1.0f : 0.0f, 0.0f, 0.0f, 0.0f };
    CommonStates(device);
    SetViewport(device, width, height, 0.0f, 1.0f);
    PointClamp(device, 0);
    device->SetTexture(0, source);
    device->SetPixelShader(s_viewPS);
    device->SetPixelShaderConstantF(0, view, 1);
    DrawQuad(device, width, height);

    IDirect3DBaseTexture9* ours[] = { source };
    RestoreState(device, ours, 1);

    return true;
}

void CoA::RenderMotionClient::Release() {
    ReleaseObjects();
    s_failed = false;
    s_vertexTextures = -1;
    s_snapshotTaken = false;
    s_haveLastIndex = false;
    MotionCache::Clear();
}
