#include "CoA/Source/Modules/FontRender.h"
#include "CoA/Tests/TestMain.h"
#include <cstring>
#include <string>
#include <vector>

namespace {
    using V = std::vector<std::string>;
    using CoA::FontRender::Glyph;
    using CoA::FontRender::GlyphBitmap;

    V s_calls;
    char s_library[4];
    int32_t s_initError = 0;
    bool s_outline = true;
    uint32_t s_index = 7;
    int32_t s_loadError = 0;
    bool s_glyphOutline = true;
    bool s_bold = false;
    int32_t s_renderError[6] = {};
    GlyphBitmap s_bitmap;
    std::vector<std::vector<uint8_t>> s_blocks;
    bool s_allocFails = false;

    void Start() {
        CoA::FontRender::Reset();
        s_calls.clear();
        s_initError = 0;
        s_outline = true;
        s_index = 7;
        s_loadError = 0;
        s_glyphOutline = true;
        s_bold = false;
        memset(s_renderError, 0, sizeof(s_renderError));
        s_bitmap = GlyphBitmap();
        s_blocks.clear();
        s_blocks.reserve(16);
        s_allocFails = false;
        CoA::FontRender::Deps& d = CoA::FontRender::Dependencies();
        d.initFreeType = [](void** lib) {
            s_calls.push_back("init");
            *lib = s_library;
            return s_initError;
        };
        d.hasOutlineRenderer = [](void* lib) { return lib == s_library && s_outline; };
        d.setSpread = [](void*, const char* renderer, int32_t spread) {
            s_calls.push_back(std::string("spread:") + renderer + "=" + std::to_string(spread));
        };
        d.doneFreeType = [](void* lib) {
            s_calls.push_back(lib == s_library ? "done" : "done:other");
            return 3;
        };
        d.charIndex = [](void*, uint32_t code) {
            s_calls.push_back("index:" + std::to_string(code));
            return s_index;
        };
        d.loadGlyph = [](void*, uint32_t index) {
            s_calls.push_back("load:" + std::to_string(index));
            return s_loadError;
        };
        d.glyphIsOutline = [](void*) { return s_glyphOutline; };
        d.faceIsBold = [](void*) { return s_bold; };
        d.embolden = [](void*, int64_t strength) { s_calls.push_back("embolden:" + std::to_string(strength)); };
        d.renderGlyph = [](void*, int32_t mode) {
            s_calls.push_back("render:" + std::to_string(mode));
            return s_renderError[mode];
        };
        d.glyphBitmap = [](void*, GlyphBitmap& out) { out = s_bitmap; };
        d.alloc = [](uint32_t size, int32_t line) -> void* {
            s_calls.push_back("alloc:" + std::to_string(size) + "@" + std::to_string(line));

            if (s_allocFails) {
                return nullptr;
            }

            s_blocks.emplace_back(size, 0xCD);
            return s_blocks.back().data();
        };
        CoA::FontRender::Init();
    }

    void Stop() {
        CoA::FontRender::Reset();
    }

    // A width x rows SDF bitmap whose byte is (row << 4) | column.
    std::vector<uint8_t> Grid(uint32_t width, uint32_t rows) {
        std::vector<uint8_t> b(width * rows);

        for (uint32_t r = 0; r < rows; ++r) {
            for (uint32_t c = 0; c < width; ++c) {
                b[r * width + c] = static_cast<uint8_t>((r << 4) | c);
            }
        }

        return b;
    }
}

