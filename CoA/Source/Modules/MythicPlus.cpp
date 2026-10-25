// AscMythicPlus.cpp: C_MythicPlus over MythicPlusMgr (FUN_100d52f0, static 0x10BDB68C).
//
//   +0x04 set<u32> encounters killed      +0x14 u32 run instance id      +0x20 u32 timer (ms)
//   +0x0C u32 trash                        +0x18 u32 keystone key         +0x24 u32 current instance
//   +0x10 u32 champions dead               +0x1C u8  state
//
//   SMSG 0x9C1 SMSG_INSTANCE_INFO: u32 -> +0x24.
//   SMSG 0x908 DUNGEON_STATE_CHANGED (FUN_102bb630): u64 skipped, u8 state, u32 key, u32 instance. A countdown (state
//        1) or a new instance resets the run and takes key and instance. Then 1 -> MYTHIC_PLUS_COUNTDOWN_STARTED("%u",
//        10), 3 -> MYTHIC_PLUS_STARTED, 4/5 -> MYTHIC_PLUS_COMPLETE("%b", state != 5).
//   SMSG 0x909 PROGRESS_UPDATE (FUN_102bb850): u32 n x u32 encounter (newly seen ids fire MYTHIC_PLUS_ENCOUNTER_UPDATE
//        ("%u"), except 0xBA5..0xBAA), u32 trash, u32 champions; a change fires MYTHIC_PLUS_TRASH_UPDATE /
//        _CHAMPIONS_UPDATE("%u%u", count, required).
//   SMSG 0x90A TIMER_UPDATE (FUN_102bbdb0): u32 ms; outside the countdown fires MYTHIC_PLUS_TIMER_UPDATE("%f", seconds).
//
// "required" comes from TimedDungeons.dbc via the keystone record the bridge store's MythicKeystones table holds for
// the key: {u32 key (the keystone item id), u32 dungeon id, u32 level}.
#include "CoA/Source/Modules/MythicPlus.h"
#include "CoA/Source/Modules/BridgeStore.h"
#include "CoA/Source/Modules/RealmInfo.h"
#include "CoA/Source/Runtime/ClientState.h"
#include "CoA/Source/Runtime/Dbc.h"
#include "CoA/Source/Runtime/Events.h"
#include "CoA/Source/Runtime/Lifecycle.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Packet.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Source/Runtime/Read.h"
#include "CoA/Source/Runtime/Script.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <ctime>
#include <set>
#include <vector>

namespace {
    using CoA::Dbc::Table;
    using CoA::Script::CheckNumber;
    using CoA::Script::PushBool;
    using CoA::Script::ToInt;

    struct Mgr {
        std::set<uint32_t> encounters;
        uint32_t trash = 0;
        uint32_t champions = 0;
        uint32_t instance = 0;
        uint32_t key = 0;
        uint8_t state = 0;
        uint32_t timerMs = 0;
        uint32_t currentInstance = 0;

        // FUN_102bb390 (the current instance is not part of the run)
        void Reset() {
            this->encounters.clear();
            this->trash = 0;
            this->champions = 0;
            this->instance = 0;
            this->key = 0;
            this->state = 0;
            this->timerMs = 0;
        }
    };

    Mgr s_mgr;

    Table& TimedDungeons() {
        return CoA::Dbc::Get("DBFilesClient\\TimedDungeons.dbc");
    }

    Table& DungeonEncounterExtra() {
        return CoA::Dbc::Get("DBFilesClient\\DungeonEncounterExtra.dbc");
    }

    Table& MythicAffixes() {
        return CoA::Dbc::Get("DBFilesClient\\MythicAffixes.dbc");
    }

    // FUN_101d0500 on the keystone table: {+0 key, +4 dungeon id, +8 level}.
    struct Keystone {
        uint32_t key;
        uint32_t dungeonId;
        uint32_t level;
    };

    bool KeystoneRecord(uint32_t key, Keystone& out) {
        uint32_t rec[3];

        if (!CoA::BridgeStore::KeystoneById(key, rec)) {
            return false;
        }

        out = { rec[0], rec[1], rec[2] };
        return true;
    }

    const uint8_t* TimedDungeonFor(const Keystone* k) {
        return k ? TimedDungeons().Row(k->dungeonId) : nullptr;
    }

