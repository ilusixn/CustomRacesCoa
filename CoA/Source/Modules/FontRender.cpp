// AscFontRender.cpp (see FontRender.h): FreeType 2.14.1 signed-distance-field text.
#include "CoA/Source/Modules/FontRender.h"
#include "CoA/Source/Modules/Modules.h"
#include <cstring>

namespace {
    CoA::FontRender::Deps s_deps;
    void* s_library = nullptr;      // 0x10BE2518
    bool s_libraryReady = false;    // 0x10BE2514
    const int32_t kSpread = 4;      // 0x10B3E1B0

    void* Alloc(uint32_t size, int32_t line) {
        return s_deps.alloc ? s_deps.alloc(size, line) : nullptr;
    }

    // (adv + ((adv >> 31) & 0x3F)) >> 6, + 1: the advance in pixels rounded toward zero, plus one.
    float Advance(int64_t horiAdvance) {
        const int32_t adv = static_cast<int32_t>(horiAdvance);
        return static_cast<float>((adv + ((adv >> 31) & 0x3F)) >> 6) + 1.0f;
    }

    uint16_t Texel(uint8_t v) {
        return static_cast<uint16_t>(((v >> 4) << 4 | (v & 0xF)) << 8);
    }
}

CoA::FontRender::Deps& CoA::FontRender::Dependencies() {
    return s_deps;
}

int32_t CoA::FontRender::CreateLibrary(void** library) {
    if (s_libraryReady) {
        *library = s_library;
        return 0;
    }

    const int32_t error = s_deps.initFreeType ? s_deps.initFreeType(&s_library) : -1;

    if (s_deps.hasOutlineRenderer && s_deps.hasOutlineRenderer(s_library) && s_deps.setSpread) {
        s_deps.setSpread(s_library, "sdf", kSpread);
        s_deps.setSpread(s_library, "bsdf", kSpread);
    }

    s_libraryReady = true;
    *library = s_library;
    return error;
}

int32_t CoA::FontRender::ReleaseLibrary() {
    const int32_t error = s_deps.doneFreeType ? s_deps.doneFreeType(s_library) : 0;
    s_library = nullptr;
    s_libraryReady = false;
    return error;
}

bool CoA::FontRender::RenderGlyph(void* face, uint32_t pixelSize, uint32_t code, int32_t baseline, Glyph& out, uint32_t padding) {
    const uint32_t index = s_deps.charIndex ? s_deps.charIndex(face, code) : 0;

    if (index == 0 || !s_deps.loadGlyph || s_deps.loadGlyph(face, index) != 0) {
        return false;
    }

    if (s_deps.glyphIsOutline && s_deps.glyphIsOutline(face)) {
        float strength = 0.0f;
        bool embolden = true;

        if (!(s_deps.faceIsBold && s_deps.faceIsBold(face))) {
            if (pixelSize <= 10) {
                strength = 0.05f;
            } else if (pixelSize <= 12) {
                strength = 0.1f;
            } else {
                embolden = false;
            }
        } else if (pixelSize <= 10) {
            strength = 0.03f;
        } else if (pixelSize <= 12) {
            strength = 0.06f;
        } else {
            embolden = false;
        }

        if (embolden && s_deps.embolden) {
            s_deps.embolden(face, static_cast<int64_t>(static_cast<int32_t>(strength * 64.0f)));
        }
    }

    if (!s_deps.renderGlyph || s_deps.renderGlyph(face, 0) != 0 || s_deps.renderGlyph(face, 5) != 0) {
        return false;
    }

    GlyphBitmap slot;

    if (s_deps.glyphBitmap) {
        s_deps.glyphBitmap(face, slot);
    }

    const float advance = Advance(slot.horiAdvance);
    const uint32_t cell = padding < 6 ? 6 : padding;

    if (!slot.buffer || !slot.width || !slot.rows) {
        out = Glyph();
        out.zeroedTail = true;
        uint8_t* p = static_cast<uint8_t*>(Alloc(1, 0xB6));

        if (!p) {
            return false;
        }

        *p = 0;
        out.bitmap = p;
        out.cellWidth = static_cast<int32_t>(cell + 1);
        out.size = 1;
        out.stride = 1;
        out.width = 1;
        out.rows = 1;
        out.left = 0.0f;
        out.yOffset = 0;
        out.yStart = 0;
        out.advance = Advance(slot.horiAdvance);
        return true;
    }

    const uint32_t rows = slot.rows;
    const int32_t pitch = slot.pitch;
    const uint32_t width = slot.width > 4 ? slot.width - 4 : 0;   // without the 2-pixel border
    const uint32_t height = rows > 4 ? rows - 4 : 0;
    const uint32_t stride = width ? width : 1;
    const uint32_t size = stride * height;
    const int32_t cellBase = static_cast<int32_t>(cell <= 3 ? 3 : cell - 3);

    if (!width || !height) {
        out = Glyph();
        out.zeroedTail = true;
        uint8_t* p = static_cast<uint8_t*>(Alloc(1, 0xDC));
        out.bitmap = p;

        if (!p) {
            return false;
        }

        *p = 0;
        out.advance = advance;
        out.cellWidth = cellBase;
        out.left = static_cast<float>(slot.left);
        out.stride = 1;
        out.width = 0;
        out.rows = 0;
        out.yOffset = 0;
        out.yStart = baseline - slot.top;
        return true;
    }

    uint8_t* dst = static_cast<uint8_t*>(Alloc(size, 0xF2));

    if (!dst) {
        return false;
    }

    const uint8_t* buffer = slot.buffer;

    if (pitch >= 0) {
        buffer += 2;

        for (uint32_t i = 0; i < height; ++i) {
            memcpy(dst + i * stride, buffer + static_cast<int64_t>(i + 2) * pitch, width);
        }
    } else {
        for (uint32_t i = 0; i < height; ++i) {
            memcpy(dst + i * stride, buffer - static_cast<int64_t>(static_cast<int32_t>(rows - i - 3)) * pitch + 2, width);
        }
    }

    out.bitmap = dst;
    out.size = size;
    out.stride = stride;
    out.advance = advance;
    out.cellWidth = cellBase + static_cast<int32_t>(width);
    out.left = static_cast<float>(slot.left);
    out.width = width;
    out.rows = height;

    // Vertical placement in the pixelSize-high cell (unsigned compares as in the original).
    const uint32_t inner = height - 4;
    int32_t y = 0;
    uint32_t top = 0;
    bool clampTop = true;

    if (inner > pixelSize) {
        top = 0;
    } else {
        const int32_t bearing = slot.top - 4;

        if (bearing > baseline) {
            y = bearing - baseline;
            top = 0;
            clampTop = false;
        } else {
            top = static_cast<uint32_t>(baseline - bearing);
        }
    }

    if (clampTop) {
        y = 0;

        if (static_cast<int32_t>(top) <= 0) {
            top = 0;
        }

        const uint32_t room = pixelSize - inner;

        if (room < top) {
            y = static_cast<int32_t>(pixelSize - top - inner);
            top = room;
        }
    }

    int32_t t = static_cast<int32_t>(top) - 2;

    if (t < 0) {
        y -= t;
        t = 0;
    } else {
        const int32_t over = t - static_cast<int32_t>(pixelSize) + static_cast<int32_t>(height);

        if (over > 0) {
            t -= over;
            y -= over;
        }
    }

    out.yStart = t;
    out.yOffset = y;
    out.zeroedTail = true;
    return true;
}

