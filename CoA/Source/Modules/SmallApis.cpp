// Small namespaces with no shared state (port of AscSmallApis.cpp). Ported so far: the glue-state natives C_CVar
// and C_EquipmentSet.PlaceInBank.
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Packet.h"
#include "CoA/Source/Runtime/Script.h"
#include "Engine/Source/ConsoleBase/ConsoleVar.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <string>

namespace {
    // ---- C_CVar (FUN_10114940 / a50 / b60) ----------------------------------------------------------------------
    // The DLL calls CVarArrayWrapper::RawGetValue / RawSetValue (0x766860 / 0x766CA0) on a bare holder of the CVar*,
    // never the wrapper's constructor, so no value-version conversion runs first.
    struct BareWrapper {
        CVar* cvar;
        uint8_t defaultValue;
    };

    CVarArrayWrapper* Wrap(BareWrapper& holder) {
        return reinterpret_cast<CVarArrayWrapper*>(&holder);
    }

    int GetCVarBitfield(lua_State* L) {
        std::string name = CoA::Script::CheckString(L, 1);
        uint32_t index = static_cast<uint32_t>(CoA::Script::ToInt(CoA::Script::CheckNumber(L, 2)));
        CVar* var = CVar::Lookup(name.c_str());

        if (!var) {
            lua_pushnil(L);
            return 1;
        }

        BareWrapper holder = { var, 0 };
        CoA::Script::PushBool(L, Wrap(holder)->RawGetValue(index));
        return 1;
    }

    int SetCVarBitfield(lua_State* L) {
        std::string name = CoA::Script::CheckString(L, 1);
        uint32_t index = static_cast<uint32_t>(CoA::Script::ToInt(CoA::Script::CheckNumber(L, 2)));
        bool value = lua_toboolean(L, 3) != 0;
        CVar* var = CVar::Lookup(name.c_str());
        bool ok = false;

        if (var) {
            BareWrapper holder = { var, 0 };
            ok = Wrap(holder)->RawSetValue(index, value);
        }

        CoA::Script::PushBool(L, ok);
        return 1;
    }

    // CVar flag 0x80 = "do not save".
    int SetCVarSave(lua_State* L) {
        std::string name = CoA::Script::CheckString(L, 1);
        bool save = lua_toboolean(L, 2) != 0;

        if (CVar* var = CVar::Lookup(name.c_str())) {
            var->m_flags = save ? (var->m_flags & ~0x80u) : (var->m_flags | 0x80u);
        }

        return 0;
    }

    // C_EquipmentSet.PlaceInBank(index) (0x10228520): luaL_checknumber; below 20, CMSG 0x55E with index - 1 (index 0
    // wraps). Returns nothing.
    int PlaceInBank(lua_State* L) {
        uint32_t index = static_cast<uint32_t>(CoA::Script::ToInt(CoA::Script::CheckNumber(L, 1)));

        if (index < 0x14) {
            CoA::Packet(0x55E).U32(index - 1).Send();
        }

        return 0;
    }

    // C_CharacterCustomizationUnlocks (handler_GetCurrentOptionInfo / ...): constant in the original.
    int GetCurrentOptionInfo(lua_State* L) {
        if (lua_gettop(L) != 1) {
            return 0;
        }

        CoA::Script::CheckNumber(L, 1);
        CoA::Script::PushBool(L, false);
        CoA::Script::PushBool(L, true);
        CoA::Script::PushInt(L, 0);
        return 3;
    }

    int GetFirstLockedRequirement(lua_State* L) {
        CoA::Script::PushInt(L, 0);
        return 1;
    }

    int HasLockedSelections(lua_State* L) {
        CoA::Script::PushBool(L, false);
        return 1;
    }

    const CoA::Binding s_bindings[] = {
        COA_NATIVE("C_CVar", "GetCVarBitfield", GetCVarBitfield),
        COA_NATIVE("C_CVar", "SetCVarBitfield", SetCVarBitfield),
        COA_NATIVE("C_CVar", "SetCVarSave", SetCVarSave),
        COA_NATIVE("C_EquipmentSet", "PlaceInBank", PlaceInBank),
        COA_NATIVE("C_CharacterCustomizationUnlocks", "GetCurrentOptionInfo", GetCurrentOptionInfo),
        COA_NATIVE("C_CharacterCustomizationUnlocks", "GetFirstLockedRequirement", GetFirstLockedRequirement),
        COA_NATIVE("C_CharacterCustomizationUnlocks", "HasLockedSelections", HasLockedSelections),
    };
}

void CoA::SmallApis::Init() {
    CoA::LuaReg::AddModule(s_bindings, sizeof(s_bindings) / sizeof(s_bindings[0]));
}
