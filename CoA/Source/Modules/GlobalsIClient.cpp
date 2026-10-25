// AscGlobalsI.cpp: CVar listing, runtime interface events, screen projection and the nearest-object helpers. All of it
// reads client state, so there is no pure half.
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/Events.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Script.h"
#include "Engine/Source/ConsoleBase/ConsoleVar.h"
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/Tempest/C3Vector.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include "WoW/Source/Object/ObjectClient/Player_C.h"
#include "WoW/Source/Object/ObjectClient/Unit_C.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"
#include "WoW/Source/Ui/GameUI.h"
#include "WoW/Source/Ui/ScriptEvents.h"
#include "WoW/Source/Ui/WorldFrame.h"
#include <cmath>
#include <cstring>
#include <utility>
#include <vector>

namespace {
    // handler_ListCVars: every registered CVar (0xCA19FC) in the hash's list order, { [name] = value }.
    int ListCVars(lua_State* L) {
        lua_createtable(L, 0, 0);

        for (CVar* var = s_registeredCVars.Head(); var; var = s_registeredCVars.Next(var)) {
            CoA::Script::PushStr(L, var->m_key.m_str);
            CoA::Script::PushStr(L, var->m_stringValue.m_str);
            lua_settable(L, -3);
        }

        return 1;
    }

    // handler_RegisterInterfaceEvent: in world only; a name the client does not know is queued as a custom and the 722
    // stock events are created again (0x81B5F0, CoA::Events::ExtendList appends the customs). Never freed, as in the
    // original (_strdup).
    int RegisterInterfaceEvent(lua_State* L) {
        if (!CGGameUI::s_inWorld) {
            luaL_error(L, "RegisterInterfaceEvent can only be called once the player is in the world.");
            return 0;
        }

        const char* name = CoA::Script::CheckString(L, 1);

        if (CoA::Events::Id(name) != -1) {
            lua_pushboolean(L, 0);
            return 1;
        }

        CoA::Events::Queue(_strdup(name));
        FrameScript_CreateEvents(g_scriptEvents, NUM_SCRIPT_EVENTS);
        lua_pushboolean(L, CoA::Events::Id(name) != -1);
        return 1;
    }

    // FUN_1019ca10: a unit's position projected by the world frame (CGWorldFrame::s_currentWorldFrame,
    // GetScreenCoordinates 0x4F6D20) -> x, y. An off-screen point leaves them at 0, as in the original.
    int GetScreenPosition(lua_State* L) {
        WOWGUID guid;

        if (!Script_GetGUIDFromToken(CoA::Script::CheckString(L, 1), guid, false)) {
            return 0;
        }

        CGObject_C* unit = ClntObjMgrObjectPtr(guid, static_cast<OBJECT_TYPE>(TYPE_UNIT | TYPE_PLAYER), __FILE__, __LINE__);

        if (!unit) {
            return 0;
        }

        const C3Vector pos = unit->GetPosition();
        C3Vector xy(0.0f, 0.0f, 0.0f);
        CGWorldFrame::s_currentWorldFrame->GetScreenCoordinates(pos, &xy, nullptr);
        lua_pushnumber(L, xy.x);
        lua_pushnumber(L, xy.y);
        return 2;
    }

    // ---- nearest objects (FUN_1019cc50 / FUN_103367a0) ---------------------------------------------------------
    // Units from ClntObjMgrEnumVisibleObjects (callback LAB_1019c4c0: ObjectPtr mask 8).
    int32_t CollectUnit(WOWGUID guid, void* param) {
        if (CGObject_C* obj = ClntObjMgrObjectPtr(guid, TYPE_UNIT, __FILE__, __LINE__)) {
            static_cast<std::vector<CGObject_C*>*>(param)->push_back(obj);
        }

        return 1;
    }

    std::vector<CGObject_C*> VisibleUnits() {
        std::vector<CGObject_C*> v;
        ClntObjMgrEnumVisibleObjects(CollectUnit, &v);
        return v;
    }

    float Distance(const C3Vector& a, const C3Vector& b) {
        const float dz = static_cast<float>(std::pow(static_cast<double>(b.z - a.z), 2.0));
        const float dx = static_cast<float>(std::pow(static_cast<double>(b.x - a.x), 2.0));
        const float dy = static_cast<float>(std::pow(static_cast<double>(b.y - a.y), 2.0));
        return static_cast<float>(std::sqrt(static_cast<double>(dy + dx + dz)));
    }

    // The first of the closest (a later candidate only wins when strictly closer).
    CGObject_C* Closest(const std::vector<std::pair<CGObject_C*, float>>& c) {
        if (c.empty()) {
            return nullptr;
        }

        size_t best = 0;

        for (size_t i = 1; i < c.size(); ++i) {
            if (!(c[best].second < c[i].second || c[best].second == c[i].second)) {
                best = i;
            }
        }

        return c[best].first;
    }

    // FUN_10111d00 -> CGGameUI::Target (0x524BF0).
    void TargetUnit(const CGObject_C* obj) {
        CGGameUI::Target(obj->m_obj->m_guid);
    }

    // Within 5.5 * 5.5 of the player -- the original compares the distance, not its square, against the squared range
    // -- interact (OnRightClick) with the closest, and target it when it is a unit.
    int InteractNearest(lua_State*) {
        CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();
        const C3Vector p = player->GetPosition();
        std::vector<std::pair<CGObject_C*, float>> c;

        for (CGObject_C* o : VisibleUnits()) {
            const float d = Distance(p, o->GetPosition());

            if (d <= 5.5f * 5.5f) {
                c.emplace_back(o, d);
            }
        }

        if (CGObject_C* best = Closest(c)) {
            best->OnRightClick();

            if (best->m_typeId == ID_UNIT) {
                TargetUnit(best);
            }
        }

        return 0;
    }

    // The closest unit the player can attack (CGUnit_C::CanAttackNow 0x729A70) becomes the target when it is a
    // creature. lua_tainted (0xD4139C) is cleared for the scan and restored only when a candidate was found, as in the
    // original: with none, the caller continues untainted.
    int TargetClosestEnemy(lua_State*) {
        const char* const saved = lua_tainted;
        lua_tainted = nullptr;
        CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();
        const C3Vector p = player->GetPosition();
        std::vector<std::pair<CGObject_C*, float>> c;

        for (CGObject_C* o : VisibleUnits()) {
            if (player->CanAttackNow(static_cast<const CGUnit_C*>(o))) {
                c.emplace_back(o, Distance(p, o->GetPosition()));
            }
        }

        if (CGObject_C* best = Closest(c)) {
            if (best->m_typeId == ID_UNIT) {
                TargetUnit(best);
            }

            lua_tainted = saved;
        }

        return 0;
    }

    const CoA::Binding s_bindings[] = {
        COA_NATIVE("", "GetScreenPosition", GetScreenPosition),
        COA_NATIVE("", "InteractNearest", InteractNearest),
        COA_NATIVE("", "ListCVars", ListCVars),
        COA_NATIVE("", "RegisterInterfaceEvent", RegisterInterfaceEvent),
        COA_NATIVE("", "TargetClosestEnemy", TargetClosestEnemy),
    };
}

void CoA::GlobalsIClient::Init() {
    CoA::LuaReg::AddModule(s_bindings, sizeof(s_bindings) / sizeof(s_bindings[0]));
}