void CoA::FontRender::CopyGlyph(const Glyph& g, int32_t height, uint16_t* dst) {
    const uint8_t* src = g.bitmap;
    const int32_t stride = static_cast<int32_t>(g.stride);
    const int32_t cellWidth = g.cellWidth;
    const int32_t rows = static_cast<int32_t>(g.rows);
    const int32_t top = g.yStart;
    int32_t dstRow = top >= 0 ? top : 0;
    int32_t srcSkip = top >= 0 ? 0 : -top;

    if (srcSkip > rows) {
        srcSkip = rows;
    }

    if (height < dstRow) {
        dstRow = height;
    }

    int32_t count = height - dstRow;

    if (rows - srcSkip < count) {
        count = rows - srcSkip;
    }

    for (int32_t r = 0; r < height; ++r) {
        if (cellWidth > 0) {
            memset(dst + r * 256, 0, static_cast<size_t>(cellWidth) * 2);
        }
    }

    uint16_t* base = dst + dstRow * 256;

    for (int32_t r = 0; r < count; ++r) {
        const uint8_t* line = stride >= 0 ? src + (srcSkip + r) * stride : src - (rows - r - srcSkip - 1) * stride;
        uint16_t* row = base + r * 256;
        const uint32_t w = g.width;
        uint16_t first = 0;
        uint16_t last = 0;

        if (w) {
            first = Texel(line[0]);
            last = Texel(line[w - 1]);
        }

        row[0] = row[1] = row[2] = first;

        for (uint32_t i = 0; i < w; ++i) {
            row[3 + i] = Texel(line[i]);
        }

        for (int32_t i = static_cast<int32_t>(w) + 3; i < cellWidth; ++i) {
            row[i] = last;
        }
    }
}

void CoA::FontRender::ShaderConstants(uint32_t fontFlags, uint32_t pixelSize, float c4[4], float c5[4]) {
    float outline = 0.0f;

    if (fontFlags & 1) {
        outline = (fontFlags & 8) ? 2.0f : 1.0f;
    }

    c4[0] = 0.0f;
    c4[1] = 0.0f;
    c4[2] = 0.0f;
    c4[3] = 1.0f;
    c5[0] = static_cast<float>(static_cast<double>(pixelSize));
    c5[1] = outline;
    c5[2] = 0.0f;
    c5[3] = 0.0f;
}

void CoA::FontRender::Init() {
    // FUN_10237650's replacements and detours are the Engine pointers FontRenderClient installs.
}

void CoA::FontRender::Reset() {
    s_deps = Deps();
    s_library = nullptr;
    s_libraryReady = false;
}
