// C_Wildcard, the mode manager (WildcardRolls.cpp): the per-spec counters, repurchasing, the roll / reset gates, the
// reveal records and deferred spellbook adds, the roll-ready poll, the packets, the resets, and the CA natives that ask
// it through the CA Deps.
#include "CoA/Tests/WildcardFixture.h"
#include "CoA/Source/Runtime/Lifecycle.h"
#include "CoA/Source/Runtime/LuaReg.h"

using namespace WCFixture;

namespace {
    // SMSG 0x6EE: n, then n records {a, b, rolls, ability rolls, talent rolls}.
    CoATest::Bytes MiscList(const std::vector<std::vector<uint32_t>>& records, uint32_t n) {
        CoATest::Bytes b;
        b.U32(n);

        for (const auto& r : records) {
            for (uint32_t v : r) {
                b.U32(v);
            }
        }

        return b;
    }

    V Counts(lua_State* L) {
        return R(L, "return C_Wildcard.GetNumRepurchasableRolls(), C_Wildcard.GetNumRepurchasableAbilityRolls(), "
                    "C_Wildcard.GetNumRepurchasableTalentRolls()");
    }

    CoATest::Bytes Learned(uint32_t entry, uint32_t ranks, const char* stop) {
        return CoATest::Bytes().U32(entry).U32(ranks).Str(stop);
    }

    // Repurchasing possible: max level, the feature on, 5 / 6 / 7 rolls for spec 0.
    lua_State* Repurchasable() {
        lua_State* L = Start({}, true, true, 60);
        CHECK(MaxLevel() == 60);
        Configs({}, { { "CONFIG_WILDCARD_ROLL_REPURCHASING_ENABLED", true } });
        Feed(0x6EE, MiscList({ { 1, 2, 5, 6, 7 } }, 1));
        return L;
    }
}

// SMSG 0x6EE fills spec i from record i (at most 20); 0x6EF rewrites the active spec and fires the three CHANGED
// events with (old, new) when a count moved.
COA_TEST(Wildcard_MiscListAndUpdate) {
    lua_State* L = Start();
    CHECK((Feed(0x6EE, MiscList({ { 1, 2, 3, 4, 5 }, { 9, 9, 9, 9, 9 } }, 2)).empty()));
    CHECK((Counts(L) == V{ "3", "4", "5" }));
    CHECK(CoA::WildcardRolls::SpecCounter(0) == 1);
    CHECK(CoA::WildcardRolls::SpecCounter(1) == 2);
    CHECK((Feed(0x6EF, CoATest::Bytes().U32(11).U32(12).U32(3).U32(14).U32(15)) ==
        V{ "WILDCARD_REPURCHASABLE_ABILITY_ROLLS_CHANGED:4,14", "WILDCARD_REPURCHASABLE_TALENT_ROLLS_CHANGED:5,15" }));
    CHECK((Counts(L) == V{ "3", "14", "15" }));
    CHECK(CoA::WildcardRolls::SpecCounter(0) == 11);
    CHECK(CoA::WildcardRolls::SpecCounter(1) == 12);
    CHECK((Feed(0x6EF, CoATest::Bytes().U32(11).U32(12).U32(8).U32(14).U32(15)) == V{ "WILDCARD_REPURCHASABLE_ROLLS_CHANGED:3,8" }));
    CHECK((Feed(0x6EF, CoATest::Bytes().U32(0).U32(0).U32(8).U32(14).U32(15)).empty()));
    // Spec 1 kept record 1.
    Feed(0x725, CoATest::Bytes().U32(1).U32(2));
    CHECK((Counts(L) == V{ "9", "9", "9" }));
    Stop(L);
}

// The count is capped at 20 records; a record is read only while its 20 bytes are left (7a).
COA_TEST(Wildcard_MiscListLimits) {
    lua_State* L = Start();
    std::vector<std::vector<uint32_t>> records;

    for (uint32_t i = 0; i < 21; ++i) {
        records.push_back({ i, i, 100 + i, 0, 0 });
    }

    Feed(0x6EE, MiscList(records, 0xFFFFFFFF));
    Feed(0x725, CoATest::Bytes().U32(19).U32(20));
    CHECK((R(L, "return C_Wildcard.GetNumRepurchasableRolls()") == V{ "119" }));
    Feed(0x725, CoATest::Bytes().U32(2).U32(20));
    CHECK((R(L, "return C_Wildcard.GetNumRepurchasableRolls()") == V{ "102" }));
    // Three records announced, two and a half sent: the third is not applied.
    CoATest::Bytes b = MiscList({ { 0, 0, 7, 0, 0 }, { 0, 0, 8, 0, 0 } }, 3);
    b.U32(0).U32(0).U32(9);
    Feed(0x6EE, b);
    CHECK((R(L, "return C_Wildcard.GetNumRepurchasableRolls()") == V{ "102" }));
    Feed(0x725, CoATest::Bytes().U32(1).U32(20));
    CHECK((R(L, "return C_Wildcard.GetNumRepurchasableRolls()") == V{ "8" }));
    Stop(L);
}

