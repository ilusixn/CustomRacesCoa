#include "CoA/Tests/ChallengeFixture.h"
#include "CoA/Tests/TestMain.h"

using namespace ChallengeFixture;

namespace {
    bool Eq(const std::string& actual, const std::string& expected) {
        if (actual != expected) {
            printf("    got:  %s\n    want: %s\n", actual.c_str(), expected.c_str());
            return false;
        }

        return true;
    }

    std::string Join(const V& v) {
        std::string s;

        for (size_t i = 0; i < v.size(); ++i) {
            s += (i ? "|" : "") + v[i];
        }

        return s;
    }

    struct TrialRec {
        const char* id;
        uint8_t owned;
        const char* author;
        const char* name;
        const char* desc;
        const char* icon;
        std::vector<std::pair<std::pair<uint32_t, uint32_t>, const char*>> challenges;
        uint8_t up;
        uint8_t down;
        uint32_t upvotes;
        uint32_t downvotes;
    };

    void Put(CoATest::Bytes& b, const TrialRec& t) {
        b.Name(t.id).U8(t.owned).Name(t.author).Name(t.name).Name(t.desc).Name(t.icon);
        b.U32(static_cast<uint32_t>(t.challenges.size()));

        for (const auto& c : t.challenges) {
            b.U32(c.first.first).U32(c.first.second).Name(c.second);
        }

        b.U8(t.up).U8(t.down).U32(t.upvotes);

        for (uint32_t i = 0; i < t.upvotes; ++i) {
            b.U64(100 + i);
        }

        b.U32(t.downvotes);

        for (uint32_t i = 0; i < t.downvotes; ++i) {
            b.U64(200 + i);
        }
    }

    const TrialRec kT1 = { "t1", 1, "Me", "Alpha Trial", "my desc", "icon1", { { { 1, 1 }, "first" }, { { 2, 1 }, "second" } }, 1, 0, 2, 1 };
    const TrialRec kT2 = { "t2", 0, "Project Ascension Archive", "Archive", "", "", { { { 2, 2 }, "" } }, 0, 0, 0, 0 };
    const TrialRec kT3 = { "t3", 0, "Bob", "Charlie", "", "", { { { 1, 1 }, "" } }, 0, 0, 3, 0 };

    V Query(uint8_t clear, uint8_t notify, const std::vector<TrialRec>& trials, uint32_t count = 0xFFFFFFFF) {
        CoATest::Bytes b;
        b.U8(clear).U8(notify).U32(count == 0xFFFFFFFF ? static_cast<uint32_t>(trials.size()) : count);

        for (const TrialRec& t : trials) {
            Put(b, t);
        }

        return Feed(0x5AC, b);
    }

    CoATest::Bytes Completion(const char* trial, const char* player, uint64_t start, uint64_t end) {
        CoATest::Bytes b;
        b.Name(trial).Name(player).U64(start).U64(end).Name("Orc").Name("Female").Name("Mage");
        return b;
    }

