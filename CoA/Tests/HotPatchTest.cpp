#include "CoA/Source/Modules/HotPatch.h"
#include "CoA/Source/Runtime/ClientHooks.h"
#include "CoA/Source/Runtime/DbcPatch.h"
#include "CoA/Source/Runtime/Lifecycle.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Tests/Support.h"
#include "CoA/Tests/TestMain.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {
    lua_State* s_L = nullptr;
    uint32_t s_stored = 0;

    lua_State* State() { return s_L; }

    void Execute(const char* source, const char* name) {
        if (luaL_loadbuffer(s_L, source, strlen(source), name) == 0) {
            lua_pcall(s_L, 0, 0, 0);
        } else {
            lua_pop(s_L, 1);
        }
    }

    // The world Lua the module needs: a frame stub for kWorldLua, the realm and addon queries.
    const char* const kWorld = R"lua(
CreateFrame = function() return { RegisterEvent = function() end, SetScript = function() end } end
SlashCmdList = {}
GetRealmName = function() return "Freepick" end
loaded = {}
IsAddOnLoaded = function(name) return loaded[name] or false end
)lua";

    void Start() {
        CoA::LuaReg::Reset();
        CoA::Packets::Clear();
        CoA::ClientHooks::afterEnterWorld.Clear();
        CoA::Lifecycle::Reset();
        CoA::HotPatch::Reset();
        CoA::HotPatch::Init();
        s_L = CoATest::NewState(false);
        luaL_dostring(s_L, kWorld);
        CoA::HotPatch::Deps& d = CoA::HotPatch::Dependencies();
        d.state = State;
        d.execute = Execute;
        CoATest::RecordPackets();
    }

    void Stop() {
        CoATest::StopRecordingPackets();
        CoA::HotPatch::Dependencies() = CoA::HotPatch::Deps {};
        CoA::DbcPatch::SetValidator(0x5BC, nullptr);
        lua_close(s_L);
        s_L = nullptr;
    }

    CoATest::Bytes PatchPacket(const char* id, const char* target, const std::string& source) {
        CoATest::Bytes b;
        b.Name(id).Name(target);
        b.U32(static_cast<uint32_t>(source.size()));
        return b.Raw(source.data(), static_cast<uint32_t>(source.size()));
    }

    void Send(const CoATest::Bytes& bytes) {
        CDataStore msg(const_cast<uint8_t*>(bytes.data.data()), static_cast<uint32_t>(bytes.data.size()));
        int32_t result = 0;
        CoA::Packets::Dispatch(0x8F0, 0, &msg, result);
    }

    std::string Global(const char* name) {
        lua_getfield(s_L, LUA_GLOBALSINDEX, name);
        std::string out = lua_isstring(s_L, -1) ? lua_tostring(s_L, -1) : (lua_isnil(s_L, -1) ? "nil" : "?");
        lua_pop(s_L, 1);
        return out;
    }

    // The outcome byte of each CMSG 0x8F2 sent, in order.
    std::vector<uint8_t> Outcomes() {
        std::vector<uint8_t> out;

        for (const std::vector<uint8_t>& p : CoATest::SentPackets()) {
            uint32_t opcode, idLength;
            memcpy(&opcode, p.data(), 4);

            if (opcode == 0x8F2) {
                memcpy(&idLength, p.data() + 4, 4);
                out.push_back(p[8 + idLength]);
            }
        }

        return out;
    }

    void Stored(const uint8_t*) { ++s_stored; }

    CoATest::Bytes RequirementType(uint32_t id, const char* formatter) {
        CoATest::Bytes b;
        std::vector<uint32_t> row(9, 0);
        row[0] = id;
        b.Raw(row.data(), 0x24).Name("CHALLENGE_REQUIREMENT_TYPE_TEST");

        for (int f = 2; f <= 5; ++f) {
            b.Name("");
        }

        return b.Name(formatter).Name("").Name("");
    }
}

