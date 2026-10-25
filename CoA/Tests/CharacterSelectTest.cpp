#include "CoA/Source/Modules/Config.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/Logger.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Tests/Support.h"
#include "CoA/Tests/TestMain.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <cstdio>

namespace {
    lua_State* Setup() {
        CoA::LuaReg::Reset();
        CoA::Packets::Clear();
        CoA::Config::Reset();
        CoATest::InitRealmModules();
        CoA::GlobalsJ::Init();
        CoA::CharacterList::Init();
        CoA::RealmMerge::Init();
        CoA::EventPackets::Init();
        return CoATest::NewState(true);
    }

    int32_t Dispatch(uint32_t opcode, CoATest::Bytes& bytes) {
        CDataStore msg(bytes.data.data(), static_cast<uint32_t>(bytes.data.size()));
        int32_t result = 0;
        CoA::Packets::Dispatch(static_cast<int32_t>(opcode), 0, &msg, result);
        return result;
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
            out.push_back(lua_isnil(L, i) ? "nil" : lua_isboolean(L, i) ? (lua_toboolean(L, i) ? "true" : "false") : lua_tostring(L, i));
        }

        lua_settop(L, base);
        return out;
    }

    // The CoA server's SMSG_CHARACTER_LIST_INFO: max active, total, active, inactive, then the entries.
    void Entry(CoATest::Bytes& b, uint32_t guid, uint8_t active, uint8_t online, uint8_t level, uint8_t race, uint8_t cls, uint8_t gender, uint32_t zone, const char* name) {
        b.U32(guid).U8(active).U8(online).U8(level).U8(race).U8(cls).U8(gender).U32(zone).Str(name);
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
}

COA_TEST(CharList_ParsesInfo) {
    lua_State* L = Setup();
    CoATest::Bytes b;
    b.U32(10).U32(2).U32(1).U32(1);
    Entry(b, 101, 1, 0, 53, 4, 31, 0, 1657, "Helix");
    Entry(b, 102, 0, 1, 12, 2, 1, 1, 14, "Second");
    CoATest::RecordSignals();
    Dispatch(0x75E, b);
    CoATest::StopRecordingSignals();

    CHECK(CoATest::Signals() == std::vector<std::string>{ "CHARACTER_LIST_UPDATED" });
    CHECK((CoATest::Call(L, "C_CharacterList", "GetCounts") == std::vector<std::string>{ "10", "2", "1", "1" }));
    CHECK((Run(L, "local e = C_CharacterList.GetEntries() local a, b = e[1], e[2] "
        "return #e, a.guid, a.name, a.active, a.online, a.level, a.race, a.class, a.gender, a.zoneId, b.name, b.online")
        == std::vector<std::string>{ "2", "101", "Helix", "true", "false", "53", "4", "31", "0", "1657", "Second", "true" }));
    CHECK(CoATest::Call(L, "C_CharacterList", "HasInactiveCharacters") == std::vector<std::string>{ "true" });
    lua_close(L);
}

COA_TEST(CharList_FirstFieldCappedAt0x80) {
    lua_State* L = Setup();
    CoATest::Bytes b;
    b.U32(0x100).U32(0).U32(0).U32(0);
    Dispatch(0x75E, b);
    CHECK((CoATest::Call(L, "C_CharacterList", "GetCounts") == std::vector<std::string>{ "128", "0", "0", "0" }));
    CHECK((Run(L, "return #C_CharacterList.GetEntries()") == std::vector<std::string>{ "0" }));
    CHECK(CoATest::Call(L, "C_CharacterList", "HasInactiveCharacters") == std::vector<std::string>{ "false" });
    lua_close(L);
}

COA_TEST(CharList_TruncatedStops) {
    lua_State* L = Setup();
    CoATest::Bytes b;
    b.U32(10).U32(3).U32(3).U32(0);
    Entry(b, 7, 1, 0, 1, 1, 1, 0, 1, "Only");
    Dispatch(0x75E, b);
    CHECK((Run(L, "local e = C_CharacterList.GetEntries() return #e, e[1].name") == std::vector<std::string>{ "1", "Only" }));
    lua_close(L);
}

COA_TEST(CharList_RequestsSendGuids) {
    lua_State* L = Setup();
    CoATest::RecordPackets();
    CHECK(CoATest::Call(L, "C_CharacterList", "RequestActivate", { "n:101" }) == std::vector<std::string>{ "true" });
    CHECK(CoATest::Call(L, "C_CharacterList", "Refresh") == std::vector<std::string>{ "true" });
    CHECK(CoATest::Call(L, "C_CharacterList", "RequestDeactivate", { "s:x" }).empty());
    CoATest::StopRecordingPackets();
    CoATest::Bytes guid;
    guid.U32(101);
    CHECK(CoATest::SentPackets().size() == 2);
    CHECK(CoATest::SentPackets().size() == 2 && CoATest::SentPackets()[0] == CoATest::Expected(0x72E, guid));
    CHECK(CoATest::SentPackets().size() == 2 && CoATest::SentPackets()[1] == CoATest::Expected(0x37, CoATest::Bytes()));
    lua_close(L);
}

