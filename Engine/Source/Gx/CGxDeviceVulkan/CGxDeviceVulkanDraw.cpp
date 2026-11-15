// The Vulkan backend's draw:
#include <cmath>
#include <cstdint>
#include <cstring>
#include "Engine/Source/Gx/CGxDeviceVulkan/CGxDeviceVulkan.h"
#include "Engine/Source/Gx/CGxDeviceVulkan/Vulkan/Frame.hpp"
#include "Engine/Source/Gx/CGxDeviceVulkan/Vulkan/Effects.hpp"
#include "Engine/Source/Gx/Gx.h"
#include "Engine/Source/Gx/GxEffects.h"
#include "Engine/Source/Gx/CGxDeviceVulkan/Vulkan/Pipelines.hpp"
#include "Engine/Source/Gx/CGxDeviceVulkan/Vulkan/Log.hpp"
#include "Engine/Source/Gx/CGxDeviceVulkan/Vulkan/Manifest.hpp"
#include "Engine/Source/Gx/CGxBuf.h"
#include "Engine/Source/Gx/CGxPool.h"
#include "Engine/Source/Gx/CGxTex.h"

namespace frame     = gxvk::vk::frame;
namespace pipelines = gxvk::vk::pipelines;

namespace {
    // The byte size of each CGxVertexAttrib::m_type, as CGxDeviceD3d::s_gxAttribToD3dAttribSize: a
    // stream's stride is the sum over the attributes it carries.
    const uint32_t s_attribSize[7] = { 4, 4, 4, 8, 12, 4, 4 };

    // D3D9 puts a pixel's centre at integer window coordinates, Vulkan at half-integers, and the engine lays its 2D
    // out for D3D9. Every draw's viewport moves this much right and down so a vertex lands where D3D9 puts it;
    constexpr float kD3d9CentreShift = 0.5f - 1.0f / 128.0f;

    // The Vulkan vertex format of a CGxVertexAttrib::m_type: what the D3D backend's D3DDECLTYPE for it delivers to a
    // shader. A D3DCOLOR is B, G, R, A in memory, which B8G8R8A8_UNORM fetches as r, g, b, a.
    bool AttribFormat(uint32_t type, VkFormat& out) {
        switch (type) {
            case 0: out = VK_FORMAT_B8G8R8A8_UNORM;   return true;   // D3DCOLOR
            case 1: out = VK_FORMAT_R8G8B8A8_USCALED; return true;   // UBYTE4
            case 2: out = VK_FORMAT_R8G8B8A8_UNORM;   return true;   // UBYTE4N
            case 3: out = VK_FORMAT_R32G32_SFLOAT;    return true;   // FLOAT2
            case 4: out = VK_FORMAT_R32G32B32_SFLOAT; return true;   // FLOAT3
            case 5: out = VK_FORMAT_R16G16_SSCALED;   return true;   // SHORT2
            case 6: out = VK_FORMAT_R32_SFLOAT;       return true;   // FLOAT1
            default: return false;
        }
    }

    int32_t Trunc(float v) {
        return static_cast<int32_t>(v);
    }

    int32_t Clamp(int32_t v, int32_t lo, int32_t hi) {
        return v < lo ? lo : (v > hi ? hi : v);
    }
}

