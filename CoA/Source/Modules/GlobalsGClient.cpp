// AscGlobalsG.cpp's natives over client objects: Internal_CopyToClipboard (glue), UnitTitle, GetTokenIDsFromGUID and
// UnitDistanceSquared. The natives that set client values are in GlobalsG.cpp.
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Script.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Source/DB/Rec/CharTitlesRec.h"
#include "WoW/Source/Object/ObjectClient/Player_C.h"
#include "WoW/Source/Object/ObjectClient/UnitCombatLog_C.h"
#include "WoW/Source/Ui/ScriptEvents.h"
#include <cmath>
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

    // The unit's chosen title (PLAYER_CHOSEN_TITLE, descriptor +0x504 -> CGUnit_C::GetRecordForTitle) as its CharTitles
    // name: the female form for a unit or player whose gender byte is set.
    int UnitTitle(lua_State* L) {
        WOWGUID guid;
        CGUnit_C* unit = nullptr;
        CharTitlesRec* row = nullptr;

        if (Script_GetGUIDFromToken(CoA::Script::CheckString(L, 1), guid, false)
            && (unit = static_cast<CGUnit_C*>(ClntObjMgrObjectPtr(guid, TYPE_PLAYER, __FILE__, __LINE__))) != nullptr) {
            const int32_t title = CGUnit_C::GetRecordForTitle(static_cast<int32_t>(static_cast<CGPlayer_C*>(unit)->m_player->m_playerChosenTitle));

            if (title) {
                row = g_charTitlesDB.GetRecord(title);
            }
        }

        if (!row) {
            lua_pushnil(L);
            return 1;
        }

        const bool female = (unit->m_typeId == ID_UNIT || unit->m_typeId == ID_PLAYER) && unit->m_unit->m_unitFieldBytes0[2] != 0;
        CoA::Script::PushStr(L, female ? row->m_name1 : row->m_name);
        return 1;
    }

    // The unit tokens that currently resolve to a GUID (StringToGuid, Script_GetTokenIDsFromGUID).
    int GetTokenIDsFromGUID(lua_State* L) {
        if (!lua_isstring(L, 1)) {
            luaL_error(L, "Usage: GetTokenIDsFromGUID(\"playerGUID\")");
        }

        const std::string text = CoA::Script::CheckString(L, 1);
        const WOWGUID guid = StringToGuid(text.c_str());

        if (!guid.m_guid) {
            luaL_error(L, "Usage: GetTokenIDsFromGUID(\"playerGUID\")");
        }

        int32_t count = 0;
        char** tokens = Script_GetTokenIDsFromGUID(guid, &count);

        for (int32_t i = 0; i < count; ++i) {
            CoA::Script::PushStr(L, tokens[i]);
        }

        return count;
    }

    // Squared distance from the player to a party / raid member (CGUnit_C::IsInMyPartyOrRaid); (0, false) when it
    // cannot be measured, nil for an unknown token.
    int UnitDistanceSquared(lua_State* L) {
        WOWGUID guid;

        if (!Script_GetGUIDFromToken(CoA::Script::CheckString(L, 1), guid, false)) {
            lua_pushnil(L);
            return 1;
        }

        CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();
        CGUnit_C* target = static_cast<CGUnit_C*>(ClntObjMgrObjectPtr(guid, TYPE_UNIT, __FILE__, __LINE__));
        float d2 = 0.0f;
        bool ok = false;

        if (target && player && player->IsInMyPartyOrRaid(target)) {
            const C3Vector a = player->GetPosition();
            const C3Vector b = target->GetPosition();
            const float dz = static_cast<float>(std::pow(static_cast<double>(a.z - b.z), 2.0));
            const float dx = static_cast<float>(std::pow(static_cast<double>(a.x - b.x), 2.0));
            const float dy = static_cast<float>(std::pow(static_cast<double>(a.y - b.y), 2.0));
            const float d = static_cast<float>(std::sqrt(static_cast<double>(dy + dx + dz)));
            d2 = d * d;
            ok = true;
        }

        lua_pushnumber(L, d2);
        lua_pushboolean(L, ok);
        return 2;
    }

    const CoA::Binding s_bindings[] = {
        COA_NATIVE("", "Internal_CopyToClipboard", Internal_CopyToClipboard),
        COA_NATIVE("", "GetTokenIDsFromGUID", GetTokenIDsFromGUID),
        COA_NATIVE("", "UnitDistanceSquared", UnitDistanceSquared),
        COA_NATIVE("", "UnitTitle", UnitTitle),
    };
}

void CoA::GlobalsGClient::Init() {
    CoA::LuaReg::AddModule(s_bindings, sizeof(s_bindings) / sizeof(s_bindings[0]));
}
