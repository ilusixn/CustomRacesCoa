// C_RealmMerge (FUN_102fd660 / 960 / da40 / dc60 / dea0); port of AscRealmMerge.cpp.
//
// Realm ids and names are the DLL's static enum (0x10B580C8 ids, 0x10B58120 names, 21 entries). SMSG 0x5F5 sets the
// target realm id; SMSG 0x5F7: u32 len + string -> REALM_MERGE_SET_TARGET_RESULT("%s"). SetTargetRealm sends CMSG 0x5F6
// u32 id. Availability comes from the config store: CONFIG_REALM_MERGE_ENABLED (bool) and
// CONFIG_REALM_MERGE_AVAILABLE_REALMS (int vector).
#include "CoA/Source/Modules/Config.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/Events.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Packet.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Source/Runtime/Read.h"
#include "CoA/Source/Runtime/Script.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <algorithm>
#include <string>
#include <vector>

namespace {
    struct Realm {
        int32_t id;
        const char* name;
    };

    const Realm s_realms[21] = {
        { 0, "None" }, { 1, "AscensionRealm" }, { 2, "Proudmoore" }, { 3, "GrizzlyHills" }, { 4, "League" }, { 5, "Bloodhoof" },
        { 6, "Daggerspine" }, { 7, "Moroes" }, { 8, "Bronzebeard" }, { 9, "TwistingNether" }, { 10, "CharacterArchive" },
        { 11, "Area52" }, { 12, "ThunderbluffQA" }, { 13, "CoA" }, { 14, "Fizzcrank" }, { 15, "Archimonde" },
        { 16, "WarcraftReborn" }, { 17, "Uther" }, { 26, "WildhammerQA" }, { 27, "Rexxar" }, { 28, "FreepickLiveQA" },
    };

    int32_t s_target = 0;   // 0x10BE3B24

    const char* RealmName(int32_t id) {
        for (const Realm& r : s_realms) {
            if (r.id == id) {
                return r.name;
            }
        }

        return nullptr;
    }

    const std::vector<int32_t>* Available() {
        return CoA::Config::IntVector("CONFIG_REALM_MERGE_AVAILABLE_REALMS");
    }

    bool Enabled() {
        const bool* enabled = CoA::Config::Bool("CONFIG_REALM_MERGE_ENABLED");
        return enabled && *enabled;
    }

    int32_t OnSetTarget(void* param, uint32_t opcode, uint32_t time, CDataStore* msg) {   // handler_0x05f5
        s_target = CoA::Read::I32(msg);
        return 1;
    }

    int32_t OnSetTargetResult(void* param, uint32_t opcode, uint32_t time, CDataStore* msg) {   // FUN_102fd410
        std::string result = CoA::Read::Name(msg);
        CoA::Events::Signal("REALM_MERGE_SET_TARGET_RESULT", "%s", result.c_str());
        return 1;
    }

    // Names of the available realm ids that are in the enum, in config order.
    int GetAvailableOptions(lua_State* L) {
        std::vector<const char*> names;

        if (const std::vector<int32_t>* ids = Available()) {
            for (int32_t id : *ids) {
                if (const char* name = RealmName(id)) {
                    names.push_back(name);
                }
            }
        }

        lua_createtable(L, 0, 0);
        lua_checkstack(L, 2);

        for (uint32_t i = 0; i < names.size(); ++i) {
            CoA::Script::PushNum(L, i + 1);
            CoA::Script::PushStr(L, names[i]);
            lua_settable(L, -3);
        }

        return 1;
    }

    int GetTargetRealm(lua_State* L) {
        if (const char* name = RealmName(s_target)) {
            CoA::Script::PushStr(L, name);
        } else {
            lua_pushnil(L);
        }

        return 1;
    }

    int IsMergeAvailable(lua_State* L) {
        const std::vector<int32_t>* ids = Available();
        CoA::Script::PushBool(L, Enabled() && ids && !ids->empty());
        return 1;
    }

    int NeedsSetTargetRealm(lua_State* L) {
        const std::vector<int32_t>* ids = Available();
        CoA::Script::PushBool(L, Enabled() && ids && !ids->empty() && std::find(ids->begin(), ids->end(), s_target) == ids->end());
        return 1;
    }

    // FUN_102fcfa0: exact name -> id; an unknown name resolves to 0 ("None"), which is refused.
    int SetTargetRealm(lua_State* L) {
        if (!CoA::Script::ValidateInput(L, { LUA_TSTRING })) {
            return 0;
        }

        std::string name = CoA::Script::CheckString(L, 1);
        int32_t id = 0;

        for (const Realm& r : s_realms) {
            if (name == r.name) {
                id = r.id;
                break;
            }
        }

        if (id == 0) {
            CoA::Script::PushBool(L, false);
            return 1;
        }

        CoA::Packet(0x5F6).U32(static_cast<uint32_t>(id)).Send();
        CoA::Script::PushBool(L, true);
        return 1;
    }

    const CoA::Binding s_bindings[] = {
        COA_NATIVE("C_RealmMerge", "GetAvailableOptions", GetAvailableOptions),
        COA_NATIVE("C_RealmMerge", "GetTargetRealm", GetTargetRealm),
        COA_NATIVE("C_RealmMerge", "IsMergeAvailable", IsMergeAvailable),
        COA_NATIVE("C_RealmMerge", "NeedsSetTargetRealm", NeedsSetTargetRealm),
        COA_NATIVE("C_RealmMerge", "SetTargetRealm", SetTargetRealm),
    };
}

void CoA::RealmMerge::Init() {
    COA_PACKET(0x5F5, OnSetTarget);
    COA_PACKET(0x5F7, OnSetTargetResult);
    COA_EVENT("REALM_MERGE_SET_TARGET_RESULT");
    CoA::LuaReg::AddModule(s_bindings, sizeof(s_bindings) / sizeof(s_bindings[0]));
}