// Mirrors CGxDeviceD3d::Draw (0x006A3620): the two gates (the context, the poisoned frame), the state sync, then the
// draw. The vertex declaration D3D builds in IStateSyncVertexPtrs becomes the pipeline's vertex input:
void CGxDeviceVulkan::Draw(CGxBatch* batch, int32_t count) {
    if (!this->m_context || this->m_intF5C) {
        return;
    }

    this->IRsSync(0);

    if (!batch || !this->m_drawPath || !this->m_primVertexMask) {
        return;
    }

    // A colour target the frame could not take (a texture without an image: a format the device
    // cannot render into) drops its draws rather than painting them over the scene.
    CGxTex* colourTarget = this->m_textureTarget[GxBuffers_Color].m_texture;

    if (colourTarget && !frame::Target()) {
        return;
    }

    // The frame first: resolving a pool stamps its chunk with the frame that binds it, which has to
    // be the frame the draw is recorded in.
    if (!frame::Pass()) {
        return;
    }

    // The target's image is named by this frame: it outlives the frame's GPU work.
    if (colourTarget) {
        this->ITexTouch(colourTarget);
    }

    pipelines::Draw d;
    CGxBuf* streamBufs[pipelines::kMaxStreams] = {};
    bool havePosition = false;

    for (uint32_t a = 0; a < GxVAs_Last && a < pipelines::kAttribCount; a++) {
        if (!(this->m_primVertexMask & (1u << a))) {
            continue;
        }

        CGxBuf* buf = this->m_primVertexFormatBuf[a];

        if (!buf || !buf->m_pool) {
            return;
        }

        uint32_t stream;

        for (stream = 0; stream < d.streamCount; stream++) {
            if (streamBufs[stream] == buf) {
                break;
            }
        }

        if (stream == d.streamCount) {
            if (d.streamCount == pipelines::kMaxStreams) {
                return;
            }

            VkBuffer vb = VK_NULL_HANDLE;

            if (!this->IPoolBuffer(buf->m_pool, vb)) {
                return;
            }

            streamBufs[stream] = buf;
            d.streams[stream].buffer = vb;
            d.streams[stream].offset = buf->m_index;
            d.streams[stream].stride = 0;
            d.streamCount++;
        }

        const CGxVertexAttrib& attrib = this->m_primVertexFormatAttrib[a];
        VkFormat format;

        if (attrib.m_type >= 7 || !AttribFormat(attrib.m_type, format)) {
            return;
        }

        d.streams[stream].stride += s_attribSize[attrib.m_type];

        d.attribs[a].binding = stream;
        d.attribs[a].offset = attrib.m_offset;
        d.attribs[a].format = format;
        d.attribMask |= 1u << a;

        if (a == GxVA_Position) {
            havePosition = format == VK_FORMAT_R32G32B32_SFLOAT;
        }
    }

    if (!havePosition) {
        return;
    }

    // A masked subset of an interleaved vertex still advances by the full vertex size.
    // D3D's cached fixed-format declaration uses this stride as well.
    if (this->m_primVertexFormat < GxVertexBufferFormats_Last && d.streamCount == 1) {
        d.streams[0].stride = this->m_primVertexSize;
    }

    CGxBuf* indexBuf = nullptr;

    if (count) {
        indexBuf = this->m_primIndexBuf;

        if (!indexBuf || !indexBuf->m_pool || !this->IPoolBuffer(indexBuf->m_pool, d.indexBuffer)) {
            return;
        }

        d.indexed = true;
        d.firstIndex = indexBuf->m_index / sizeof(uint16_t) + batch->m_start;
    }

    d.count = batch->m_count;
    d.topology = batch->m_primType;
    d.blend = this->m_appRenderStates[GxRs_BlendingMode].m_value.m_data.i[0];
    this->IRenderStates(d);

    // The constant registers, then the programs (which read them for their transform) and the
    // texture stages they sample.
    this->IShaderConstantsFlush();
    const uint32_t samplerMask = this->IShaderPrograms(d);

    for (uint32_t stage = 0; stage < pipelines::kTextureStages; stage++) {
        if (samplerMask & (1u << stage)) {
            this->ITexResolve(static_cast<CGxTex*>(this->m_appRenderStates[GxRs_Texture0 + stage].m_value.m_data.p), stage, d);

            // Not in the original binary: a texture an HD material put on this stage (GxEffects::SetDrawTexture)
            gxvk::vk::effects::DrawTexture over;
            if (gxvk::vk::effects::DrawTextureFor(stage, over)) {
                pipelines::Texture& t = d.textures[stage];
                t.view = over.view;
                t.imageId = over.id;
                t.samplerKey = over.key;
                t.cube = false;
                d.textureMask |= 1u << stage;
            }
        }
    }

    // The alpha test, the textured stages and the ops of stages 0 and 1 (Shaders/Push.glsl's params: stage 1's ops in
    // bits 3..5), the stage-0 ops alone in the translated interface's misc row, and the stages' texture transforms.
    const int32_t alphaRef = this->m_appRenderStates[GxRs_AlphaRef].m_value.m_data.i[0];
    const uint32_t colourOp0 = this->m_appRenderStates[GxRs_ColorOp0].m_value.m_data.u[0] & 7u;
    const uint32_t alphaOp0 = this->m_appRenderStates[GxRs_AlphaOp0].m_value.m_data.u[0] & 7u;
    const uint32_t colourOp1 = this->m_appRenderStates[GxRs_ColorOp1].m_value.m_data.u[0] & 7u;
    const uint32_t alphaOp1 = this->m_appRenderStates[GxRs_AlphaOp1].m_value.m_data.u[0] & 7u;
    d.params[0] = alphaRef > 0 ? static_cast<float>(alphaRef) : 0.0f;
    d.params[1] = static_cast<float>(d.textureMask & 3u);
    d.params[2] = static_cast<float>(colourOp0 | (colourOp1 << 3));
    d.params[3] = static_cast<float>(alphaOp0 | (alphaOp1 << 3));
    d.misc[0] = d.params[0] / 255.0f;
    d.misc[1] = 0.0f;
    d.misc[2] = static_cast<float>(colourOp0);
    d.misc[3] = static_cast<float>(alphaOp0);
    this->ITexTransforms(d);

    // The shared register files. Pixel rows 14 and 15, which no shipped pixel shader reads, carry fixed-function fog
    // distances, colour and enable for the built-in and companion stages:
    float pixelRows[pipelines::kPixelRows * 4];
    memcpy(pixelRows, this->m_pixelRegs, sizeof(pixelRows));
    const uint32_t fogColour = this->m_appRenderStates[GxRs_FogColor].m_value.m_data.u[0];
    const bool fogOn = this->m_appRenderStates[GxRs_Fog].m_value.m_data.i[0] != 0 && (this->m_appMasterEnables & (1 << GxMasterEnable_Fog)) != 0;
    const float fogStart = this->m_appRenderStates[GxRs_FogStart].m_value.m_data.f[0];
    const float fogEnd = this->m_appRenderStates[GxRs_FogEnd].m_value.m_data.f[0];
    pixelRows[14 * 4 + 0] = fogStart;
    pixelRows[14 * 4 + 1] = fogEnd == fogStart ? fogStart + 1.0f : fogEnd;
    pixelRows[15 * 4 + 0] = ((fogColour >> 16) & 0xFF) / 255.0f;
    pixelRows[15 * 4 + 1] = ((fogColour >> 8) & 0xFF) / 255.0f;
    pixelRows[15 * 4 + 2] = (fogColour & 0xFF) / 255.0f;
    pixelRows[15 * 4 + 3] = fogOn ? 1.0f : 0.0f;
    d.pixelRegs = pixelRows;
    d.vertexRegs = this->m_vertexRegs;
    this->ITexGen(d);
    d.pixelExtra = d.pixelExtraRows ? this->m_pixelRegs + pipelines::kPixelRows * 4 : nullptr;

    if (!this->IPlaceD3d(d.viewport, d.scissor)) {
        return;
    }

    d.viewport.x += kD3d9CentreShift;
    d.viewport.y += kD3d9CentreShift;

    // The texture update may have ended the pass for an inline upload: open (resume) it for the draw.
    VkCommandBuffer cmd = frame::Pass();

    if (!cmd) {
        return;
    }

    const bool recorded = pipelines::Record(cmd, frame::RenderPass(), frame::PassGeneration(), frame::Serial(), frame::Slot(), d);
    static bool loggedLiquid = false;
    if (!loggedLiquid && d.fragmentRecord && std::strstr(gxvk::vk::manifest::FirstName(*d.fragmentRecord), "Liquid")) {
        loggedLiquid = true;
        gxvk::log::Write(gxvk::log::Info, "vulkan: first liquid draw: %s, %s; %u indices, texture mask 0x%X, alpha ref %.0f",
                         gxvk::vk::manifest::FirstName(*d.fragmentRecord), recorded ? "recorded" : "skipped",
                         d.count, d.textureMask, d.params[0]);
    }
    if (!recorded) {
        return;
    }

    GxPerfCountBatch(batch);

    for (uint32_t stream = 0; stream < d.streamCount; stream++) {
        this->INoteBufDrawn(streamBufs[stream]);
    }

    if (indexBuf) {
        this->INoteBufDrawn(indexBuf);
    }
}