    // TimedDungeons row +off for the active keystone's dungeon, 0 when either lookup fails.
    uint32_t Required(uint32_t off) {
        Keystone k;

        if (!KeystoneRecord(s_mgr.key, k)) {
            return 0;
        }

        const uint8_t* row = TimedDungeons().Row(k.dungeonId);
        return row ? Table::U32(row, off) : 0;
    }

    // FUN_102bb0c0(L, level): this week's MythicAffixes row for `level` (week = yday / 7, UTC). With no match the scan
    // leaves the LAST row in hand, and the original uses it.
    void PushAffixes(lua_State* L, uint32_t level) {
        __time64_t now = _time64(nullptr);
        tm t{};
        _gmtime64_s(&t, &now);
        int32_t week = static_cast<int32_t>(static_cast<float>(t.tm_yday) / 7.0f);
        lua_createtable(L, 0, 0);
        lua_checkstack(L, 2);
        Table& tbl = MythicAffixes();
        const uint8_t* row = nullptr;

        for (uint32_t id = tbl.MinId(); id < tbl.MaxId() + 1; ++id) {
            row = tbl.Row(id);

            if (row && Table::U32(row, 8) == level && Table::I32(row, 4) == week) {
                break;
            }
        }

        if (!row) {
            return;
        }

        std::vector<uint32_t> ids;
        auto add = [&](uint32_t off) {
            if (uint32_t v = Table::U32(row, off)) {
                ids.push_back(v);
            }
        };
        add(0x1C);
        add(0x2C);
        add(0x0C);
        add(0x30);
        add(0x34);

        for (uint32_t i = 1; i < 4; ++i) {
            add(0x1C + i * 4);
        }

        add(0x38);

        for (uint32_t i = 1; i < 4; ++i) {
            add(0x0C + i * 4);
        }

        for (uint32_t i = 0; i < ids.size(); ++i) {
            lua_pushinteger(L, static_cast<int>(i + 1));
            lua_pushinteger(L, static_cast<int>(ids[i]));
            lua_settable(L, -3);
        }
    }

    uint32_t RowU32(const uint8_t* row, uint32_t off) {
        return row ? Table::U32(row, off) : 0;
    }

    float RowSeconds(const uint8_t* row, uint32_t off) {
        return row ? static_cast<float>(Table::I32(row, off)) / 1000.0f : 0.0f;
    }

    float RewardMultiplier(const uint8_t* timed) {
        return timed ? Table::F32(timed, 0x14) : 1.0f;
    }

    void SetInt(lua_State* L, const char* k, uint32_t v) {
        lua_pushstring(L, k);
        lua_pushinteger(L, static_cast<int>(v));
        lua_settable(L, -3);
    }

    void SetNum(lua_State* L, const char* k, float v) {
        lua_pushstring(L, k);
        lua_pushnumber(L, static_cast<double>(v));
        lua_settable(L, -3);
    }

    // The active run's keystone; false when the store does not know the key.
    bool ActiveKeystone(Keystone& k) {
        return KeystoneRecord(s_mgr.key, k);
    }

    int ActivateKeystone(lua_State* L) {
        CoA::Packet(0x527).Send();
        return 0;
    }

    int GetCurrentAffixes(lua_State* L) {
        PushAffixes(L, 0xFE);
        return 1;
    }

    // Encounters whose DungeonEncounterExtra row names this map in +0xC or +0x8.
    int GetMapEncounters(lua_State* L) {
        int32_t map = ToInt(CheckNumber(L, 1));
        lua_createtable(L, 0, 0);
        lua_checkstack(L, 2);
        Table& tbl = DungeonEncounterExtra();
        int32_t n = 1;

        for (uint32_t id = tbl.MinId(); id < tbl.MaxId() + 1; ++id) {
            if (id >= 0xBA5 && id <= 0xBAA) {
                continue;
            }

            const uint8_t* row = tbl.Row(id);

            if (!row) {
                continue;
            }

            int32_t a = Table::I32(row, 0xC);
            int32_t b = Table::I32(row, 8);

            if ((a != 0 && a == map) || (b != 0 && b == map)) {
                lua_pushinteger(L, n++);
                lua_pushinteger(L, static_cast<int>(Table::U32(row, 0)));
                lua_settable(L, -3);
            }
        }

        return 1;
    }

