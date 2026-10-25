// AscGlobalsF.cpp's client natives: the cursor, the level-scaling request (gated by CanPerformAction), SignalEvent and the
// two unit queries.
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/Events.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Packet.h"
#include "CoA/Source/Runtime/Script.h"
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/Os/Gui.h"
#include "Engine/Source/Os/Input.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include "WoW/Source/Object/ObjectClient/Unit_C.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"
#include "WoW/Source/Ui/GameUI.h"
#include "WoW/Source/Ui/ScriptEvents.h"
#include <windows.h>
#include <string>

namespace {
    // (x, y) in 0..1 of the client rect (OsGetDefaultWindowRect 0x86A1A0), y measured from the bottom; returns 1
    // without pushing.
    int SetCursorPosition(lua_State* L) {
        const float x = static_cast<float>(lua_tonumber(L, 1));
        const float y = static_cast<float>(lua_tonumber(L, 2));
        tagRECT rect;

        if (OsGetDefaultWindowRect(&rect)) {
            POINT pt;
            pt.x = static_cast<LONG>(static_cast<float>(rect.right - rect.left) * x) + rect.left;
            pt.y = static_cast<LONG>(static_cast<float>(rect.bottom - rect.top) * (1.0f - y)) + rect.top;
            ClientToScreen(static_cast<HWND>(OsGuiGetWindow(0)), &pt);
            SetCursorPos(pt.x, pt.y);
        }

        return 1;
    }

    // FUN_10111ce0: CGGameUI::CanPerformAction(0x11) gates CMSG 0x667 { u32 on }.
    int SetLevelScaling(lua_State* L) {
        if (CGGameUI::CanPerformAction(0x11)) {
            CoA::Packet(0x667).U32(lua_toboolean(L, 1) != 0).Send();
        }

        return 0;
    }

    // Fires a registered interface event with the remaining arguments (FrameScript_SignalEvent 0x81AA00, the count
    // includes the name), in world only.
    int SignalEvent(lua_State* L) {
        if (!CGGameUI::s_inWorld) {
            luaL_error(L, "Script_SignalEvent can only be called once the player is in the world.");
            return 0;
        }

        const char* name = CoA::Script::CheckString(L, 1);
        const int32_t id = CoA::Events::Id(name);

        if (id == -1) {
            const std::string msg = std::string("Event ") + name + " not found. Use RegisterInterfaceEvent(\"" + name + "\") to register it.";
            luaL_error(L, msg.c_str());
            return 0;
        }

        FrameScript_SignalEvent(static_cast<uint32_t>(id), L, lua_gettop(L));
        return 0;
    }

    // Unit by token (GetUnitFromName 0x60C1F0): the spell being cast (+0xA6C), else the one channelled (+0xA80).
    int UnitCastingSpellID(lua_State* L) {
        std::string token;

        if (!CoA::Script::ReadString(L, token)) {
            return 0;
        }

        const CGUnit_C* unit = GetUnitFromName(token.c_str());

        if (!unit) {
            return 0;
        }

        uint32_t id = unit->m_castingSpellId;

        if (id == 0) {
            id = unit->m_channelSpellId;
        }

        lua_pushinteger(L, static_cast<int32_t>(id));
        return 1;
    }

    // FUN_103086d0: unit by token (GetGuidByKeyword 0x60C1C0, ObjectPtr mask 8), its dynamic flags (descriptor +0x13C)
    // bit 8; nil without a unit.
    int UnitIsLevelScaling(lua_State* L) {
        const std::string token = CoA::Script::CheckString(L, 1);
        const WOWGUID guid = GetGuidByKeyword(token.c_str());
        const CGUnit_C* unit = guid.m_guid ? static_cast<const CGUnit_C*>(ClntObjMgrObjectPtr(guid, TYPE_UNIT, __FILE__, __LINE__)) : nullptr;

        if (!unit) {
            lua_pushnil(L);
            return 1;
        }

        lua_pushboolean(L, (unit->m_unit->m_unitDynamicFlags >> 8) & 1);
        return 1;
    }

    const CoA::Binding s_bindings[] = {
        COA_NATIVE("", "SetCursorPosition", SetCursorPosition),
        COA_NATIVE("", "SetLevelScaling", SetLevelScaling),
        COA_NATIVE("", "SignalEvent", SignalEvent),
        COA_NATIVE("", "UnitCastingSpellID", UnitCastingSpellID),
        COA_NATIVE("", "UnitIsLevelScaling", UnitIsLevelScaling),
    };
}

void CoA::GlobalsFClient::Init() {
    CoA::LuaReg::AddModule(s_bindings, sizeof(s_bindings) / sizeof(s_bindings[0]));
}