// The texture transforms of stages 0 and 1 as CGxDeviceD3d::IXformSetTex (0x006A5AA0) sets them for a stage with
// GxRs_TextureShader at 1 and no texgen:
void CGxDeviceVulkan::ITexTransforms(pipelines::Draw& d) {
    for (uint32_t stage = 0; stage < 2; stage++) {
        if (this->m_appRenderStates[GxRs_TextureShader0 + stage].m_value.m_data.i[0] != 1 || this->m_appRenderStates[GxRs_TexGen0 + stage].m_value.m_data.i[0] != GxTexGen_Disabled) {
            continue;
        }

        const CGxMatrixStack& texGen = this->m_texGen[stage];
        const CGxMatrixStack& xform = this->m_xforms[GxXform_Tex0 + stage];
        const bool genIdentity = (texGen.m_flags[texGen.m_level] & CGxMatrixStack::F_Identity) != 0;
        const bool xformIdentity = (xform.m_flags[xform.m_level] & CGxMatrixStack::F_Identity) != 0;

        if (genIdentity && xformIdentity) {
            continue;
        }

        C44Matrix m;

        if (genIdentity) {
            m = xform.m_mtx[xform.m_level];
        } else if (xformIdentity) {
            m = texGen.m_mtx[texGen.m_level];
        } else {
            m = texGen.m_mtx[texGen.m_level] * xform.m_mtx[xform.m_level];
        }

        float* part = &d.tex[stage * 4];
        part[0] = m.a0;
        part[1] = m.b0;
        part[2] = m.a1;
        part[3] = m.b1;
        d.tex[8 + stage * 2] = m.d0;
        d.tex[9 + stage * 2] = m.d1;
    }
}

