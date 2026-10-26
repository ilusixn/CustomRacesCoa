// C_Wildcard, rapid rolling (WildcardRapid.cpp): the quick-roll configs and breakpoints, the gates and their codes, the
// session's unlearn -> roll -> learn -> reveal loop with its CMSGs and events, the cancel and drain paths, the
// outstanding-unlearn flag, the selections, the plain roll / unlearn natives, the starting choice, the roll icons, and
// the resets.
#include "CoA/Tests/WildcardFixture.h"
#include "CoA/Source/Modules/SkillCard.h"
#include "CoA/Source/Runtime/Lifecycle.h"
#include <algorithm>

using namespace WCFixture;

namespace {
    bool Has(const V& v, const std::string& s) {
        return std::find(v.begin(), v.end(), s) != v.end();
    }

    // SMSG 0x6F0: n x {u8 kind, u32 threshold, u32 max}.
    CoATest::Bytes Configs6F0(const std::vector<std::vector<uint32_t>>& configs, uint32_t n) {
        CoATest::Bytes b;
        b.U32(n);

        for (const auto& c : configs) {
            b.U8(static_cast<uint8_t>(c[0])).U32(c[1]).U32(c[2]);
        }

        return b;
    }

    CoATest::Bytes Learned(uint32_t entry, uint32_t ranks, const char* stop) {
        return CoATest::Bytes().U32(entry).U32(ranks).Str(stop);
    }

    CoATest::Bytes Str(const char* s) {
        return CoATest::Bytes().Str(s);
    }

    V State(lua_State* L) {
        return R(L, "local s = C_Wildcard.GetRapidRollingState() return s.SessionID, s.Phase, s.IsActive, s.TargetEntryID, s.IsAbility, "
                    "s.RollCount, s.Reason, s.LearnedEntryID, s.NewRank, s.PreRollRank, s.StopCode, s.IsDesired, s.CanStart, s.CanStartReason");
    }

    V Phase(lua_State* L) {
        return R(L, "local s = C_Wildcard.GetRapidRollingState() return s.Phase, s.Reason");
    }

    V CanStart(lua_State* L) {
        return R(L, "return C_Wildcard.CanStartRapidRolling()");
    }

    // A Warrior at 60 holding 100 (ability) and 102 (talent) with every essence spent, quick rolling on, ability configs
    // {0 -> 3} and talent configs {0 -> 2}, 5 + 0 roll tokens, 103 desired and 100 undesired: a session may start.
    lua_State* Ready() {
        lua_State* L = Start({ { 100, 1, 0, 1 }, { 102, 1, 0, 2 } }, true, true, 60);
        SetEssence(5, 60, 2, 0);   // 100 costs 2 wildcard AE; 102 no wildcard TE
        Configs({}, { { "CONFIG_WILDCARD_QUICK_ROLLING_ENABLED", true } });
        Feed(0x6F0, Configs6F0({ { 1, 0, 3 }, { 0, 0, 2 } }, 2));
        SetToken(0, 5);
        Run(L, "C_Wildcard.AddDesiredID(103, 'Ability') C_Wildcard.AddUndesiredID(100, 'Ability')");
        return L;
    }

    // Ready, then a session started (phase 1, the unlearn sent).
    lua_State* Started() {
        lua_State* L = Ready();
        CHECK((R(L, "return C_Wildcard.StartRapidRolling()") == V{ "true", "nil" }));
        return L;
    }
}

// SMSG 0x6F0 replaces the configs (7a: only whole 9-byte records); the breakpoints report the spec's count, the next
// threshold and its max, or nils once the last threshold is reached.
COA_TEST(WildcardRapid_ConfigsAndBreakpoints) {
    lua_State* L = Ready();
    Feed(0x6F0, Configs6F0({ { 1, 0, 3 }, { 1, 10, 5 }, { 0, 0, 2 }, { 0, 4, 6 } }, 4));
    CoATest::Bytes misc;
    misc.U32(1).U32(4).U32(4).U32(0).U32(0).U32(0);
    Feed(0x6EE, misc);
    CHECK((R(L, "return C_Wildcard.GetRapidRollAbilityBreakpointInfo()") == V{ "4", "10", "5" }));
    CHECK((R(L, "return C_Wildcard.GetRapidRollTalentBreakpointInfo()") == V{ "nil", "nil", "nil" }));
    CHECK((R(L, "return C_Wildcard.GetMaximumRapidRolls()") == V{ "3" }));
    misc = CoATest::Bytes();
    misc.U32(1).U32(10).U32(3).U32(0).U32(0).U32(0);
    Feed(0x6EE, misc);
    CHECK((R(L, "return C_Wildcard.GetRapidRollAbilityBreakpointInfo()") == V{ "nil", "nil", "nil" }));
    CHECK((R(L, "return C_Wildcard.GetMaximumRapidRolls()") == V{ "5" }));
    CHECK((R(L, "return C_Wildcard.GetRapidRollTalentBreakpointInfo()") == V{ "3", "4", "6" }));
    // A new list replaces the old one; a short record is dropped.
    CoATest::Bytes b = Configs6F0({ { 1, 0, 7 } }, 2);
    b.U8(1).U32(1);
    Feed(0x6F0, b);
    CHECK((R(L, "return C_Wildcard.GetMaximumRapidRolls(), C_Wildcard.GetRapidRollAbilityBreakpointInfo()") == V{ "7", "nil", "nil", "nil" }));
    // No player: no config at all.
    s_player = false;
    CHECK((R(L, "return C_Wildcard.GetMaximumRapidRolls()") == V{ "0" }));
    Stop(L);
}

