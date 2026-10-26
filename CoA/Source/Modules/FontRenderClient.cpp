// AscFontRender.cpp's client half: FreeType 2.14.1 (ThirdParty/freetype), the client's glyph record and font texture,
// the "Font" pixel shader (0x10BE251C) and the Engine pointers in place of the client functions the DLL replaces.
#include "CoA/Source/Modules/FontRender.h"
#include "CoA/Source/Modules/Modules.h"
#include "Engine/Source/Gx/CGxDevice/CGxDevice.h"
#include "Engine/Source/Gx/CGxDevice/CGxShader.h"
#include "Engine/Source/Gx/Gx.h"
#include "Engine/Source/Gxu/GxuFont.h"
#include "Engine/Source/Gxu/GxuFontFont.h"
#include "Engine/Source/Gxu/GxuFontMiscClasses.h"
#include "Engine/Source/Storm/H/storm.h"
#include <freetype/freetype.h>
#include <freetype/ftmodapi.h>
#include <freetype/ftoutln.h>
#include <freetype/ftrender.h>
#include <cstring>

namespace {
    // FUN_100b2ce0's file name for SMemAlloc.
    const char kSource[] = "C:\\a\\Ascension.CustomDLLs\\Ascension.CustomDLLs\\src\\Ascension.Extensions\\FontRender.cpp";

    CGxShader* s_fontShader = nullptr;   // 0x10BE251C

    FT_Face Face(void* face) {
        return static_cast<FT_Face>(face);
    }

    int32_t InitFreeType(void** library) {
        return FT_Init_FreeType(reinterpret_cast<FT_Library*>(library));
    }

    bool HasOutlineRenderer(void* library) {
        return FT_Get_Renderer(static_cast<FT_Library>(library), FT_GLYPH_FORMAT_OUTLINE) != nullptr;
    }

    void SetSpread(void* library, const char* renderer, int32_t spread) {
        const FT_Int value = spread;
        FT_Property_Set(static_cast<FT_Library>(library), renderer, "spread", &value);
    }

    int32_t DoneFreeType(void* library) {
        return FT_Done_FreeType(static_cast<FT_Library>(library));
    }

    uint32_t CharIndex(void* face, uint32_t code) {
        return FT_Get_Char_Index(Face(face), code);
    }

    int32_t LoadGlyph(void* face, uint32_t index) {   // 0xA
        return FT_Load_Glyph(Face(face), index, FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP);
    }

    bool GlyphIsOutline(void* face) {   // slot +0x48 == 'outl'
        return Face(face)->glyph->format == FT_GLYPH_FORMAT_OUTLINE;
    }

    bool FaceIsBold(void* face) {   // +0xC & 2
        return (Face(face)->style_flags & FT_STYLE_FLAG_BOLD) != 0;
    }

    void Embolden(void* face, int64_t strength) {   // slot +0x6C
        FT_Outline_Embolden(&Face(face)->glyph->outline, static_cast<FT_Pos>(strength));
    }

    int32_t RenderGlyphMode(void* face, int32_t mode) {
        return FT_Render_Glyph(Face(face)->glyph, static_cast<FT_Render_Mode>(mode));
    }

    void GlyphBitmap(void* face, CoA::FontRender::GlyphBitmap& out) {
        const FT_GlyphSlot slot = Face(face)->glyph;
        out.buffer = slot->bitmap.buffer;
        out.width = slot->bitmap.width;
        out.rows = slot->bitmap.rows;
        out.pitch = slot->bitmap.pitch;
        out.left = slot->bitmap_left;
        out.top = slot->bitmap_top;
        out.horiAdvance = slot->metrics.horiAdvance;
    }

    void* Alloc(uint32_t size, int32_t line) {   // FUN_100b2ce0 -> SMemAlloc 0x76E540
        return SMemAlloc(size, kSource, line, 0);
    }

    // The Engine pointers.
    void FontInitialize() {   // 0x10237210: 0x6AA130(device, &shader, 4, "Shaders\Pixel", "Font", 1)
        g_theGxDevicePtr->ShaderCreate(&s_fontShader, GxSh_Pixel, "Shaders\\Pixel", "Font", 1);
    }

    FT_Error CreateLibrary(FT_Memory, FT_Library* library) {   // 0x10237150
        return CoA::FontRender::CreateLibrary(reinterpret_cast<void**>(library));
    }

    FT_Error ReleaseLibrary(FT_Library) {   // 0x10237100
        return CoA::FontRender::ReleaseLibrary();
    }