    // Challenges 1 (prestige, PvE, hard, solo at level 1), 2 (PvP, easy; levels 1 and 2) and 3, all trials; group 50 =
    // {1, 2}; group-keyed rows shared by both level-1s; the three trials above.
    lua_State* Fixture() {
        lua_State* L = Setup();
        ChallengeRow a;
        a.id = 1;
        a.trial = 1;
        a.prestige = 1;
        a.pve = 1;
        a.difficulty = "CHALLENGE_DIFFICULTY_HARD";
        AddChallenge(a);
        ChallengeRow b;
        b.id = 2;
        b.trial = 1;
        b.pvp = 1;
        AddChallenge(b);
        ChallengeRow c;
        c.id = 3;
        c.trial = 1;
        AddChallenge(c);
        AddGroup(1, 1, 50);
        AddGroup(2, 2, 50);
        AddLevel(1, 1, 0, 1);
        AddLevel(2, 2, 0, 1);
        AddLevel(3, 2, 0, 2);
        AddLevel(4, 3, 0, 1);
        AddRule(1, 1, 0, 1, "CHALLENGE_RULES_TYPE_NO_GROUP");
        AddRule(2, 0, 50, 1, "CHALLENGE_RULES_TYPE_NO_BANK");
        AddSpell(1, 1, 0, 1, 10, 11);
        AddSpell(2, 0, 50, 1, 20, 21);
        AddSpell(3, 2, 0, 1, 30, 0);
        AddCondition(1, 0, 50, 1, "CHALLENGE_CONDITIONS_TYPE_NONE");
        AddRequirement(1, 2, 0, 1, "CHALLENGE_REQUIREMENT_TYPE_DUO", 1, 1);
        AddRequirement(2, 3, 0, 1, "CHALLENGE_REQUIREMENT_TYPE_COMPLETE_WITHOUT_DEATH", 1, 1);
        AddReward(1, 1, 0, 1, "MAX_EXPANSIONS", "CLASS_NONE", 1, 100, 1, 0, 0);
        AddReward(2, 2, 0, 1, "MAX_EXPANSIONS", "CLASS_NONE", 1, 200, 1, 0, 0);
        AddReward(3, 0, 50, 1, "MAX_EXPANSIONS", "CLASS_NONE", 1, 300, 1, 0, 9);
        C().icons = { "Interface\\Icons\\A" };
        Load();
        Query(1, 0, { kT1, kT2, kT3 });
        return L;
    }

    std::string Can(lua_State* L, const std::string& call) {
        return Run(L, "local ok, t = " + call + " r = tostring(ok) .. '|' .. (t and seq(t) or 'nil')");
    }

    std::string Filter(lua_State* L, const std::string& text, const std::string& flags) {
        luaL_dostring(L, ("C_TrialCreator.SetTrialFilter('" + text + "', {" + flags + "})").c_str());
        return Run(L, "local s = '' for i = 1, C_TrialCreator.GetNumTrials() do s = s .. (i > 1 and ',' or '') .. C_TrialCreator.GetTrialIDByIndex(i) end r = s");
    }
}

// SMSG 0x5AC (clear / notify / append), the trial lists (the archive's hidden unless active; active first, then score),
// the trial table and its merged level fields.
COA_TEST(TrialCreator_Query) {
    lua_State* L = Fixture();
    CHECK(Eq(Run(L, "r = seq(C_TrialCreator.GetAllTrials(), 'ID')"), "t3,t1"));
    CHECK(Eq(Run(L, "r = seq(C_TrialCreator.GetOwnedTrials(), 'ID')"), "t1"));
    CHECK(Eq(Run(L, "local t = C_TrialCreator.GetTrialInfo('t1') r = t.ID .. tostring(t.Owned) .. t.Author .. '/' .. t.Name .. '/' .. "
                    "tostring(t.Description == 'Created By: Me\\nmy desc') .. t.Icon .. tostring(t.Upvoted) .. tostring(t.Downvoted) .. t.Score .. "
                    "tostring(t.IsCompleted)"),
        "t1trueMe/Alpha Trial/trueicon1truefalse1false"));
    CHECK(Eq(Run(L, "r = j(C_TrialCreator.GetTrialInfo('t1').Challenges)"),
        "{1={ChallengeId=1,Description=first,Level=1},2={ChallengeId=2,Description=second,Level=1}}"));
    // Rewards concatenated per level; spells, rules, conditions and requirements merged, in row order, without duplicates
    // (and without the hidden-spell filter).
    CHECK(Eq(Run(L, "local t = C_TrialCreator.GetTrialInfo('t1') r = seq(t.Rewards, 'ItemID') .. '/' .. seq(t.Rewards, 'IsFirstCompletion') .. '/' .. "
                    "seq(t.Spells, 'PvESpellID') .. '/' .. seq(t.Rules) .. '/' .. #t.Conditions .. t.Conditions[1].ConditionType .. '/' .. "
                    "seq(t.Requirements, 'RequirementType') .. '/' .. tostring(t.Modifiers)"),
        "100,300,200,300/true,true,true,true/10,20,30/CHALLENGE_RULES_TYPE_NO_GROUP,CHALLENGE_RULES_TYPE_NO_BANK/1CHALLENGE_CONDITIONS_TYPE_NONE/"
        "CHALLENGE_REQUIREMENT_TYPE_DUO/nil"));
    CHECK(Eq(Run(L, "r = tostring(C_TrialCreator.GetTrialInfo('nope'))"), "nil"));
    CHECK(Eq(Run(L, "r = j(C_TrialCreator.GetTrialInfo('t2').Spells) .. #C_TrialCreator.GetTrialInfo('t2').Rewards"), "{}0"));

    // Appending (clear 0) keeps the entry list as it was; a notifying query fires TRIAL_QUERY_RESULT.
    CHECK(Eq(Filter(L, "", ""), "t1,t2,t3"));
    const TrialRec t4 = { "t4", 1, "Zed", "Delta", "", "", {}, 0, 1, 0, 1 };
    CHECK(Query(0, 0, { t4 }).empty());
    CHECK(Eq(Run(L, "r = seq(C_TrialCreator.GetAllTrials(), 'ID')"), "t3,t1,t4"));
    CHECK(Eq(Filter(L, "", ""), "t1,t2,t3"));
    CHECK(Eq(Join(Query(1, 1, { kT3, t4 }, 5)), "TRIAL_QUERY_RESULT"));
    CHECK(Eq(Filter(L, "", ""), "t3,t4"));
    CHECK(Eq(Run(L, "r = seq(C_TrialCreator.GetAllTrials(), 'ID') .. '/' .. C_TrialCreator.GetTrialInfo('t4').Score"), "t3,t4/-1"));
    // The archive's trial is listed while it is the active one, and the active one sorts first.
    Query(1, 0, { kT1, kT2, kT3 });
    Feed(0x5B1, CoATest::Bytes().Name("t2"));
    CHECK(Eq(Run(L, "r = seq(C_TrialCreator.GetAllTrials(), 'ID')"), "t2,t3,t1"));
    CHECK(Eq(Join(CoATest::Call(L, "C_TrialCreator", "GetActiveTrial")), "t2"));
    Done();
}