// 0x10237150 / 0x10237100: one shared library; spread 4 on sdf / bsdf when an outline renderer exists.
COA_TEST(FontRender_Library) {
    Start();
    void* lib = nullptr;
    CHECK(CoA::FontRender::CreateLibrary(&lib) == 0);
    CHECK(lib == s_library);
    CHECK((s_calls == V{ "init", "spread:sdf=4", "spread:bsdf=4" }));
    void* again = nullptr;
    CHECK(CoA::FontRender::CreateLibrary(&again) == 0 && again == s_library);
    CHECK(s_calls.size() == 3);
    CHECK(CoA::FontRender::ReleaseLibrary() == 3);
    s_calls.clear();
    s_outline = false;
    s_initError = 9;
    CHECK(CoA::FontRender::CreateLibrary(&lib) == 9);   // the error is returned, the library still marked ready
    CHECK((s_calls == V{ "init" }));
    CHECK(CoA::FontRender::CreateLibrary(&lib) == 0);
    CHECK(s_calls.size() == 1);
    Stop();
}

// FUN_10237240's start: no index / a load error fail before anything else; the embolden strengths by size and boldness.
COA_TEST(FontRender_LoadAndEmbolden) {
    Start();
    Glyph g;
    s_index = 0;
    CHECK(!CoA::FontRender::RenderGlyph(nullptr, 12, 65, 10, g, 0));
    CHECK((s_calls == V{ "index:65" }));
    s_index = 7;
    s_loadError = 1;
    s_calls.clear();
    CHECK(!CoA::FontRender::RenderGlyph(nullptr, 12, 65, 10, g, 0));
    CHECK((s_calls == V{ "index:65", "load:7" }));
    s_loadError = 0;

    struct Case {
        bool bold;
        uint32_t size;
        const char* embolden;
    };
    const Case cases[] = { { false, 10, "embolden:3" }, { false, 11, "embolden:6" }, { false, 12, "embolden:6" }, { false, 13, nullptr },
        { true, 9, "embolden:1" }, { true, 12, "embolden:3" }, { true, 14, nullptr } };

    for (const Case& c : cases) {
        s_bold = c.bold;
        s_calls.clear();
        CoA::FontRender::RenderGlyph(nullptr, c.size, 65, 10, g, 0);
        CHECK(s_calls.size() >= 3);

        if (s_calls.size() >= 3) {
            CHECK(c.embolden ? s_calls[2] == c.embolden : s_calls[2] == "render:0");
        }
    }

    s_glyphOutline = false;   // a bitmap glyph is never emboldened
    s_calls.clear();
    CoA::FontRender::RenderGlyph(nullptr, 10, 65, 10, g, 0);
    CHECK(s_calls.size() >= 3 && s_calls[2] == "render:0");

    // Normal render, then SDF (mode 5); either failing fails the glyph.
    s_renderError[0] = 1;
    s_calls.clear();
    CHECK(!CoA::FontRender::RenderGlyph(nullptr, 10, 65, 10, g, 0));
    CHECK(s_calls.back() == "render:0");
    s_renderError[0] = 0;
    s_renderError[5] = 1;
    s_calls.clear();
    CHECK(!CoA::FontRender::RenderGlyph(nullptr, 10, 65, 10, g, 0));
    CHECK(s_calls.back() == "render:5");
    Stop();
}

// No bitmap: a 1-byte placeholder (line 0xB6), cell max(padding, 6) + 1, advance (horiAdvance >> 6) + 1.
COA_TEST(FontRender_EmptyGlyph) {
    Start();
    Glyph g;
    s_bitmap.horiAdvance = 7 * 64 + 10;
    CHECK(CoA::FontRender::RenderGlyph(nullptr, 12, 32, 10, g, 2));
    CHECK(s_calls.back() == "alloc:1@182");
    CHECK(g.bitmap && g.bitmap[0] == 0 && g.size == 1 && g.stride == 1 && g.width == 1 && g.rows == 1);
    CHECK(g.cellWidth == 7 && g.advance == 8.0f && g.left == 0.0f && g.yOffset == 0 && g.yStart == 0 && g.zeroedTail);
    CHECK(CoA::FontRender::RenderGlyph(nullptr, 12, 32, 10, g, 9));
    CHECK(g.cellWidth == 10);
    s_bitmap.horiAdvance = -65;   // rounds toward zero: -1 + 1
    CHECK(CoA::FontRender::RenderGlyph(nullptr, 12, 32, 10, g, 0));
    CHECK(g.advance == 0.0f);
    s_allocFails = true;
    CHECK(!CoA::FontRender::RenderGlyph(nullptr, 12, 32, 10, g, 0));
    Stop();
}