// Reproduced original behaviour: FUN_1016f580 indexes the arrays unchecked, so with no spec yet (-1) each array's
// reader sees the dword before it -- the previous array's spec 19.
COA_TEST(Wildcard_NoSpecReadsTheDwordBefore) {
    lua_State* L = Start({}, true, false);
    CHECK(CoA::CA::ActiveSpecIndex() == -1);
    std::vector<std::vector<uint32_t>> records(20, std::vector<uint32_t>(5, 0));
    records[19] = { 41, 42, 43, 44, 45 };
    Feed(0x6EE, MiscList(records, 20));
    CHECK((Counts(L) == V{ "42", "43", "44" }));
    CHECK(CoA::WildcardRolls::SpecCounter(1) == 41);
    CHECK(CoA::WildcardRolls::SpecCounter(0) == 0);   // +0x04, outside the arrays
    // 0x6EF writes there too: rolls[-1] is b[19].
    CHECK((Feed(0x6EF, CoATest::Bytes().U32(1).U32(2).U32(3).U32(4).U32(5)) ==
        V{ "WILDCARD_REPURCHASABLE_ROLLS_CHANGED:42,3", "WILDCARD_REPURCHASABLE_ABILITY_ROLLS_CHANGED:43,4",
            "WILDCARD_REPURCHASABLE_TALENT_ROLLS_CHANGED:44,5" }));
    CHECK(CoA::WildcardRolls::SpecCounter(1) == 2);   // b[-1] is a[19]
    CHECK(CoA::WildcardRolls::SpecCounter(0) == 1);   // a[-1] is +0x04
    Stop(L);
}

// FUN_10a39e70's codes in order: NOT_WILDCARD, NO_COUNT, NOT_MAX_LEVEL, NOT_ENABLED, TOO_LOW, TOO_HIGH, NOT_ENOUGH,
// then NO_TOKENS (Marks of Ascension) or NO_MONEY.
COA_TEST(Wildcard_CanRepurchaseCodes) {
    lua_State* L = Start({}, false, true, 60);
    Feed(0x6EE, MiscList({ { 1, 2, 5, 6, 0 } }, 1));
    CHECK((R(L, "return C_Wildcard.CanRepurchaseRolls(1, true)") == V{ "false", "REPURCHASE_WILDCARD_ROLL_NOT_WILDCARD" }));
    Mode(0x40);
    CHECK((R(L, "return C_Wildcard.CanRepurchaseTalentRolls(1, true)") == V{ "false", "REPURCHASE_WILDCARD_ROLL_NO_COUNT" }));
    s_player = false;
    CHECK((R(L, "return C_Wildcard.CanRepurchaseRolls(1, true)") == V{ "false", "REPURCHASE_WILDCARD_ROLL_NOT_MAX_LEVEL" }));
    s_player = true;
    s_level = 59;
    CHECK((R(L, "return C_Wildcard.CanRepurchaseRolls(1, true)") == V{ "false", "REPURCHASE_WILDCARD_ROLL_NOT_MAX_LEVEL" }));
    s_level = 60;
    CHECK((R(L, "return C_Wildcard.CanRepurchaseRolls(1, true)") == V{ "false", "REPURCHASE_WILDCARD_ROLL_NOT_ENABLED" }));
    Configs({}, { { "CONFIG_WILDCARD_ROLL_REPURCHASING_ENABLED", true } });
    CHECK((R(L, "return C_Wildcard.CanRepurchaseRolls(0, true)") == V{ "false", "REPURCHASE_WILDCARD_ROLL_TOO_LOW_COUNT" }));
    CHECK((R(L, "return C_Wildcard.CanRepurchaseRolls(10738, true)") == V{ "false", "REPURCHASE_WILDCARD_ROLL_TOO_HIGH_COUNT" }));
    CHECK((R(L, "return C_Wildcard.CanRepurchaseRolls(6, true)") == V{ "false", "REPURCHASE_WILDCARD_ROLL_NOT_ENOUGH_COUNT" }));
    CHECK((R(L, "return C_Wildcard.CanRepurchaseRolls(2, true)") == V{ "false", "REPURCHASE_WILDCARD_ROLL_NO_MONEY" }));
    s_money = 400000;
    CHECK((R(L, "return C_Wildcard.CanRepurchaseRolls(2, true)") == V{ "true", "nil" }));
    CHECK((R(L, "return C_Wildcard.CanRepurchaseRolls(3, true)") == V{ "false", "REPURCHASE_WILDCARD_ROLL_NO_MONEY" }));
    CHECK((R(L, "return C_Wildcard.CanRepurchaseRolls(2, false)") == V{ "false", "REPURCHASE_WILDCARD_ROLL_NO_TOKENS" }));
    s_marks = 100;
    CHECK((R(L, "return C_Wildcard.CanRepurchaseRolls(2, false)") == V{ "true", "nil" }));
    CHECK((R(L, "return C_Wildcard.CanRepurchaseAbilityRolls(2, false)") == V{ "true", "nil" }));
    CHECK((R(L, "return C_Wildcard.CanRepurchaseAbilityRolls(3, false)") == V{ "false", "REPURCHASE_WILDCARD_ROLL_NO_TOKENS" }));
    // Without a player the gold price is NO_MONEY; the marks are counted as none.
    s_player = false;
    CHECK((R(L, "return C_Wildcard.CanRepurchaseRolls(2, true)") == V{ "false", "REPURCHASE_WILDCARD_ROLL_NOT_MAX_LEVEL" }));
    s_player = true;
    // A bad argument list returns nothing.
    CHECK((R(L, "return C_Wildcard.CanRepurchaseRolls('x', true)").empty()));
    Stop(L);
}

