#include "CoA/Source/Modules/Account.h"
#include "CoA/Source/Modules/CanToggle.h"
#include "CoA/Source/Modules/Config.h"
#include "CoA/Source/Modules/GameEvents.h"
#include "CoA/Source/Modules/GameMode.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/Lifecycle.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Tests/Support.h"
#include "CoA/Tests/TestMain.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <set>
#include <string>
#include <vector>

namespace {
    using V = std::vector<std::string>;

    bool s_player = true;
    uint32_t s_level = 1;
    uint8_t s_class = 10;
    std::set<uint32_t> s_auras;

    lua_State* Setup() {
        CoA::LuaReg::Reset();
        CoA::Packets::Clear();
        CoA::Lifecycle::Reset();
        CoA::Config::Reset();
        CoA::GameEvents::Reset();
        CoA::Account::Reset();
        CoA::GameMode::Reset();
        CoA::GameMode::Init();
        CoA::CanToggle::Init();
        CoA::BuildDraft::Reset();
        CoA::BuildDraft::Init();
        CoA::GameMode::Dependencies().activeSpecIndex = [] { return 0; };
        s_player = true;
        s_level = 1;
        s_class = 10;
        s_auras.clear();
        CoA::CanToggle::Deps& deps = CoA::CanToggle::Dependencies();
        deps.hasPlayer = [] { return s_player; };
        deps.playerLevel = [] { return s_level; };
        deps.playerClass = [] { return s_class; };
        deps.hasAura = [](uint32_t spell) { return s_auras.count(spell) != 0; };
        return CoATest::NewState(false);
    }

    V Feed(uint32_t opcode, CoATest::Bytes bytes) {
        CoATest::RecordSignals();
        CDataStore msg(bytes.data.data(), static_cast<uint32_t>(bytes.data.size()));
        int32_t result = 0;
        CoA::Packets::Dispatch(static_cast<int32_t>(opcode), 0, &msg, result);
        CoATest::StopRecordingSignals();
        return CoATest::Signals();
    }

    void Bools(const std::vector<std::pair<const char*, bool>>& bools) {
        CoATest::Bytes b;
        b.U32(0).U32(static_cast<uint32_t>(bools.size()));

        for (const auto& kv : bools) {
            b.Name(kv.first).U8(kv.second ? 1 : 0);
        }

        b.U32(0).U32(0);
        CoATest::Deliver(CoA::Config::OnUpdateConfigs, 0x58D, b);
    }

    void Mode(uint32_t mode) {
        Feed(0x90B, CoATest::Bytes().U32(mode));
    }

    void GmLevel(uint32_t level) {
        CoATest::Bytes b;
        b.U32(0).U32(level).U8(0).U8(0).U32(0);
        CoATest::Deliver(CoA::Account::OnAccountInfo, 0x9BB, b);
    }

    void Event(uint16_t id, bool active) {
        CoATest::Bytes b;
        uint8_t raw[0x34] = {};
        memcpy(raw, &id, 2);
        raw[0x13] = active ? 1 : 0;
        b.Raw(raw, sizeof(raw));
        CoATest::Deliver(CoA::GameEvents::OnGameEventInfo, 0x9BD, b);
    }

    // "ok|reason,reason"
    std::string Can(lua_State* L, const char* fn, bool enable) {
        std::string code = std::string("local ok, t = ") + fn + "(" + (enable ? "true" : "false") +
            ") r = tostring(ok) .. '|' if t then for i = 1, #t do r = r .. (i > 1 and ',' or '') .. t[i] end else r = r .. 'nil' end";
        luaL_dostring(L, code.c_str());
        lua_getglobal(L, "r");
        std::string out = lua_isstring(L, -1) ? lua_tostring(L, -1) : "?";
        lua_pop(L, 1);
        return out;
    }
}