// Four rows / columns or fewer: nothing past the border -- a 1-byte block (line 0xDC), width / rows 0.
COA_TEST(FontRender_BorderOnlyGlyph) {
    Start();
    std::vector<uint8_t> grid = Grid(4, 9);
    s_bitmap.buffer = grid.data();
    s_bitmap.width = 4;
    s_bitmap.rows = 9;
    s_bitmap.pitch = 4;
    s_bitmap.left = -2;
    s_bitmap.top = 15;
    s_bitmap.horiAdvance = 5 * 64;
    Glyph g;
    CHECK(CoA::FontRender::RenderGlyph(nullptr, 12, 65, 10, g, 0));
    CHECK(s_calls.back() == "alloc:1@220");
    CHECK(g.bitmap && g.bitmap[0] == 0 && g.width == 0 && g.rows == 0 && g.stride == 1 && g.size == 0);
    CHECK(g.cellWidth == 3 && g.advance == 6.0f && g.left == -2.0f && g.yOffset == 0 && g.yStart == -5 && g.zeroedTail);
    s_allocFails = true;
    CHECK(!CoA::FontRender::RenderGlyph(nullptr, 12, 65, 10, g, 0));
    Stop();
}

// The bitmap without its 2-pixel border (line 0xF2), positive and negative pitch; the cell placement.
COA_TEST(FontRender_GlyphCopyAndPlacement) {
    Start();
    std::vector<uint8_t> grid = Grid(9, 10);   // inner 5 x 6
    s_bitmap.buffer = grid.data();
    s_bitmap.width = 9;
    s_bitmap.rows = 10;
    s_bitmap.pitch = 9;
    s_bitmap.left = 1;
    s_bitmap.top = 12;
    s_bitmap.horiAdvance = 9 * 64;
    Glyph g;
    CHECK(CoA::FontRender::RenderGlyph(nullptr, 16, 65, 12, g, 8));
    CHECK(s_calls.back() == "alloc:30@242");
    CHECK(g.width == 5 && g.rows == 6 && g.stride == 5 && g.size == 30 && g.cellWidth == 10 && g.left == 1.0f && g.advance == 10.0f);
    CHECK(g.bitmap && g.bitmap[0] == ((2 << 4) | 2) && g.bitmap[4] == ((2 << 4) | 6) && g.bitmap[5] == ((3 << 4) | 2) && g.bitmap[29] == ((7 << 4) | 6));
    // inner = 2 <= 16; bearing 8 <= baseline 12: top 4, room 14 -> t = 2, y = 0.
    CHECK(g.yStart == 2 && g.yOffset == 0 && g.zeroedTail);
    Glyph untouched;
    s_index = 0;
    CHECK(!CoA::FontRender::RenderGlyph(nullptr, 16, 65, 12, untouched, 8));
    CHECK(!untouched.zeroedTail);
    s_index = 7;

    // Negative pitch: the rows come bottom-up.
    s_bitmap.pitch = -9;
    // FreeType's buffer is the block's start; with a negative pitch the image's top row is the last in memory.
    CHECK(CoA::FontRender::RenderGlyph(nullptr, 16, 65, 12, g, 8));
    CHECK(g.bitmap && g.bitmap[0] == ((7 << 4) | 2) && g.bitmap[25] == ((2 << 4) | 2));

    // bearing above the baseline: y = bearing - baseline, no clamp; t = -2 -> 0, y + 2.
    s_bitmap.pitch = 9;
    s_bitmap.buffer = grid.data();
    s_bitmap.top = 20;
    CHECK(CoA::FontRender::RenderGlyph(nullptr, 16, 65, 12, g, 8));
    CHECK(g.yStart == 0 && g.yOffset == 6);

    // Not enough room: top clamps to the room, then the overflow past the cell is taken back.
    s_bitmap.top = 0;
    CHECK(CoA::FontRender::RenderGlyph(nullptr, 8, 65, 12, g, 8));   // top 16 > room 6
    CHECK(g.yStart == 2 && g.yOffset == -12);
    CHECK(CoA::FontRender::RenderGlyph(nullptr, 7, 65, 12, g, 8));   // room 5 -> t 3, over 2
    CHECK(g.yStart == 1 && g.yOffset == -13);
    s_allocFails = true;
    CHECK(!CoA::FontRender::RenderGlyph(nullptr, 16, 65, 12, g, 8));
    Stop();
}

