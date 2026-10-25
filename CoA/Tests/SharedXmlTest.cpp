#include "CoA/Source/Modules/Json.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/CVars.h"
#include "CoA/Source/Runtime/ClientState.h"
#include "CoA/Source/Runtime/Lifecycle.h"
#include "CoA/Source/Runtime/Logger.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Tests/Support.h"
#include "CoA/Tests/TestMain.h"
#include "Engine/Source/ConsoleBase/ConsoleVar.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <cstdio>
#include <cstring>

namespace {
    lua_State* Setup() {
        CoA::LuaReg::Reset();
        CoA::Packets::Clear();
        CoA::Lifecycle::Reset();
        CoA::Json::Init();
        CoA::LoggerApi::Init();
        CoA::SmallApis::Init();
        return CoATest::NewState(true);
    }

    std::vector<std::string> Run(lua_State* L, const char* chunk) {
        std::vector<std::string> out;
        int base = lua_gettop(L);

        if (luaL_loadstring(L, chunk) != 0 || lua_pcall(L, 0, LUA_MULTRET, 0) != 0) {
            out.push_back(std::string("<error> ") + lua_tostring(L, -1));
            lua_settop(L, base);
            return out;
        }

        for (int i = base + 1; i <= lua_gettop(L); ++i) {
            out.push_back(lua_isnil(L, i) ? "nil" : lua_tostring(L, i));
        }

        lua_settop(L, base);
        return out;
    }

    std::string ReadAll(const char* path) {
        std::string text;
        FILE* f = fopen(path, "r");

        if (f) {
            char buffer[512];
            size_t n;

            while ((n = fread(buffer, 1, sizeof(buffer), f)) > 0) {
                text.append(buffer, n);
            }

            fclose(f);
        }

        return text;
    }

    int32_t Dispatch(uint32_t opcode, CoATest::Bytes& bytes) {
        CDataStore msg(bytes.data.data(), static_cast<uint32_t>(bytes.data.size()));
        int32_t result = 0;
        CoA::Packets::Dispatch(static_cast<int32_t>(opcode), 0, &msg, result);
        return result;
    }
}

COA_TEST(Json_DecodesNestedTable) {
    lua_State* L = Setup();
    CHECK((Run(L, "local t = DecodeJSON('{\"a\":[7,8],\"b\":\"x\",\"c\":true}') return t.a[1], t.a[2], t.b, tostring(t.c)")
        == std::vector<std::string>{ "7", "8", "x", "true" }));
    // Digit-only object keys become key + 1; floats go through float.
    CHECK((Run(L, "local t = DecodeJSON('{\"0\":\"zero\",\"f\":0.1}') return t[1], t.f")
        == std::vector<std::string>{ "zero", "0.10000000149012" }));
    lua_close(L);
}

COA_TEST(Json_InvalidOrEmptyReturnsNothing) {
    lua_State* L = Setup();
    CHECK(CoATest::Call(L, "", "DecodeJSON", { "s:{" }).empty());
    CHECK(CoATest::Call(L, "", "DecodeJSON", { "s:" }).empty());
    CHECK(CoATest::Call(L, "", "DecodeJSONContent", { "s:Anything" }).empty());
    lua_close(L);
}

