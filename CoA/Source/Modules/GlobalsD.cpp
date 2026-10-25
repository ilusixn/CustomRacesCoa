// GLOBAL bindings, batch D; port of AscGlobalsD.cpp. The natives over client objects are in GlobalsDClient.cpp.
#include "CoA/Source/Modules/BridgeStore.h"
#include "CoA/Source/Modules/GameEvents.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Modules/RealmInfo.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Script.h"
#include "Engine/Source/lua-5.1/src/lua.h"

namespace {
    // Expansion cap (RealmInfo +0x08: 0 -> 60, 1 -> 70, 2 -> 80), lowered by the level-cap game events. The fixed
    // caps (0x1E9..0x1E6) are returned as they are, even above the ruleset cap.
    int GetRealmMaxLevel(lua_State* L) {
        uint32_t ruleset = CoA::RealmInfo::Get().ruleset;
        uint32_t cap = ruleset == 1 ? 70 : ruleset == 2 ? 80 : 60;
        using CoA::GameEvents::IsActive;

        if (!IsActive(0x164)) {
            uint32_t eventCap;

            if (IsActive(0xAD)) {
                eventCap = 70;
            } else if (IsActive(0x1EA)) {
                eventCap = 60;
            } else {
                static const struct {
                    uint16_t event;
                    int32_t level;
                } fixed[] = { { 0x1E9, 55 }, { 0x1E8, 45 }, { 0x1E7, 35 }, { 0x1E6, 25 } };

                for (const auto& f : fixed) {
                    if (IsActive(f.event)) {
                        CoA::Script::PushInt(L, f.level);
                        return 1;
                    }
                }

                eventCap = IsActive(0x160) ? 60 : cap;
            }

            if (eventCap < cap) {
                cap = eventCap;
            }
        }

        CoA::Script::PushInt(L, static_cast<int32_t>(cap));
        return 1;
    }

    // FUN_1020a*: exactly one numeric argument, then the item addon record (FUN_101a2690).
    const CoA::ItemAddon::Record* ItemArg(lua_State* L) {
        if (lua_gettop(L) != 1 || !lua_isnumber(L, 1)) {
            return nullptr;
        }

        return CoA::ItemAddon::Find(static_cast<uint32_t>(CoA::Script::ToInt(lua_tonumber(L, 1))));
    }

    int ItemField(lua_State* L, uint32_t field) {
        if (const CoA::ItemAddon::Record* r = ItemArg(L)) {
            lua_pushinteger(L, static_cast<int>(r->fields[field]));
        } else {
            lua_pushnil(L);
        }

        return 1;
    }

    int GetItemQuality(lua_State* L) {   // +0x10
        return ItemField(L, 0);
    }

    int GetItemPvEPower(lua_State* L) {   // +0x18
        return ItemField(L, 2);
    }

    int GetItemPvPPower(lua_State* L) {   // +0x1C
        return ItemField(L, 3);
    }

    int GetItemName(lua_State* L) {   // +0x08
        if (const CoA::ItemAddon::Record* r = ItemArg(L)) {
            CoA::Script::PushStr(L, r->name);
        } else {
            lua_pushnil(L);
        }

        return 1;
    }

    int GetItemFlavorText(lua_State* L) {   // +0x0C
        if (const CoA::ItemAddon::Record* r = ItemArg(L)) {
            CoA::Script::PushStr(L, r->description);
        } else {
            lua_pushnil(L);
        }

        return 1;
    }

    const CoA::Binding s_bindings[] = {
        COA_NATIVE("", "GetItemFlavorText", GetItemFlavorText),
        COA_NATIVE("", "GetItemName", GetItemName),
        COA_NATIVE("", "GetItemPvEPower", GetItemPvEPower),
        COA_NATIVE("", "GetItemPvPPower", GetItemPvPPower),
        COA_NATIVE("", "GetItemQuality", GetItemQuality),
        COA_NATIVE("", "GetRealmMaxLevel", GetRealmMaxLevel),
    };
}

void CoA::GlobalsD::Init() {
    CoA::LuaReg::AddModule(s_bindings, sizeof(s_bindings) / sizeof(s_bindings[0]));
}