// FUN_10a336b0's CAN_USE_RAPID_ROLLING_* codes.
COA_TEST(WildcardRapid_CanUse) {
    lua_State* L = Ready();
    CHECK((R(L, "return C_Wildcard.CanUseRapidRolling()") == V{ "true", "nil" }));
    CHECK((R(L, "return C_Wildcard.IsRapidRollingEnabled()") == V{ "true" }));
    SetEssence(5, 60, 2, 1);
    CHECK((R(L, "return C_Wildcard.CanUseRapidRolling()") == V{ "false", "CAN_USE_RAPID_ROLLING_UNSPENT_TALENT_ESSENCE" }));
    SetEssence(5, 60, 4, 1);
    CHECK((R(L, "return C_Wildcard.CanUseRapidRolling()") == V{ "false", "CAN_USE_RAPID_ROLLING_UNSPENT_ABILITY_ESSENCE" }));
    s_level = 59;
    CHECK((R(L, "return C_Wildcard.CanUseRapidRolling()") == V{ "false", "CAN_USE_RAPID_ROLLING_NOT_MAX_LEVEL" }));
    Mode(0);
    CHECK((R(L, "return C_Wildcard.CanUseRapidRolling()") == V{ "false", "CAN_USE_RAPID_ROLLING_NOT_WILDCARD" }));
    Configs({}, {});
    CHECK((R(L, "return C_Wildcard.CanUseRapidRolling()") == V{ "false", "CAN_USE_RAPID_ROLLING_NOT_ENABLED" }));
    CHECK((R(L, "return C_Wildcard.IsRapidRollingEnabled()") == V{ "false" }));
    Stop(L);
}

// FUN_10a2a410 / FUN_10a333d0: the session gates first, then the CAN_START_RAPID_ROLLING_* codes in order.
COA_TEST(WildcardRapid_CanStartCodes) {
    lua_State* L = Ready();
    CHECK((CanStart(L) == V{ "true", "nil" }));
    CHECK((R(L, "return C_Wildcard.GetNextUnlearnedID()") == V{ "100" }));
    s_skillCard = true;
    CHECK((CanStart(L) == V{ "false", "CAN_START_RAPID_ROLLING_UNBLOCKED_SKILL_CARDS" }));
    CHECK(s_skillCardSpec == 0);
    s_skillCard = false;
    Run(L, "C_Wildcard.ClearDesiredSpells() C_Wildcard.AddDesiredID(2, 'Suggestion')");
    CHECK((CanStart(L) == V{ "false", "CAN_START_RAPID_ROLLING_NO_DESIRED_ABILITIES" }));
    Run(L, "C_Wildcard.AddDesiredID(1, 'Tag')");   // a tag counts as both
    CHECK((CanStart(L) == V{ "true", "nil" }));
    Run(L, "C_Wildcard.ClearDesiredSpells() C_Wildcard.AddDesiredID(103, 'Ability') C_Wildcard.ClearUndesiredSpells() "
           "C_Wildcard.AddUndesiredID(102, 'Talent')");
    CHECK((CanStart(L) == V{ "false", "CAN_START_RAPID_ROLLING_NO_DESIRED_TALENTS" }));
    CHECK((R(L, "return C_Wildcard.GetMaximumRapidRolls()") == V{ "2" }));
    Run(L, "C_Wildcard.AddDesiredID(2, 'Suggestion')");
    CHECK((CanStart(L) == V{ "true", "nil" }));   // 5 roll tokens cover the talent max 2
    SetToken(0, 1);
    CHECK((CanStart(L) == V{ "false", "CAN_START_RAPID_ROLLING_MISSING_TALENT_TOKENS" }));
    SetToken(0x28, 1);   // the spec's talent tokens add up
    CHECK((CanStart(L) == V{ "true", "nil" }));
    Run(L, "C_Wildcard.ClearUndesiredSpells() C_Wildcard.AddUndesiredID(100, 'Ability')");
    CHECK((CanStart(L) == V{ "false", "CAN_START_RAPID_ROLLING_MISSING_ABILITY_TOKENS" }));
    SetToken(0x14, 2);   // the spec's ability tokens add up
    CHECK((CanStart(L) == V{ "true", "nil" }));
    CHECK((R(L, "return C_Wildcard.GetNextUnlearnedID()") == V{ "100" }));
    Run(L, "C_Wildcard.ClearUndesiredSpells()");
    CHECK((CanStart(L) == V{ "false", "CAN_START_RAPID_ROLLING_NO_UNDESIRED_ENTRIES" }));
    CHECK((R(L, "return C_Wildcard.GetNextUnlearnedID()") == V{ "nil" }));
    Run(L, "C_Wildcard.AddUndesiredID(100, 'Ability') C_Wildcard.ClearDesiredSpells()");
    CHECK((CanStart(L) == V{ "false", "CAN_START_RAPID_ROLLING_NO_DESIRED_ENTRIES" }));
    Run(L, "C_Wildcard.AddDesiredID(103, 'Ability')");
    SetEssence(5, 60, 2, 1);
    CHECK((CanStart(L) == V{ "false", "CAN_START_RAPID_ROLLING_UNSPENT_TALENT_ESSENCE" }));
    SetEssence(5, 60, 4, 0);
    CHECK((CanStart(L) == V{ "false", "CAN_START_RAPID_ROLLING_UNSPENT_ABILITY_ESSENCE" }));
    s_level = 70;
    s_player = false;
    CHECK((CanStart(L) == V{ "false", "CAN_START_RAPID_ROLLING_NOT_MAX_LEVEL" }));
    s_player = true;
    Mode(0);
    CHECK((CanStart(L) == V{ "false", "CAN_START_RAPID_ROLLING_NOT_WILDCARD" }));
    Configs({}, {});
    CHECK((CanStart(L) == V{ "false", "CAN_START_RAPID_ROLLING_NOT_ENABLED" }));
    Stop(L);
}

