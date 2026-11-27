// RenderMotion's engine half: the motion passes of CoA64's render pipeline (stage 2), run by RenderPipelineClient while
// (continued: docs/2026-10-29-render-pipeline-notes.md, RenderMotionClient.cpp #1)
#include "CoA/Shaders/BiasMaskPS.h"
#include "CoA/Shaders/FullscreenVS.h"
#include "CoA/Shaders/MotionKeyPS.h"
#include "CoA/Shaders/MotionPS.h"
#include "CoA/Shaders/MotionViewPS.h"
#include "CoA/Shaders/SkinnedMotionVS.h"
#include "CoA/Shaders/StaticMotionPS.h"
#include "CoA/Source/Runtime/Dlss.h"
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
#include "Engine/Source/Services/Texture.h"
#include "Engine/Source/Tempest/C44Matrix.h"
#include "WoW/Source/WorldClient/World.h"
#include <d3d9.h>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <vector>

namespace {
    using namespace CoA::RenderPipeline;
    namespace MotionCache = CoA::MotionCache;

    // The model pass's depth test against the world's INTZ (read-only, LESSEQUAL): the re-drawn batches land on the
    // (continued: docs/2026-10-29-render-pipeline-notes.md, RenderMotionClient.cpp #2)
    constexpr float kDepthBias = -0.00002f;
    constexpr float kSlopeScaleDepthBias = -1.0f;

    // M2Model.cpp / CM2SceneRender.cpp flags: m_fFlags 0x1000 = animated with AnimateMTSimple (static model), 0x2000 =
    // culled (not drawn); M2Element::m_flags 4 = optimized geometry (the model's own CPU-skinned buffers).
    constexpr uint32_t kModelStaticSimple = 0x1000;
    constexpr uint32_t kModelCulled = 0x2000;
    constexpr uint32_t kElementOptimized = 4;

    // Palette: A32B32G32R32F texel stream (RenderMotion::kPaletteStreamWidth wide, bucketed heights), 3 texels per bone;
    // a slot holds this frame's bones then last frame's. At most kMaxBatches slots per frame.
    constexpr uint32_t kMaxBatches = 512;
    // SkinnedMotionVS divides the row by this height.

    static_assert(sizeof(C44Matrix) == sizeof(Mat4), "C44Matrix and Mat4 must share a layout");

    IDirect3DVertexShader9* s_fullscreenVS;
    IDirect3DVertexShader9* s_skinnedVS;
    IDirect3DPixelShader9* s_staticPS;
    IDirect3DPixelShader9* s_motionPS;
    IDirect3DPixelShader9* s_keyPS;
    IDirect3DPixelShader9* s_maskPS;
    IDirect3DPixelShader9* s_viewPS;
    IDirect3DVertexDeclaration9* s_quadDecl;
    IDirect3DVertexDeclaration9* s_modelDecl;
    // Created lazily with the model pass; nullptr when the device has no A32B32G32R32F vertex textures.
    IDirect3DTexture9* s_palettes[CoA::RenderMotion::kPaletteBuckets];
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
    bool s_loggedLockFailed;
    bool s_loggedTargets;

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
    // (continued: docs/2026-10-29-render-pipeline-notes.md, RenderMotionClient.cpp #3)
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
        IUnknown* objects[] = { s_fullscreenVS, s_skinnedVS, s_staticPS, s_motionPS, s_keyPS, s_maskPS, s_viewPS, s_quadDecl, s_modelDecl, s_palettes[0], s_palettes[1], s_palettes[2], s_palettes[3] };

        for (IUnknown* object : objects) {
            if (object) {
                object->Release();
            }
        }

