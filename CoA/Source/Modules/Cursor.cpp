// AscCursor.cpp (see Cursor.h): cursor sizes, the cursor table and the software cursor's state.
#include "CoA/Source/Modules/Cursor.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/Lifecycle.h"
#include "CoA/Source/Runtime/Script.h"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <share.h>
#include <string>

namespace {
    CoA::Cursor::Deps s_deps;

    uint32_t s_size = 32;          // the pixel size behind 0x10D3CBCC / 0x10D3CBD0
    uint32_t s_variant = 0;        // 0x10D3CBD4
    float s_width = 0.0f;          // 0x10D3CBCC
    float s_height = 0.0f;         // 0x10D3CBD0
    void* s_table[CoA::Cursor::kCustomMode * CoA::Cursor::kSlots] = {};   // 0x10D3C7A8
    void* s_custom = nullptr;      // 0x10D3C7A4
    bool s_show = true;            // 0x10BCC87C
    bool s_hardware = true;        // 0x10BCC87D

    void* Load(const char* path) {
        return s_deps.load ? s_deps.load(path) : nullptr;
    }

    // FUN_10a1e320's size: width px / (float)g_widthPixels, height PixelToScreenHeight(px).
    void SetSize(uint32_t px) {
        s_size = px;

        if (s_deps.screenWidth) {
            s_width = static_cast<float>(px) / s_deps.screenWidth();
        }

        if (s_deps.pixelToScreenHeight) {
            s_height = s_deps.pixelToScreenHeight(static_cast<int32_t>(px));
        }
    }
}

void CoA::Cursor::SizePreferred(const char* value, bool hdAvailable) {
    if (value && strcmp(value, "0") != 0) {
        if (strcmp(value, "1") == 0 || strcmp(value, "2") == 0 || strcmp(value, "3") == 0) {
            SetSize(value[0] == '1' ? 48 : value[0] == '2' ? 64 : 96);

            if (hdAvailable) {
                s_variant = 2;
            }

            return;
        }

        if (strcmp(value, "4") == 0) {
            SetSize(128);

            if (hdAvailable) {
                s_variant = 4;
            }

            return;
        }
    }

    SetSize(32);
    s_variant = 0;
}

bool CoA::Cursor::OnSizePreferred(CVar*, const char*, const char* value, void*) {
    // The original probes the HD archive with a shared-read ifstream.
    std::ifstream probe("Data\\Patch-Q.mpq", std::ios::in, _SH_DENYNO);
    const bool hd = probe.is_open();
    probe.close();
    CoA::Cursor::SizePreferred(value, hd);
    return true;
}

uint32_t CoA::Cursor::Size() {
    return s_size;
}

uint32_t CoA::Cursor::Variant() {
    return s_variant;
}

float CoA::Cursor::Width() {
    return s_width;
}

float CoA::Cursor::Height() {
    return s_height;
}

void CoA::Cursor::OnGxCursor(const char* value) {
    if (value && strcmp(value, "1") == 0) {
        s_hardware = true;
        // 0x493C80(*0xB7A9DC): CSimpleTop::SetCursor with a client global nothing ever writes -- null, so a no-op (ruling).

        if (s_deps.releaseRenderLayer) {
            s_deps.releaseRenderLayer();
        }
    } else {
        s_hardware = false;

        if (s_deps.addRenderLayer) {
            s_deps.addRenderLayer();
        }
    }
}

bool CoA::Cursor::Hardware() {
    return s_hardware;
}

bool CoA::Cursor::SetFromName(const char* name) {
    if (s_hardware) {
        return false;
    }

    std::string s(name ? name : "");

    if (s_variant != 0 && s.size() >= 10 && s.find("Directions") != std::string::npos) {
        s = s.substr(0, s.size() - 4);
        s += std::to_string(s_variant);
        s_custom = Load(s.c_str());
    } else {
        s_custom = Load(name ? name : "");
    }

    if (s_deps.setMode) {
        s_deps.setMode(kCustomMode);
    }

    return true;
}