// FUN_10238130: the cell rows cleared to the height, a 3-texel left border of each row's first value, padding with its
// last, the value in the texel's high byte; the copy starts at yStart (a negative start skips source rows).
COA_TEST(FontRender_CopyGlyph) {
    uint8_t bitmap[6] = { 0x10, 0x20, 0x30, 0x40, 0x50, 0x60 };   // 3 x 2
    Glyph g;
    g.bitmap = bitmap;
    g.stride = 3;
    g.width = 3;
    g.rows = 2;
    g.cellWidth = 8;
    g.yStart = 1;
    std::vector<uint16_t> dst(256 * 4, 0xBEEF);
    CoA::FontRender::CopyGlyph(g, 4, dst.data());

    for (int x = 0; x < 8; ++x) {
        CHECK(dst[x] == 0 && dst[3 * 256 + x] == 0);
    }

    CHECK(dst[8] == 0xBEEF);   // past the cell
    const uint16_t row1[8] = { 0x1000, 0x1000, 0x1000, 0x1000, 0x2000, 0x3000, 0x3000, 0x3000 };
    const uint16_t row2[8] = { 0x4000, 0x4000, 0x4000, 0x4000, 0x5000, 0x6000, 0x6000, 0x6000 };

    for (int x = 0; x < 8; ++x) {
        CHECK(dst[256 + x] == row1[x] && dst[512 + x] == row2[x]);
    }

    // Starting above the cell: the first source row is skipped.
    std::vector<uint16_t> d2(256 * 3, 0xBEEF);
    g.yStart = -1;
    CoA::FontRender::CopyGlyph(g, 3, d2.data());
    CHECK(d2[0] == 0x4000 && d2[3] == 0x4000 && d2[5] == 0x6000 && d2[256] == 0 && d2[512] == 0);

    // A cell lower than the glyph: only the rows that fit.
    std::vector<uint16_t> d3(256 * 2, 0xBEEF);
    g.yStart = 1;
    CoA::FontRender::CopyGlyph(g, 2, d3.data());
    CHECK(d3[0] == 0 && d3[256] == 0x1000 && d3[256 + 7] == 0x3000);
}

// FUN_10236fd0's constants.
COA_TEST(FontRender_ShaderConstants) {
    float c4[4];
    float c5[4];
    CoA::FontRender::ShaderConstants(0, 14, c4, c5);
    CHECK(c4[0] == 0.0f && c4[1] == 0.0f && c4[2] == 0.0f && c4[3] == 1.0f);
    CHECK(c5[0] == 14.0f && c5[1] == 0.0f && c5[2] == 0.0f && c5[3] == 0.0f);
    CoA::FontRender::ShaderConstants(1, 14, c4, c5);
    CHECK(c5[1] == 1.0f);
    CoA::FontRender::ShaderConstants(9, 14, c4, c5);
    CHECK(c5[1] == 2.0f);
    CoA::FontRender::ShaderConstants(8, 20, c4, c5);   // thick without outline: none
    CHECK(c5[0] == 20.0f && c5[1] == 0.0f);
}
