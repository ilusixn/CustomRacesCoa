#ifndef COA_SOURCE_MODULES_FONTRENDER_H
#define COA_SOURCE_MODULES_FONTRENDER_H

#include <cstdint>

// AscFontRender.cpp (module init FUN_10237650): the client's text rendered by FreeType 2.14.1 as signed distance fields.
//
// The client links a FreeType 2.0-era library; the original swaps its entry points for its statically linked 2.14.1 (the
// hooks never call the originals) -- CoA links 2.14.1 in its place (ThirdParty/freetype) -- and replaces the one client
// function that reads FreeType's structs:
//   0x991320 -> 0x10237150  library creation: one shared FT_Init_FreeType library for the process; with an outline
//                           renderer the sdf and bsdf renderers get "spread" = 4 (0x10B3E1B0)
//   0x992CB0 -> 0x10237100  library release: FT_Done_FreeType on it; the next creation starts over
//   0x993370 FT_New_Memory_Face  0x992610 FT_Done_Face  0x9911A0 FT_Get_Char_Index  0x991050 FT_Get_Kerning
//   0x992DA0 FT_Load_Glyph  0x992780 FT_Set_Pixel_Sizes  0x992B60 FT_Render_Glyph
//   0x6C8CC0 -> FUN_10237240  one glyph into the client's GLYPHBITMAPDATA: loaded unhinted, outlines emboldened by size
//                           (less for bold faces), rendered normally and then as an SDF; copied without its 2-pixel border
//   0x6BE230 -> 0x10237210  GxuFontInitialize: first creates pixel shader "Font" (Shaders\Pixel)
//   0x6C4AD0 -> FUN_10236fd0  BATCHEDRENDERFONTDESC::RenderBatch, before: binds that shader with c4 = {0, 0, 0, 1} and
//                           c5 = {font +0x24C, outline 0 / 1 / 2 (+0x180 bits 1, 8), 0, 0}
//   0x6C8E70 / 0x6C9330 / 0x6C9420 / 0x6C9C70 -> FUN_10238130  the glyph into the font texture (CopyGlyph)
// Patches: 0x6BE290 NOPs GxuFontInitialize's FT_Add_Default_Modules (0x990650) after the library creation; the setne at
// 0x6CA067 (TEXTURECACHE::CreateTexture's filter) becomes "mov al, 1".
namespace CoA::FontRender {
    // FreeType's glyph slot after the SDF render.
    struct GlyphBitmap {
        const uint8_t* buffer = nullptr;
        uint32_t width = 0;
        uint32_t rows = 0;
        int32_t pitch = 0;
        int32_t left = 0;           // bitmap_left
        int32_t top = 0;            // bitmap_top
        int64_t horiAdvance = 0;    // metrics.horiAdvance (26.6)
    };

    // The client's glyph record (GLYPHBITMAPDATA, 0x38 bytes in the original).
    struct Glyph {
        uint8_t* bitmap = nullptr;  // +0x00, SMemAlloc'd
        uint32_t size = 0;          // +0x04
        uint32_t width = 0;         // +0x08
        uint32_t rows = 0;          // +0x0C
        int32_t cellWidth = 0;      // +0x10
        float advance = 0.0f;       // +0x14
        float left = 0.0f;          // +0x18
        uint32_t stride = 0;        // +0x1C
        int32_t yOffset = 0;        // +0x20
        int32_t yStart = 0;         // +0x24 (the row the copy starts at)
        bool zeroedTail = false;    // +0x28..+0x37 (the texture rect) zeroed: every path that writes the record
    };

    struct Deps {
        int32_t (*initFreeType)(void** library) = nullptr;                    // FT_Init_FreeType
        bool (*hasOutlineRenderer)(void* library) = nullptr;                   // FT_Get_Renderer(.., FT_GLYPH_FORMAT_OUTLINE)
        void (*setSpread)(void* library, const char* renderer, int32_t spread) = nullptr;   // FT_Property_Set
        int32_t (*doneFreeType)(void* library) = nullptr;                     // FT_Done_FreeType
        uint32_t (*charIndex)(void* face, uint32_t code) = nullptr;            // FT_Get_Char_Index
        int32_t (*loadGlyph)(void* face, uint32_t index) = nullptr;            // FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP
        bool (*glyphIsOutline)(void* face) = nullptr;
        bool (*faceIsBold)(void* face) = nullptr;                              // style_flags & FT_STYLE_FLAG_BOLD
        void (*embolden)(void* face, int64_t strength) = nullptr;              // FT_Outline_Embolden on the slot
        int32_t (*renderGlyph)(void* face, int32_t mode) = nullptr;            // FT_Render_Glyph (0 normal, 5 SDF)
        void (*glyphBitmap)(void* face, GlyphBitmap& out) = nullptr;
        void* (*alloc)(uint32_t size, int32_t line) = nullptr;                // FUN_100b2ce0 -> SMemAlloc 0x76E540
    };

    Deps& Dependencies();

    int32_t CreateLibrary(void** library);       // 0x10237150
    int32_t ReleaseLibrary();                     // 0x10237100
    // FUN_10237240(face, pixelSize, code, baseline, out, monochrome (unused), padding).
    bool RenderGlyph(void* face, uint32_t pixelSize, uint32_t code, int32_t baseline, Glyph& out, uint32_t padding);
    // FUN_10238130: the 8-bit SDF value in the high byte of 16-bit texels, rows 0x200 bytes apart. height is the face's
    // cell height (the glyph's rows without a face).
    void CopyGlyph(const Glyph& glyph, int32_t height, uint16_t* dst);
    // FUN_10236fd0's constants: c4 and c5 from the font's flags (+0x180) and pixel size (+0x24C).
    void ShaderConstants(uint32_t fontFlags, uint32_t pixelSize, float c4[4], float c5[4]);

    void Init();
    // Tests only.
    void Reset();
}

#endif