// SaveTrial's CMSG 0x5A7 and CanSaveTrial's codes (SAVE_CHALLENGE_* renamed SAVE_TRIAL_*), in the original's order.
COA_TEST(TrialCreator_Save) {
    lua_State* L = Fixture();
    CoATest::RecordPackets();
    CHECK(Eq(Run(L, "r = tostring(C_TrialCreator.SaveTrial('', 'Name', 'Desc', 'ic', {1, 2}, {1, 3}, {'a'}))"), "true"));
    CHECK(Eq(Run(L, "r = tostring(C_TrialCreator.SaveTrial('t1', 'N', '', '', {1}, {1, 2}, {}))"), "false"));
    CHECK(Eq(Run(L, "r = tostring(C_TrialCreator.SaveTrial('t1', 'N', '', '', {1, 'x'}, {}, {}))"), "true"));   // a bad array reads as empty
    CHECK(Eq(Run(L, "r = select('#', C_TrialCreator.SaveTrial('t1', 'N', '', ''))"), "0"));
    CoATest::StopRecordingPackets();
    const auto& sent = CoATest::SentPackets();
    CHECK(sent.size() == 2);

    if (sent.size() == 2) {
        CoATest::Bytes p;
        p.U32(0).Name("Name").Name("Desc").Name("ic").U32(2).U32(1).U32(1).Name("a").U32(2).U32(3).U32(0);
        CHECK(sent[0] == CoATest::Expected(0x5A7, p));
        CoATest::Bytes q;
        q.Name("t1").Name("N").U32(0).U32(0).U32(0);
        CHECK(sent[1] == CoATest::Expected(0x5A7, q));
    }

    CHECK(Eq(Can(L, "C_TrialCreator.CanSaveTrial('', 'N', '', 'Interface\\\\Icons\\\\A', {1}, {1}, {})"), "true|nil"));
    CHECK(Eq(Can(L, "C_TrialCreator.CanSaveTrial('t1', 'N', '', 'Interface\\\\Icons\\\\A', {2, 1}, {1, 1}, {})"), "true|nil"));
    CHECK(Eq(Can(L, "C_TrialCreator.CanSaveTrial('nope', '', '', 'x', {}, {}, {})"),
        "true|SAVE_TRIAL_DOES_NOT_EXIST,SAVE_TRIAL_NO_TITLE,SAVE_TRIAL_NO_CHALLENGES,SAVE_TRIAL_INVALID_ICON"));
    CHECK(Eq(Can(L, "C_TrialCreator.CanSaveTrial('t3', 'N', '', 'Interface\\\\Icons\\\\A', {1}, {1}, {})"), "true|SAVE_TRIAL_NOT_OWNER"));
    CHECK(Eq(Can(L, "C_TrialCreator.CanSaveTrial('t3', 'N', '', 'Interface\\\\Icons\\\\A', {2}, {1}, {})"),
        "true|SAVE_TRIAL_NOT_OWNER,SAVE_TRIAL_CANNOT_EDIT_CHALLENGES"));
    CHECK(Eq(Can(L, "C_TrialCreator.CanSaveTrial('t1', 'N', '', 'Interface\\\\Icons\\\\A', {1, 2}, {1, 2}, {})"), "true|SAVE_TRIAL_CANNOT_EDIT_CHALLENGES"));
    CHECK(Eq(Can(L, "C_TrialCreator.CanSaveTrial(rep('i', 37), rep('n', 129), rep('d', 8193), rep('c', 129), {1}, {1}, "
                    "{rep('e', 1025), rep('e', 1025)})"),
        "true|SAVE_TRIAL_DOES_NOT_EXIST,SAVE_TRIAL_INVALID_ICON,SAVE_TRIAL_ID_TOO_LONG,SAVE_TRIAL_TITLE_TOO_LONG,SAVE_TRIAL_DESCRIPTION_TOO_LONG,"
        "SAVE_TRIAL_ICON_TOO_LONG,SAVE_TRIAL_ENTRY_DESCRIPTION_TOO_LONG"));
    CHECK(Eq(Can(L, "C_TrialCreator.CanSaveTrial('', rep('n', 128), rep('d', 8192), 'Interface\\\\Icons\\\\A', {1}, {1}, {rep('e', 1024)})"),
        "true|nil"));
    CHECK(Eq(Can(L, "C_TrialCreator.CanSaveTrial(rep('i', 36), 'N', '', 'Interface\\\\Icons\\\\A', {1}, {1}, {})"), "true|SAVE_TRIAL_DOES_NOT_EXIST"));
    CHECK(Eq(Run(L, "local ok, t = C_TrialCreator.CanSaveTrial('', 'N', '', 'x', {1}, {}, {}) r = tostring(ok) .. tostring(t)"), "falsenil"));
    Done();
}