// The newest undesired entry whose kind has enough roll tokens for its quick-roll max is the next one unlearned; failing
// that, the newest with a row.
COA_TEST(WildcardRapid_NextUnlearned) {
    lua_State* L = Ready();
    Feed(0x6F0, Configs6F0({ { 1, 0, 3 }, { 0, 0, 6 } }, 2));
    Run(L, "C_Wildcard.AddUndesiredID(102, 'Talent')");   // newest, but 5 tokens are short of its max 6
    CHECK((R(L, "return C_Wildcard.GetNextUnlearnedID(), C_Wildcard.GetMaximumRapidRolls()") == V{ "100", "3" }));
    SetToken(0, 1);   // nothing has enough: the newest
    CHECK((R(L, "return C_Wildcard.GetNextUnlearnedID(), C_Wildcard.GetMaximumRapidRolls()") == V{ "102", "6" }));
    SetToken(0x28, 5);   // 1 + 5 talent tokens cover its max
    CHECK((R(L, "return C_Wildcard.GetNextUnlearnedID()") == V{ "102" }));
    SetToken(0x28, 4);
    SetToken(0x14, 2);   // the ability one has enough again, the talent one not
    CHECK((R(L, "return C_Wildcard.GetNextUnlearnedID()") == V{ "100" }));
    Stop(L);
}

// The session from start to Completed: CMSG 0x61D {target}, the poll armed, then on SMSG 0x61E CA_UNLEARN_OK
// WILDCARD_RAPID_ROLL_UNLEARNED and CMSG 0x643 {count, desired ids, tag ids, is-ability}; SMSG 0x644 OK -> waiting for
// the learn; SMSG 0x621 with a stop code -> Revealing; the reveal's end -> Completed.
COA_TEST(WildcardRapid_SessionCompletes) {
    lua_State* L = Ready();
    Run(L, "C_Wildcard.AddDesiredID(1, 'Tag')");
    V r;
    CoATest::RecordSignals();
    CHECK((Sent(L, "return C_Wildcard.StartRapidRolling()", &r) == Packets{ CoATest::Expected(0x61D, CoATest::Bytes().U32(100)) }));
    CoATest::StopRecordingSignals();
    CHECK((r == V{ "true", "nil" }));
    CHECK((CoATest::Signals() == V{ "WILDCARD_RAPID_ROLL_STATE_CHANGED" }));
    CHECK(s_scheduled.size() == 1);
    CHECK(s_scheduled[0].ms == 50);
    CHECK((State(L) == V{ "1", "WaitingForUnlearn", "true", "100", "true", "3", "WAITING_FOR_UNLEARN", "0", "0", "1",
        "STOP_RAPID_ROLLING_NONE", "false", "false", "RAPID_ROLLING_SESSION_ACTIVE" }));
    CHECK((R(L, "return C_Wildcard.IsRollRequestBlocked()") == V{ "true" }));
    Packets sent;
    CHECK((FeedSent(0x61E, Str("CA_UNLEARN_OK"), sent) == V{ "WILDCARD_UNLEARN_ABILITY_RESULT:CA_UNLEARN_OK",
        "WILDCARD_RAPID_ROLL_UNLEARNED:100,1", "WILDCARD_RAPID_ROLL_STATE_CHANGED" }));
    CHECK((sent == Packets{ CoATest::Expected(0x643, CoATest::Bytes().U32(3).U32(1).U32(103).U32(1).U32(1).U8(1)) }));
    CHECK((Phase(L) == V{ "WaitingForRoll", "CA_UNLEARN_OK" }));
    CHECK((Feed(0x644, Str("ROLL_ABILITIES_OK")) == V{ "WILDCARD_RAPID_ROLL_STATE_CHANGED", "WILDCARD_ROLL_ABILITIES_RESULT:ROLL_ABILITIES_OK" }));
    CHECK((Phase(L) == V{ "WaitingForLearn", "ROLL_ABILITIES_OK" }));
    CHECK((Feed(0x621, Learned(103, 1, "STOP_RAPID_ROLLING_DESIRED_ENTRY_LEARNED")) == V{ "WILDCARD_RAPID_ROLL_STATE_CHANGED",
        "WILDCARD_RAPID_ROLL_LEARNED:103,1,0,STOP_RAPID_ROLLING_DESIRED_ENTRY_LEARNED" }));
    CHECK((State(L) == V{ "1", "Revealing", "true", "100", "true", "3", "STOP_RAPID_ROLLING_DESIRED_ENTRY_LEARNED", "103", "1", "0",
        "STOP_RAPID_ROLLING_DESIRED_ENTRY_LEARNED", "true", "false", "RAPID_ROLLING_SESSION_ACTIVE" }));
    CoATest::RecordSignals();
    FireLast();   // the reveal ends
    CoATest::StopRecordingSignals();
    CHECK((CoATest::Signals() == V{ "WILDCARD_RAPID_ROLL_STATE_CHANGED" }));
    CHECK((Phase(L) == V{ "Completed", "STOP_RAPID_ROLLING_DESIRED_ENTRY_LEARNED" }));
    CHECK((R(L, "local s = C_Wildcard.GetRapidRollingState() return s.IsActive") == V{ "false" }));
    // A new session takes the next id.
    CHECK((R(L, "return C_Wildcard.StartRapidRolling()") == V{ "true", "nil" }));
    CHECK((R(L, "return C_Wildcard.GetRapidRollingState().SessionID") == V{ "2" }));
    Stop(L);
}