COA_TEST(JsonCache_PayloadPacketsAndClear) {
    lua_State* L = Setup();
    CoATest::Bytes poi;
    poi.U32(5).Str("{\"p\":1}");
    Dispatch(0x77C, poi);
    CoATest::Bytes lfg;
    lfg.Str("lfg");
    Dispatch(0x77B, lfg);

    CHECK(CoATest::Call(L, "", "GetJsonCacheData", { "s:AREA_POI_PAYLOAD", "n:5" }) == std::vector<std::string>{ "{\"p\":1}" });
    CHECK(CoATest::Call(L, "", "GetJsonCacheData", { "s:OVERWRITE_LFG_DATA_PAYLOAD", "n:0" }) == std::vector<std::string>{ "lfg" });
    CHECK(CoATest::Call(L, "", "HasJsonCacheData", { "s:AREA_POI_PAYLOAD", "n:5" }) == std::vector<std::string>{ "true" });
    CoATest::Call(L, "", "ClearJsonCache", { "s:AREA_POI_PAYLOAD" });
    CHECK(CoATest::Call(L, "", "HasJsonCacheData", { "s:AREA_POI_PAYLOAD", "n:5" }) == std::vector<std::string>{ "false" });
    CHECK(CoATest::Call(L, "", "GetJsonCacheData", { "s:AREA_POI_PAYLOAD", "n:5" }) == std::vector<std::string>{ "" });
    CoATest::Call(L, "", "ClearJsonCache");
    CHECK(CoATest::Call(L, "", "HasJsonCacheData", { "s:OVERWRITE_LFG_DATA_PAYLOAD", "n:0" }) == std::vector<std::string>{ "false" });
    lua_close(L);
}

COA_TEST(JsonCache_InWorldSignalsCarrierEvent) {
    Setup();
    CoA::ClientState::SetInWorldProbe([] { return true; });
    CoATest::RecordSignals();
    CoATest::Bytes misc;
    misc.U32(9).Str("x");
    Dispatch(0x77D, misc);
    CoATest::StopRecordingSignals();
    CoA::ClientState::SetInWorldProbe(nullptr);
    CHECK(CoATest::Signals() == std::vector<std::string>{ "COMMENTATOR_SKIRMISH_QUEUE_REQUEST:ASCENSION_CUSTOM_JSON_MISC_PLAYER_DATA_PAYLOAD_UPDATE,9" });
}

COA_TEST(JsonCache_GlueScreenEmptiesBuildPayloads) {
    lua_State* L = Setup();
    CoA::Json::Set("BUILD_CREATOR_DATA_PAYLOAD", 1, "x");
    CoA::Json::Set("AREA_POI_PAYLOAD", 1, "kept");
    CHECK(CoATest::Call(L, "", "HasJsonCacheData", { "s:BUILD_CREATOR_DATA_PAYLOAD", "n:1" }) == std::vector<std::string>{ "true" });
    CoA::Lifecycle::RunGlueScreen();
    CHECK(CoATest::Call(L, "", "HasJsonCacheData", { "s:AREA_POI_PAYLOAD", "n:1" }) == std::vector<std::string>{ "true" });
    CHECK(CoATest::Call(L, "", "HasJsonCacheData", { "s:BUILD_CREATOR_DATA_PAYLOAD", "n:1" }) == std::vector<std::string>{ "false" });
    lua_close(L);
}

COA_TEST(Logger_TraceWritesCallerLine) {
    CoATest::TempCwd cwd;
    CoA::Logger::Init();
    lua_State* L = Setup();
    CHECK(Run(L, "C_Logger.Trace('hello %d', 5)").empty());
    CHECK(Run(L, "C_Logger.Error('')").empty());
    CoA::Logger::Shutdown();
    std::string trace = ReadAll("Logs\\Trace.txt");
    CHECK(trace.size() > 9 && trace.find("[[string \"C_Logger.Trace('hello %d', 5)\"]:1] hello 5\n") == 9);
    CHECK(ReadAll("Logs\\Error.txt").empty());
    lua_close(L);
}

COA_TEST(Logger_InfoNeedsLogInfoCVar) {
    CoATest::TempCwd cwd;
    CoA::Logger::Init();
    lua_State* L = Setup();
    Run(L, "C_Logger.Info('first')");
    CVar::Register("logInfo", nullptr, 1, "1", nullptr, 5, false, nullptr, false);
    Run(L, "C_Logger.Info('second')");
    CoA::Logger::Shutdown();
    std::string info = ReadAll("Logs\\Info.txt");
    CHECK(info.find("first") == std::string::npos);
    CHECK(info.find("second") != std::string::npos);
    lua_close(L);
}