    int GetMapFinalEncounter(lua_State* L) {
        int32_t map = ToInt(CheckNumber(L, 1));
        Table& tbl = DungeonEncounterExtra();
        uint32_t found = 0;

        for (uint32_t id = tbl.MinId(); id < tbl.MaxId() + 1; ++id) {
            const uint8_t* row = tbl.Row(id);

            if (row && Table::I32(row, 0xC) != 0 && Table::I32(row, 0xC) == map) {
                found = Table::U32(row, 0);
                break;
            }
        }

        lua_pushinteger(L, static_cast<int>(found));
        return 1;
    }

    // Active: the run is in this instance and counting down, running or complete.
    int IsKeystoneActive(lua_State* L) {
        uint8_t s = s_mgr.state;
        PushBool(L, s_mgr.instance == s_mgr.currentInstance && (s == 1 || s == 3 || s == 4 || s == 5));
        return 1;
    }

    int GetActiveKeystoneChampions(lua_State* L) {
        Keystone k;
        const uint8_t* timed = ActiveKeystone(k) ? TimedDungeonFor(&k) : nullptr;
        lua_createtable(L, 0, 0);
        lua_checkstack(L, 2);
        SetInt(L, "championsDead", s_mgr.champions);
        SetInt(L, "championsRequired", RowU32(timed, 0xC));
        return 1;
    }

    int GetActiveKeystoneTrash(lua_State* L) {
        Keystone k;
        const uint8_t* timed = ActiveKeystone(k) ? TimedDungeonFor(&k) : nullptr;
        lua_createtable(L, 0, 0);
        lua_checkstack(L, 2);
        SetInt(L, "trashDead", s_mgr.trash);
        SetInt(L, "trashRequired", RowU32(timed, 4));
        return 1;
    }

    // Completed = encounters seen, not counting the dungeon's final one (the first DungeonEncounterExtra row naming
    // the dungeon in +0xC); required = TimedDungeons +8, minus one.
    int GetActiveKeystoneEncounters(lua_State* L) {
        Keystone k;
        bool found = ActiveKeystone(k);
        const uint8_t* timed = found ? TimedDungeonFor(&k) : nullptr;
        lua_createtable(L, 0, 0);
        lua_checkstack(L, 2);
        uint32_t dungeon = found ? k.dungeonId : 0;
        uint32_t last = 0;
        Table& tbl = DungeonEncounterExtra();

        for (uint32_t id = tbl.MinId(); id < tbl.MaxId() + 1; ++id) {
            const uint8_t* row = tbl.Row(id);

            if (row && Table::U32(row, 0xC) != 0 && Table::U32(row, 0xC) == dungeon) {
                last = Table::U32(row, 0);
                break;
            }
        }

        uint32_t seen = static_cast<uint32_t>(s_mgr.encounters.size());
        SetInt(L, "encountersCompleted", s_mgr.encounters.count(last) ? seen - 1 : seen);
        uint32_t req = RowU32(timed, 8);
        SetInt(L, "encountersRequired", req ? req - 1 : 0);
        return 1;
    }

    int GetActiveKeystoneInfo(lua_State* L) {
        Keystone k;
        bool found = ActiveKeystone(k);
        const uint8_t* timed = found ? TimedDungeonFor(&k) : nullptr;
        lua_createtable(L, 0, 0);
        lua_checkstack(L, 2);
        SetInt(L, "keystoneLevel", found ? k.level : 0);
        SetInt(L, "dungeonID", found ? k.dungeonId : 0);
        lua_pushstring(L, "activeAffixes");
        PushAffixes(L, found ? k.level : 0);
        lua_settable(L, -3);
        SetNum(L, "rewardMultiplier", RewardMultiplier(timed));
        return 1;
    }

    // Elapsed seconds, then the dungeon's time limit.
    int GetActiveKeystoneTime(lua_State* L) {
        Keystone k;
        const uint8_t* timed = ActiveKeystone(k) ? TimedDungeonFor(&k) : nullptr;
        lua_pushnumber(L, static_cast<double>(static_cast<float>(static_cast<int32_t>(s_mgr.timerMs)) / 1000.0f));
        lua_pushnumber(L, static_cast<double>(RowSeconds(timed, 0x10)));
        return 2;
    }

