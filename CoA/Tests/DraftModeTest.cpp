#include "CoA/Source/Modules/DraftMode.h"
#include "CoA/Source/Modules/GameEvents.h"
#include "CoA/Source/Modules/GameMode.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Modules/RealmInfo.h"
#include "CoA/Source/Runtime/Lifecycle.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Tests/Support.h"
#include "CoA/Tests/TestMain.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <string>
#include <vector>

namespace {
    using V = std::vector<std::string>;

    bool s_player = true;
    uint32_t s_level = 80;
    uint32_t s_marks = 1000;
    int64_t s_now = 5000;

    lua_State* Setup() {
        CoA::LuaReg::Reset();
        CoA::Packets::Clear();
        CoA::Lifecycle::Reset();
        CoA::RealmInfo::Reset();
        CoA::GameEvents::Reset();
        CoA::GameMode::Reset();
        CoA::GameMode::Init();
        CoA::DraftMode::Reset();
        CoA::DraftMode::Init();
        CoA::ScrollOfFortune::Reset();
        CoA::ScrollOfFortune::Init();
        s_player = true;
        s_level = 80;
        s_marks = 1000;
        s_now = 5000;
        CoA::DraftMode::Deps& d = CoA::DraftMode::Dependencies();
        d.hasPlayer = [] { return s_player; };
        d.playerLevel = [] { return s_level; };
        d.itemCount = [](uint32_t entry) { return entry == 0x5B9D2 ? s_marks : 0u; };
        d.now = [] { return s_now; };
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

    std::string Run(lua_State* L, const char* code) {
        luaL_dostring(L, code);
        lua_getglobal(L, "r");
        std::string out = lua_isstring(L, -1) ? lua_tostring(L, -1) : (lua_isboolean(L, -1) ? (lua_toboolean(L, -1) ? "true" : "false") : "nil");
        lua_pop(L, 1);
        lua_pushnil(L);
        lua_setglobal(L, "r");
        return out;
    }

    struct Pick {
        uint32_t spell;
        uint8_t a;
        uint8_t b;
    };

    // 0x753 with the given header bytes and lists.
    CoATest::Bytes State(uint8_t kind, uint32_t selected, const uint8_t flags[6], const std::vector<Pick>& picks, const std::vector<Pick>& hof,
        const std::vector<uint32_t>& sacrifice, uint8_t mastery = 0) {
        CoATest::Bytes b;
        b.U32(7).U8(kind).U32(selected).U32(0).U8(mastery).U32(0);

        for (int i = 0; i < 6; ++i) {
            b.U8(flags[i]);
        }

        b.U8(static_cast<uint8_t>(picks.size()));

        for (const Pick& p : picks) {
            b.U32(p.spell).U8(p.a).U8(p.b);
        }

        b.U8(static_cast<uint8_t>(hof.size()));

        for (const Pick& p : hof) {
            b.U32(p.spell).U8(p.a).U8(p.b);
        }

        b.U8(static_cast<uint8_t>(sacrifice.size()));

        for (uint32_t s : sacrifice) {
            b.U32(s);
        }

        b.U32(1).U32(0xAAAAAAAA).U32(0xBBBBBBBB);
        return b;
    }

    // A Hand of Fate reward record (0x32 bytes).
    void HofReward(CoATest::Bytes& b, uint32_t tier, uint8_t flagA, uint8_t flagB, uint32_t level, uint32_t item, uint32_t count, uint32_t cost, uint32_t release) {
        b.U32(tier).U8(flagA).U8(flagB).U32(level).U32(item).U32(0).U32(0).U32(0).U32(count).U32(0).U32(0).U32(0).U32(cost).U32(release);
    }

    // A Scroll of Fortune reward record (0x31 bytes).
    void SofReward(CoATest::Bytes& b, uint32_t tier, uint8_t flag, uint32_t level, uint32_t item, uint32_t count, uint32_t cost, uint32_t release) {
        b.U32(tier).U8(flag).U32(level).U32(item).U32(item).U32(0).U32(0).U32(count).U32(count).U32(0).U32(0).U32(cost).U32(release);
    }
}

// 0x753: the lists, the copied flag bytes and the per-pick bytes; each changed part fires its event, then the state
// event; a repeated packet fires nothing.
COA_TEST(DraftMode_StateAndEvents) {
    lua_State* L = Setup();
    const uint8_t flags[6] = { 1, 0, 0, 0, 0, 0 };
    CHECK((Feed(0x753, State(1, 0, flags, { { 11, 1, 0 }, { 12, 0, 1 } }, { { 21, 0, 0 } }, {})) ==
        V{ "ASCENSION_DRAFTMODE_DRAFT_PICK_CHANGED", "ASCENSION_DRAFTMODE_HAND_OF_FATE_CHANGED", "ASCENSION_DRAFTMODE_DRAFT_PICK_METADATA_CHANGED",
            "ASCENSION_DRAFTMODE_HAND_OF_FATE_METADATA_CHANGED", "ASCENSION_DRAFT_STATE_UPDATED" }));
    CHECK(Feed(0x753, State(1, 0, flags, { { 11, 1, 0 }, { 12, 0, 1 } }, { { 21, 0, 0 } }, {})).empty());
    CHECK(CoATest::Call(L, "", "GetDraftModePickCount") == V{ "2" });
    CHECK(CoATest::Call(L, "", "GetDraftModePickSpellAtIndex", { "n:1" }) == V{ "12" });
    CHECK(CoATest::Call(L, "", "GetDraftModePickSpellAtIndex", { "n:5" }) == V{ "0" });
    CHECK(CoATest::Call(L, "", "GetHandOfFatePickCount") == V{ "1" });
    CHECK(CoATest::Call(L, "", "GetHandOfFateSacrificePickCount") == V{ "0" });
    CHECK(CoATest::Call(L, "", "GetHandOfFatePickSpellAtIndex", { "n:0" }) == V{ "21" });
    CHECK(CoATest::Call(L, "", "IsSkillCardDraft", { "n:0" }) == V{ "true" });
    CHECK(CoATest::Call(L, "", "IsSkillCardDraft", { "n:1" }) == V{ "false" });
    CHECK(CoATest::Call(L, "", "IsLuckySkillCardDraft", { "n:1" }) == V{ "true" });
    CHECK(CoATest::Call(L, "", "IsLuckySkillCardDraft", { "n:259" }) == V{ "false" });   // byte 3
    CHECK(CoATest::Call(L, "", "IsCardSwapPick") == V{ "true" });
    CHECK(CoATest::Call(L, "", "IsCardSwap") == V{ "true" });
    CHECK(CoATest::Call(L, "", "IsMasteryDraft") == V{ "false" });

    // Draft mode off: no picks are reported.
    CHECK(CoATest::Call(L, "", "HasDraftModePick") == V{ "false" });
    Feed(0x90B, CoATest::Bytes().U32(8));
    CHECK(CoATest::Call(L, "", "HasDraftModePick") == V{ "true" });
    CHECK(CoATest::Call(L, "", "HasHandOfFatePick") == V{ "true" });
    CHECK(CoATest::Call(L, "", "HasHandOfFateSacrificePick") == V{ "false" });

    // Sacrifice kind (2): the sacrifice list counts, the pick list no longer does.
    const uint8_t none[6] = {};
    CHECK((Feed(0x753, State(2, 0, none, { { 11, 1, 0 }, { 12, 0, 1 } }, { { 21, 0, 0 } }, { 0, 31 })) ==
        V{ "ASCENSION_DRAFTMODE_HAND_OF_FATE_CHANGED", "ASCENSION_DRAFTMODE_HAND_OF_FATE_SACRIFICE_CHANGED",
            "ASCENSION_DRAFTMODE_HAND_OF_FATE_METADATA_CHANGED", "ASCENSION_DRAFT_STATE_UPDATED" }));
    CHECK(CoATest::Call(L, "", "HasHandOfFateSacrificePick") == V{ "true" });
    CHECK(CoATest::Call(L, "", "HasHandOfFatePick") == V{ "false" });
    CHECK(CoATest::Call(L, "", "GetHandOfFatePickCount") == V{ "0" });
    CHECK(CoATest::Call(L, "", "GetHandOfFateSacrificePickCount") == V{ "2" });
    CHECK(CoATest::Call(L, "", "GetHandOfFateSacrificePickSpellAtIndex", { "n:1" }) == V{ "31" });
    CHECK(CoATest::Call(L, "", "GetHandOfFatePickSpellAtIndex", { "n:0" }) == V{ "0" });
    Feed(0x90B, CoATest::Bytes().U32(0));
    CHECK(CoATest::Call(L, "", "HasHandOfFateSacrificePick") == V{ "false" });
    Feed(0x90B, CoATest::Bytes().U32(8));

    // Only +0x49 changes: just the state event.
    const uint8_t last[6] = { 0, 0, 0, 0, 0, 1 };
    CHECK(Feed(0x753, State(2, 0, last, { { 11, 1, 0 }, { 12, 0, 1 } }, { { 21, 0, 0 } }, { 0, 31 })) == V{ "ASCENSION_DRAFT_STATE_UPDATED" });

    // +0x45 -> card swap sacrifice, +0x46 -> card swap picked.
    const uint8_t swap[6] = { 0, 1, 0, 0, 0, 0 };
    Feed(0x753, State(2, 0, swap, {}, {}, {}));
    CHECK(CoATest::Call(L, "", "IsCardSwapSacrifice") == V{ "true" });
    CHECK(CoATest::Call(L, "", "IsCardSwapPicked") == V{ "false" });

    // The glue screen clears the state.
    CoA::Lifecycle::RunGlueScreen();
    CHECK(CoATest::Call(L, "", "GetDraftModePickCount") == V{ "0" });
}

// The selected Hand of Fate spell: +0x68 / +0x69 take the matching pick's a / b bytes when either is set.
COA_TEST(DraftMode_SelectedSpell) {
    lua_State* L = Setup();
    const uint8_t picked[6] = { 0, 0, 1, 1, 0, 0 };   // +0x46 card swap picked, +0x47 skill card picked
    CHECK((Feed(0x753, State(1, 22, picked, {}, { { 21, 0, 0 }, { 22, 0, 1 } }, {}, 1)) ==
        V{ "ASCENSION_DRAFTMODE_HAND_OF_FATE_CHANGED", "ASCENSION_DRAFTMODE_HAND_OF_FATE_SELECTED_CHANGED", "ASCENSION_DRAFTMODE_DRAFT_PICK_METADATA_CHANGED",
            "ASCENSION_DRAFTMODE_HAND_OF_FATE_METADATA_CHANGED", "ASCENSION_DRAFTMODE_HAND_OF_FATE_SELECTED_METADATA_CHANGED", "ASCENSION_DRAFT_STATE_UPDATED" }));
    CHECK(CoATest::Call(L, "", "GetSelectedHandOfFateSpell") == V{ "22" });
    CHECK(CoATest::Call(L, "", "IsSkillCardPicked") == V{ "false" });
    CHECK(CoATest::Call(L, "", "IsLuckySkillCardPicked") == V{ "true" });
    CHECK(CoATest::Call(L, "", "IsCardSwapPicked") == V{ "true" });
    CHECK(CoATest::Call(L, "", "IsMasteryDraft") == V{ "true" });
    CHECK(CoATest::Call(L, "", "IsLuckySkillCardPick", { "n:1" }) == V{ "true" });

    // Not among the picks: the bytes stay as sent.
    CoA::DraftMode::Reset();
    Feed(0x753, State(1, 99, picked, {}, { { 21, 0, 0 }, { 22, 0, 1 } }, {}));
    CHECK(CoATest::Call(L, "", "IsSkillCardPicked") == V{ "true" });
    CHECK(CoATest::Call(L, "", "IsLuckySkillCardPicked") == V{ "false" });

    // Neither +0x68 nor +0x69 set: the selected pick's bytes are not taken.
    const uint8_t clear[6] = {};
    Feed(0x753, State(1, 22, clear, {}, { { 21, 0, 0 }, { 22, 1, 1 } }, {}));
    CHECK(CoATest::Call(L, "", "IsSkillCardPicked") == V{ "false" });
    CHECK(CoATest::Call(L, "", "IsLuckySkillCardPicked") == V{ "false" });

    CoATest::RecordPackets();
    CoATest::Call(L, "", "SendHandOfFateSpellSelection", { "n:21" });
    CoATest::Call(L, "", "SendHandOfFateSacrificeSpellSelection", { "n:31" });
    CoATest::Call(L, "", "CancelHandOfFateSelection");
    CoATest::StopRecordingPackets();
    CHECK(CoATest::SentPackets().size() == 3);
    CHECK(CoATest::SentPackets()[0] == CoATest::Expected(0x750, CoATest::Bytes().U32(21)));
    CHECK(CoATest::SentPackets()[1] == CoATest::Expected(0x751, CoATest::Bytes().U32(31)));
    CHECK(CoATest::SentPackets()[2] == CoATest::Expected(0x752, CoATest::Bytes()));
}

// 0x6AA inflates the rewards; claims check tier, count, Marks, level and release in that order; CMSG 0x6B3 and the
// 0x6B4 result update the claimed count; the glue screen keeps the rewards.
COA_TEST(DraftMode_HandOfFateRewards) {
    lua_State* L = Setup();
    CoATest::Bytes plain;
    plain.U32(4);
    HofReward(plain, 0, 0, 0, 10, 500, 2, 100, 1000);
    HofReward(plain, 0, 0, 0, 20, 500, 3, 200, 9000);
    HofReward(plain, 0, 1, 0, 30, 600, 1, 50, 0);
    HofReward(plain, 0, 1, 0, 90, 600, 1, 50, 0);
    Feed(0x6AA, CoATest::Compressed(plain));
    CHECK(Run(L, "local t = C_DraftRewards.GetRewards(1, 2, false) r = t[500]") == "5");
    CHECK(Run(L, "r = C_DraftRewards.GetMarkOfAscensionCost(1, 2, false)") == "300");

    CHECK(Run(L, "local ok, why = C_DraftRewards.CanClaimHandOfFateRewards(1, 1, false) r = tostring(ok) .. tostring(why)") ==
        "falseCOLLECT_HAND_OF_FATE_REWARDS_SPECIALIZATION_OUT_OF_RANGE");
    Feed(0x6B7, CoATest::Bytes().U32(1).U32(0).U32(0).U32(0).U32(0).U32(0));
    CHECK(Run(L, "local ok, why = C_DraftRewards.CanClaimHandOfFateRewards(1, 101, false) r = tostring(why)") == "COLLECT_HAND_OF_FATE_REWARDS_COUNT_TOO_HIGH");
    CHECK(Run(L, "local ok, why = C_DraftRewards.CanClaimHandOfFateRewards(1, 3, false) r = tostring(why)") == "COLLECT_HAND_OF_FATE_REWARDS_INVALID_COUNT");
    s_marks = 299;
    CHECK(Run(L, "local ok, why = C_DraftRewards.CanClaimHandOfFateRewards(1, 2, false) r = tostring(why)") ==
        "COLLECT_HAND_OF_FATE_REWARDS_NOT_ENOUGH_MARKS_OF_ASCENSION");
    s_marks = 1000;
    s_level = 15;
    CHECK(Run(L, "local ok, why = C_DraftRewards.CanClaimHandOfFateRewards(1, 2, false) r = tostring(why)") == "COLLECT_HAND_OF_FATE_REWARDS_TOO_LOW_LEVEL");
    s_player = false;
    CHECK(Run(L, "local ok, why = C_DraftRewards.CanClaimHandOfFateRewards(1, 2, false) r = tostring(why)") == "COLLECT_HAND_OF_FATE_REWARDS_NOT_RELEASED");
    s_player = true;
    s_level = 80;
    CHECK(Run(L, "local ok, why = C_DraftRewards.CanClaimHandOfFateRewards(1, 1, false) r = tostring(ok) .. tostring(why)") == "truenil");
    CHECK(Run(L, "r = C_DraftRewards.GetNumClaimableHandOfFateRewards(1, false)") == "1");
    CHECK(Run(L, "r = C_DraftRewards.GetNumClaimableHandOfFateRewards(1, true)") == "1");   // level 90 is above 80

    CoATest::RecordPackets();
    CHECK(Run(L, "r = C_DraftRewards.ClaimHandOfFateRewards(1, 2, false)") == "false");
    CHECK(Run(L, "r = C_DraftRewards.ClaimHandOfFateRewards(1, 1, false)") == "true");
    CoATest::StopRecordingPackets();
    CHECK(CoATest::SentPackets().size() == 1);
    CHECK(CoATest::SentPackets()[0] == CoATest::Expected(0x6B3, CoATest::Bytes().U32(0).U32(1).U8(0)));
    CHECK(Feed(0x6B4, CoATest::Bytes().Str("COLLECT_HAND_OF_FATE_REWARDS_OK")) == V{ "COLLECT_HAND_OF_FATE_REWARDS_RESULT:COLLECT_HAND_OF_FATE_REWARDS_OK" });
    CHECK(Run(L, "r = C_DraftRewards.GetNumClaimedHandOfFateRewards(1, false)") == "1");
    CHECK(Run(L, "r = C_DraftRewards.GetNumClaimedHandOfFateRewards(1, true)") == "0");
    CHECK(Run(L, "r = C_DraftRewards.GetMarkOfAscensionCost(1, 1, false)") == "200");
    Feed(0x6B4, CoATest::Bytes().Str("COLLECT_HAND_OF_FATE_REWARDS_UNKNOWN"));
    CHECK(Run(L, "r = C_DraftRewards.GetNumClaimedHandOfFateRewards(1, false)") == "1");

    // Release info over the (0,0) rewards: one released, two total, 4000 s to the next.
    CHECK(Run(L, "local a, b, c = C_DraftRewards.GetHandOfFateRewardsReleaseInfo(1) r = a .. ',' .. b .. ',' .. c") == "1,2,4000");
    CHECK(Run(L, "local a, b, c = C_DraftRewards.GetHandOfFateRewardsReleaseInfo(2) r = tostring(a) .. tostring(c)") == "nilnil");

    // Leveling info: the (1,0) rewards up to the server max level (60 on a default realm).
    CHECK(Run(L, "local t = C_DraftRewards.GetLevelingInfo(1) r = #t .. ',' .. t[1].Level .. ',' .. t[1].Rewards[600] .. ',' .. tostring(t[1].IsClaimed)") ==
        "1,30,1,false");

    CoA::Lifecycle::RunGlueScreen();
    CHECK(Run(L, "r = C_DraftRewards.GetNumClaimedHandOfFateRewards(1, false)") == "1");
    CHECK(Run(L, "r = C_DraftRewards.GetMarkOfAscensionCost(1, 1, false)") == "200");

    // A stream that does not inflate leaves the lists empty.
    CoATest::Bytes bad;
    bad.U32(60).U32(1).U32(2);
    Feed(0x6AA, bad);
    CHECK(Run(L, "r = C_DraftRewards.GetMarkOfAscensionCost(1, 1, false)") == "0");
}

// C_WildcardRewards: 0x6AB always fires its event, 0x6B8 replaces claimed and limits, the claim path and limits table,
// CMSG 0x6B5 and the 0x6B6 result, and the glue reset.
COA_TEST(ScrollOfFortune_Rewards) {
    lua_State* L = Setup();
    CoATest::Bytes plain;
    plain.U32(3);
    SofReward(plain, 1, 0, 10, 700, 2, 40, 1000);
    SofReward(plain, 1, 0, 10, 700, 1, 60, 1000);
    SofReward(plain, 1, 1, 50, 800, 3, 0, 0);
    CHECK(Feed(0x6AB, CoATest::Compressed(plain)) == V{ "SCROLL_OF_FORTUNE_REWARDS_LIST_UPDATED" });
    CHECK(Feed(0x6AB, CoATest::Bytes().U8(1)) == V{ "SCROLL_OF_FORTUNE_REWARDS_LIST_UPDATED" });
    CHECK(Run(L, "r = C_WildcardRewards.GetMarkOfAscensionCost(2, 2, false)") == "0");
    Feed(0x6AB, CoATest::Compressed(plain));
    CHECK(Run(L, "r = C_WildcardRewards.GetMarkOfAscensionCost(2, 2, false)") == "100");
    CHECK(Run(L, "local t = C_WildcardRewards.GetRewards(2, 2, false) r = t[700]") == "6");

    CHECK(Run(L, "local ok, why = C_WildcardRewards.CanClaimScrollOfFortuneRewards(2, 1, false) r = tostring(why)") ==
        "COLLECT_SCROLL_OF_FORTUNE_REWARDS_SPECIALIZATION_OUT_OF_RANGE");
    CoATest::Bytes lists;
    lists.U32(2).U32(0).U32(0).U32(0).U32(0).U32(0).U32(0);
    lists.U32(1).U32(1).U32(5).U32(6).U32(7).U32(1).U32(2).U32(3);
    CHECK(Feed(0x6B8, lists) == V{ "SCROLL_OF_FORTUNE_LIMITS_UPDATED" });
    CHECK(Run(L, "local t = C_WildcardRewards.GetScrollOfFortuneLimits(2) r = t.ability.used .. t.ability.cap .. t.talent.used .. t.talent.cap .. "
        "t.generic.used .. t.generic.cap") == "152637");
    CHECK(Run(L, "r = C_WildcardRewards.GetScrollOfFortuneLimits(1)") == "nil");
    CHECK(Run(L, "r = C_WildcardRewards.GetNumClaimableScrollOfFortuneRewards(2, false)") == "2");
    s_now = 999;
    CHECK(Run(L, "local ok, why = C_WildcardRewards.CanClaimScrollOfFortuneRewards(2, 1, false) r = tostring(why)") ==
        "COLLECT_SCROLL_OF_FORTUNE_REWARDS_NOT_RELEASED");
    CHECK(Run(L, "local a, b, c = C_WildcardRewards.GetScrollOfFortuneRewardsReleaseInfo(2) r = a .. ',' .. b .. ',' .. c") == "0,2,1");
    s_now = 1000;   // a release at exactly now counts as released
    CHECK(Run(L, "local a, b, c = C_WildcardRewards.GetScrollOfFortuneRewardsReleaseInfo(2) r = a .. ',' .. b .. ',' .. tostring(c)") == "2,2,nil");
    s_now = 5000;
    s_level = 49;
    CHECK(Run(L, "local ok, why = C_WildcardRewards.CanClaimScrollOfFortuneRewards(2, 1, true) r = tostring(why)") ==
        "COLLECT_SCROLL_OF_FORTUNE_REWARDS_TOO_LOW_LEVEL");
    s_level = 80;

    CoATest::RecordPackets();
    CHECK(Run(L, "r = C_WildcardRewards.ClaimScrollOfFortuneRewards(2, 1, true)") == "true");
    CoATest::StopRecordingPackets();
    CHECK(CoATest::SentPackets().size() == 1);
    CHECK(CoATest::SentPackets()[0] == CoATest::Expected(0x6B5, CoATest::Bytes().U32(1).U32(1).U8(1)));
    CHECK(Feed(0x6B6, CoATest::Bytes().Str("COLLECT_SCROLL_OF_FORTUNE_REWARDS_OK")) ==
        V{ "COLLECT_SCROLL_OF_FORTUNE_REWARDS_RESULT:COLLECT_SCROLL_OF_FORTUNE_REWARDS_OK" });
    CHECK(Run(L, "r = C_WildcardRewards.GetNumClaimedScrollOfFortuneRewards(2, true)") == "1");
    CHECK(Run(L, "local t = C_WildcardRewards.GetLevelingInfo(2) r = #t .. t[1].Level .. t[1].Rewards[800] .. tostring(t[1].IsClaimed)") == "1503true");

    CoA::Lifecycle::RunGlueScreen();
    CHECK(Run(L, "r = C_WildcardRewards.GetNumClaimedScrollOfFortuneRewards(2, true)") == "0");
    CHECK(Run(L, "r = C_WildcardRewards.GetMarkOfAscensionCost(2, 1, false)") == "0");
}
