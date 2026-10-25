#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/Dbc.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Tests/Support.h"
#include "CoA/Tests/TestMain.h"
#include <cstring>
#include <string>
#include <vector>

namespace {
    lua_State* Setup() {
        CoA::LuaReg::Reset();
        CoA::Bindings::Init();
        return CoATest::NewState(false);
    }

    uint32_t Bits(float f) {
        uint32_t u;
        memcpy(&u, &f, 4);
        return u;
    }

    // UICameras.dbc: id, SD fov x y z, HD fov x y z, flags.
    bool ReadCameras(const char* path, std::vector<uint8_t>& out) {
        if (strstr(path, "UICameras.dbc") == nullptr) {
            return false;
        }

        out = CoATest::Wdbc({ { 7, Bits(0.5f), Bits(1.0f), Bits(2.0f), Bits(3.0f), Bits(0.75f), Bits(4.0f), Bits(5.0f), Bits(6.0f), 9 } }, "");
        return true;
    }
}

// The resurrection choices and the callboard request.
COA_TEST(Bindings_WorldRequests) {
    lua_State* L = Setup();
    CoATest::RecordPackets();
    CoATest::Call(L, "", "RessurectInClosestTown");
    CoATest::Call(L, "", "RessurectInClosestCapital");
    CoATest::Call(L, "", "RessurectAtCheckpoint");
    CHECK(CoATest::Call(L, "C_Callboard", "RequestView")[0] == "true");
    CoATest::StopRecordingPackets();

    const auto& sent = CoATest::SentPackets();
    CHECK(sent.size() == 4);
    CHECK(sent[0] == CoATest::Expected(0x524, CoATest::Bytes().U8(1)));
    CHECK(sent[1] == CoATest::Expected(0x524, CoATest::Bytes().U8(2)));
    CHECK(sent[2] == CoATest::Expected(0x524, CoATest::Bytes().U8(3)));
    CHECK(sent[3] == CoATest::Expected(0x55F, CoATest::Bytes()));
}

// The original's unimplemented bindings return nothing.
COA_TEST(Bindings_ReturnNothing) {
    lua_State* L = Setup();
    CHECK(CoATest::Call(L, "C_RecoveryService", "SwapFactionChangePotion").empty());
    CHECK(CoATest::Call(L, "C_RecoveryService", "SwapRaceChangePotion").empty());
    CHECK(CoATest::Call(L, "C_CharacterAdvancement", "ClearRecentlyLearnedEntries").empty());
    CHECK(CoATest::Call(L, "C_CollectorCache", "GetCollectorCacheRarityRatesInfo", { "n:3" }).empty());
}

// FUN_100c9000: (fov, x, y, z, flags), the SD set without Data\Patch-Q.mpq; nil x 5 without a row.
COA_TEST(Bindings_CameraInfo) {
    CoA::Dbc::SetFileReader(ReadCameras);
    lua_State* L = Setup();
    CHECK((CoATest::Call(L, "C_UICamera", "GetCameraInfo", { "n:7" }) == std::vector<std::string>{ "0.5", "1", "2", "3", "9" }));
    CHECK(CoATest::Call(L, "C_UICamera", "GetCameraInfo", { "n:8" }).size() == 5);
    CHECK(CoATest::Call(L, "C_UICamera", "GetCameraInfo", { "n:8" })[0] == "nil");
    CoA::Dbc::SetFileReader(nullptr);
}