// A plain learn (STOP_RAPID_ROLLING_NONE) does not move the session; a count-depleted stop waits for ContinueRapidRolling,
// which starts the next session from AwaitingContinue.
COA_TEST(WildcardRapid_ContinueAfterDepleted) {
    lua_State* L = Started();
    Feed(0x61E, Str("CA_UNLEARN_OK"));
    Feed(0x644, Str("CAN_START_RAPID_ROLLING_OK"));
    CHECK((Phase(L) == V{ "WaitingForLearn", "CAN_START_RAPID_ROLLING_OK" }));
    CHECK((R(L, "return C_Wildcard.ContinueRapidRolling()") == V{ "false", "CAN_START_RAPID_ROLLING_OK" }));
    Feed(0x621, Learned(104, 1, "STOP_RAPID_ROLLING_NONE"));
    CHECK((Phase(L) == V{ "WaitingForLearn", "CAN_START_RAPID_ROLLING_OK" }));
    Feed(0x621, Learned(105, 1, "STOP_RAPID_ROLLING_COUNT_DEPLETED"));
    CHECK((Phase(L) == V{ "Revealing", "STOP_RAPID_ROLLING_COUNT_DEPLETED" }));
    Fire(1);   // 104's reveal: not the learned entry
    CHECK((Phase(L) == V{ "Revealing", "STOP_RAPID_ROLLING_COUNT_DEPLETED" }));
    FireLast();
    CHECK((Phase(L) == V{ "AwaitingContinue", "STOP_RAPID_ROLLING_COUNT_DEPLETED" }));
    CHECK((R(L, "return C_Wildcard.IsRollRequestBlocked(), C_Wildcard.IsAwaitingRapidRollingTalentUpgradeRoll()") == V{ "true", "false" }));
    CHECK((CanStart(L) == V{ "false", "RAPID_ROLLING_SESSION_ACTIVE" }));
    V r;
    CHECK((Sent(L, "return C_Wildcard.ContinueRapidRolling()", &r) == Packets{ CoATest::Expected(0x61D, CoATest::Bytes().U32(100)) }));
    CHECK((r == V{ "true", "nil" }));
    CHECK((State(L)[0] == "2"));
    CHECK((Phase(L) == V{ "WaitingForUnlearn", "WAITING_FOR_UNLEARN" }));
    Stop(L);
}

// TALENT_UPGRADE_LEARNED earns one more talent roll at the reveal's end (CMSG 0x643 {0, 0, 0, 0}); its learn (stop NONE
// or TALENT_UPGRADE_LEARNED) becomes the BONUS_LEARNED stop and waits for Continue.
COA_TEST(WildcardRapid_TalentUpgradeBonus) {
    lua_State* L = Started();
    Feed(0x61E, Str("CA_UNLEARN_OK"));
    Feed(0x644, Str("ROLL_ABILITIES_OK"));
    Feed(0x621, Learned(102, 1, "STOP_RAPID_ROLLING_TALENT_UPGRADE_LEARNED"));
    CoATest::RecordPackets();
    FireLast();
    CoATest::StopRecordingPackets();
    CHECK((CoATest::SentPackets() == Packets{ CoATest::Expected(0x643, CoATest::Bytes().U32(0).U32(0).U32(0).U8(0)) }));
    CHECK((State(L) == V{ "1", "WaitingForRoll", "true", "100", "false", "0", "WAITING_FOR_TALENT_UPGRADE_ROLL", "0", "0", "0",
        "STOP_RAPID_ROLLING_TALENT_UPGRADE_BONUS_PENDING", "false", "false", "RAPID_ROLLING_SESSION_ACTIVE" }));
    Feed(0x644, Str("ROLL_ABILITIES_OK"));
    Feed(0x621, Learned(102, 1, "STOP_RAPID_ROLLING_NONE"));
    CHECK((State(L) == V{ "1", "Revealing", "true", "100", "false", "0", "STOP_RAPID_ROLLING_TALENT_UPGRADE_BONUS_LEARNED", "102",
        "2", "1", "STOP_RAPID_ROLLING_TALENT_UPGRADE_BONUS_LEARNED", "false", "false", "RAPID_ROLLING_SESSION_ACTIVE" }));
    FireLast();
    CHECK((Phase(L) == V{ "AwaitingContinue", "STOP_RAPID_ROLLING_TALENT_UPGRADE_BONUS_LEARNED" }));
    // From WaitingForRoll the bonus roll's own TALENT_UPGRADE_LEARNED is the bonus too.
    Stop(L);
    L = Started();
    Feed(0x61E, Str("CA_UNLEARN_OK"));
    Feed(0x644, Str("ROLL_ABILITIES_OK"));
    Feed(0x621, Learned(102, 1, "STOP_RAPID_ROLLING_TALENT_UPGRADE_LEARNED"));
    FireLast();
    Feed(0x621, Learned(102, 1, "STOP_RAPID_ROLLING_TALENT_UPGRADE_LEARNED"));
    CHECK((Phase(L) == V{ "Revealing", "STOP_RAPID_ROLLING_TALENT_UPGRADE_BONUS_LEARNED" }));
    Stop(L);
}