// The talent kind: its own limit (0x37EC) and prices (150000 copper / 33 marks).
COA_TEST(Wildcard_TalentPricesAndLimits) {
    lua_State* L = Repurchasable();
    Feed(0x6EE, MiscList({ { 1, 2, 5, 6, 20000 } }, 1));
    CHECK((R(L, "return C_Wildcard.CanRepurchaseTalentRolls(14317, true)") == V{ "false", "REPURCHASE_WILDCARD_ROLL_TOO_HIGH_COUNT" }));
    CHECK((R(L, "return C_Wildcard.CanRepurchaseTalentRolls(14316, true)") == V{ "false", "REPURCHASE_WILDCARD_ROLL_NO_MONEY" }));
    CHECK((R(L, "return C_Wildcard.CanRepurchaseRolls(10737, true)") == V{ "false", "REPURCHASE_WILDCARD_ROLL_NOT_ENOUGH_COUNT" }));
    s_money = 300000;
    CHECK((R(L, "return C_Wildcard.CanRepurchaseTalentRolls(2, true)") == V{ "true", "nil" }));
    CHECK((R(L, "return C_Wildcard.CanRepurchaseTalentRolls(3, true)") == V{ "false", "REPURCHASE_WILDCARD_ROLL_NO_MONEY" }));
    s_marks = 66;
    CHECK((R(L, "return C_Wildcard.CanRepurchaseTalentRolls(2, false)") == V{ "true", "nil" }));
    CHECK((R(L, "return C_Wildcard.CanRepurchaseTalentRolls(3, false)") == V{ "false", "REPURCHASE_WILDCARD_ROLL_NO_TOKENS" }));
    CHECK((R(L, "return C_Wildcard.GetRepurchaseTalentRollCost(3, true), C_Wildcard.GetRepurchaseTalentRollCost(3, false), "
                "C_Wildcard.GetRepurchaseTalentRollCost(14315, false), C_Wildcard.GetRepurchaseTalentRollCost(14316, false)") ==
        V{ "450000", "99", "472395", "-1" }));
    CHECK((R(L, "return C_Wildcard.GetRepurchaseRollCost(3, true), C_Wildcard.GetRepurchaseAbilityRollCost(3, false), "
                "C_Wildcard.GetRepurchaseRollCost(10736, false), C_Wildcard.GetRepurchaseRollCost(10737, false)") ==
        V{ "600000", "150", "536800", "-1" }));
    Stop(L);
}

// RepurchaseRolls & co. send CMSG 0x6F1 / 0x6F2 / 0x6F3 {u32 count, u8 gold} when the check passes, else false and
// nothing.
COA_TEST(Wildcard_RepurchaseSends) {
    lua_State* L = Repurchasable();
    s_money = 1000000;
    s_marks = 1000;
    V r;
    CHECK((Sent(L, "return C_Wildcard.RepurchaseRolls(2, true)", &r) == Packets{ CoATest::Expected(0x6F1, CoATest::Bytes().U32(2).U8(1)) }));
    CHECK((r == V{ "true" }));
    CHECK((Sent(L, "return C_Wildcard.RepurchaseAbilityRolls(3, false)", &r) == Packets{ CoATest::Expected(0x6F2, CoATest::Bytes().U32(3).U8(0)) }));
    CHECK((Sent(L, "return C_Wildcard.RepurchaseTalentRolls(1, true)", &r) == Packets{ CoATest::Expected(0x6F3, CoATest::Bytes().U32(1).U8(1)) }));
    CHECK((Sent(L, "return C_Wildcard.RepurchaseRolls(6, true)", &r).empty()));
    CHECK((r == V{ "false" }));
    // FUN_100e92d0 wants both arguments.
    CHECK((Sent(L, "return C_Wildcard.RepurchaseRolls(1)", &r).empty()));
    CHECK(r.empty());
    Stop(L);
}