int32_t CoA::Cursor::CursorVisible(int32_t show) {
    const bool stock = s_hardware || (s_deps.itemOnCursor && s_deps.itemOnCursor());
    s_show = (stock ? 0 : show) != 0;
    return stock ? show : 0;
}

bool CoA::Cursor::Show() {
    return s_show;
}

void CoA::Cursor::AfterUpdateCursor() {
    if (s_deps.itemOnCursor && s_deps.itemOnCursor() && s_deps.deviceCursorVisible) {
        s_deps.deviceCursorVisible(1);
    }
}

void CoA::Cursor::LoadTable() {
    char path[260];

    for (uint32_t i = 1; i < kCustomMode; ++i) {
        void** row = &s_table[i * kSlots];
        const char* name = s_deps.cursorName ? s_deps.cursorName(i) : nullptr;

        if (!name) {
            name = "";
        }

        if (!row[0]) {
            snprintf(path, sizeof(path), "Interface\\Cursor\\%s", name);
            row[0] = Load(path);
        }

        for (uint32_t n : { 2u, 4u }) {
            if (row[n]) {
                continue;
            }

            snprintf(path, sizeof(path), "Interface\\Cursor\\%s%u", name, n);
            row[n] = Load(path);
        }
    }
}

void CoA::Cursor::ResetMode() {
    if (s_deps.setMode) {
        s_deps.setMode(1);
    }
}

void* CoA::Cursor::DrawTexture() {
    if (!s_show) {
        return nullptr;
    }

    const uint32_t mode = s_deps.mode ? s_deps.mode() : 1;

    if (mode == kCustomMode) {
        return s_custom;
    }

    const uint32_t index = s_variant + (mode ? mode : 1) * kSlots;
    return index < kCustomMode * kSlots ? s_table[index] : nullptr;
}

void* CoA::Cursor::Slot(uint32_t mode, uint32_t slot) {
    const uint32_t index = mode * kSlots + slot;
    return index < kCustomMode * kSlots ? s_table[index] : nullptr;
}

int CoA::Cursor::SetAttackCursor(lua_State* L) {
    const std::string name = CoA::Script::CheckString(L, 1);
    void** attack = &s_table[4 * kSlots];      // 0x10D3C7F8
    void** unable = &s_table[30 * kSlots];     // 0x10D3CA00
    char path[260];
    char unablePath[260];
    snprintf(path, sizeof(path), "Interface\\Cursor\\%s", name.c_str());
    snprintf(unablePath, sizeof(unablePath), "Interface\\Cursor\\Unable%s", name.c_str());
    attack[0] = Load(path);
    unable[0] = Load(unablePath);

    for (uint32_t n : { 2u, 4u }) {
        snprintf(path, sizeof(path), "Interface\\Cursor\\%s%u", name.c_str(), n);
        snprintf(unablePath, sizeof(unablePath), "Interface\\Cursor\\Unable%s%u", name.c_str(), n);
        attack[n] = Load(path);
        unable[n] = Load(unablePath);
    }

    return 0;
}

CoA::Cursor::Deps& CoA::Cursor::Dependencies() {
    return s_deps;
}

void CoA::Cursor::Init() {
    // FUN_10a1e950: the four detours (the marked client sites, CursorClient.cpp) and the two glue-screen callbacks. The
    // CVar is queued with the glue vector (GlueCVars.cpp).
    CoA::Lifecycle::OnGlueScreen(CoA::Cursor::LoadTable);
    CoA::Lifecycle::OnGlueScreen(CoA::Cursor::ResetMode);
}

void CoA::Cursor::Reset() {
    s_deps = Deps();
    s_size = 32;
    s_variant = 0;
    s_width = 0.0f;
    s_height = 0.0f;
    memset(s_table, 0, sizeof(s_table));
    s_custom = nullptr;
    s_show = true;
    s_hardware = true;
}