// The failures: an unlearn result other than OK (an unknown one reads CA_UNLEARN_UNKNOWN), a failed roll (an empty
// result reads ROLL_ABILITIES_NO_ROLL), an unhandled stop at the reveal's end.
COA_TEST(WildcardRapid_Failures) {
    lua_State* L = Started();
    Feed(0x61E, Str("CA_UNLEARN_LOCKED"));
    CHECK((Phase(L) == V{ "Failed", "CA_UNLEARN_LOCKED" }));
    CHECK((CanStart(L) == V{ "true", "nil" }));
    Stop(L);
    L = Started();
    Feed(0x61E, Str("WHATEVER"));
    CHECK((Phase(L) == V{ "Failed", "CA_UNLEARN_UNKNOWN" }));
    Stop(L);
    L = Started();
    Feed(0x61E, Str("CA_UNLEARN_OK"));
    CHECK((Feed(0x644, Str("ROLL_ABILITIES_NO_TOKENS")) == V{ "WILDCARD_RAPID_ROLL_STATE_CHANGED", "WILDCARD_ROLL_ABILITIES_RESULT:ROLL_ABILITIES_NO_TOKENS" }));
    CHECK((Phase(L) == V{ "Failed", "ROLL_ABILITIES_NO_TOKENS" }));
    Stop(L);
    L = Started();
    Feed(0x61E, Str("CA_UNLEARN_OK"));
    Feed(0x644, Str(""));
    CHECK((Phase(L) == V{ "Failed", "ROLL_ABILITIES_NO_ROLL" }));
    Stop(L);
    L = Started();
    Feed(0x61E, Str("CA_UNLEARN_OK"));
    Feed(0x644, Str("ROLL_ABILITIES_OK"));
    Feed(0x621, Learned(103, 1, "STOP_RAPID_ROLLING_UNHANDLED"));
    FireLast();
    CHECK((Phase(L) == V{ "Failed", "STOP_RAPID_ROLLING_UNHANDLED" }));
    Stop(L);
    L = Started();
    Feed(0x61E, Str("CA_UNLEARN_OK"));
    Feed(0x644, Str("ROLL_ABILITIES_OK"));
    Feed(0x621, Learned(103, 1, "NOT_A_STOP_CODE"));   // FUN_10a2f5e0 reads it as UNKNOWN (1)
    CHECK((Phase(L) == V{ "Revealing", "NOT_A_STOP_CODE" }));
    FireLast();
    CHECK((Phase(L) == V{ "Failed", "NOT_A_STOP_CODE" }));
    Stop(L);
    // A roll answered before the unlearn is not the session's.
    L = Started();
    CHECK((Feed(0x644, Str("ROLL_ABILITIES_OK")) == V{ "WILDCARD_ROLL_ABILITIES_RESULT:ROLL_ABILITIES_OK" }));
    CHECK((Phase(L) == V{ "WaitingForUnlearn", "WAITING_FOR_UNLEARN" }));
    Stop(L);
}

// Cancelling mid-request marks the session Cancelled and drains the answer it was waiting for: the unlearn result
// (phase 1), the roll result (phase 2: a failed one drains, an OK one waits for the learn). Outside a request it simply
// ends; idle it does nothing.
COA_TEST(WildcardRapid_CancelAndDrain) {
    lua_State* L = Started();
    CoATest::RecordSignals();
    CHECK((R(L, "return C_Wildcard.CancelRapidRolling()") == V{ "true", "nil" }));
    CoATest::StopRecordingSignals();
    CHECK((CoATest::Signals() == V{ "WILDCARD_RAPID_ROLL_STATE_CHANGED" }));
    CHECK((State(L) == V{ "1", "Cancelled", "false", "0", "false", "0", "CANCELLED", "0", "0", "0", "STOP_RAPID_ROLLING_NONE", "false",
        "false", "RAPID_ROLLING_REQUEST_IN_FLIGHT" }));
    CHECK((R(L, "return C_Wildcard.StartRapidRolling()") == V{ "false", "RAPID_ROLLING_REQUEST_IN_FLIGHT" }));
    CHECK((R(L, "return C_Wildcard.CancelRapidRolling()") == V{ "true", "nil" }));   // already draining: nothing
    Packets sent;
    CHECK((FeedSent(0x61E, Str("CA_UNLEARN_OK"), sent) == V{ "WILDCARD_UNLEARN_ABILITY_RESULT:CA_UNLEARN_OK", "WILDCARD_RAPID_ROLL_STATE_CHANGED" }));
    CHECK(sent.empty());
    CHECK((Phase(L) == V{ "Cancelled", "CANCELLED_UNLEARN_RESULT_DRAINED" }));
    CHECK((CanStart(L) == V{ "true", "nil" }));
    Stop(L);

    L = Started();
    Feed(0x61E, Str("CA_UNLEARN_OK"));
    Run(L, "C_Wildcard.CancelRapidRolling()");
    CHECK((Feed(0x644, Str("ROLL_ABILITIES_NO_ROLL")) == V{ "WILDCARD_RAPID_ROLL_STATE_CHANGED", "WILDCARD_ROLL_ABILITIES_RESULT:ROLL_ABILITIES_NO_ROLL" }));
    CHECK((Phase(L) == V{ "Cancelled", "CANCELLED_ROLL_RESULT_DRAINED" }));
    Stop(L);

    L = Started();
    Feed(0x61E, Str("CA_UNLEARN_OK"));
    Run(L, "C_Wildcard.CancelRapidRolling()");
    CHECK((Feed(0x644, Str("ROLL_ABILITIES_OK")) == V{ "WILDCARD_ROLL_ABILITIES_RESULT:ROLL_ABILITIES_OK" }));
    CHECK((Phase(L) == V{ "Cancelled", "CANCELLED" }));
    CHECK((R(L, "return C_Wildcard.IsRollRequestBlocked()") == V{ "true" }));
    Feed(0x621, Learned(103, 1, "STOP_RAPID_ROLLING_NONE"));   // a plain learn drains nothing
    CHECK((Phase(L) == V{ "Cancelled", "CANCELLED" }));
    Feed(0x621, Learned(103, 1, "STOP_RAPID_ROLLING_COUNT_DEPLETED"));
    CHECK((Phase(L) == V{ "Cancelled", "CANCELLED_ENTRY_LEARNED_DRAINED" }));
    CHECK((R(L, "return C_Wildcard.IsRollRequestBlocked()") == V{ "true" }));   // the reveals still run
    Stop(L);

    L = Started();
    Feed(0x61E, Str("CA_UNLEARN_OK"));
    Feed(0x644, Str("ROLL_ABILITIES_OK"));
    Feed(0x621, Learned(103, 1, "STOP_RAPID_ROLLING_COUNT_DEPLETED"));
    CHECK((Fired(L, "C_Wildcard.CancelRapidRolling()") == V{ "WILDCARD_RAPID_ROLL_STATE_CHANGED" }));
    CHECK((Phase(L) == V{ "Idle", "" }));
    FireLast();   // the reveal ends on an idle session: nothing
    CHECK((Phase(L) == V{ "Idle", "" }));
    CHECK((Fired(L, "C_Wildcard.CancelRapidRolling()").empty()));
    Stop(L);
}