// Activation: CanActivateTrial / CanDeactivateTrial, 0x5AE / 0x5B0 / 0x5B1 and the challenge lock (start code 0xF, stop
// code 4); 0x5A8 / 0x5AA.
COA_TEST(TrialCreator_Activation) {
    lua_State* L = Fixture();
    CHECK(Eq(Can(L, "C_TrialCreator.CanActivateTrial('t1')"), "true|"));
    CHECK(Eq(Can(L, "C_TrialCreator.CanActivateTrial('nope')"), "false|ACTIVATE_TRIAL_NOT_FOUND"));
    // A running challenge that cannot be stopped (a death under permadeath).
    Configs({}, { { "CONFIG_CHALLENGE_PERMADEATH_ENABLED", true } }, {});
    Feed(0x596, CoATest::Bytes().U32(1).Raw(ActiveRecord(3, 1, 1).data.data(), 0x1A));
    CHECK(Eq(Can(L, "C_TrialCreator.CanActivateTrial('t1')"), "false|ACTIVATE_TRIAL_CANNOT_ACTIVATE_CHALLENGE"));
    Feed(0x596, CoATest::Bytes().U32(0));
    // A level that cannot be started (not the party leader): reported once.
    C().leader = 99;
    CHECK(Eq(Can(L, "C_TrialCreator.CanActivateTrial('t1')"), "false|ACTIVATE_TRIAL_CANNOT_ACTIVATE_CHALLENGE"));
    CHECK(Eq(Can(L, "C_TrialCreator.CanDeactivateTrial()"), "false|DEACTIVATE_TRIAL_NO_ACTIVE_CHALLENGE,DEACTIVATE_TRIAL_NOT_FOUND"));
    C().leader = 0;

    CoATest::RecordPackets();
    CHECK(Eq(Join(Feed(0x5AE, CoATest::Bytes().Name("t1").Name("ACTIVATE_CHALLENGE_FAILED"))), "TRIAL_ACTIVATE_RESULT:t1,ACTIVATE_TRIAL_FAILED"));
    CHECK(!CoA::TrialCreator::HasActiveTrial());
    CHECK(Eq(Join(Feed(0x5AE, CoATest::Bytes().Name("t1").Name("ACTIVATE_CHALLENGE_OK"))), "TRIAL_ACTIVATE_RESULT:t1,ACTIVATE_TRIAL_OK"));
    CoATest::StopRecordingPackets();
    CHECK(CoATest::SentPackets().size() == 1 && CoATest::SentPackets()[0] == CoATest::Expected(0x5AB, CoATest::Bytes()));
    CHECK(CoA::TrialCreator::HasActiveTrial());
    CHECK(Eq(Join(CoATest::Call(L, "C_TrialCreator", "GetActiveTrial")), "t1"));
    CHECK(Eq(Can(L, "C_TrialCreator.CanActivateTrial('t1')"), "false|ACTIVATE_TRIAL_ALREADY_ACTIVE,ACTIVATE_TRIAL_CANNOT_ACTIVATE_CHALLENGE"));
    CHECK(Eq(Can(L, "C_Challenge.CanActivateChallenge(1, 1)"), "false|CHALLENGE_START_PARTICIPATING_IN_COMMUNITY_TRIAL"));
    SetActive({ { 1, 1 } });
    CHECK(Eq(Can(L, "C_Challenge.CanDeactivateChallenge(1)"), "false|CHALLENGE_STOP_PARTICIPATING_IN_COMMUNITY_TRIAL"));
    // Its challenges must be stoppable apart from "not active" and the lock itself.
    CHECK(Eq(Can(L, "C_TrialCreator.CanDeactivateTrial()"), "true|"));
    C().leader = 99;
    CHECK(Eq(Can(L, "C_TrialCreator.CanDeactivateTrial()"), "false|DEACTIVATE_TRIAL_CANNOT_DEACTIVATE_CHALLENGE"));
    C().leader = 0;

    CoATest::RecordPackets();
    CHECK(Eq(Join(Feed(0x5B0, CoATest::Bytes().Name("t1").Name("DEACTIVATE_CHALLENGE_BUSY"))), "TRIAL_DEACTIVATE_RESULT:t1,DEACTIVATE_TRIAL_BUSY"));
    CHECK(CoA::TrialCreator::HasActiveTrial());
    CHECK(Eq(Join(Feed(0x5B0, CoATest::Bytes().Name("t1").Name("DEACTIVATE_CHALLENGE_OK"))), "TRIAL_DEACTIVATE_RESULT:t1,DEACTIVATE_TRIAL_OK"));
    CoATest::StopRecordingPackets();
    CHECK(CoATest::SentPackets().size() == 1);
    CHECK(!CoA::TrialCreator::HasActiveTrial());

    // 0x5B1 sets the active id silently; a short result keeps its name.
    CHECK(Feed(0x5B1, CoATest::Bytes().Name("t3")).empty());
    CHECK(Eq(Join(CoATest::Call(L, "C_TrialCreator", "GetActiveTrial")), "t3"));
    CHECK(Eq(Join(Feed(0x5B0, CoATest::Bytes().Name("t3").Name("CHALLENGE"))), "TRIAL_DEACTIVATE_RESULT:t3,TRIAL"));
    CHECK(Eq(Join(Feed(0x5B0, CoATest::Bytes().Name("t3").Name("CHALLENG"))), "TRIAL_DEACTIVATE_RESULT:t3,CHALLENG"));
    CHECK(Eq(Join(Feed(0x5A8, CoATest::Bytes().Name("t9").Name("SAVE_TRIAL_OK"))), "TRIAL_SAVE_RESULT:t9,SAVE_TRIAL_OK"));
    CHECK(Eq(Join(Feed(0x5AA, CoATest::Bytes().Name("t9").Name("DELETE_OK"))), "TRIAL_DELETE_RESULT:t9,DELETE_OK"));
    Done();
}