// GetMax*: the largest count up to the available one that validates; CanRepurchaseAnyRolls: some kind fails only on a
// count / money reason.
COA_TEST(Wildcard_MaxAndAny) {
    lua_State* L = Repurchasable();
    s_money = 650000;
    CHECK((R(L, "return C_Wildcard.GetMaxRepurchasableRolls(true), C_Wildcard.GetMaxRepurchasableAbilityRolls(true), "
                "C_Wildcard.GetMaxRepurchasableTalentRolls(true)") == V{ "3", "3", "4" }));
    s_marks = 120;
    CHECK((R(L, "return C_Wildcard.GetMaxRepurchasableRolls(false), C_Wildcard.GetMaxRepurchasableTalentRolls(false)") == V{ "2", "3" }));
    CHECK((R(L, "return C_Wildcard.CanRepurchaseAnyRolls(true)") == V{ "true" }));
    s_money = 0;
    CHECK((R(L, "return C_Wildcard.CanRepurchaseAnyRolls(true)") == V{ "true" }));   // NO_MONEY still counts
    Configs({}, {});
    CHECK((R(L, "return C_Wildcard.CanRepurchaseAnyRolls(true), C_Wildcard.GetMaxRepurchasableRolls(true)") == V{ "false", "0" }));
    CHECK((R(L, "return C_Wildcard.CanRepurchaseAnyRolls('x')").empty()));
    CHECK((R(L, "return C_Wildcard.GetMaxRepurchasableRolls('x')").empty()));
    Stop(L);
}

// The three result packets each fire their event with the string.
COA_TEST(Wildcard_ResultPackets) {
    lua_State* L = Start();
    CHECK((Feed(0x6F4, CoATest::Bytes().Str("REPURCHASE_WILDCARD_ROLL_OK")) == V{ "WILDCARD_REPURCHASE_ROLLS_RESULT:REPURCHASE_WILDCARD_ROLL_OK" }));
    CHECK((Feed(0x646, CoATest::Bytes().Str("R1")) == V{ "WILDCARD_RESET_ABILITIES_RESULT:R1" }));
    CHECK((Feed(0x64C, CoATest::Bytes().Str("R2")) == V{ "WILDCARD_REROLL_UNLOCKED_STARTING_ABILITIES_RESULT:R2" }));
    CHECK((Feed(0x644, CoATest::Bytes().Str("ROLL_ABILITIES_OK")) == V{ "WILDCARD_ROLL_ABILITIES_RESULT:ROLL_ABILITIES_OK" }));
    CHECK((Feed(0x644, CoATest::Bytes().Str("X_CAN_START_RAPID_ROLLING_Y")) == V{ "WILDCARD_RAPID_ROLL_RESULT:X_CAN_START_RAPID_ROLLING_Y" }));
    CHECK((Feed(0x644, CoATest::Bytes().Str("CAN_START_RAPID_ROLLING")) == V{ "WILDCARD_ROLL_ABILITIES_RESULT:CAN_START_RAPID_ROLLING" }));
    CHECK((Feed(0x61E, CoATest::Bytes().Str("CA_UNLEARN_LOCKED")) == V{ "WILDCARD_UNLEARN_ABILITY_RESULT:CA_UNLEARN_LOCKED" }));
    Stop(L);
}

// The starting-choice gates read the build's cached ability count (+0x44) against CONFIG_WILDCARD_STARTING_ABILITY_COUNT
// (4 by default) on a wildcard build.
COA_TEST(Wildcard_StartingGates) {
    lua_State* L = Start({ { 100 } });
    const char* gates = "return C_Wildcard.CanResetAbilities(), C_Wildcard.CanShowStartingChoice(), "
                        "C_Wildcard.WillRollStartingAbilities(), C_Wildcard.WillRollFirstNonStartingAbility()";
    CoA::CA::ActiveBuild()->u44 = 3;
    CHECK((R(L, gates) == V{ "true", "true", "true", "false" }));
    CoA::CA::ActiveBuild()->u44 = 4;
    CHECK((R(L, gates) == V{ "true", "true", "false", "true" }));
    CoA::CA::ActiveBuild()->u44 = 5;
    CHECK((R(L, gates) == V{ "false", "false", "false", "false" }));
    Configs({ { "CONFIG_WILDCARD_STARTING_ABILITY_COUNT", 5 } }, {});
    CHECK((R(L, gates) == V{ "true", "true", "false", "true" }));
    CHECK(CoA::WildcardRolls::StartingCount() == 5);
    CoA::CA::ActiveBuild()->wildcard = 0;
    CHECK((R(L, gates) == V{ "false", "false", "false", "false" }));
    CoA::CA::ActiveBuild()->wildcard = 1;
    // A reveal running hides the starting choice only.
    Feed(0x621, Learned(100, 1, "STOP_RAPID_ROLLING_NONE"));
    CHECK((R(L, gates) == V{ "true", "false", "false", "true" }));
    Stop(L);
}