// D3D9's texgen (CGxDeviceD3d::ISetTexCoordIndex, IXformSetTex) for the fixed-function vertex programs: camera-space
// position, normal or reflection vector through the stage's texture matrix. Rows in Shaders/Fog.glsl's ff.v.
void CGxDeviceVulkan::ITexGen(pipelines::Draw& d) {
    if (d.vertex != pipelines::VertexProgram::FixedFunction && d.vertex != pipelines::VertexProgram::FixedFunctionSemantic) {
        return;
    }

    float* rows = this->m_ffRows;
    rows[0] = rows[1] = rows[2] = rows[3] = 0.0f;
    bool any = false;

    for (uint32_t stage = 0; stage < 2; stage++) {
        const int32_t mode = this->m_appRenderStates[GxRs_TexGen0 + stage].m_value.m_data.i[0];
        uint32_t kind = 0;

        switch (mode) {
            case GxTexGen_ObjectLinear:
            case GxTexGen_World:
            case GxTexGen_View:          kind = 1; break;
            case GxTexGen_ReflectionMap:
            case GxTexGen_SphereMap:     kind = 2; break;
            case GxTexGen_Normal:        kind = 3; break;
            default:                     break;
        }

        if (!kind) {
            continue;
        }

        const CGxMatrixStack& texGen = this->m_texGen[stage];
        const CGxMatrixStack& xform = this->m_xforms[GxXform_Tex0 + stage];
        const int32_t shader = this->m_appRenderStates[GxRs_TextureShader0 + stage].m_value.m_data.i[0];
        uint32_t type = 0;
        C44Matrix m;

        if (shader == 0) {
            if (!(texGen.m_flags[texGen.m_level] & CGxMatrixStack::F_Identity)) {
                m = texGen.m_mtx[texGen.m_level];
                type = 1;
            }
        } else if (shader == 1 || shader == 2) {
            m = texGen.m_mtx[texGen.m_level] * xform.m_mtx[xform.m_level];
            type = shader == 1 ? 1 : 2;
        }

        rows[stage] = static_cast<float>(kind | (type << 2));
        memcpy(&rows[(5 + stage * 4) * 4], &m, 16 * sizeof(float));
        any = true;
    }

    if (!any) {
        return;
    }

    const CGxMatrixStack& world = this->m_xforms[GxXform_World];
    const CGxMatrixStack& view = this->m_xforms[GxXform_View];
    const C44Matrix worldView = (world.m_flags[world.m_level] & CGxMatrixStack::F_Identity)
        ? view.m_mtx[view.m_level] : world.m_mtx[world.m_level] * view.m_mtx[view.m_level];
    memcpy(&rows[4], &worldView, 16 * sizeof(float));
    d.vertexRegs = rows;
    d.vertexRows = pipelines::kVertexRowsSmall;
}