COA_TEST(ReportMetric_SendsNameAndValue) {
    lua_State* L = Setup();
    CoATest::RecordPackets();
    CoATest::Call(L, "", "ReportMetric", { "s:fps", "s:60" });
    CoATest::Call(L, "", "ReportMetric", { "s:", "s:60" });
    CoATest::StopRecordingPackets();
    CoATest::Bytes payload;
    payload.Name("fps").Name("60");
    CHECK(CoATest::SentPackets().size() == 1);
    CHECK(CoATest::SentPackets().size() == 1 && CoATest::SentPackets()[0] == CoATest::Expected(0x55A, payload));
    lua_close(L);
}

COA_TEST(CVarApi_BitfieldAndSave) {
    lua_State* L = Setup();
    CVar::Register("coaTestBits", nullptr, 1, "", nullptr, 5, false, nullptr, false);
    CHECK(CoATest::Call(L, "C_CVar", "SetCVarBitfield", { "s:coaTestBits", "n:3", "b:1" }) == std::vector<std::string>{ "true" });
    CHECK(CoATest::Call(L, "C_CVar", "GetCVarBitfield", { "s:coaTestBits", "n:3" }) == std::vector<std::string>{ "true" });
    CHECK(CoATest::Call(L, "C_CVar", "GetCVarBitfield", { "s:coaTestBits", "n:2" }) == std::vector<std::string>{ "false" });
    CHECK(CoATest::Call(L, "C_CVar", "GetCVarBitfield", { "s:noSuchCVar", "n:2" }) == std::vector<std::string>{ "nil" });
    CoATest::Call(L, "C_CVar", "SetCVarSave", { "s:coaTestBits", "b:0" });
    CHECK((CVar::Lookup("coaTestBits")->m_flags & 0x80) != 0);
    CoATest::Call(L, "C_CVar", "SetCVarSave", { "s:coaTestBits", "b:1" });
    CHECK((CVar::Lookup("coaTestBits")->m_flags & 0x80) == 0);
    lua_close(L);
}

COA_TEST(PlaceInBank_SendsBelowTwenty) {
    lua_State* L = Setup();
    CoATest::RecordPackets();
    CoATest::Call(L, "C_EquipmentSet", "PlaceInBank", { "n:3" });
    CoATest::Call(L, "C_EquipmentSet", "PlaceInBank", { "n:20" });
    CoATest::StopRecordingPackets();
    CoATest::Bytes payload;
    payload.U32(2);
    CHECK(CoATest::SentPackets().size() == 1 && CoATest::SentPackets()[0] == CoATest::Expected(0x55E, payload));
    lua_close(L);
}

COA_TEST(GlueCVars_TheOriginalTwentyOne) {
    CoA::CVars::Reset();
    CoA::GlueCVars::Init();
    size_t count = 0;
    const CoA::CVarEntry* list = CoA::CVars::Glue(count);
    CHECK(count == 21);
    CHECK(count == 21 && strcmp(list[0].name, "useRaidVideoSettings") == 0);
    CHECK(count == 21 && strcmp(list[1].name, "farclip_raid") == 0 && strcmp(list[1].defaultValue, "507") == 0);
    CHECK(count == 21 && strcmp(list[12].name, "loadUnknownAddOns") == 0 && list[12].category == 5);
    CHECK(count == 21 && strcmp(list[14].name, "fogOverride") == 0 && list[14].setCommand);
    CHECK(count == 21 && strcmp(list[18].name, "logDebug") == 0 && strcmp(list[19].name, "logInfo") == 0);
    CHECK(count == 21 && strcmp(list[20].name, "cursorSizePreferred") == 0 && list[20].category == 1);

    for (size_t i = 0; i < count; ++i) {
        CHECK(list[i].flags == 1 && list[i].help == nullptr);
    }

    CoA::CVars::Reset();
}