// ResetAbilities always sends CMSG 0x645; RerollUnlockedStartingAbilities sends 0x64B unless a reveal runs.
COA_TEST(Wildcard_ResetAndReroll) {
    lua_State* L = Start();
    V r;
    CHECK((Sent(L, "return C_Wildcard.ResetAbilities()", &r) == Packets{ CoATest::Expected(0x645, CoATest::Bytes()) }));
    CHECK((r == V{ "true" }));
    CHECK((Sent(L, "return C_Wildcard.RerollUnlockedStartingAbilities()", &r) == Packets{ CoATest::Expected(0x64B, CoATest::Bytes()) }));
    CHECK((r == V{ "true" }));
    Feed(0x621, Learned(100, 1, "STOP_RAPID_ROLLING_NONE"));
    CHECK((Sent(L, "return C_Wildcard.RerollUnlockedStartingAbilities()", &r).empty()));
    CHECK((r == V{ "false" }));
    Stop(L);
}

// CanRollAbilities: the unspent essence (budget at the build's level above the spent wildcard costs) covers one roll
// (CONFIG_WILDCARD_ABILITY_ROLL_COST 2 / _TALENT_ 1 by default), and no reveal runs.
COA_TEST(Wildcard_CanRollAbilities) {
    lua_State* L = Start({ { 100 } });
    CHECK((R(L, "return C_Wildcard.CanRollAbilities()") == V{ "true" }));
    SetEssence(4, 10, 3, 0);   // 3 AE, 2 spent: 1 left
    CHECK((R(L, "return C_Wildcard.CanRollAbilities()") == V{ "false" }));
    CHECK(!CoA::WildcardRolls::CanAffordRoll(false));
    SetEssence(4, 10, 3, 1);   // one TE covers a talent roll
    CHECK((R(L, "return C_Wildcard.CanRollAbilities()") == V{ "true" }));
    CHECK(CoA::WildcardRolls::CanAffordRoll(true));
    Configs({ { "CONFIG_WILDCARD_TALENT_ROLL_COST", 2 }, { "CONFIG_WILDCARD_ABILITY_ROLL_COST", 1 } }, {});
    CHECK(!CoA::WildcardRolls::CanAffordRoll(true));
    CHECK(CoA::WildcardRolls::CanAffordRoll(false));
    SetEssence(4, 10, 2, 0);   // spent equals the budget: nothing left
    CHECK((R(L, "return C_Wildcard.CanRollAbilities()") == V{ "false" }));
    SetEssence(4, 10, 1, 0);   // spent above the budget: still nothing
    CHECK((R(L, "return C_Wildcard.CanRollAbilities()") == V{ "false" }));
    SetEssence(4, 10, 20, 5);
    Feed(0x621, Learned(100, 1, "STOP_RAPID_ROLLING_NONE"));
    CHECK((R(L, "return C_Wildcard.CanRollAbilities()") == V{ "false" }));
    Stop(L);
}

// FUN_10a31760: the poll re-armed at 50 ms; it polls every 50 ms until a second has passed, then every second, and fires
// WILDCARD_ROLL_READY only on a not-ready -> ready edge.
COA_TEST(Wildcard_ReadyPoll) {
    lua_State* L = Start({ { 100 } });
    s_tick = 5000;
    CoA::WildcardRolls::ArmPoll();
    CHECK(s_scheduled.size() == 1);
    CHECK(s_scheduled[0].ms == 50);
    CHECK(s_scheduled[0].param == nullptr);
    CHECK(s_cancelled.empty());
    s_tick = 5999;
    CoATest::RecordSignals();
    FireLast();
    CHECK((CoATest::Signals() == V{ "WILDCARD_ROLL_READY" }));
    CHECK(s_scheduled.back().ms == 50);
    FireLast();   // still ready: no event
    CHECK((CoATest::Signals() == V{ "WILDCARD_ROLL_READY" }));
    s_tick = 6000;
    FireLast();
    CHECK(s_scheduled.back().ms == 1000);
    SetEssence(4, 10, 2, 0);
    FireLast();   // not ready
    SetEssence(4, 10, 20, 5);
    FireLast();   // ready again: the edge fires
    CoATest::StopRecordingSignals();
    CHECK((CoATest::Signals() == V{ "WILDCARD_ROLL_READY", "WILDCARD_ROLL_READY" }));
    // Re-arming cancels the running timer (the last scheduled id) first.
    const uint32_t running = s_scheduled.back().id;
    s_tick = 0xFFFFFF00;   // the deadline wraps; the signed difference still says "before"
    CoA::WildcardRolls::ArmPoll();
    CHECK(s_cancelled.size() == 1);
    CHECK(s_cancelled[0].id == running);
    FireLast();
    CHECK(s_scheduled.back().ms == 50);
    Stop(L);
}