// The render states as CGxDeviceD3d::IRsSendToHw (0x006A4C30) sends them, each under its master enable:
void CGxDeviceVulkan::IRenderStates(pipelines::Draw& d) {
    const uint32_t master = this->m_appMasterEnables;

    d.depthTest = (master & (1 << GxMasterEnable_DepthTest)) && this->m_appRenderStates[GxRs_DepthTest].m_value.m_data.i[0];
    d.depthFunc = static_cast<uint32_t>(this->m_appRenderStates[GxRs_DepthFunc].m_value.m_data.i[0]);
    d.depthWrite = GxEffects::DepthWriteForced()
        || ((master & (1 << GxMasterEnable_DepthWrite)) && this->m_appRenderStates[GxRs_DepthWrite].m_value.m_data.i[0]);
    d.colourMask = (master & (1 << GxMasterEnable_ColorWrite)) ? this->m_appRenderStates[GxRs_ColorWrite].m_value.m_data.u[0] : 0u;
    d.wireframe = !(master & (1 << GxMasterEnable_PolygonFill));

    uint32_t cull = 0;

    if (master & (1 << GxMasterEnable_Culling)) {
        const int32_t value = this->m_appRenderStates[GxRs_Culling].m_value.m_data.i[0];
        cull = value < 0 ? 0u : (value > 2 ? 2u : static_cast<uint32_t>(value));
    }

    d.cull = cull;

    if (this->m_caps.m_depthBias) {
        float step;

        switch (frame::DepthFormat()) {
            case VK_FORMAT_D16_UNORM:  step = 65536.0f;    break;
            case VK_FORMAT_D32_SFLOAT: step = 8388608.0f;  break;
            default:                   step = 16777216.0f; break;
        }

        d.depthBias = -this->m_appRenderStates[GxRs_PolygonOffset].m_value.m_data.f[0] * step;
    }

    const float factor = this->m_appRenderStates[GxRs_BlendFactor].m_value.m_data.f[0];
    const float quantised = static_cast<float>(static_cast<uint32_t>(factor * 255.0f) & 0xFF) / 255.0f;
    d.blendConstant[0] = quantised;
    d.blendConstant[1] = quantised;
    d.blendConstant[2] = quantised;
    d.blendConstant[3] = quantised;
}

// The viewport and scissor D3D would set for the current state, in pixels of the frame's target:
int32_t CGxDeviceVulkan::IPlaceD3d(VkViewport& viewport, VkRect2D& scissor) {
    const VkExtent2D extent = frame::Extent();

    if (extent.width == 0 || extent.height == 0) {
        return 0;
    }

    const float W = static_cast<float>(extent.width);
    const float H = static_cast<float>(extent.height);
    const float minX = this->m_viewport.m_x.m_l;
    const float maxX = this->m_viewport.m_x.m_h;
    const float minY = this->m_viewport.m_y.m_l;
    const float maxY = this->m_viewport.m_y.m_h;

    const int32_t X = Trunc(minX * W + 0.5f);
    const int32_t Y = Trunc((1.0f - maxY) * H + 0.5f);
    int32_t vw = Trunc(maxX * W - static_cast<float>(X) + 0.5f);
    int32_t vh = Trunc((1.0f - minY) * H - static_cast<float>(Y) + 0.5f);
    const int32_t iw = static_cast<int32_t>(extent.width);
    const int32_t ih = static_cast<int32_t>(extent.height);

    const bool target = this->m_textureTarget[0].m_texture != nullptr || this->m_textureTarget[1].m_texture != nullptr;
    const int32_t Yp = target ? Trunc(minY * H + 0.5f) : Y;
    const int32_t x0 = Clamp(X, 0, iw);
    const int32_t y0 = Clamp(Yp, 0, ih);
    const int32_t x1 = Clamp(X + vw, x0, iw);
    const int32_t y1 = Clamp(Yp + vh, y0, ih);
    vw = x1 - x0;
    vh = y1 - y0;

    float minZ = this->m_viewport.m_z.m_l;
    float maxZ = this->m_viewport.m_z.m_h;
    minZ = minZ < 0.0f ? 0.0f : (minZ > 1.0f ? 1.0f : minZ);
    maxZ = maxZ < 0.0f ? 0.0f : (maxZ > 1.0f ? 1.0f : maxZ);
    viewport.x = static_cast<float>(x0);
    viewport.y = static_cast<float>(y0);
    viewport.width = static_cast<float>(vw);
    viewport.height = static_cast<float>(vh);
    viewport.minDepth = minZ;
    viewport.maxDepth = maxZ;

    int32_t l = x0;
    int32_t t = y0;
    int32_t r = x1;
    int32_t b = y1;

    if (this->m_appRenderStates[GxRs_ScissorTest].m_value.m_data.i[0]) {
        const CRect& rect = this->m_scissorRect;
        const int32_t sl = Clamp(Trunc(rect.minX * W + 0.5f), 0, iw);
        const int32_t sr = Clamp(Trunc(rect.maxX * W + 1.0f), 0, iw);
        const int32_t st = Clamp(target ? Trunc(rect.minY * H + 0.5f) : Trunc((1.0f - rect.maxY) * H + 0.5f), 0, ih);
        const int32_t sb = Clamp(target ? Trunc(rect.maxY * H + 1.0f) : Trunc((1.0f - rect.minY) * H + 1.0f), 0, ih);
        l = l > sl ? l : sl;
        t = t > st ? t : st;
        r = r < sr ? r : sr;
        b = b < sb ? b : sb;
    }

    if (r < l) {
        r = l;
    }

    if (b < t) {
        b = t;
    }

    scissor.offset.x = l;
    scissor.offset.y = t;
    scissor.extent.width = static_cast<uint32_t>(r - l);
    scissor.extent.height = static_cast<uint32_t>(b - t);

    return vw > 0 && vh > 0 && r > l && b > t;
}

