// AscBindings.cpp's client natives. Ported so far: the glue-state SendWindowsFlash, SetPortraitToTexture and
// FillLocalizedClassList.
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "Engine/Source/Os/Gui.h"
#include <Windows.h>

// The client's own natives the DLL also registers on the glue state (GameUI_Scripts.cpp / ScriptEvents_Scripts.cpp).
int32_t Script_SetPortraitToTexture(lua_State* L);
int32_t Script_FillLocalizedClassList(lua_State* L);

namespace {
    // handler_SendWindowsFlash: the game window (OsGuiGetWindow(0)), FlashWindow(hwnd, FALSE).
    int SendWindowsFlash(lua_State*) {
        if (HWND hwnd = static_cast<HWND>(OsGuiGetWindow(0))) {
            FlashWindow(hwnd, FALSE);
        }

        return 0;
    }

    const CoA::Binding s_bindings[] = {
        COA_NATIVE("", "SendWindowsFlash", SendWindowsFlash),
        // handler_SetPortraitToTexture tail-calls the client's own native (0x516970); it is registered directly.
        COA_NATIVE("", "SetPortraitToTexture", Script_SetPortraitToTexture),
        // 0x101912E0 is `mov eax, 0x60A510; jmp eax`: the glue state gets the client's own world native (glue-only in
        // the capture; the world state has the client's registration already).
        COA_NATIVE("", "FillLocalizedClassList", Script_FillLocalizedClassList),
    };
}

void CoA::BindingsClient::Init() {
    CoA::LuaReg::AddModule(s_bindings, sizeof(s_bindings) / sizeof(s_bindings[0]));
}