// Standard modes: DISABLED without the config (the Survivalist typo is the name looked up), each interaction bit then
// OUTSIDE_INTERACTION, and ALREADY_ACTIVE / ALREADY_INACTIVE against the mode's own bit.
COA_TEST(CanToggle_StandardModes) {
    lua_State* L = Setup();
    CHECK(Can(L, "CanToggleIronmanMode", true) == "false|TOGGLE_IRONMAN_DISABLED");
    CHECK(Can(L, "CanToggleIronmanMode", false) == "false|TOGGLE_IRONMAN_ALREADY_INACTIVE");
    Bools({ { "CONFIG_IRONMAN_ENABLE", true }, { "CONFIG_SURVIALIST_ENABLE", true }, { "CONFIG_SURVIVALIST_ENABLE", false },
        { "CONFIG_CRUSADER_ENABLED", true } });
    CHECK(Can(L, "CanToggleIronmanMode", true) == "true|nil");
    CHECK(Can(L, "CanToggleSurvivalistMode", true) == "true|nil");
    CHECK(Can(L, "CanToggleCrusaderMode", true) == "true|nil");
    CHECK(Can(L, "CanToggleWildCardMode", true) == "false|TOGGLE_WILDCARD_DISABLED");
    Mode((1u << 1) | (1u << 15) | (1u << 21));
    CHECK(Can(L, "CanToggleIronmanMode", true) == "false|TOGGLE_IRONMAN_AUCTION_HOUSE,TOGGLE_IRONMAN_TRADE,TOGGLE_IRONMAN_OUTSIDE_INTERACTION,TOGGLE_IRONMAN_ALREADY_ACTIVE");
    CHECK(Can(L, "CanToggleIronmanMode", false) == "true|nil");
    Mode(1u << 21);
    CHECK(Can(L, "CanToggleIronmanMode", true) == "false|TOGGLE_IRONMAN_TRADE,TOGGLE_IRONMAN_OUTSIDE_INTERACTION");
    Mode((1u << 12) | (1u << 8) | (1u << 5) | (1u << 6) | (1u << 2));
    CHECK(Can(L, "CanToggleCrusaderMode", false) == "true|nil");
    CHECK(Can(L, "CanToggleNightmareMode", true) == "false|TOGGLE_NIGHTMARE_DISABLED,TOGGLE_NIGHTMARE_ALREADY_ACTIVE");
    CHECK(Can(L, "CanToggleResoluteMode", false) == "true|nil");
    CHECK(Can(L, "CanToggleWildCardMode", false) == "true|nil");
    CHECK(Can(L, "CanToggleSurvivalistMode", false) == "true|nil");
}

// Felforged: disabling needs GM level 2; enabling needs the config and game event 0x148, no other mode and none of the
// four auras.
COA_TEST(CanToggle_Felforged) {
    lua_State* L = Setup();
    CHECK(Can(L, "CanToggleFelforgedMode", false) == "false|TOGGLE_FELFORGED_CANNOT_BE_DISABLED,TOGGLE_FELFORGED_ALREADY_INACTIVE");
    GmLevel(2);
    CHECK(Can(L, "CanToggleFelforgedMode", false) == "false|TOGGLE_FELFORGED_ALREADY_INACTIVE");
    Bools({ { "CONFIG_FELFORGED_ENABLE", true } });
    CHECK(Can(L, "CanToggleFelforgedMode", true) == "false|TOGGLE_FELFORGED_DISABLED");
    Event(0x148, true);
    CHECK(Can(L, "CanToggleFelforgedMode", true) == "true|nil");
    s_auras.insert(0x9788BF);
    Mode((1u << 16) | 0x20);
    CHECK(Can(L, "CanToggleFelforgedMode", true) ==
        "false|TOGGLE_FELFORGED_LEVEL_UP,TOGGLE_FELFORGED_OUTSIDE_INTERACTION,TOGGLE_FELFORGED_OTHER_GAME_MODE_ACTIVE,TOGGLE_FELFORGED_BLOCKED_AURAS");
    s_player = false;
    Mode(0x80);
    CHECK(Can(L, "CanToggleFelforgedMode", true) == "false|TOGGLE_FELFORGED_ALREADY_ACTIVE");
}

// Build draft: config, level <= 9, the active spec's draft flag, Hero class 10 and no draft mode (bit 3).
COA_TEST(CanToggle_BuildDraftAndDraft) {
    lua_State* L = Setup();
    s_level = 10;
    s_class = 1;
    Mode(8);
    CHECK(Can(L, "CanToggleBuildDraftMode", true) ==
        "false|TOGGLE_BUILDDRAFT_DISABLED,TOGGLE_BUILDDRAFT_LEVEL_TOO_HIGH,TOGGLE_BUILDDRAFT_NOT_HERO,TOGGLE_BUILDDRAFT_DRAFT_ACTIVE");
    CHECK(Can(L, "CanToggleBuildDraftMode", false) == "false|TOGGLE_BUILDDRAFT_ALREADY_INACTIVE");
    Bools({ { "CONFIG_BUILD_DRAFT_ENABLED", true }, { "CONFIG_DRAFT_ENABLE", false } });
    Mode(0);
    s_level = 9;
    s_class = 10;
    CHECK(Can(L, "CanToggleBuildDraftMode", true) == "true|nil");
    Feed(0x5CC, CoATest::Bytes().U32(0).U32(1));
    CHECK(Can(L, "CanToggleBuildDraftMode", true) == "false|TOGGLE_BUILDDRAFT_ALREADY_ACTIVE");
    CHECK(Can(L, "CanToggleBuildDraftMode", false) == "true|nil");
    s_player = false;
    CHECK(Can(L, "CanToggleBuildDraftMode", true) == "false|TOGGLE_BUILDDRAFT_LEVEL_TOO_HIGH,TOGGLE_BUILDDRAFT_ALREADY_ACTIVE,TOGGLE_BUILDDRAFT_NOT_HERO");

    CHECK(Can(L, "CanToggleDraftMode", true) == "false|nil");
    CHECK(Can(L, "CanToggleDraftMode", false) == "true|nil");
    CoA::Config::Reset();
    CHECK(Can(L, "CanToggleDraftMode", true) == "true|nil");
}

