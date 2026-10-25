// AscGlobalsG.cpp's client natives. Ported so far: the glue-state Internal_CopyToClipboard.
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Script.h"
#include "WoW/Source/Object/ObjectClient/Player_C.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"
#include <Windows.h>
#include <cstring>
#include <string>

namespace {
    // A non-empty string to the clipboard (CF_TEXT), unless the active player's unit flags (descriptor +0xEC) have
    // bit 19. The original frees the handle after SetClipboardData; that is kept.
    int Internal_CopyToClipboard(lua_State* L) {
        std::string text;

        if (!CoA::Script::ReadString(L, text) || text.empty()) {
            return 0;
        }

        if (CGPlayer_C* player = ClntObjMgrGetActivePlayerObj()) {
            if ((player->m_unit->m_unitFieldFlags >> 19) & 1) {
                return 0;
            }
        }

        if (!OpenClipboard(nullptr)) {
            return 0;
        }

        if (EmptyClipboard()) {
            SIZE_T bytes = text.size() + 1;

            if (HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes)) {
                memcpy(GlobalLock(memory), text.c_str(), bytes);
                SetClipboardData(CF_TEXT, memory);
                CloseClipboard();
                GlobalFree(memory);
                return 0;
            }
        }

        CloseClipboard();
        return 0;
    }

    const CoA::Binding s_bindings[] = {
        COA_NATIVE("", "Internal_CopyToClipboard", Internal_CopyToClipboard),
    };
}

void CoA::GlobalsGClient::Init() {
    CoA::LuaReg::AddModule(s_bindings, sizeof(s_bindings) / sizeof(s_bindings[0]));
}