// RateTrial: a changed, non-contradictory vote replaces the previous one locally and sends CMSG 0x5BF.
COA_TEST(TrialCreator_Rate) {
    lua_State* L = Fixture();
    auto rate = [&](const char* id, bool up, bool down) {
        return Run(L, std::string("r = tostring(C_TrialCreator.RateTrial('") + id + "', " + (up ? "true" : "false") + ", " + (down ? "true" : "false") + "))");
    };
    auto score = [&](const char* id) {
        return Run(L, std::string("local t = C_TrialCreator.GetTrialInfo('") + id + "') r = t.Score .. tostring(t.Upvoted) .. tostring(t.Downvoted)");
    };

    CoATest::RecordPackets();
    CHECK(Eq(rate("t3", true, false), "true"));
    CHECK(Eq(score("t3"), "4truefalse"));
    CHECK(Eq(rate("t3", true, false), "false"));
    CHECK(Eq(rate("t3", true, true), "false"));
    CHECK(Eq(rate("nope", true, false), "false"));
    CHECK(Eq(rate("t3", false, true), "true"));
    CHECK(Eq(score("t3"), "2falsetrue"));
    CHECK(Eq(rate("t3", false, false), "true"));
    CHECK(Eq(score("t3"), "3falsefalse"));
    CHECK(Eq(rate("t1", false, false), "true"));   // t1 was upvoted: one upvote dropped
    CHECK(Eq(score("t1"), "0falsefalse"));
    CHECK(Eq(Run(L, "r = select('#', C_TrialCreator.RateTrial('t1', 'x', true))"), "0"));
    CoATest::StopRecordingPackets();
    const auto& sent = CoATest::SentPackets();
    CHECK(sent.size() == 4);

    if (sent.size() == 4) {
        CHECK(sent[0] == CoATest::Expected(0x5BF, CoATest::Bytes().Name("t3").U8(1).U8(0)));
        CHECK(sent[1] == CoATest::Expected(0x5BF, CoATest::Bytes().Name("t3").U8(0).U8(1)));
        CHECK(sent[2] == CoATest::Expected(0x5BF, CoATest::Bytes().Name("t3").U8(0).U8(0)));
        CHECK(sent[3] == CoATest::Expected(0x5BF, CoATest::Bytes().Name("t1").U8(0).U8(0)));
    }

    Done();
}