// C_CharacterAdvancement.UnlearnID on a wildcard realm asks the mode manager (CA Deps): refused while a rapid roll runs
// or a reveal is pending; otherwise one CMSG 0x61D per id, the poll re-armed, the unlearn marked outstanding -- and
// SMSG 0x61E CA_UNLEARN_OK then rolls (CMSG 0x643 with nothing desired), once.
COA_TEST(Wildcard_UnlearnIDThroughCADeps) {
    lua_State* L = Start({ { 100 }, { 102 } });
    V r;
    CHECK((Sent(L, "return C_CharacterAdvancement.UnlearnID({100, 102})", &r) ==
        Packets{ CoATest::Expected(0x61D, CoATest::Bytes().U32(100)), CoATest::Expected(0x61D, CoATest::Bytes().U32(102)) }));
    CHECK((r == V{ "true" }));
    CHECK(s_scheduled.size() == 1);
    CHECK(s_scheduled[0].ms == 50);
    CHECK((R(L, "local ok, why = C_Wildcard.CanStartRapidRolling() return why") == V{ "WILDCARD_REROLL_PENDING" }));
    Packets sent;
    CHECK((FeedSent(0x61E, CoATest::Bytes().Str("CA_UNLEARN_OK"), sent) == V{ "WILDCARD_UNLEARN_ABILITY_RESULT:CA_UNLEARN_OK" }));
    CHECK((sent == Packets{ CoATest::Expected(0x643, CoATest::Bytes().U32(0).U32(0).U32(0).U8(0)) }));
    FeedSent(0x61E, CoATest::Bytes().Str("CA_UNLEARN_OK"), sent);
    CHECK(sent.empty());   // the flag was taken
    // A failed unlearn clears the flag without rolling.
    Run(L, "C_CharacterAdvancement.UnlearnID(100)");
    FeedSent(0x61E, CoATest::Bytes().Str("CA_UNLEARN_LOCKED"), sent);
    CHECK(sent.empty());
    FeedSent(0x61E, CoATest::Bytes().Str("CA_UNLEARN_OK"), sent);
    CHECK(sent.empty());
    // A reveal pending refuses the unlearn.
    Feed(0x621, Learned(100, 1, "STOP_RAPID_ROLLING_NONE"));
    CHECK((Sent(L, "return C_CharacterAdvancement.UnlearnID(100)", &r).empty()));
    CHECK((r == V{ "false" }));
    Stop(L);
}

// Wildcard::Init fills the CA Deps left open in Plan 7.
COA_TEST(Wildcard_CADepsWired) {
    lua_State* L = Start({ { 100 } });
    CHECK(CoA::CA::Dependencies().wildcardRollActive == CoA::WildcardRapid::Active);
    CHECK(CoA::CA::Dependencies().wildcardRollInFlight == CoA::WildcardRapid::InFlight);
    CHECK(CoA::CA::Dependencies().wildcardMarkRerollPending == CoA::WildcardRapid::MarkRerollPending);
    CHECK(CoA::CA::Dependencies().wildcardArmPoll == CoA::WildcardRolls::ArmPoll);
    CHECK(CoA::CA::Dependencies().wildcardUndesired == CoA::WildcardSets::IsUndesired);
    Stop(L);
}

