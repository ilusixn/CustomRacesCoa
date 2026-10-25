// GLOBAL bindings, batch D; port of AscGlobalsD.cpp. Ported so far: the glue-state natives.
#include "CoA/Source/Modules/GameEvents.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Modules/RealmInfo.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Script.h"

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

    const CoA::Binding s_bindings[] = {
        COA_NATIVE("", "GetRealmMaxLevel", GetRealmMaxLevel),
    };
}

void CoA::GlobalsD::Init() {
    CoA::LuaReg::AddModule(s_bindings, sizeof(s_bindings) / sizeof(s_bindings[0]));
}