    bool RenderGlyph(FT_Face face, uint32_t pixelSize, uint32_t code, uint32_t baseline, GLYPHBITMAPDATA* data, int32_t, uint32_t padding) {
        CoA::FontRender::Glyph g;
        g.bitmap = static_cast<uint8_t*>(data->m_data);
        g.size = data->m_dataSize;
        g.width = data->m_glyphWidth;
        g.rows = data->m_glyphHeight;
        g.cellWidth = static_cast<int32_t>(data->m_glyphCellWidth);
        g.advance = data->m_glyphAdvance;
        g.left = data->m_glyphBearing;
        g.stride = data->m_glyphPitch;
        g.yOffset = data->m_yOffset;
        g.yStart = static_cast<int32_t>(data->m_yStart);
        const bool ok = CoA::FontRender::RenderGlyph(face, pixelSize, code, static_cast<int32_t>(baseline), g, padding);
        data->m_data = g.bitmap;
        data->m_dataSize = g.size;
        data->m_glyphWidth = g.width;
        data->m_glyphHeight = g.rows;
        data->m_glyphCellWidth = static_cast<uint32_t>(g.cellWidth);
        data->m_glyphAdvance = g.advance;
        data->m_glyphBearing = g.left;
        data->m_glyphPitch = g.stride;
        data->m_yOffset = g.yOffset;
        data->m_yStart = static_cast<uint32_t>(g.yStart);

        if (g.zeroedTail) {
            memset(&data->m_textureCoords, 0, sizeof(data->m_textureCoords));
        }

        return ok;
    }

    // FUN_10238130(this = the texture cache, glyph, dest): the height is the owning face's cell height (+4 -> +0x178).
    void PasteGlyph(TEXTURECACHE* cache, const GLYPHBITMAPDATA& data, uint16_t* dest) {
        CoA::FontRender::Glyph g;
        g.bitmap = static_cast<uint8_t*>(data.m_data);
        g.size = data.m_dataSize;
        g.width = data.m_glyphWidth;
        g.rows = data.m_glyphHeight;
        g.cellWidth = static_cast<int32_t>(data.m_glyphCellWidth);
        g.stride = data.m_glyphPitch;
        g.yStart = static_cast<int32_t>(data.m_yStart);
        const int32_t height = cache && cache->m_theFace ? static_cast<int32_t>(cache->m_theFace->m_cellHeight) : static_cast<int32_t>(g.rows);
        CoA::FontRender::CopyGlyph(g, height, dest);
    }

    // FUN_10236fd0: with a valid shader and a font, 0x408240(0x4E, shader) and the pixel constants c4 / c5.
    void BeforeRenderBatch(BATCHEDRENDERFONTDESC* desc) {
        const CGxFont* font = desc->m_face;

        if (!s_fontShader || !s_fontShader->Valid() || !font) {
            return;
        }

        GxRsSet(GxRs_PixelShader, s_fontShader);
        float c4[4];
        float c5[4];
        CoA::FontRender::ShaderConstants(font->m_fontFlags, font->m_pixelSize, c4, c5);
        GxShaderConstantsSet(GxSh_Pixel, 4, c4, 1);
        GxShaderConstantsSet(GxSh_Pixel, 5, c5, 1);
    }
}

void CoA::FontRenderClient::Init() {
    CoA::FontRender::Deps& d = CoA::FontRender::Dependencies();
    d.initFreeType = InitFreeType;
    d.hasOutlineRenderer = HasOutlineRenderer;
    d.setSpread = SetSpread;
    d.doneFreeType = DoneFreeType;
    d.charIndex = CharIndex;
    d.loadGlyph = LoadGlyph;
    d.glyphIsOutline = GlyphIsOutline;
    d.faceIsBold = FaceIsBold;
    d.embolden = Embolden;
    d.renderGlyph = RenderGlyphMode;
    d.glyphBitmap = GlyphBitmap;
    d.alloc = Alloc;
    g_gxuFontInitializeHook = FontInitialize;
    g_gxuFontCreateLibraryHook = CreateLibrary;
    g_gxuFontReleaseLibraryHook = ReleaseLibrary;
    g_gxuFontRenderGlyphHook = RenderGlyph;
    g_gxuFontPasteGlyphHook = PasteGlyph;
    g_gxuFontRenderBatchHook = BeforeRenderBatch;
    g_gxuFontAlwaysFilter = true;   // 0x6CA067
}