// SMSG 0x5CA / 0x5CB: completions per trial, fastest first; GetTrialCompletion (1-based), GetTrialCompletions (ranks).
COA_TEST(TrialCreator_Completions) {
    lua_State* L = Fixture();
    CoATest::Bytes list;
    list.Name("t1").U32(5);
    list.Raw(Completion("t1", "Slow", 0, 30).data.data(), static_cast<uint32_t>(Completion("t1", "Slow", 0, 30).data.size()));
    list.Raw(Completion("t1", "Fast", 100, 110).data.data(), static_cast<uint32_t>(Completion("t1", "Fast", 100, 110).data.size()));
    list.Raw(Completion("t1", "Tied", 5, 15).data.data(), static_cast<uint32_t>(Completion("t1", "Tied", 5, 15).data.size()));
    CHECK(Eq(Join(Feed(0x5CA, list)), "TRIAL_COMPLETION_LIST_CHANGED:t1"));
    CHECK(Eq(Run(L, "r = j(C_TrialCreator.GetTrialCompletion('t1', 1))"),
        "{Challenge=t1,CompleteTime=110,PlayerClass=Mage,PlayerGender=Female,PlayerName=Fast,PlayerRace=Orc,StartTime=100}"));
    CHECK(Eq(Run(L, "r = C_TrialCreator.GetTrialCompletion('t1', 3).PlayerName .. tostring(C_TrialCreator.GetTrialCompletion('t1', 4))"), "Slownil"));
    CHECK(Eq(Run(L, "r = select('#', C_TrialCreator.GetTrialCompletion('t1', 0))"), "0"));
    CHECK(Eq(Run(L, "local t = C_TrialCreator.GetTrialCompletions('t1', 5) r = #t .. seq(t[1], 'PlayerName') .. '/' .. seq(t[2], 'PlayerName')"), "2Fast,Tied/Slow"));
    CHECK(Eq(Run(L, "r = select('#', C_TrialCreator.GetTrialCompletions('t1', 0)) .. #C_TrialCreator.GetTrialCompletions('t1', 0)"), "10"));
    CHECK(Eq(Run(L, "r = tostring(C_TrialCreator.GetTrialCompletion('zz', 1)) .. #C_TrialCreator.GetTrialCompletions('zz', 2)"), "nil0"));
    CHECK(Eq(Join(Feed(0x5CB, Completion("t1", "Late", 0, 1))), "TRIAL_COMPLETION_ADDED:t1,3"));
    CHECK(Eq(Run(L, "r = C_TrialCreator.GetTrialCompletion('t1', 1).PlayerName"), "Late"));
    CHECK(Eq(Join(Feed(0x5CB, Completion("t2", "Other", 0, 1))), "TRIAL_COMPLETION_ADDED:t2,0"));
    CoATest::Bytes again;
    again.Name("t1").U32(1).Raw(Completion("x", "Only", 0, 5).data.data(), static_cast<uint32_t>(Completion("x", "Only", 0, 5).data.size()));
    CHECK(Eq(Join(Feed(0x5CA, again)), "TRIAL_COMPLETION_LIST_CHANGED:t1"));
    CHECK(Eq(Run(L, "r = C_TrialCreator.GetTrialCompletion('t1', 1).PlayerName .. C_TrialCreator.GetTrialCompletion('t1', 1).Challenge .. "
                    "tostring(C_TrialCreator.GetTrialCompletion('t1', 2))"),
        "Onlyxnil"));
    Done();
}