// The draftable list (0x636 event only on a change, 0x637 grows the list), drafted build, completed builds, the role
// prompt and its result, SelectRole's CMSG 0x63A, and the glue reset.
COA_TEST(BuildDraft_PacketsAndNatives) {
    lua_State* L = Setup();
    CHECK(Feed(0x636, CoATest::Bytes().U32(2).Str("A").Str("B")) == V{ "DRAFTABLE_BUILDS_UPDATE" });
    CHECK(Feed(0x636, CoATest::Bytes().U32(2).Str("A").Str("B")).empty());
    CHECK(Feed(0x637, CoATest::Bytes().U32(3).Str("D")) == V{ "DRAFTABLE_BUILD_UPDATE:4" });
    CHECK(Feed(0x637, CoATest::Bytes().U32(3).Str("D")).empty());
    CHECK(CoATest::Call(L, "C_BuildDraft", "GetNumDraftableBuilds") == V{ "4" });
    CHECK(CoATest::Call(L, "C_BuildDraft", "GetDraftableBuild", { "n:2" }) == V{ "B" });
    CHECK(CoATest::Call(L, "C_BuildDraft", "GetDraftableBuild", { "n:3" }) == V{ "" });
    CHECK(CoATest::Call(L, "C_BuildDraft", "GetDraftableBuild", { "n:9" }) == V{ "" });
    CHECK(CoATest::Call(L, "C_BuildDraft", "GetDraftableBuild", { "n:0" }).empty());
    CHECK(CoATest::Call(L, "C_BuildDraft", "IsDraftableBuild", { "s:D" }) == V{ "true" });
    CHECK(Feed(0x638, CoATest::Bytes().Str("A")) == V{ "DRAFTED_BUILD_UPDATE" });
    CHECK(CoATest::Call(L, "C_BuildDraft", "GetDraftedBuild") == V{ "A" });
    CHECK(CoATest::Call(L, "C_BuildDraft", "IsDraftedBuild", { "s:A" }) == V{ "true" });
    CHECK(Feed(0x63C, CoATest::Bytes().U32(1).Str("X")).empty());
    CHECK(Feed(0x63D, CoATest::Bytes().Str("Y")) == V{ "COMPLETED_BUILD_UPDATE:Y" });
    CHECK(CoATest::Call(L, "C_BuildDraft", "IsCompletedBuild", { "s:X" }) == V{ "true" });
    CHECK(CoATest::Call(L, "C_BuildDraft", "IsCompletedBuild", { "s:Y" }) == V{ "true" });
    CHECK(Feed(0x639, CoATest::Bytes()) == V{ "BUILD_DRAFT_ROLE_PROMPT" });
    CHECK(CoATest::Call(L, "C_BuildDraft", "IsAwaitingRolePrompt") == V{ "true" });
    CHECK(Feed(0x63B, CoATest::Bytes().Str("BUILD_DRAFT_SELECT_ROLE_BAD")) == V{ "BUILD_DRAFT_SELECT_ROLE_RESULT:BUILD_DRAFT_SELECT_ROLE_BAD" });
    CHECK(CoATest::Call(L, "C_BuildDraft", "IsAwaitingRolePrompt") == V{ "true" });
    Feed(0x63B, CoATest::Bytes().Str("BUILD_DRAFT_SELECT_ROLE_OK"));
    CHECK(CoATest::Call(L, "C_BuildDraft", "IsAwaitingRolePrompt") == V{ "false" });
    CoATest::RecordPackets();
    CHECK(CoATest::Call(L, "C_BuildDraft", "SelectRole", { "s:TANK" }) == V{ "true" });
    CoATest::StopRecordingPackets();
    CHECK(CoATest::SentPackets().size() == 1);
    CHECK(CoATest::SentPackets()[0] == CoATest::Expected(0x63A, CoATest::Bytes().Str("TANK")));
    Feed(0x639, CoATest::Bytes());
    CoA::Lifecycle::RunGlueScreen();
    CHECK(CoATest::Call(L, "C_BuildDraft", "GetNumDraftableBuilds") == V{ "0" });
    CHECK(CoATest::Call(L, "C_BuildDraft", "GetDraftedBuild") == V{ "" });
    CHECK(CoATest::Call(L, "C_BuildDraft", "IsCompletedBuild", { "s:X" }) == V{ "false" });
    CHECK(CoATest::Call(L, "C_BuildDraft", "IsAwaitingRolePrompt") == V{ "false" });
}