COA_TEST(HotPatch_ShippedFormattersPass) {
    const char* shipped[] = {
        "return C_Item.GetColoredItemName(value)",
        "return C_Quest.GetTitleByID(value)",
        "return GetColoredReputationName(value) .. \" |cffffffff(\" .. value .. \")|r\"",
        "return GetCreatureNameByID(value)",
        "return GetFactionInfoByID(value)",
        "return GetMoneyString(value) or value",
        "return SecondsToTime(value, false, true, 2)",
        "",
    };

    for (const char* text : shipped) {
        std::string reason;
        CHECK(CoA::HotPatch::FormatterAllowed(text, reason));
    }
}

COA_TEST(HotPatch_SmuggledFormattersRefused) {
    const char* smuggled[] = {
        "end, (function() print(1) end)(), function(value)",
        "end, (function()\nAscensionClientLuaTokens = {}\nend)(), function(value)",
        "return value end, (function() X = 1 end)(), function(value) return value",
        "return SendChatMessage(\"hi\", \"SAY\")",
        "return _G[\"SendChatMessage\"](\"hi\")",
        "return getfenv(0).RunScript(\"x\")",
        "return RunScript(\"x\")",
        "return loadstring(\"x\")()",
        "return value; X = 1",
        "return (function() return 1 end)()",
        "return value -- comment",
        "return [[long string]]",
        "return C_Quest.GetTitleByID(value):upper()",
        "return {value}",
        "return value == 1 and X",
        "X = value",
        "return SecondsToTime(value",
        "return \"unterminated",
        "return",
        "value",
    };

    for (const char* text : smuggled) {
        std::string reason;
        CHECK(!CoA::HotPatch::FormatterAllowed(text, reason));
        CHECK(!reason.empty());
    }
}

COA_TEST(HotPatch_RefusedRequirementTypeIsNotStored) {
    Start();
    s_stored = 0;
    CoA::DbcPatch::RegisterSized(0x5BC, "DBFilesClient\\HotPatchTestRequirementTypes.dbc", 0x24, 0x1FEull, Stored);
    CoATest::Bytes ok = RequirementType(1, "return GetMoneyString(value) or value");
    CoATest::Bytes bad = RequirementType(2, "end, (function() X = 1 end)(), function(value)");
    int32_t result = 0;
    CDataStore okMsg(ok.data.data(), static_cast<uint32_t>(ok.data.size()));
    CoA::Packets::Dispatch(0x5BC, 0, &okMsg, result);
    CHECK(s_stored == 1);
    CDataStore badMsg(bad.data.data(), static_cast<uint32_t>(bad.data.size()));
    CoA::Packets::Dispatch(0x5BC, 0, &badMsg, result);
    CHECK(s_stored == 1);
    Stop();
}

COA_TEST(HotPatch_Guards) {
    std::string reason;
    CHECK(CoA::HotPatch::Guard("a", "X = 1", 0, reason));
    CHECK(!CoA::HotPatch::Guard("", "X = 1", 0, reason));
    CHECK(!CoA::HotPatch::Guard("a", "", 0, reason));
    CHECK(!CoA::HotPatch::Guard("a", std::string(32 * 1024 + 1, ' '), 0, reason));
    CHECK(!CoA::HotPatch::Guard("a", "X = 1", 128 * 1024, reason));
    CHECK(!CoA::HotPatch::Guard("a", "\x1bLua", 0, reason));
    CHECK(!CoA::HotPatch::Guard("a", "loadstring('x')()", 0, reason));
    CHECK(!CoA::HotPatch::Guard("a", "string.dump(f)", 0, reason));
}

COA_TEST(HotPatch_RunsOnceAndReportsOutcome) {
    CoATest::TempCwd cwd;
    Start();
    CoA::HotPatch::OnEnteredWorld();
    Send(PatchPacket("p1", "", "HOT = (HOT or 0) + 1 return 'applied'"));
    CHECK(Global("HOT") == "1");
    Send(PatchPacket("p1", "", "HOT = (HOT or 0) + 1 return 'applied'"));
    CHECK(Global("HOT") == "1");
    Send(PatchPacket("p2", "", "return 'skipped: not needed'"));
    Send(PatchPacket("p3", "", "error('boom')"));
    Send(PatchPacket("p4", "", "return loadstring('x')"));
    const std::vector<uint8_t> want = {
        CoA::HotPatch::Applied, CoA::HotPatch::Duplicate, CoA::HotPatch::Skipped, CoA::HotPatch::Error,
        CoA::HotPatch::RefusedGuard,
    };
    CHECK(Outcomes() == want);
    Stop();
}