// The matrix the fixed-function vertex programs push: the World stack's top (skipped when flagged identity) times the
// View stack's top times ProjNative, the engine's row-vector product, as 16 floats in the engine's row-major order.
void CGxDeviceVulkan::IModelViewProj(float out[16]) {
    const CGxMatrixStack& world = this->m_xforms[GxXform_World];
    const CGxMatrixStack& view = this->m_xforms[GxXform_View];
    C44Matrix mvp;

    if (world.m_flags[world.m_level] & CGxMatrixStack::F_Identity) {
        mvp = view.m_mtx[view.m_level] * this->m_projNative;
    } else {
        mvp = world.m_mtx[world.m_level] * view.m_mtx[view.m_level] * this->m_projNative;
    }

    memcpy(out, &mvp, 16 * sizeof(float));
}

// Mirrors CGxDeviceD3d::XformSetProjection (0x006A9B40):
void CGxDeviceVulkan::XformSetProjection(const C44Matrix& matrix) {
    CGxDevice::XformSetProjection(matrix);

    C44Matrix projNative = matrix;

    if (!(fabs(projNative.c3 - 1.0f) < 0.00000023841858f) && !(fabs(projNative.c3) < 0.00000023841858f)) {
        projNative *= 1.0f / projNative.c3;
    }

    if (projNative.d3 == 0.0f) {
        float minZ = -(projNative.d2 / (projNative.c2 + 1.0f));
        float maxZ = -(projNative.d2 / (projNative.c2 - 1.0f));
        projNative.c2 = maxZ / (maxZ - minZ);
        projNative.d2 = maxZ * minZ / (minZ - maxZ);
    } else {
        float invC2 = 1.0f / projNative.c2;
        float minZ = (-1.0f - projNative.d2) * invC2;
        float maxZ = (1.0f - projNative.d2) * invC2;
        projNative.c2 = 1.0f / (maxZ - minZ);
        projNative.d2 = minZ / (minZ - maxZ);
    }

    if (!(this->m_appMasterEnables & (1 << GxMasterEnable_NormalProjection)) && projNative.d3 != 1.0f) {
        C44Matrix scale;
        scale.a0 = 0.2f;
        scale.b1 = 0.2f;
        scale.c2 = 0.2f;
        projNative = projNative * scale;
    }

    this->m_xforms[GxXform_Projection].m_dirty = 1;
    this->m_projNative = projNative;
}

void CGxDeviceVulkan::XformSetView(const C44Matrix& matrix) {
    CGxDevice::XformSetView(matrix);
}
