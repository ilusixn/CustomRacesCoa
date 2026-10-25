#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/Dbc.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Tests/Support.h"
#include "CoA/Tests/TestMain.h"
#include <cstring>
#include <string>
#include <vector>

namespace {
    void Feed(uint32_t opcode, CoATest::Bytes& bytes) {
        CDataStore msg(bytes.data.data(), static_cast<uint32_t>(bytes.data.size()));
        int32_t result = 0;
        CoA::Packets::Dispatch(static_cast<int32_t>(opcode), 0, &msg, result);
    }

    lua_State* Setup() {
        CoA::LuaReg::Reset();
        CoA::GlobalsB::Init();
        return CoATest::NewState(false);
    }

    // DungeonEncounterExtra.dbc: id, ?, ?, dungeon.
    bool ReadEncounters(const char* path, std::vector<uint8_t>& out) {
        if (strstr(path, "DungeonEncounterExtra") == nullptr) {
            return false;
        }

        out = CoATest::Wdbc({ { 40, 0, 0, 285 }, { 41, 0, 0, 286 } }, "");
        return true;
    }
}

// Before SMSG 0x768 the zones are nil; after it a table of the zones.
COA_TEST(GlobalsB_HonorableCombatZones) {
    lua_State* L = Setup();
    CHECK(CoATest::Call(L, "", "GetHonorableCombatZones")[0] == "nil");

    CoATest::RecordSignals();
    CoATest::Bytes b;
    b.U32(2).U32(1519).U32(12);
    Feed(0x768, b);
    CoATest::StopRecordingSignals();
    CHECK(CoATest::Signals().size() == 1 && CoATest::Signals()[0] == "HONORABLE_COMBAT_ZONES_PAYLOAD");
    CHECK(CoATest::Call(L, "", "GetHonorableCombatZones")[0] == "table");
}

// SMSG 0x66D: CURRENT_LFG_DUNGEON_ID_CHANGED only on a change; GetLFGBoss the first encounter of that dungeon.
COA_TEST(GlobalsB_LfgDungeonAndBoss) {
    CoA::Dbc::SetFileReader(ReadEncounters);
    lua_State* L = Setup();
    CHECK(CoATest::Call(L, "", "GetLFGBoss")[0] == "nil");

    CoATest::RecordSignals();
    CoATest::Bytes b;
    b.U32(286);
    Feed(0x66D, b);
    Feed(0x66D, b);
    CoATest::StopRecordingSignals();
    CHECK(CoATest::Signals().size() == 1 && CoATest::Signals()[0] == "CURRENT_LFG_DUNGEON_ID_CHANGED");
    CHECK(CoATest::Call(L, "", "GetLFGBoss")[0] == "41");
    CoA::Dbc::SetFileReader(nullptr);
}

// FUN_10005da0's static list.
COA_TEST(GlobalsB_ArenaBracketCutoffs) {
    lua_State* L = Setup();
    CHECK((CoATest::Call(L, "", "Ascension_GetArenaBracketCutoffs") == std::vector<std::string>{ "0", "1400", "1700", "2000", "2200", "2400" }));
}