// SMSG 0x621: a reveal record {entry, its first-rank spell, ranks gained, pre-roll rank} the CA natives read (the
// known check is false and the rank shown is the pre-roll one while it runs), a 1.5 s timer with the entry as its
// param, then WILDCARD_ENTRY_LEARNED (stop NONE) or WILDCARD_RAPID_ROLL_LEARNED (any other stop, unknown ones too).
COA_TEST(Wildcard_EntryLearnedReveals) {
    lua_State* L = Start({ { 100, 1 } });
    CHECK((R(L, "return C_CharacterAdvancement.IsKnownID(100)") == V{ "true" }));
    CHECK((Feed(0x621, Learned(100, 1, "STOP_RAPID_ROLLING_NONE")) == V{ "WILDCARD_ENTRY_LEARNED:100,2,1" }));
    const std::vector<CoA::CA::Request>& reveals = CoA::CA::WildcardReveals();
    CHECK(reveals.size() == 1);
    CHECK(reveals[0].id == 100);
    CHECK(reveals[0].u4 == 5000);
    CHECK(reveals[0].u8 == 1);
    CHECK(reveals[0].rank == 1);
    CHECK(CoA::WildcardRolls::RevealsPending());
    CHECK(s_scheduled.size() == 1);
    CHECK(s_scheduled[0].ms == 0x5DC);
    CHECK(reinterpret_cast<uintptr_t>(s_scheduled[0].param) == 100);
    CHECK((R(L, "return C_CharacterAdvancement.IsKnownID(100)") == V{ "false" }));
    CHECK((Feed(0x621, Learned(102, 3, "STOP_RAPID_ROLLING_COUNT_DEPLETED")) ==
        V{ "WILDCARD_RAPID_ROLL_LEARNED:102,3,0,STOP_RAPID_ROLLING_COUNT_DEPLETED" }));
    CHECK(reveals.size() == 2);
    CHECK(reveals[1].u4 == 5100);
    CHECK(reveals[1].rank == 0);
    CHECK((Feed(0x621, Learned(999, 2, "SOMETHING_ELSE")) == V{ "WILDCARD_RAPID_ROLL_LEARNED:999,2,0,SOMETHING_ELSE" }));
    CHECK(reveals[2].u4 == 0);   // no row, no spell
    // The 1.5 s timer ends the reveal of its own entry only.
    Fire(1);
    CHECK(reveals.size() == 2);
    CHECK(reveals[0].id == 100);
    CHECK(reveals[1].id == 999);
    Fire(0);
    CHECK((R(L, "return C_CharacterAdvancement.IsKnownID(100)") == V{ "true" }));
    Fire(0);   // already gone: nothing
    CHECK(reveals.size() == 1);
    Stop(L);
}

// The spellbook-add detour (0x10275ED0): a spell whose first rank is being revealed waits in the deferred list until the
// reveal ends; any other add goes straight through with the learn messages muted on a wildcard realm, then list
// 0x10be2aec.
COA_TEST(Wildcard_SpellbookAddsDeferredBehindReveals) {
    lua_State* L = Start();
    CHECK(CoA::WildcardRolls::AddKnownSpellHook(7000, 1, 0));
    CHECK(s_adds.size() == 1);
    CHECK(s_adds[0].spell == 7000);
    CHECK(s_adds[0].notify == 1);
    CHECK(s_adds[0].suppressed == 1);
    CHECK(CoA::ClientHooks::suppressLearnMessages == 0);
    CHECK((s_afterAdds == std::vector<uint32_t>{ 7000 }));
    Feed(0x621, Learned(100, 1, "STOP_RAPID_ROLLING_NONE"));
    CHECK(CoA::WildcardRolls::AddKnownSpellHook(5001, 1, 5000));   // rank 2 of the revealed spell: deferred
    CHECK(CoA::WildcardRolls::AddKnownSpellHook(5000, 0, 0));
    CHECK(s_adds.size() == 1);
    CHECK(CoA::WildcardRolls::AddKnownSpellHook(7001, 0, 0));      // another spell: added
    CHECK(s_adds.size() == 2);
    FireLast();   // the reveal ends: its adds run in order
    CHECK(s_adds.size() == 4);
    CHECK(s_adds[2].spell == 5001);
    CHECK(s_adds[2].replace == 5000);
    CHECK(s_adds[3].spell == 5000);
    CHECK((s_afterAdds == std::vector<uint32_t>{ 7000, 7001, 5001, 5000 }));
    CHECK(CoA::WildcardRolls::AddKnownSpellHook(5001, 1, 0));      // no reveal any more
    CHECK(s_adds.size() == 5);
    // Not wildcard: no muting.
    Mode(0);
    CHECK(CoA::WildcardRolls::AddKnownSpellHook(7002, 1, 0));
    CHECK(s_adds.back().suppressed == 0);
    Stop(L);
}

// While the module's own add runs, the hook lets the client body through (the trampoline).
COA_TEST(Wildcard_SpellbookHookPassesItsOwnAdd) {
    lua_State* L = Start();
    static bool s_inner = true;
    CoA::Wildcard::Dependencies().addKnownSpell = [](uint32_t spell, uint32_t, uint32_t) {
        s_inner = CoA::WildcardRolls::AddKnownSpellHook(spell, 0, 0);
    };
    CHECK(CoA::WildcardRolls::AddKnownSpellHook(7000, 0, 0));
    CHECK(!s_inner);
    Stop(L);
}