// The outstanding-unlearn flag (C_CharacterAdvancement.UnlearnID) blocks a start until SMSG 0x61E takes it; a roll
// result does not; the glue screen drops it.
COA_TEST(WildcardRapid_RerollPending) {
    lua_State* L = Ready();
    Run(L, "C_CharacterAdvancement.UnlearnID(102)");
    CHECK((CanStart(L) == V{ "false", "WILDCARD_REROLL_PENDING" }));
    CHECK((R(L, "return C_Wildcard.StartRapidRolling()") == V{ "false", "WILDCARD_REROLL_PENDING" }));
    Feed(0x644, Str("ROLL_ABILITIES_OK"));
    CHECK((CanStart(L) == V{ "false", "WILDCARD_REROLL_PENDING" }));
    Feed(0x61E, Str("CA_UNLEARN_NOT_KNOWN"));
    CHECK((CanStart(L) == V{ "true", "nil" }));
    Run(L, "C_CharacterAdvancement.UnlearnID(102)");
    CoA::Lifecycle::RunGlueScreen();
    CHECK(!CoA::WildcardRapid::TakeRerollPending());
    Stop(L);
}

// A reveal pending blocks a start, a plain roll and the roll-request check.
COA_TEST(WildcardRapid_RevealPendingBlocks) {
    lua_State* L = Ready();
    Feed(0x621, Learned(104, 1, "STOP_RAPID_ROLLING_NONE"));
    CHECK((CanStart(L) == V{ "false", "WILDCARD_REVEAL_PENDING" }));
    CHECK((R(L, "return C_Wildcard.StartRapidRolling()") == V{ "false", "WILDCARD_REVEAL_PENDING" }));
    CHECK((R(L, "return C_Wildcard.IsRollRequestBlocked(), C_Wildcard.RollAbilities()") == V{ "true", "false" }));
    FireLast();
    CHECK((R(L, "return C_Wildcard.IsRollRequestBlocked()") == V{ "false" }));
    Stop(L);
}

// A failed start answers its code's name and sends nothing.
COA_TEST(WildcardRapid_StartRefused) {
    lua_State* L = Ready();
    Run(L, "C_Wildcard.ClearUndesiredSpells()");
    V r;
    CHECK((Sent(L, "return C_Wildcard.StartRapidRolling()", &r).empty()));
    CHECK((r == V{ "false", "CAN_START_RAPID_ROLLING_NEXT_UNDESIRED_ENTRY_NOT_FOUND" }));
    Run(L, "C_Wildcard.AddUndesiredID(100, 'Ability') C_Wildcard.ClearDesiredSpells()");
    CHECK((Sent(L, "return C_Wildcard.StartRapidRolling()", &r).empty()));
    CHECK((r == V{ "false", "CAN_START_RAPID_ROLLING_NO_DESIRED_ENTRIES" }));
    CHECK((R(L, "return C_Wildcard.ContinueRapidRolling()") == V{ "false", "RAPID_ROLLING_NOT_AWAITING_CONTINUE" }));
    CHECK(s_scheduled.empty());   // no poll armed
    Stop(L);
}

// The session snapshot of the desired list: CA entries, tag ids, and the desirable suggested entries behind a
// suggestion entry (FUN_10333770, at most 250).
COA_TEST(WildcardRapid_DesiredSnapshot) {
    lua_State* L = Ready();
    CoA::CA::detail::Sugg().entries = { { 104, 10 }, { 100, 9 }, { 102, 8 }, { 105, 7 } };
    CoA::CA::detail::Sugg().dirty = false;
    Run(L, "C_Wildcard.AddDesiredID(1, 'Suggestion') C_Wildcard.AddDesiredID(2, 'Tag') C_Wildcard.AddDesiredID(2, 'Suggestion')");
    Run(L, "C_Wildcard.StartRapidRolling()");
    Packets sent;
    FeedSent(0x61E, Str("CA_UNLEARN_OK"), sent);
    // 103, then suggestion 1 (abilities not known: 104, 105), tag 2, suggestion 2 (talents not known: none).
    CHECK((sent == Packets{ CoATest::Expected(0x643, CoATest::Bytes().U32(3).U32(3).U32(103).U32(104).U32(105).U32(1).U32(2).U8(1)) }));
    Stop(L);
}