// The ChallengeCreatorEntryContainer: trials in arrival order; the argument groups; the active trial always matches;
// text over name and author.
COA_TEST(TrialCreator_Filter) {
    lua_State* L = Fixture();
    CHECK(Eq(Filter(L, "", ""), "t1,t2,t3"));
    CHECK(Eq(Filter(L, "", "FILTER_PRESTIGE = true"), "t1,t3"));
    CHECK(Eq(Filter(L, "", "FILTER_NOT_PRESTIGE = true"), "t1,t2"));
    CHECK(Eq(Filter(L, "", "FILTER_PRESTIGE = true, FILTER_NOT_PRESTIGE = true"), "t1,t2,t3"));
    CHECK(Eq(Filter(L, "", "FILTER_PVE = true"), "t1,t3"));
    CHECK(Eq(Filter(L, "", "FILTER_PVP = true"), "t1,t2"));
    CHECK(Eq(Filter(L, "", "FILTER_PVP = true, FILTER_PRESTIGE = true"), "t1"));
    CHECK(Eq(Filter(L, "", "FILTER_SOLO = true"), "t1,t3"));
    CHECK(Eq(Filter(L, "", "FILTER_GROUP = true"), "t2"));
    CHECK(Eq(Filter(L, "", "FILTER_DIFFICULTY_HARD = true"), "t1,t3"));
    CHECK(Eq(Filter(L, "", "FILTER_DIFFICULTY_EASY = true"), "t1,t2"));
    CHECK(Eq(Filter(L, "", "FILTER_DIFFICULTY_NORMAL = true"), ""));
    CHECK(Eq(Filter(L, "", "FILTER_CAN_ACTIVATE = true"), "t1,t2,t3"));
    CHECK(Eq(Filter(L, "", "FILTER_NONE = true"), ""));
    CHECK(Eq(Filter(L, "alpha", ""), "t1"));
    CHECK(Eq(Filter(L, "BOB", ""), "t3"));
    CHECK(Eq(Filter(L, "arch", ""), "t2"));
    C().leader = 99;
    CHECK(Eq(Filter(L, "", "FILTER_CAN_ACTIVATE = true"), ""));
    C().leader = 0;
    Feed(0x5B1, CoATest::Bytes().Name("t2"));
    CHECK(Eq(Filter(L, "", "FILTER_PRESTIGE = true"), "t1,t2,t3"));
    CHECK(Eq(Filter(L, "", "FILTER_NONE = true"), "t2"));
    CHECK(Eq(Filter(L, "zzz", ""), ""));

    CHECK(Eq(Filter(L, "", "FILTER_PVP = true"), "t1,t2"));
    CHECK(Eq(Run(L, "local t = C_TrialCreator.GetTrialAtIndex(2, true) r = t.ID .. t.Name"), "t2Archive"));
    CHECK(Eq(Run(L, "r = tostring(C_TrialCreator.GetTrialAtIndex(3, false))"), "nil"));
    CHECK(Eq(Run(L, "r = select('#', C_TrialCreator.GetTrialAtIndex(0, false))"), "0"));
    CHECK(Eq(Run(L, "r = tostring(C_TrialCreator.GetTrialIDByIndex(3))"), "nil"));
    CHECK(Eq(Run(L, "r = select('#', C_TrialCreator.GetTrialIDByIndex(0))"), "0"));
    Done();
}