// FUN_10a2f950: the arrays zeroed, the poll stopped, the outstanding unlearn dropped, the reveal timers cancelled by
// the name "WildcardReveal", the reveals and deferred adds dropped.
COA_TEST(Wildcard_ModeManagerGlueReset) {
    lua_State* L = Start({ { 100 } });
    Feed(0x6EE, MiscList({ { 1, 2, 3, 4, 5 } }, 1));
    Run(L, "C_CharacterAdvancement.UnlearnID(100)");   // poll armed, unlearn outstanding
    Feed(0x621, Learned(100, 1, "STOP_RAPID_ROLLING_NONE"));
    CoA::WildcardRolls::AddKnownSpellHook(5000, 0, 0);
    const uint32_t poll = s_scheduled[0].id;
    const uint32_t reveal = s_scheduled[1].id;
    CoA::Lifecycle::RunGlueScreen();
    CHECK((Counts(L) == V{ "0", "0", "0" }));
    CHECK(CoA::WildcardRolls::SpecCounter(0) == 0);
    CHECK(s_cancelled.size() == 2);
    CHECK(s_cancelled[0].id == poll);
    CHECK(s_cancelled[0].param.empty());
    CHECK(s_cancelled[1].id == reveal);
    CHECK(s_cancelled[1].param == "WildcardReveal");
    CHECK(!CoA::WildcardRolls::RevealsPending());
    CHECK(!CoA::WildcardRapid::TakeRerollPending());
    Fire(1);   // a stale reveal timer finds nothing: no deferred add
    CHECK(s_adds.empty());
    CoA::Lifecycle::RunGlueScreen();   // nothing armed: nothing cancelled
    CHECK(s_cancelled.size() == 2);
    Stop(L);
}

// FUN_10a318b0 (every world entry): the reveals cancelled and dropped, the rapid-rolling session reset, the poll
// restarted at 1000 ms (cancelling the running one) with the ready edge cleared.
COA_TEST(Wildcard_ModeManagerEnterWorld) {
    lua_State* L = Start({ { 100 } });
    CoATest::RecordSignals();
    CoA::ClientHooks::enterWorld.Run();
    CHECK(s_scheduled.size() == 1);
    CHECK(s_scheduled[0].ms == 1000);
    FireLast();
    CHECK((CoATest::Signals() == V{ "WILDCARD_ROLL_READY" }));
    Feed(0x621, Learned(100, 1, "STOP_RAPID_ROLLING_NONE"));
    CoA::WildcardRolls::AddKnownSpellHook(5000, 0, 0);
    const uint32_t reveal = s_scheduled.back().id;
    const uint32_t poll = s_scheduled[1].id;
    CoATest::RecordSignals();
    CoA::ClientHooks::enterWorld.Run();
    CHECK(!CoA::WildcardRolls::RevealsPending());
    CHECK(s_cancelled.size() == 2);
    CHECK(s_cancelled[0].id == reveal);
    CHECK(s_cancelled[1].id == poll);
    CHECK(s_scheduled.back().ms == 1000);
    FireLast();   // the edge was cleared: ready fires again
    CoATest::StopRecordingSignals();
    CHECK((CoATest::Signals() == V{ "WILDCARD_ROLL_READY" }));
    Stop(L);
}

// The suggestion manager's marks in the wildcard lists: an undesired entry added or removed marks the suggestions
// dirty; the CA suggestion weight asks IsUndesired.
COA_TEST(Wildcard_SuggestionListsWired) {
    lua_State* L = Start({ { 100 } });
    CoA::CA::detail::Sugg().dirty = false;
    Run(L, "C_Wildcard.AddDesiredID(103, 'Ability')");
    CHECK(!CoA::CA::detail::Sugg().dirty);   // desired changes notify nobody
    Run(L, "C_Wildcard.AddUndesiredID(100, 'Ability')");
    CHECK(CoA::CA::detail::Sugg().dirty);
    CHECK(CoA::CA::Dependencies().wildcardUndesired(100, 1));
    CHECK(!CoA::CA::Dependencies().wildcardUndesired(100, 4));
    CoA::CA::detail::Sugg().dirty = false;
    Run(L, "C_Wildcard.RemoveUndesiredID(100, 'Ability')");
    CHECK(CoA::CA::detail::Sugg().dirty);
    CHECK(!CoA::CA::Dependencies().wildcardUndesired(100, 1));
    Stop(L);
}

// Every C_Wildcard native is registered for the world state only.
COA_TEST(Wildcard_NativesRegistered) {
    lua_State* L = Start();
    CHECK((R(L, "local n = 0 for k, v in pairs(C_Wildcard) do n = n + 1 end return n") == V{ "61" }));
    lua_State* G = CoATest::NewState(true);
    CHECK((Run(G, "return C_Wildcard == nil or C_Wildcard.RollAbilities == nil") == V{ "true" }));
    lua_close(G);
    Stop(L);
}