// SetRapidRollingSelections: both lists replaced through the quiet adds, an event per list that changed; a Type that
// is not a kind refuses the whole call; a value that is not a table empties that list; refused during a session.
COA_TEST(WildcardRapid_Selections) {
    lua_State* L = Ready();
    CHECK((Fired(L, "C_Wildcard.SetRapidRollingSelections({ { ID = 103, Type = 'Ability' } }, { { ID = 100, Type = 'Ability' } })").empty()));
    CHECK((R(L, "return C_Wildcard.SetRapidRollingSelections({ { ID = 1, Type = 'Tag' }, { ID = 100, Type = 'Ability' } }, {})") == V{ "true", "nil" }));
    CHECK((R(L, "return C_Wildcard.IsDesiredID(1, 'Tag'), C_Wildcard.IsDesiredID(103, 'Ability'), C_Wildcard.IsUndesiredID(100, 'Ability')") ==
        V{ "true", "false", "false" }));   // 100 is known: not desired
    CHECK((Fired(L, "C_Wildcard.SetRapidRollingSelections({ { ID = 1, Type = 'Tag' } }, { { ID = 100, Type = 'Ability' } })") ==
        V{ "WILDCARD_UNDESIRED_ENTRIES_CHANGED" }));
    CHECK((R(L, "return C_Wildcard.SetRapidRollingSelections({ { ID = 2, Type = 'Tag' }, { ID = 3, Type = 'Spell' } }, {})") ==
        V{ "false", "INVALID_SELECTIONS" }));
    CHECK((R(L, "return C_Wildcard.IsDesiredID(1, 'Tag'), C_Wildcard.IsDesiredID(2, 'Tag')") == V{ "true", "false" }));
    CHECK((R(L, "return C_Wildcard.SetRapidRollingSelections({}, 5)") == V{ "false", "INVALID_SELECTIONS" }));
    CHECK((R(L, "return C_Wildcard.SetRapidRollingSelections({})") == V{ "false", "INVALID_SELECTIONS" }));
    CHECK((R(L, "return C_Wildcard.SetRapidRollingSelections({ 7 }, { { ID = 100, Type = 'Ability' } })") == V{ "true", "nil" }));
    CHECK((R(L, "return C_Wildcard.IsDesiredID(1, 'Tag'), C_Wildcard.IsUndesiredID(100, 'Ability')") == V{ "false", "true" }));
    // Undesired selections pass AddUndesired's checks: a tag never lands there.
    Run(L, "C_Wildcard.SetRapidRollingSelections({ { ID = 103, Type = 'Ability' } }, { { ID = 1, Type = 'Tag' }, { ID = 100, Type = 'Ability' } })");
    CHECK((R(L, "return C_Wildcard.IsUndesiredID(1, 'Tag'), C_Wildcard.IsUndesiredID(100, 'Ability')") == V{ "false", "true" }));
    Run(L, "C_Wildcard.StartRapidRolling()");
    CHECK((R(L, "return C_Wildcard.SetRapidRollingSelections({}, {})") == V{ "false", "RAPID_ROLLING_SESSION_ACTIVE" }));
    Run(L, "C_Wildcard.CancelRapidRolling()");
    CHECK((R(L, "return C_Wildcard.SetRapidRollingSelections({}, {})") == V{ "false", "RAPID_ROLLING_REQUEST_IN_FLIGHT" }));
    Stop(L);
}

// UnlearnAbility: CMSG 0x61D {id} unless a session is active or in flight. RollAbilities: CMSG 0x643 with nothing
// desired unless a request is in flight, a non-awaiting session is active, or a reveal runs.
COA_TEST(WildcardRapid_PlainUnlearnAndRoll) {
    lua_State* L = Ready();
    V r;
    CHECK((Sent(L, "return C_Wildcard.UnlearnAbility(102)", &r) == Packets{ CoATest::Expected(0x61D, CoATest::Bytes().U32(102)) }));
    CHECK((r == V{ "true" }));
    CHECK((Sent(L, "return C_Wildcard.UnlearnAbility('x')", &r).empty()));
    CHECK(r.empty());
    CHECK((Sent(L, "return C_Wildcard.RollAbilities()", &r) == Packets{ CoATest::Expected(0x643, CoATest::Bytes().U32(0).U32(0).U32(0).U8(0)) }));
    CHECK((r == V{ "true" }));
    CHECK((R(L, "return C_Wildcard.IsRollRequestBlocked()") == V{ "false" }));
    Run(L, "C_Wildcard.StartRapidRolling()");
    CHECK((Sent(L, "return C_Wildcard.UnlearnAbility(102)", &r).empty()));
    CHECK((r == V{ "false" }));
    CHECK((Sent(L, "return C_Wildcard.RollAbilities()", &r).empty()));
    CHECK((r == V{ "false" }));
    // C_CharacterAdvancement.UnlearnID asks the session through the CA Deps too.
    CHECK((Sent(L, "return C_CharacterAdvancement.UnlearnID(102)", &r).empty()));
    CHECK((r == V{ "false" }));
    Run(L, "C_Wildcard.CancelRapidRolling()");   // in flight now
    CHECK((Sent(L, "return C_CharacterAdvancement.UnlearnID(102)", &r).empty()));
    CHECK((r == V{ "false" }));
    CHECK((Sent(L, "return C_Wildcard.UnlearnAbility(102)", &r).empty()));
    Stop(L);
}

// GetStartingChoiceEntries: while the starting choice shows, the build's ability entries in learn order (+0x18; unset
// ones last), at most the starting count.
COA_TEST(WildcardRapid_StartingChoiceEntries) {
    lua_State* L = Start({ { 100, 2, 0, 0 }, { 102, 1, 0, 1 }, { 103, 1, 1, 7 }, { 104, 1, 0, 3 }, { 142, 1, 0, 2 } });
    CoA::CA::ActiveBuild()->u44 = 4;
    CHECK((R(L, "local out = {} for i, e in ipairs(C_Wildcard.GetStartingChoiceEntries()) do out[#out + 1] = e.EntryID end "
                "return unpack(out)") == V{ "142", "104", "103", "100" }));
    CHECK((R(L, "local e = C_Wildcard.GetStartingChoiceEntries()[2] return e.EntryID, e.EntryId, e.internalID, e.SpellID, e.Rank, "
                "e.LearnedRank, e.Quality, e.IsLocked, e.Locked") == V{ "104", "104", "104", "5300", "1", "1", "Normal", "false", "false" }));
    CHECK((R(L, "local e = C_Wildcard.GetStartingChoiceEntries()[3] return e.IsLocked, e.Locked") == V{ "true", "true" }));
    Configs({ { "CONFIG_WILDCARD_STARTING_ABILITY_COUNT", 2 } }, {});
    CoA::CA::ActiveBuild()->u44 = 2;
    CHECK((R(L, "return #C_Wildcard.GetStartingChoiceEntries()") == V{ "2" }));
    CoA::CA::ActiveBuild()->u44 = 3;
    CHECK((R(L, "return #C_Wildcard.GetStartingChoiceEntries()") == V{ "0" }));
    Stop(L);
}