        s_fullscreenVS = nullptr;
        s_skinnedVS = nullptr;
        s_staticPS = nullptr;
        s_motionPS = nullptr;
        s_keyPS = nullptr;
        s_maskPS = nullptr;
        s_viewPS = nullptr;
        s_quadDecl = nullptr;
        s_modelDecl = nullptr;
        std::fill(std::begin(s_palettes), std::end(s_palettes), nullptr);
    }

    // Shaders and declarations; false (logged once until Release) when any creation fails.
    bool CreateObjects(IDirect3DDevice9* device) {
        if (s_modelDecl) {
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
        // M2Vertex (the shared vertex buffer of CM2Shared::SetVertices with shaders): position, weights, indices, uv 0.
        const D3DVERTEXELEMENT9 model[] = {
            { 0, offsetof(M2Vertex, m_position), D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
            { 0, offsetof(M2Vertex, m_weights), D3DDECLTYPE_UBYTE4N, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_BLENDWEIGHT, 0 },
            { 0, offsetof(M2Vertex, m_indices), D3DDECLTYPE_UBYTE4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_BLENDINDICES, 0 },
            { 0, offsetof(M2Vertex, m_texcoord), D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
            D3DDECL_END()
        };

        if (FAILED(device->CreateVertexShader(reinterpret_cast<const DWORD*>(kFullscreenVS), &s_fullscreenVS))
            || FAILED(device->CreateVertexShader(reinterpret_cast<const DWORD*>(kSkinnedMotionVS), &s_skinnedVS))
            || FAILED(device->CreatePixelShader(reinterpret_cast<const DWORD*>(kStaticMotionPS), &s_staticPS))
            || FAILED(device->CreatePixelShader(reinterpret_cast<const DWORD*>(kMotionPS), &s_motionPS))
            || FAILED(device->CreatePixelShader(reinterpret_cast<const DWORD*>(kMotionKeyPS), &s_keyPS))
            || FAILED(device->CreatePixelShader(reinterpret_cast<const DWORD*>(kBiasMaskPS), &s_maskPS))
            || FAILED(device->CreatePixelShader(reinterpret_cast<const DWORD*>(kMotionViewPS), &s_viewPS))
            || FAILED(device->CreateVertexDeclaration(quad, &s_quadDecl))
            || FAILED(device->CreateVertexDeclaration(model, &s_modelDecl))) {
            ReleaseObjects();
            s_failed = true;
            CoA::Log::Printf("RenderMotion: motion passes unavailable (D3D9 object creation failed)");
            return false;
        }

        return true;
    }

    // The device can sample A32B32G32R32F in the vertex shader (checked once). False: no model pass.
    bool PaletteSupported(IDirect3DDevice9* device) {
        if (s_vertexTextures >= 0) {
            return s_vertexTextures != 0;
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
                return false;
            }
        }

        return true;
    }

    // The palette texture of a bucket, created on first use; nullptr when creation fails (logged once).
    IDirect3DTexture9* PaletteTexture(IDirect3DDevice9* device, uint32_t bucket) {
        IDirect3DTexture9*& texture = s_palettes[bucket];

        if (!texture && FAILED(device->CreateTexture(CoA::RenderMotion::kPaletteStreamWidth, CoA::RenderMotion::kPaletteBucketRows[bucket], 1,
                D3DUSAGE_DYNAMIC, D3DFMT_A32B32G32R32F, D3DPOOL_DEFAULT, &texture, nullptr))) {
            texture = nullptr;
            LogOnce(s_loggedVertexTextures, "RenderMotion: palette texture creation failed, the model pass is skipped (camera motion only)");
        }

        return texture;
    }

    // Point / clamp sampling. Only the sampler states the engine itself manages are set (ExternalStateRestore re-sends
    // (continued: docs/2026-10-29-render-pipeline-notes.md, RenderMotionClient.cpp #4)
    void PointClamp(IDirect3DDevice9* device, DWORD sampler) {
        device->SetSamplerState(sampler, D3DSAMP_MINFILTER, D3DTEXF_POINT);
        device->SetSamplerState(sampler, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
        device->SetSamplerState(sampler, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
        device->SetSamplerState(sampler, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
        device->SetSamplerState(sampler, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
    }

    // The render states every pass shares; exactly the set ExternalStateRestore re-sends. Stencil, sRGB write and
    // (continued: docs/2026-10-29-render-pipeline-notes.md, RenderMotionClient.cpp #5)
    void CommonStates(IDirect3DDevice9* device) {
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

    // The highest shader constant registers the passes write (+1): VS c0..c12 (SkinnedMotionVS; FullscreenVS uses c0),
    // (continued: docs/2026-10-29-render-pipeline-notes.md, RenderMotionClient.cpp #6)
    constexpr uint32_t kVsConstantsUsed = 13;
    constexpr uint32_t kPsConstantsUsed = 10;

    // CPU time of OpaqueDone / BeforeCopyBack (QueryPerformanceCounter ticks) and world renders since the last log.
    int64_t s_opaqueTicks;
    int64_t s_passTicks;
    // Of s_passTicks, per phase (PhaseName order).
    enum Phase { kScene, kStatic, kModel, kMask, kRestore, kStore, kFind, kPack, kLock, kUnlock, kPhaseCount };
    int64_t s_phaseTicks[kPhaseCount];

    struct PhaseTimer {
        Phase phase;
        int64_t start;
        explicit PhaseTimer(Phase p);
        ~PhaseTimer();
    };
    uint32_t s_timedFrames;

    int64_t Ticks();

    PhaseTimer::PhaseTimer(Phase p) : phase(p), start(Ticks()) {}
    PhaseTimer::~PhaseTimer() { s_phaseTicks[this->phase] += Ticks() - this->start; }

    int64_t Ticks() {
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        return now.QuadPart;
    }

    int DebugValue() {
        CVar* var = *CoA::RenderPipelineClient::DebugSlot();
        return var ? var->m_intValue : 0;
    }

    // coaRenderMotion 1, or DLSS wanted (it forces the passes on; the CVar is not rewritten).
    bool MotionOn(CGxDevice* gx) {
        CVar* var = *CoA::RenderPipelineClient::MotionSlot();
        return ((var && var->m_intValue != 0) || CoA::DlssClient::Wanted()) && gx && gx->D3dDevice() && gx->WorldMotionSupported();
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

    // Per-frame diagnostics of the model pass (logged every kStatsIntervalMs while coaRenderMotion is on).
    struct Stats {
        uint32_t elements, models, animated, culled, staticUnchanged, noHistory, eligible, drawn;
        uint32_t batchesDrawn, slots, skippedAlpha, skippedOptimized, skippedSlots, skippedBuffers, skippedRange, skippedOverflow;
    };

    constexpr uint64_t kStatsIntervalMs = 5000;
    Stats s_stats;
    uint64_t s_statsLogged;

    // A model the model pass draws: last frame's bones; `drawn` set once one of its batches is drawn.
    struct Eligible {
        const CM2Model* model;
        const Mat4* bones;
        uint32_t count;
        bool drawn;
    };

    // This frame's eligible models, sorted by pointer (reused: no allocation per frame after warm-up).
    std::vector<Eligible> s_eligible;

    // One batch gathered for drawing (its two palette rows already packed).
    struct BatchDraw {
        IDirect3DVertexBuffer9* vertices;
        IDirect3DIndexBuffer9* indices;
        UINT offset;
        UINT stride;
        INT baseVertex;
        UINT minIndex;
        UINT vertexCount;
        UINT startIndex;
        UINT primCount;
        float curBase, prevBase;   // palette texels of this frame's / last frame's bones
        float boneCount;           // the section's bone slots (SkinnedMotionVS clamps indices to them)
        IDirect3DBaseTexture9* keyTex;   // nullptr: opaque (MotionPS)
        D3DTEXTUREADDRESS keyWrapU, keyWrapV;
        float keyRef;
    };

    std::vector<BatchDraw> s_batches;

    // The unique models of this frame's opaque batch elements (scene->m_passElements[0], type 0), sorted. The pass and
    // the history work on these only: the scene's whole m_modelList holds every loaded model (tens of thousands).
    std::vector<const CM2Model*> s_frameModels;

    void CollectModels(const CM2Scene* scene) {
        s_frameModels.clear();
        const TSGrowableArray<uint32_t>& list = scene->m_passElements[0];
        s_stats.elements = list.m_count;

        for (uint32_t i = 0; i < list.m_count; ++i) {
            const M2Element& element = scene->m_elements.m_data[list.m_data[i]];

            if (element.m_type == 0 && element.m_model) {
                s_frameModels.push_back(element.m_model);
            }
        }

        std::sort(s_frameModels.begin(), s_frameModels.end());
        s_frameModels.erase(std::unique(s_frameModels.begin(), s_frameModels.end()), s_frameModels.end());
        s_stats.models = static_cast<uint32_t>(s_frameModels.size());
    }

    // Classifies this frame's models: the eligible ones (ModelNeedsPass) into s_eligible (sorted, as s_frameModels is),
    // the rest counted by reason.
    void ClassifyModels(const CM2Scene* scene, const Frame& frame) {
        s_eligible.clear();

        for (const CM2Model* model : s_frameModels) {
            const bool animated = model->m_frameStamp == scene->m_frameStamp;
            const bool culled = (model->m_fFlags & kModelCulled) != 0;
            const bool staticSimple = (model->m_fFlags & kModelStaticSimple) != 0;

            if (!animated) {
                continue;
            }

            ++s_stats.animated;

            if (culled) {
                ++s_stats.culled;
                continue;
            }

            const MotionCache::Key key = { model, model->m_handle };
            uint32_t count = 0;
            const Mat4* bones = MotionCache::Previous(key, frame.index, count);
            const Mat4* world = MotionCache::PreviousWorld(key, frame.index);
            const bool worldChanged = world && memcmp(world, &model->m_worldTransform, sizeof(Mat4)) != 0;
            // A model whose bone count changed (reloaded under the same pointer and handle) has no usable history.
            const bool havePrevious = bones && model->m_boneMatrices && model->m_shared && model->m_shared->m_data
                && count == model->m_shared->m_data->m_bones.m_count;

            if (!havePrevious) {
                ++s_stats.noHistory;
                continue;
            }

            if (!CoA::RenderMotion::ModelNeedsPass(animated, culled, staticSimple, worldChanged, havePrevious)) {
                ++s_stats.staticUnchanged;
                continue;
            }

            ++s_stats.eligible;
            s_eligible.push_back({ model, bones, count, false });
        }
    }

    Eligible* FindEligible(const CM2Model* model) {
        auto it = std::lower_bound(s_eligible.begin(), s_eligible.end(), model, [](const Eligible& e, const CM2Model* m) { return e.model < m; });
        return (it != s_eligible.end() && it->model == model) ? &*it : nullptr;
    }

    // Every bone slot of the section maps to a bone the array holds.
    bool SectionBonesValid(const M2Data* data, const M2SkinSection* section, uint32_t boneCount) {
        for (uint32_t i = 0; i < section->m_boneCount; ++i) {
            if (data->m_boneCombos[section->m_boneComboIndex + i] >= boneCount) {
                return false;
            }
        }

        return true;
    }

    // Slot i of the section (model bone m_boneCombos[boneComboIndex + i]) as 3 texels from stream texel `base`: texel k
    // holds the engine's c31+ register k (column k of the bone matrix), as SkinnedMotionVS reads them.
    void WriteBones(const D3DLOCKED_RECT& locked, uint32_t base, const M2Data* data, const M2SkinSection* section, const Mat4* bones) {
        for (uint32_t i = 0; i < section->m_boneCount; ++i) {
            const Mat4& m = bones[data->m_boneCombos[section->m_boneComboIndex + i]];

            for (uint32_t k = 0; k < 3; ++k) {
                const uint32_t t = base + 3 * i + k;
                float* texel = reinterpret_cast<float*>(static_cast<uint8_t*>(locked.pBits)
                    + static_cast<size_t>(locked.Pitch) * (t / CoA::RenderMotion::kPaletteStreamWidth)) + 4 * (t % CoA::RenderMotion::kPaletteStreamWidth);
                // Non-finite bones (stale history) become 0: a NaN motion turns into black blocks under DLSS.
                for (uint32_t r = 0; r < 4; ++r) {
                    texel[r] = std::isfinite(m.m[r][k]) ? m.m[r][k] : 0.0f;
                }
            }
        }
    }

    // A drawable batch found by FindBatches.
    struct BatchSource {
        const M2Element* element;
        const Eligible* eligible;
        const CGxTex* key;   // alpha-keyed: the texture the world tested; nullptr for opaque
    };

    std::vector<BatchSource> s_sources;

    // Phase 1: the drawable opaque batches of the eligible models into s_sources (at most kMaxBatches), the rest
    // counted by reason.
    void FindBatches(const CM2Scene* scene) {
        s_sources.clear();
        const TSGrowableArray<uint32_t>& list = scene->m_passElements[0];

        for (uint32_t i = 0; i < list.m_count; ++i) {
            const M2Element* element = &scene->m_elements.m_data[list.m_data[i]];

            if (element->m_type != 0 || !element->m_model || !element->m_batch || !element->m_skinSection) {
                continue;
            }

            const Eligible* eligible = FindEligible(element->m_model);

            if (!eligible) {
                continue;
            }

            if (element->m_flags & kElementOptimized) {
                ++s_stats.skippedOptimized;
                LogOnce(s_loggedOptimized, "RenderMotion: optimized-geometry M2 batches are not in the model pass (camera motion only)");
                continue;
            }

            const CM2Shared* shared = element->m_model->m_shared;
            const M2Data* data = shared->m_data;
            const M2SkinSection* section = element->m_skinSection;

            // Alpha-keyed batches are alpha-tested against the texture the world used (MotionKeyPS); blended ones and
            // keyed ones with animated UVs keep the camera motion.
            const M2Batch* batch = element->m_batch;
            const uint16_t shader = batch->m_shader;
            const bool sphereMap = !(shader & 0x8000) && ((shader >> 4) & 8);
            const uint32_t transform = data->m_textureTransformCombos[batch->m_textureTransformComboIndex];
            const CoA::RenderMotion::BatchKind kind = CoA::RenderMotion::ClassifyBatch(data->m_materials[batch->m_materialIndex].m_blendMode,
                sphereMap ? 0u : transform, sphereMap ? 1u : data->m_textureTransforms.m_count);
            const CGxTex* key = nullptr;

            if (kind == CoA::RenderMotion::BatchKind::Keyed && batch->m_textureCount) {
                const uint32_t combo = data->m_textureCombos[batch->m_textureComboIndex];
                CTexture* texture = combo < data->m_textures.m_count ? element->m_model->m_textures[combo] : nullptr;
                const CGxTex* gxTex = texture ? TextureGetGxTex(texture, 0, nullptr) : nullptr;
                key = gxTex && gxTex->m_apiSpecificData ? gxTex : nullptr;
            }

            if (kind == CoA::RenderMotion::BatchKind::Skip || (kind == CoA::RenderMotion::BatchKind::Keyed && !key)) {
                ++s_stats.skippedAlpha;
                continue;
            }

            if (section->m_boneCount == 0 || section->m_indexCount < 3) {
                continue;
            }

            if (section->m_boneCount > MotionCache::kMaxBones) {
                ++s_stats.skippedSlots;
                LogOnce(s_loggedBones, "RenderMotion: an M2 batch uses more bone slots than the palette holds; it keeps the camera motion");
                continue;
            }

            // The geometry the world drew for this batch: CM2Shared's index and vertex buffers (SetIndices /
            // SetVertices(0), both already uploaded for this frame's draw), the range of CM2SceneRender::DrawBatch.
            const CGxBuf* vertexBuf = shared->m_vertexBuf;
            const CGxBuf* indexBuf = shared->m_indexBuf;

            if (!vertexBuf || !indexBuf || !vertexBuf->m_pool || !indexBuf->m_pool || !vertexBuf->m_pool->m_apiSpecific || !indexBuf->m_pool->m_apiSpecific
                || !vertexBuf->m_hasData || !vertexBuf->m_valid || !indexBuf->m_hasData || !indexBuf->m_valid || vertexBuf->m_itemSize != sizeof(M2Vertex)) {
                ++s_stats.skippedBuffers;
                continue;
            }

            if (s_sources.size() >= kMaxBatches) {
                ++s_stats.skippedOverflow;
                continue;
            }

            s_sources.push_back({ element, eligible, key });
        }
    }

    // Phase 2: one D3DLOCK_DISCARD lock of the whole palette level (DXVK ignores DISCARD on a sub-rect lock and may stall
    // (continued: docs/2026-10-29-render-pipeline-notes.md, RenderMotionClient.cpp #8)
    CoA::RenderMotion::PaletteSlots s_slots(kMaxBatches);

    // A slot's bones to write: this frame's then last frame's, from its base texel.
    struct SlotFill {
        uint32_t base;
        const M2Data* data;
        const M2SkinSection* section;
        const Mat4* cur;
        const Mat4* prev;
    };

    std::vector<SlotFill> s_fills;
    std::vector<int64_t> s_slotBase;   // -1: the section's bones are out of range

    // Phase A assigns slots and texels (no lock); phase B locks the smallest palette that holds them and writes the bones.
    // False when nothing could be packed; `palette` / `rows` are the texture the draws sample.
    bool PackBatches(CGxDevice* gx, IDirect3DDevice9* device, IDirect3DTexture9*& palette, uint32_t& rows) {
        s_batches.clear();
        s_slots.Clear();
        s_fills.clear();
        s_slotBase.clear();
        uint32_t texels = 0;

        for (uint32_t batch = 0; batch < s_sources.size(); ++batch) {
            const M2Element* element = s_sources[batch].element;
            const Eligible* eligible = s_sources[batch].eligible;
            const CM2Model* model = element->m_model;
            const M2Data* data = model->m_shared->m_data;
            const M2SkinSection* section = element->m_skinSection;
            uint32_t slot = 0;
            bool fresh = false;

            if (!s_slots.Assign(model, section, slot, fresh)) {
                ++s_stats.skippedOverflow;
                continue;
            }

            if (fresh) {
                const Mat4* cur = reinterpret_cast<const Mat4*>(model->m_boneMatrices);
                const bool valid = SectionBonesValid(data, section, data->m_bones.m_count) && SectionBonesValid(data, section, eligible->count);
                s_slotBase.push_back(valid ? texels : -1);

                if (valid) {
                    s_fills.push_back({ texels, data, section, cur, eligible->bones });
                    texels += 2 * 3 * section->m_boneCount;
                }
            }

            if (s_slotBase[slot] < 0) {
                ++s_stats.skippedRange;
                continue;
            }

            // As CGxDeviceD3d::Draw / ISetVertexBuffer: a byte offset into the stream, or a base vertex without stream
            // offsets.
            const CGxBuf* vertexBuf = model->m_shared->m_vertexBuf;
            const CGxBuf* indexBuf = model->m_shared->m_indexBuf;
            BatchDraw draw;
            draw.vertices = static_cast<IDirect3DVertexBuffer9*>(vertexBuf->m_pool->m_apiSpecific);
            draw.indices = static_cast<IDirect3DIndexBuffer9*>(indexBuf->m_pool->m_apiSpecific);
            draw.offset = vertexBuf->m_index;
            draw.stride = vertexBuf->m_itemSize;
            draw.baseVertex = 0;

            if (!gx->m_caps.m_streamOffset) {
                draw.offset = 0;
                draw.baseVertex = static_cast<INT>(vertexBuf->m_index / vertexBuf->m_itemSize);
            }

            draw.minIndex = section->m_vertexStart;
            draw.vertexCount = section->m_vertexCount;
            draw.startIndex = section->m_indexStart + (indexBuf->m_index >> 1);
            draw.primCount = section->m_indexCount / 3u;
            draw.curBase = static_cast<float>(s_slotBase[slot]);
            draw.prevBase = static_cast<float>(s_slotBase[slot] + 3 * section->m_boneCount);
            draw.boneCount = static_cast<float>(section->m_boneCount);
            const CGxTex* key = s_sources[batch].key;
            draw.keyTex = key ? static_cast<IDirect3DBaseTexture9*>(key->m_apiSpecificData) : nullptr;
            draw.keyWrapU = key && key->m_flags.m_wrapU ? D3DTADDRESS_WRAP : D3DTADDRESS_CLAMP;
            draw.keyWrapV = key && key->m_flags.m_wrapV ? D3DTADDRESS_WRAP : D3DTADDRESS_CLAMP;
            draw.keyRef = CoA::RenderMotion::KeyAlphaRef(element->m_alpha);
            s_batches.push_back(draw);

            Eligible* drawn = const_cast<Eligible*>(eligible);

            if (!drawn->drawn) {
                drawn->drawn = true;
                ++s_stats.drawn;
            }
        }

        s_stats.slots += s_slots.Count();
        const uint32_t bucket = CoA::RenderMotion::PaletteBucket(texels);

        if (s_batches.empty()) {
            return true;
        }

        if (bucket >= CoA::RenderMotion::kPaletteBuckets || !(palette = PaletteTexture(device, bucket))) {
            s_batches.clear();
            return false;
        }

        rows = CoA::RenderMotion::kPaletteBucketRows[bucket];
        D3DLOCKED_RECT locked;
        HRESULT hr;

        {
            PhaseTimer timer(kLock);
            hr = palette->LockRect(0, &locked, nullptr, D3DLOCK_DISCARD);
        }

        if (FAILED(hr)) {
            s_batches.clear();
            return false;
        }

        for (const SlotFill& fill : s_fills) {
            WriteBones(locked, fill.base, fill.data, fill.section, fill.cur);
            WriteBones(locked, fill.base + 3 * fill.section->m_boneCount, fill.data, fill.section, fill.prev);
        }

        {
            PhaseTimer timer(kUnlock);
            palette->UnlockRect(0);
        }

        return true;
    }

    // 2. Model pass: the opaque batches of every model that moved on its own (skinned, or a static model whose world
    // (continued: docs/2026-10-29-render-pipeline-notes.md, RenderMotionClient.cpp #9)
    void ModelPass(CGxDevice* gx, IDirect3DDevice9* device, IDirect3DSurface9* depthSurface, const Frame& frame, uint32_t width, uint32_t height) {
        const CM2Scene* scene = World::s_m2Scene;

        if (!scene || s_eligible.empty()) {
            return;
        }

        {
            PhaseTimer timer(kFind);
            FindBatches(scene);
        }

        if (s_sources.empty()) {
            return;
        }

        bool packed;
        IDirect3DTexture9* palette = nullptr;
        uint32_t rows = 1;

        {
            PhaseTimer timer(kPack);
            packed = PackBatches(gx, device, palette, rows);
        }

        if (!packed) {
            LogOnce(s_loggedLockFailed, "RenderMotion: the palette texture could not be created or locked, the model pass is skipped (camera motion only)");
            return;
        }

        if (s_batches.empty()) {
            return;
        }

        // c0..c3: the projection the world rasterised with (the jittered cur.proj in the device's native form), so the
        // (continued: docs/2026-10-29-render-pipeline-notes.md, RenderMotionClient.cpp #10)
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
        // c0.x = 1 with coaRenderDebug 5 (coverage): MotionPS writes a constant motion wherever the pass draws.
        const float coverage[4] = { DebugValue() == 5 ? 1.0f : 0.0f, 0.0f, 0.0f, 0.0f };
        device->SetPixelShaderConstantF(0, coverage, 1);
        device->SetVertexShaderConstantF(0, constants[0], 12);
        PointClamp(device, D3DVERTEXTEXTURESAMPLER0);
        device->SetTexture(D3DVERTEXTEXTURESAMPLER0, palette);

        IDirect3DVertexBuffer9* boundVertices = nullptr;
        UINT boundOffset = 0;
        UINT boundStride = 0;
        IDirect3DIndexBuffer9* boundIndices = nullptr;
        bool keyed = false;

        for (const BatchDraw& draw : s_batches) {
            const float bases[4] = { draw.curBase, draw.prevBase, 1.0f / static_cast<float>(rows), draw.boneCount };
            device->SetVertexShaderConstantF(12, bases, 1);

            if ((draw.keyTex != nullptr) != keyed) {
                keyed = draw.keyTex != nullptr;
                device->SetPixelShader(keyed ? s_keyPS : s_motionPS);
            }

            if (keyed) {
                // Linear + mips as the world sampled it, so the cut-out edge lands where the world's did.
                const float ref[4] = { draw.keyRef, 0.0f, 0.0f, 0.0f };
                device->SetPixelShaderConstantF(1, ref, 1);
                device->SetTexture(0, draw.keyTex);
                device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
                device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
                device->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR);
                device->SetSamplerState(0, D3DSAMP_ADDRESSU, draw.keyWrapU);
                device->SetSamplerState(0, D3DSAMP_ADDRESSV, draw.keyWrapV);
            }

            if (draw.vertices != boundVertices || draw.offset != boundOffset || draw.stride != boundStride) {
                device->SetStreamSource(0, draw.vertices, draw.offset, draw.stride);
                boundVertices = draw.vertices;
                boundOffset = draw.offset;
                boundStride = draw.stride;
            }

            if (draw.indices != boundIndices) {
                device->SetIndices(draw.indices);
                boundIndices = draw.indices;
            }

            device->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, draw.baseVertex, draw.minIndex, draw.vertexCount, draw.startIndex, draw.primCount);
        }

        s_stats.batchesDrawn += static_cast<uint32_t>(s_batches.size());
        device->SetTexture(D3DVERTEXTEXTURESAMPLER0, nullptr);
        device->SetTexture(0, nullptr);
    }

    // One line every kStatsIntervalMs with the current frame's counts.
    void LogStats() {
        const uint64_t now = GetTickCount64();

        if (now - s_statsLogged < kStatsIntervalMs) {
            return;
        }

        s_statsLogged = now;
        const Stats& s = s_stats;
        LARGE_INTEGER frequency;
        QueryPerformanceFrequency(&frequency);
        const double usPerTick = 1e6 / static_cast<double>(frequency.QuadPart);
        const uint32_t frames = s_timedFrames ? s_timedFrames : 1;
        const unsigned opaqueUs = static_cast<unsigned>(static_cast<double>(s_opaqueTicks) * usPerTick / frames + 0.5);
        const unsigned passUs = static_cast<unsigned>(static_cast<double>(s_passTicks) * usPerTick / frames + 0.5);
        unsigned phaseUs[kPhaseCount];

        for (int i = 0; i < kPhaseCount; ++i) {
            phaseUs[i] = static_cast<unsigned>(static_cast<double>(s_phaseTicks[i]) * usPerTick / frames + 0.5);
            s_phaseTicks[i] = 0;
        }

        s_opaqueTicks = 0;
        s_passTicks = 0;
        s_timedFrames = 0;
        CoA::Log::Printf("RenderMotion: elements=%u models=%u animated=%u culled=%u staticunchanged=%u nohistory=%u eligible=%u drawn=%u; "
                         "batches drawn=%u (palette slots %u) skipped alpha=%u optgeo=%u slots=%u buffers=%u range=%u overflow=%u; cpu opaque=%uus passes=%uus (scene %u, static %u, model %u [find %u, pack %u, lock %u, unlock %u], mask %u, restore %u, store %u us)",
                         s.elements, s.models, s.animated, s.culled, s.staticUnchanged, s.noHistory, s.eligible, s.drawn,
                         s.batchesDrawn, s.slots, s.skippedAlpha, s.skippedOptimized, s.skippedSlots, s.skippedBuffers, s.skippedRange, s.skippedOverflow, opaqueUs, passUs, phaseUs[kScene], phaseUs[kStatic], phaseUs[kModel], phaseUs[kFind], phaseUs[kPack], phaseUs[kLock], phaseUs[kUnlock], phaseUs[kMask], phaseUs[kRestore], phaseUs[kStore]);
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

    // 4. This frame's bones and world transform of every model of this frame's opaque elements (s_frameModels) that was
    // (continued: docs/2026-10-29-render-pipeline-notes.md, RenderMotionClient.cpp #11)
    void StoreBones(const Frame& frame) {
        if (frame.reset) {
            MotionCache::Clear();
        }

        if (const CM2Scene* scene = World::s_m2Scene) {
            for (const CM2Model* model : s_frameModels) {
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

    const int64_t start = Ticks();
    // Ensure first: a Begin at a new size leaves the old-size motion targets in place until then.
    s_snapshotTaken = gx->WorldMotionTargetsEnsure() && gx->WorldSnapshotOpaque();
    s_opaqueTicks += Ticks() - start;
}

// BeforeCopyBack's work (coaRenderMotion is on).
static void RunPasses(CGxDevice* gx) {
    const bool snapshotTaken = s_snapshotTaken;
    s_snapshotTaken = false;

    if (!gx->WorldMotionTargetsEnsure()) {
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

    s_stats = {};
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

    // This frame's models, classified on every new frame (also feeds the diagnostics line); drawn only with a palette.
    if (!repeated && World::s_m2Scene) {
        PhaseTimer timer(kScene);
        CollectModels(World::s_m2Scene);
        ClassifyModels(World::s_m2Scene, frame);
    } else {
        s_frameModels.clear();
        s_eligible.clear();
    }

    IDirect3DSurface9* savedTarget = nullptr;
    IDirect3DSurface9* savedDepth = nullptr;
    IDirect3DSurface9* depthSurface = nullptr;

    // Render targets are objects, not state: Get works on the pure device (the engine does it in IStateSetD3DDefaults).
    const bool haveTarget = SUCCEEDED(device->GetRenderTarget(0, &savedTarget)) && savedTarget;
    const bool haveDepth = SUCCEEDED(device->GetDepthStencilSurface(&savedDepth)) && savedDepth;

    // Without both surfaces there is nothing to put back: skip the passes, change no state.
    if (!haveTarget || !haveDepth) {
        LogOnce(s_loggedTargets, "RenderMotion: the device's render target or depth surface could not be read, the motion passes are skipped");

        if (savedTarget) {
            savedTarget->Release();
        }

        if (savedDepth) {
            savedDepth->Release();
        }

        if (!repeated) {
            StoreBones(frame);
        }

        return;
    }

    CommonStates(device);

    // 1. Static pass (or the reset clear), no depth surface bound while INTZ is sampled.
    device->SetDepthStencilSurface(nullptr);

    if (BindTarget(device, motion)) {
        SetViewport(device, width, height, 0.0f, 1.0f);

        if (reproject) {
            PhaseTimer timer(kStatic);
            StaticPass(device, depth, frame, width, height);
        } else {
            device->Clear(0, nullptr, D3DCLEAR_TARGET, 0, 1.0f, 0);
        }

        // 2. Model pass, over the static result.
        if (reproject && PaletteSupported(device) && SUCCEEDED(depth->GetSurfaceLevel(0, &depthSurface)) && depthSurface) {
            PhaseTimer timer(kModel);
            ModelPass(gx, device, depthSurface, frame, width, height);
            device->SetDepthStencilSurface(nullptr);
            CommonStates(device);
        }
    }

    // 3. Mask pass (cleared when OpaqueDone did not snapshot this world render, e.g. no M2 scene).
    if (BindTarget(device, mask)) {
        SetViewport(device, width, height, 0.0f, 1.0f);

        if (snapshotTaken) {
            PhaseTimer timer(kMask);
            MaskPass(device, color, snapshot, width, height);
        } else {
            device->Clear(0, nullptr, D3DCLEAR_TARGET, 0, 1.0f, 0);
        }
    }

    // Put the world target's surfaces back, then every other state from the device's caches.
    device->SetRenderTarget(0, savedTarget);
    device->SetDepthStencilSurface(savedDepth);

    IDirect3DSurface9* surfaces[] = { savedTarget, savedDepth, depthSurface };

    for (IDirect3DSurface9* surface : surfaces) {
        if (surface) {
            surface->Release();
        }
    }

    {
        PhaseTimer timer(kRestore);
        gx->ExternalStateRestore(kVsConstantsUsed, kPsConstantsUsed);
    }

    // 4. Next frame's history.
    if (!repeated) {
        PhaseTimer timer(kStore);
        StoreBones(frame);
    }
}

void CoA::RenderMotionClient::BeforeCopyBack() {
    CGxDevice* gx = g_theGxDevicePtr;

    if (!MotionOn(gx)) {
        s_snapshotTaken = false;
        return;
    }

    const int64_t start = Ticks();
    RunPasses(gx);
    s_passTicks += Ticks() - start;
    ++s_timedFrames;
    LogStats();
}

bool CoA::RenderMotionClient::DrawDebugView(int mode) {
    CGxDevice* gx = g_theGxDevicePtr;

    if (!MotionOn(gx)) {
        return false;
    }

    IDirect3DDevice9* device = static_cast<IDirect3DDevice9*>(gx->D3dDevice());
    IDirect3DBaseTexture9* source = static_cast<IDirect3DBaseTexture9*>(mode == 4 ? gx->WorldMaskTexture() : gx->WorldMotionTexture());

    if (!source || !CreateObjects(device)) {
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

    // c0.x: 0 = motion hue view (coaRenderDebug 3 / 5 / 6), 1 = grey mask (coaRenderDebug 4); c0.y: brightness scale,
    // 16 for the amplified view (coaRenderDebug 6), else 1.
    const float view[4] = { mode == 4 ? 1.0f : 0.0f, mode == 6 ? 16.0f : 1.0f, 0.0f, 0.0f };
    CommonStates(device);
    SetViewport(device, width, height, 0.0f, 1.0f);
    PointClamp(device, 0);
    device->SetTexture(0, source);
    device->SetPixelShader(s_viewPS);
    device->SetPixelShaderConstantF(0, view, 1);
    DrawQuad(device, width, height);

    gx->ExternalStateRestore(kVsConstantsUsed, kPsConstantsUsed);

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