COA_TEST(ActivateResult_IndexAndUnexpected) {
    CoATest::TempCwd cwd;
    CoA::Logger::Init();
    lua_State* L = Setup();
    CoATest::RecordSignals();
    CoATest::Bytes ok;
    ok.Str("ACTIVATE_CHARACTER_OK");
    Dispatch(0x75F, ok);
    CoATest::Bytes odd;
    odd.Str("SOMETHING_ELSE");
    Dispatch(0x760, odd);
    CoATest::StopRecordingSignals();
    CoA::Logger::Shutdown();
    CHECK((CoATest::Signals() == std::vector<std::string>{ "CHARACTER_ACTIVATE_RESULT:true,ACTIVATE_CHARACTER_OK",
        "CHARACTER_DEACTIVATE_RESULT:false,SOMETHING_ELSE" }));
    CHECK(ReadAll("Logs\\Debug.txt").find("Unexpected Value: SOMETHING_ELSE") != std::string::npos);
    lua_close(L);
}

COA_TEST(RealmMerge_TargetFromPacketAndConfig) {
    lua_State* L = Setup();
    CHECK(CoATest::Call(L, "C_RealmMerge", "IsMergeAvailable") == std::vector<std::string>{ "false" });

    CoATest::Bytes config;
    config.U32(0).U32(1).Name("CONFIG_REALM_MERGE_ENABLED").U8(1).U32(0).U32(0);
    config.U32(1).Name("CONFIG_REALM_MERGE_AVAILABLE_REALMS").U32(2).I32(13).I32(99).U32(0);
    CoATest::Deliver(CoA::Config::OnUpdateConfigs, 0x58D, config);
    CHECK(CoATest::Call(L, "C_RealmMerge", "IsMergeAvailable") == std::vector<std::string>{ "true" });
    CHECK(CoATest::Call(L, "C_RealmMerge", "NeedsSetTargetRealm") == std::vector<std::string>{ "true" });
    CHECK((Run(L, "local t = C_RealmMerge.GetAvailableOptions() return #t, t[1]") == std::vector<std::string>{ "1", "CoA" }));

    CoATest::Bytes target;
    target.U32(13);
    Dispatch(0x5F5, target);
    CHECK(CoATest::Call(L, "C_RealmMerge", "GetTargetRealm") == std::vector<std::string>{ "CoA" });
    CHECK(CoATest::Call(L, "C_RealmMerge", "NeedsSetTargetRealm") == std::vector<std::string>{ "false" });

    CoATest::RecordPackets();
    CHECK(CoATest::Call(L, "C_RealmMerge", "SetTargetRealm", { "s:Rexxar" }) == std::vector<std::string>{ "true" });
    CHECK(CoATest::Call(L, "C_RealmMerge", "SetTargetRealm", { "s:Nowhere" }) == std::vector<std::string>{ "false" });
    CoATest::StopRecordingPackets();
    CoATest::Bytes rexxar;
    rexxar.U32(27);
    CHECK(CoATest::SentPackets().size() == 1 && CoATest::SentPackets()[0] == CoATest::Expected(0x5F6, rexxar));

    CoATest::RecordSignals();
    CoATest::Bytes result;
    result.Name("DONE");
    Dispatch(0x5F7, result);
    CoATest::StopRecordingSignals();
    CHECK(CoATest::Signals() == std::vector<std::string>{ "REALM_MERGE_SET_TARGET_RESULT:DONE" });
    lua_close(L);
}

COA_TEST(SelectionData_MailGameModeSortOrder) {
    lua_State* L = Setup();
    CHECK(CoATest::Call(L, "", "GetCharacterSelectionMailData", { "n:1" }) == std::vector<std::string>{ "nil" });
    CHECK(CoATest::Call(L, "", "GetCharacterSelectionSortOrder") == std::vector<std::string>{ "nil" });

    CoATest::Bytes mail;
    mail.U32(1).U8(1).U8(0);
    Dispatch(0x770, mail);
    CoATest::Bytes mode;
    mode.U32(1).U32(3).U32(7).U32(469).U8(0).U8(2);
    Dispatch(0x771, mode);
    CoATest::Bytes order;
    order.Str("2 1");
    Dispatch(0x76F, order);

    CHECK((CoATest::Call(L, "", "GetCharacterSelectionMailData", { "n:1" }) == std::vector<std::string>{ "true", "false" }));
    CHECK((CoATest::Call(L, "", "GetCharacterSelectionGameModeData", { "n:1" }) == std::vector<std::string>{ "3", "7", "469", "false", "2" }));
    CHECK(CoATest::Call(L, "", "GetCharacterSelectionSortOrder") == std::vector<std::string>{ "2 1" });

    CoATest::RecordPackets();
    CoATest::Call(L, "", "SetCharacterSelectionSortOrder", { "s:1 2" });
    CoATest::StopRecordingPackets();
    CoATest::Bytes sent;
    sent.Str("1 2");
    CHECK(CoATest::SentPackets().size() == 1 && CoATest::SentPackets()[0] == CoATest::Expected(0x772, sent));
    lua_close(L);
}

COA_TEST(GlueReload_NativesBackAfterWorld) {
    Setup();
    lua_State* glue = CoATest::NewState(true);
    lua_State* world = CoATest::NewState(false);
    lua_State* again = CoATest::NewState(true);
    const char* const names[][2] = {
        { "C_Realm", "IsLive" }, { "C_Config", "GetIntConfig" }, { "C_CharacterList", "GetEntries" }, { "C_RealmMerge", "GetTargetRealm" },
        { "", "ReadCustomWTF" }, { "", "GetCharacterSelectionMailData" }, { "", "GetCharacterSelectionSortOrder" }, { "", "GetServerMaxLevel" },
    };

    for (lua_State* L : { glue, again }) {
        for (const auto& name : names) {
            std::vector<std::string> result = CoATest::Call(L, name[0], name[1]);
            CHECK(result.empty() || (result[0] != "<no function>" && result[0] != "<no namespace>"));
        }
    }

    lua_close(glue);
    lua_close(world);
    lua_close(again);
}