    // GetKeystoneInfo(key): scaling is the first MythicPlusScaling row for the keystone's level whose +8 is this
    // realm's ruleset.
    int GetKeystoneInfo(lua_State* L) {
        uint32_t key = static_cast<uint32_t>(ToInt(CheckNumber(L, 1)));
        Keystone k;
        bool found = KeystoneRecord(key, k);
        const uint8_t* timed = found ? TimedDungeonFor(&k) : nullptr;
        const uint8_t* scaling = nullptr;

        if (found) {
            Table& tbl = CoA::Dbc::Get("DBFilesClient\\MythicPlusScaling.dbc");
            uint32_t ruleset = CoA::RealmInfo::Get().ruleset;

            for (uint32_t id = tbl.MinId(); id < tbl.MaxId() + 1 && !scaling; ++id) {
                const uint8_t* row = tbl.Row(id);

                if (row && Table::U32(row, 4) == k.level && Table::U32(row, 8) == ruleset) {
                    scaling = row;
                }
            }
        }

        lua_createtable(L, 0, 0);
        lua_checkstack(L, 2);
        SetInt(L, "keystoneLevel", found ? k.level : 0);
        SetInt(L, "dungeonID", found ? k.dungeonId : 0);
        SetInt(L, "healthPercentage", RowU32(scaling, 0x18));
        SetInt(L, "magicDamagePercentage", RowU32(scaling, 0xC));
        SetInt(L, "physicalDamagePercentage", RowU32(scaling, 0x14));
        SetNum(L, "timeLimit", RowSeconds(timed, 0x10));
        lua_pushstring(L, "affixIDs");
        PushAffixes(L, found ? k.level : 0);
        lua_settable(L, -3);
        SetNum(L, "rewardMultiplier", RewardMultiplier(timed));
        return 1;
    }

    // A keystone: an item with display 67762 (Item.dbc +0x14) that the keystone table knows.
    int IsItemKeystone(lua_State* L) {
        if (!CoA::Script::ValidateInput(L, { LUA_TNUMBER })) {
            return luaL_error(L, "Usage: IsItemKeystone(itemId)");
        }

        uint32_t item = static_cast<uint32_t>(ToInt(CheckNumber(L, 1)));
        Keystone k;
        PushBool(L, CoA::ClientState::ItemDisplayId(item) == 0x108B2 && KeystoneRecord(item, k));
        return 1;
    }

    const CoA::Binding s_bindings[] = {
        COA_NATIVE("C_MythicPlus", "ActivateKeystone", ActivateKeystone),
        COA_NATIVE("C_MythicPlus", "GetActiveKeystoneChampions", GetActiveKeystoneChampions),
        COA_NATIVE("C_MythicPlus", "GetActiveKeystoneEncounters", GetActiveKeystoneEncounters),
        COA_NATIVE("C_MythicPlus", "GetActiveKeystoneInfo", GetActiveKeystoneInfo),
        COA_NATIVE("C_MythicPlus", "GetActiveKeystoneTime", GetActiveKeystoneTime),
        COA_NATIVE("C_MythicPlus", "GetActiveKeystoneTrash", GetActiveKeystoneTrash),
        COA_NATIVE("C_MythicPlus", "GetCurrentAffixes", GetCurrentAffixes),
        COA_NATIVE("C_MythicPlus", "GetKeystoneInfo", GetKeystoneInfo),
        COA_NATIVE("C_MythicPlus", "IsItemKeystone", IsItemKeystone),
        COA_NATIVE("C_MythicPlus", "GetMapEncounters", GetMapEncounters),
        COA_NATIVE("C_MythicPlus", "GetMapFinalEncounter", GetMapFinalEncounter),
        COA_NATIVE("C_MythicPlus", "IsKeystoneActive", IsKeystoneActive),
    };
}

void CoA::MythicPlus::Reset() {
    s_mgr.Reset();
}

bool CoA::MythicPlus::EncounterKilled(uint32_t encounter) {
    return s_mgr.encounters.count(encounter) != 0;
}

// SMSG 0x56D (FUN_101e24d0): {key, dungeon, level}.
int32_t CoA::MythicPlus::OnPatchKeystone(void* param, uint32_t opcode, uint32_t time, CDataStore* msg) {
    uint32_t rec[3];

    for (uint32_t& v : rec) {
        v = CoA::Read::U32(msg);
    }

    CoA::BridgeStore::PatchKeystone(rec);
    return 1;
}