COA_TEST(HotPatch_CapabilityAfterWorldEntry) {
    CoATest::TempCwd cwd;
    Start();
    CoA::HotPatch::OnEnteredWorld();
    CHECK(CoATest::SentPackets().size() == 1);
    CHECK(CoATest::SentPackets()[0] == CoATest::Expected(0x8F1, CoATest::Bytes().U32(1).U8(2)));
    Stop();
}

COA_TEST(HotPatch_NamedTargetWaitsForTheAddon) {
    CoATest::TempCwd cwd;
    Start();
    CoA::HotPatch::OnEnteredWorld();
    Send(PatchPacket("p1", "SomeAddon", "HOT = 1"));
    CHECK(Global("HOT") == "nil");
    CoATest::Call(s_L, "C_HotPatch", "OnAddonLoaded", { "s:OtherAddon" });
    CHECK(Global("HOT") == "nil");
    CoATest::Call(s_L, "C_HotPatch", "OnAddonLoaded", { "s:SomeAddon" });
    CHECK(Global("HOT") == "1");
    const std::vector<uint8_t> want = { CoA::HotPatch::Deferred, CoA::HotPatch::Ran };
    CHECK(Outcomes() == want);
    Stop();
}

COA_TEST(HotPatch_ReplayedInANewLuaState) {
    CoATest::TempCwd cwd;
    Start();
    CoA::HotPatch::OnEnteredWorld();
    Send(PatchPacket("p1", "", "HOT = 1"));
    CHECK(Global("HOT") == "1");
    // A /reload: a new Lua state, then the world entry again.
    lua_close(s_L);
    s_L = CoATest::NewState(false);
    luaL_dostring(s_L, kWorld);
    CoA::HotPatch::OnEnteredWorld();
    CHECK(Global("HOT") == "1");
    // A second world entry in the same state runs nothing again.
    luaL_dostring(s_L, "HOT = nil");
    CoA::HotPatch::OnEnteredWorld();
    CHECK(Global("HOT") == "nil");
    // The glue screen ends the connection's patches.
    CoA::HotPatch::OnGlueScreen();
    lua_close(s_L);
    s_L = CoATest::NewState(false);
    luaL_dostring(s_L, kWorld);
    CoA::HotPatch::OnEnteredWorld();
    CHECK(Global("HOT") == "nil");
    Stop();
}

COA_TEST(HotPatch_OffRefusesAndSavesTheMode) {
    CoATest::TempCwd cwd;
    Start();
    CoA::HotPatch::OnEnteredWorld();
    CoATest::Call(s_L, "C_HotPatch", "SetMode", { "s:off" });
    CHECK(CoATest::Call(s_L, "C_HotPatch", "GetMode")[0] == "off");
    Send(PatchPacket("p1", "", "HOT = 1"));
    CHECK(Global("HOT") == "nil");
    const std::vector<uint8_t> want = { CoA::HotPatch::RefusedByPlayer };
    CHECK(Outcomes() == want);
    FILE* f = fopen("WTF\\HotPatch.txt", "r");
    CHECK(f != nullptr);

    if (f) {
        char line[32] = {};
        fgets(line, sizeof(line), f);
        fclose(f);
        CHECK(std::string(line) == "mode=0\n");
    }

    Stop();
}

COA_TEST(HotPatch_AskUsesTheRealmAnswer) {
    CoATest::TempCwd cwd;
    FILE* f = fopen("WTF\\HotPatch.txt", "w");
    fputs("mode=1\nrealm=Freepick=0\n", f);
    fclose(f);
    Start();
    CoA::HotPatch::OnEnteredWorld();
    Send(PatchPacket("p1", "", "HOT = 1"));
    CHECK(Global("HOT") == "nil");
    CoATest::Call(s_L, "C_HotPatch", "SetRealmConsent", { "b:1" });
    Send(PatchPacket("p2", "", "HOT = 2"));
    CHECK(Global("HOT") == "2");
    const std::vector<uint8_t> want = { CoA::HotPatch::RefusedByPlayer, CoA::HotPatch::Ran };
    CHECK(Outcomes() == want);
    Stop();
}