// GetRollIcons: entry ids of the indexed, visible, class-allowed rows with distinct icon pointers, five times over,
// shuffled, cut to the count.
COA_TEST(WildcardRapid_RollIcons) {
    lua_State* L = Start();
    CoA::Dbc::Table& ca = CoA::Dbc::Get("DBFilesClient\\CharacterAdvancement.dbc");
    const uint32_t shared = ca.AddString("icon-a");
    const uint32_t other = ca.AddString("icon-b");

    for (const std::pair<uint32_t, uint32_t> set : { std::make_pair(100u, shared), std::make_pair(103u, shared), std::make_pair(102u, other),
             std::make_pair(101u, other) }) {
        const uint8_t* src = ca.Row(set.first);
        std::vector<uint8_t> row(src, src + 0x1FC);
        memcpy(&row[0xC0], &set.second, 4);
        ca.Upsert(set.first, row);
    }

    CoA::CA::detail::BuildIndex();
    CHECK((R(L, "return #C_Wildcard.GetRollIcons(100)") == V{ "10" }));
    CHECK((R(L, "local n = {} for _, id in ipairs(C_Wildcard.GetRollIcons(100)) do n[id] = (n[id] or 0) + 1 end "
                "return n[100], n[102], n[103], n[101]") == V{ "5", "5", "nil", "nil" }));
    CHECK((R(L, "return #C_Wildcard.GetRollIcons(4)") == V{ "4" }));
    CHECK((R(L, "return #C_Wildcard.GetRollIcons(0)") == V{ "0" }));
    CHECK((R(L, "return C_Wildcard.GetRollIcons('x')").empty()));
    Stop(L);
    CoA::Dbc::Get("DBFilesClient\\CharacterAdvancement.dbc").Unload();   // the next CA test loads the fixture's rows again
}

// FUN_10a33b10 at every world entry and the glue reset: the session back to its default (id 0); the counter keeps
// counting. The glue also drops the quick-roll configs. A spec change cancels the session and empties the lists.
COA_TEST(WildcardRapid_Resets) {
    lua_State* L = Started();
    CoA::ClientHooks::enterWorld.Run();
    CHECK((State(L) == V{ "0", "Idle", "false", "0", "false", "0", "", "0", "0", "0", "STOP_RAPID_ROLLING_NONE", "false", "true", "" }));
    Run(L, "C_Wildcard.StartRapidRolling()");
    CHECK((State(L)[0] == "2"));
    Run(L, "C_Wildcard.CancelRapidRolling()");   // cancel pending
    CoA::Lifecycle::RunGlueScreen();
    CHECK((Phase(L) == V{ "Idle", "" }));
    CHECK((R(L, "return C_Wildcard.GetRapidRollingState().SessionID, C_Wildcard.GetMaximumRapidRolls()") == V{ "0", "0" }));
    CHECK(!CoA::WildcardRapid::InFlight());
    Stop(L);
    L = Started();
    V s = Feed(0x725, CoATest::Bytes().U32(1).U32(2));
    CHECK(Has(s, "WILDCARD_RAPID_ROLL_STATE_CHANGED"));
    CHECK(Has(s, "WILDCARD_DESIRED_ENTRIES_CHANGED"));
    CHECK(Has(s, "WILDCARD_UNDESIRED_ENTRIES_CHANGED"));
    CHECK((Phase(L) == V{ "Cancelled", "CANCELLED" }));
    CHECK(CoA::WildcardRapid::InFlight());
    CHECK(!CoA::WildcardRapid::Active());
    Stop(L);
}

// The real skillCardUnblocked (CoA::SkillCard::AnyUnblocked, FUN_102f4f70, Plan 13): a slotted, unblocked card in the
// active spec (0) refuses the start; the server's block flag or an empty slot does not.
COA_TEST(WildcardRapid_RealSkillCards) {
    lua_State* L = Ready();
    CoA::SkillCard::Reset();
    CoA::SkillCardSlots::Reset();
    CoA::SkillCard::Init();
    CoA::SkillCardSlots::Init();
    CoA::SkillCard::Dependencies().hasPlayer = [] { return true; };
    CoA::SkillCard::Dependencies().playerLevel = [] { return 60u; };
    CHECK((CanStart(L) == V{ "true", "nil" }));
    // The card in a default and a starter slot: one of them is open whatever the build's starting phase.
    auto slots = [](uint32_t card, uint8_t blocked) {
        CoATest::Bytes b;
        b.U32(1);

        for (int t = 0; t < 9; ++t) {
            if (t == 0 || t == 2) {
                b.U32(1).U32(card).U32(1).U8(blocked);
            } else {
                b.U32(0);
            }
        }

        CoATest::Deliver(CoA::SkillCardSlots::OnList, 0x624, b);
    };
    slots(200, 0);
    CHECK((CanStart(L) == V{ "false", "CAN_START_RAPID_ROLLING_UNBLOCKED_SKILL_CARDS" }));
    slots(200, 1);
    CHECK((CanStart(L) == V{ "true", "nil" }));
    slots(0, 0);
    CHECK((CanStart(L) == V{ "true", "nil" }));
    CoA::SkillCard::Reset();
    CoA::SkillCardSlots::Reset();
    Stop(L);
}