// The CMSGs (ids as u32 length + bytes) and CanEditTrial.
COA_TEST(TrialCreator_Cmsgs) {
    lua_State* L = Fixture();
    CoATest::RecordPackets();
    CHECK(Eq(Join(CoATest::Call(L, "C_TrialCreator", "DeleteTrial", { "s:t1" })), "true"));
    CHECK(Eq(Join(CoATest::Call(L, "C_TrialCreator", "ActivateTrial", { "s:t1" })), "true"));
    CHECK(Eq(Join(CoATest::Call(L, "C_TrialCreator", "ActivateTrial", { "s:" })), "true"));
    CHECK(Eq(Join(CoATest::Call(L, "C_TrialCreator", "DeactivateTrial")), "true"));
    CHECK(Eq(Join(CoATest::Call(L, "C_TrialCreator", "QueryTrials")), "true"));
    CHECK(Eq(Join(CoATest::Call(L, "C_TrialCreator", "QueryTrialCompletions", { "s:t3" })), "true"));
    CHECK(CoATest::Call(L, "C_TrialCreator", "DeleteTrial").empty());
    CoATest::StopRecordingPackets();
    const auto& sent = CoATest::SentPackets();
    CHECK(sent.size() == 6);

    if (sent.size() == 6) {
        CHECK(sent[0] == CoATest::Expected(0x5A9, CoATest::Bytes().Name("t1")));
        CHECK(sent[1] == CoATest::Expected(0x5AD, CoATest::Bytes().Name("t1")));
        CHECK(sent[2] == CoATest::Expected(0x5AD, CoATest::Bytes().U32(0)));
        CHECK(sent[3] == CoATest::Expected(0x5AF, CoATest::Bytes()));
        CHECK(sent[4] == CoATest::Expected(0x5AB, CoATest::Bytes()));
        CHECK(sent[5] == CoATest::Expected(0x5C9, CoATest::Bytes().Name("t3")));
    }

    CHECK(Eq(Join(CoATest::Call(L, "C_TrialCreator", "CanEditTrial", { "s:t1" })), "true"));
    CHECK(Eq(Join(CoATest::Call(L, "C_TrialCreator", "CanEditTrial", { "s:t3" })), "false"));
    CHECK(Eq(Join(CoATest::Call(L, "C_TrialCreator", "CanEditTrial", { "s:nope" })), "false"));
    Done();
}

// The glue screen (FUN_10119df0): trials, the active id, completions and the list.
COA_TEST(TrialCreator_GlueReset) {
    lua_State* L = Fixture();
    Feed(0x5B1, CoATest::Bytes().Name("t1"));
    Feed(0x5CB, Completion("t1", "Fast", 0, 1));
    CHECK(Eq(Filter(L, "", ""), "t1,t2,t3"));
    CoA::Lifecycle::RunGlueScreen();
    CHECK(!CoA::TrialCreator::HasActiveTrial());
    CHECK(Eq(Run(L, "r = #C_TrialCreator.GetAllTrials() .. C_TrialCreator.GetActiveTrial() .. tostring(C_TrialCreator.GetTrialCompletion('t1', 1)) .. "
                    "C_TrialCreator.GetNumTrials()"),
        "0nil0"));
    Done();
}