int32_t CoA::MythicPlus::OnInstanceInfo(void* param, uint32_t opcode, uint32_t time, CDataStore* msg) {
    s_mgr.currentInstance = CoA::Read::U32(msg);
    return 1;
}

int32_t CoA::MythicPlus::OnStateChanged(void* param, uint32_t opcode, uint32_t time, CDataStore* msg) {
    CoA::Read::U64(msg);
    uint8_t state = CoA::Read::U8(msg);
    uint32_t key = CoA::Read::U32(msg);
    uint32_t instance = CoA::Read::U32(msg);

    if (state == 1 || s_mgr.instance != instance) {
        s_mgr.Reset();
        s_mgr.instance = instance;
        s_mgr.key = key;
    }

    s_mgr.state = state;

    switch (state) {
    case 1:
        CoA::Events::Signal("MYTHIC_PLUS_COUNTDOWN_STARTED", "%u", 10u);
        break;
    case 3:
        CoA::Events::Signal("MYTHIC_PLUS_STARTED", nullptr);
        break;
    case 4:
    case 5:
        CoA::Events::Signal("MYTHIC_PLUS_COMPLETE", "%b", state != 5 ? 1 : 0);
        break;
    default:
        break;
    }

    return 1;
}

int32_t CoA::MythicPlus::OnProgressUpdate(void* param, uint32_t opcode, uint32_t time, CDataStore* msg) {
    for (uint32_t n = CoA::Read::U32(msg); n && !CoA::Read::Failed(msg); --n) {
        uint32_t id = CoA::Read::U32(msg);

        if (!s_mgr.encounters.insert(id).second || (id >= 0xBA5 && id <= 0xBAA)) {
            continue;
        }

        CoA::Events::Signal("MYTHIC_PLUS_ENCOUNTER_UPDATE", "%u", id);
    }

    uint32_t trash = CoA::Read::U32(msg);

    if (s_mgr.trash != trash) {
        s_mgr.trash = trash;
        CoA::Events::Signal("MYTHIC_PLUS_TRASH_UPDATE", "%u%u", s_mgr.trash, Required(4));
    }

    uint32_t champions = CoA::Read::U32(msg);

    if (s_mgr.champions != champions) {
        s_mgr.champions = champions;
        CoA::Events::Signal("MYTHIC_PLUS_CHAMPIONS_UPDATE", "%u%u", s_mgr.champions, Required(0xC));
    }

    return 1;
}

int32_t CoA::MythicPlus::OnTimerUpdate(void* param, uint32_t opcode, uint32_t time, CDataStore* msg) {
    s_mgr.timerMs = CoA::Read::U32(msg);

    if (s_mgr.state != 1) {
        CoA::Events::Signal("MYTHIC_PLUS_TIMER_UPDATE", "%f", static_cast<double>(static_cast<float>(static_cast<int32_t>(s_mgr.timerMs)) / 1000.0f));
    }

    return 1;
}

void CoA::MythicPlus::Init() {
    CoA::LuaReg::AddModule(s_bindings, sizeof(s_bindings) / sizeof(s_bindings[0]));
    CoA::Lifecycle::OnGlueScreen(CoA::MythicPlus::Reset);
    COA_PACKET(0x56D, CoA::MythicPlus::OnPatchKeystone);
    COA_PACKET(0x908, CoA::MythicPlus::OnStateChanged);
    COA_PACKET(0x909, CoA::MythicPlus::OnProgressUpdate);
    COA_PACKET(0x90A, CoA::MythicPlus::OnTimerUpdate);
    COA_PACKET(0x9C1, CoA::MythicPlus::OnInstanceInfo);
    COA_EVENT("MYTHIC_PLUS_COUNTDOWN_STARTED");
    COA_EVENT("MYTHIC_PLUS_STARTED");
    COA_EVENT("MYTHIC_PLUS_COMPLETE");
    COA_EVENT("MYTHIC_PLUS_ENCOUNTER_UPDATE");
    COA_EVENT("MYTHIC_PLUS_TRASH_UPDATE");
    COA_EVENT("MYTHIC_PLUS_CHAMPIONS_UPDATE");
    COA_EVENT("MYTHIC_PLUS_TIMER_UPDATE");
}
