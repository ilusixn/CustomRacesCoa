#include "CoA/Source/Modules/Config.h"
#include "CoA/Source/Runtime/ClientState.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Tests/Support.h"
#include "CoA/Tests/TestMain.h"
#include "Engine/Source/lua-5.1/src/lua.h"

namespace {
    lua_State* Setup() {
        CoA::LuaReg::Reset();
        CoA::Config::Reset();
        CoATest::InitRealmModules();
        return CoATest::NewState(true);
    }
}

COA_TEST(Config_ReplacesScalarMaps) {
    lua_State* L = Setup();
    CoATest::Bytes first;
    first.U32(2).Name("A").I32(5).Name("B").I32(-3);
    first.U32(1).Name("FLAG").U8(1);
    first.U32(1).Name("F").F32(0.5f);
    first.U32(1).Name("R").F32(2.0f);
    CoATest::Deliver(CoA::Config::OnUpdateConfigs, 0x58D, first);

    CHECK(CoATest::Call(L, "C_Config", "GetIntConfig", { "s:B" }) == std::vector<std::string>{ "-3" });
    CHECK(CoATest::Call(L, "C_Config", "GetBoolConfig", { "s:FLAG" }) == std::vector<std::string>{ "true" });
    CHECK(CoATest::Call(L, "C_Config", "GetFloatConfig", { "s:F" }) == std::vector<std::string>{ "0.5" });
    CHECK(CoATest::Call(L, "C_Config", "GetRateConfig", { "s:R" }) == std::vector<std::string>{ "2" });

    CoATest::Bytes second;
    second.U32(1).Name("A").I32(6).U32(0).U32(0).U32(0);
    CoATest::Deliver(CoA::Config::OnUpdateConfigs, 0x58D, second);

    CHECK(CoATest::Call(L, "C_Config", "GetIntConfig", { "s:A" }) == std::vector<std::string>{ "6" });
    CHECK(CoATest::Call(L, "C_Config", "GetIntConfig", { "s:B" }) == std::vector<std::string>{ "nil" });
    CHECK(CoATest::Call(L, "C_Config", "GetBoolConfig", { "s:FLAG" }) == std::vector<std::string>{ "nil" });
    // A missing name argument returns nothing.
    CHECK(CoATest::Call(L, "C_Config", "GetIntConfig").empty());
    lua_close(L);
}

COA_TEST(Config_VectorsOnlyWithEightBytesLeft) {
    lua_State* L = Setup();
    CoATest::Bytes withVectors;
    withVectors.U32(0).U32(0).U32(0).U32(0);
    withVectors.U32(1).Name("IV").U32(2).I32(4).I32(9);
    withVectors.U32(0);
    CoATest::Deliver(CoA::Config::OnUpdateConfigs, 0x58D, withVectors);
    const std::vector<int32_t>* iv = CoA::Config::IntVector("IV");
    CHECK(iv && iv->size() == 2 && (*iv)[1] == 9);
    CHECK(CoATest::Call(L, "C_Config", "GetIntVectorConfig", { "s:IV" }) == std::vector<std::string>{ "table" });

    // Fewer than eight bytes after the scalars: vectors are not read, the old ones stay (merge semantics).
    CoATest::Bytes without;
    without.U32(0).U32(0).U32(0).U32(0).U32(0);
    CoATest::Deliver(CoA::Config::OnUpdateConfigs, 0x58D, without);
    CHECK(CoA::Config::IntVector("IV") != nullptr);
    lua_close(L);
}

COA_TEST(Config_TruncatedCountStops) {
    Setup();
    CoATest::Bytes b;
    b.U32(5).Name("ONLY").I32(1);
    CoATest::Deliver(CoA::Config::OnUpdateConfigs, 0x58D, b);
    const int32_t* only = CoA::Config::Int("ONLY");
    CHECK(only && *only == 1);
    CHECK(CoA::Config::Int("") == nullptr);
}

COA_TEST(Config_InWorldChangeFiresEvent) {
    Setup();
    CoA::ClientState::SetInWorldProbe([] { return true; });
    CoATest::Bytes first;
    first.U32(1).Name("X").I32(1).U32(0).U32(0).U32(0);
    CoATest::RecordSignals();
    CoATest::Deliver(CoA::Config::OnUpdateConfigs, 0x58D, first);
    CoATest::Bytes same;
    same.U32(1).Name("X").I32(1).U32(0).U32(0).U32(0);
    CoATest::Deliver(CoA::Config::OnUpdateConfigs, 0x58D, same);
    CoATest::StopRecordingSignals();
    CoA::ClientState::SetInWorldProbe(nullptr);
    CHECK(CoATest::Signals() == std::vector<std::string>{ "INT_CONFIG_UPDATED:X,0,1" });
}
