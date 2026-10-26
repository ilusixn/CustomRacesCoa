#include "CoA/Source/Modules/AttachFixes.h"
#include "CoA/Source/Modules/ChallengeInternal.h"
#include "CoA/Tests/ChallengeFixture.h"
#include "CoA/Tests/TestMain.h"
#include <unordered_map>

using namespace ChallengeFixture;

namespace {
    // CHECK with the actual value printed on a mismatch.
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

    // "ok|NAME,NAME" from CanActivateChallenge / CanDeactivateChallenge.
    std::string Can(lua_State* L, const std::string& call) {
        return Run(L, "local ok, t = " + call + " r = tostring(ok) .. '|' .. (t and seq(t) or 'nil')");
    }

    CoATest::Bytes Failure(const char* name, uint32_t challenge, uint32_t zone, uint64_t end, float x) {
        CoATest::Bytes b;
        b.Name(name).U32(challenge).U32(1).U64(end - 50).U64(end).U32(2).U8(9).U8(1).U32(30).U8(3).U32(zone).U32(77);
        b.F32(x).F32(200.0f).F32(5.0f);
        b.Name("Drowned").Name("Human").Name("Male").Name("Warrior").Name("Creature").Name("Murloc");
        b.U32(11).U32(12).U32(13).U32(14);
        return b;
    }

    CoATest::Bytes Completion(uint32_t challenge, uint32_t level, const char* player, uint64_t start, uint64_t end) {
        CoATest::Bytes b;
        b.U32(challenge).U32(level).Name(player).U64(start).U64(end).Name("Orc").Name("Female").Name("Mage");
        return b;
    }

    CoATest::Bytes Criteria(uint32_t challenge, uint32_t requirement, uint64_t achieve, bool active) {
        CoATest::Bytes b;
        b.U32(7).U32(challenge).U32(1).U32(requirement).U64(5).U64(achieve).U8(active ? 1 : 0);
        return b;
    }
}

// UpdateChallengeDataStores: every challenge row gets an entry; group rows (an unknown challenge logged and skipped, a
// duplicate member still listed in GroupIds); group-keyed levels / rules attach to every challenge of the group; min and
// max levels; the challenge table's fields.
COA_TEST(Challenge_DataStores) {
    lua_State* L = Setup();
    ChallengeRow a;
    a.id = 1;
    a.pve = 1;
    a.requiresMode = 1;
    a.mode = "Ironman";
    a.flags = 0x40;
    a.exclusive = 6;
    a.name = "Alpha";
    a.desc = "The first";
    a.event = 33;
    a.icon = "icon1";
    a.difficulty = "CHALLENGE_DIFFICULTY_HARD";
    a.checkMap = 1;
    a.map = 30;
    a.mapDifficulty = 2;
    a.sort = 5;
    a.artwork = "art1";
    AddChallenge(a);
    ChallengeRow b;
    b.id = 2;
    b.pvp = 1;
    b.trial = 1;
    b.name = "Beta";
    b.mode = "Crusader";
    b.prestige = 1;
    AddChallenge(b);
    ChallengeRow c;
    c.id = 3;
    c.dungeon = 1;
    c.name = "Gamma";
    c.mode = "Bogus";
    AddChallenge(c);
    ChallengeRow d;
    d.id = 4;
    d.trial = 1;
    d.dungeon = 1;
    AddChallenge(d);
    AddGroup(1, 1, 50);
    AddGroup(2, 2, 50);
    AddGroup(3, 3, 60);
    AddGroup(4, 9, 50);   // no challenge 9
    AddGroup(5, 1, 50);   // duplicate member
    AddLevel(1, 1, 0, 1);
    AddLevel(2, 1, 0, 3);
    AddLevel(3, 2, 50, 1);   // every challenge of group 50
    AddLevel(4, 9, 0, 1);    // no challenge 9
    AddLevel(5, 3, 0, 2);
    AddLevel(6, 3, 70, 4);   // no group 70: nothing
    AddLevel(7, 2, 50, 5);   // owned by 2, gives challenge 1 a level 5 too
    AddRule(1, 0, 50, 1, "CHALLENGE_RULES_TYPE_NO_GROUP");
    AddRule(2, 1, 0, 3, "CHALLENGE_RULES_TYPE_NO_BANK");
    AddRule(3, 2, 0, 9, "CHALLENGE_RULES_TYPE_NO_MOUNTS");   // no level 9
    Load();

    CHECK(Eq(Run(L, "local t = C_Challenge.GetChallengeInfo(1) r = t.ID .. t.Name .. '/' .. t.Description .. tostring(t.PvE) .. tostring(t.PvP) .. "
                    "tostring(t.RequiresGameMode) .. t.RequiredGameMode .. ',' .. t.Flags .. ',' .. t.ExclusiveGroup .. ',' .. t.RequiredGameEvent .. t.Icon .. "
                    "t.DifficultyRating .. seq(t.GroupIds) .. tostring(t.IsTrial) .. tostring(t.IsDungeonTrial) .. tostring(t.IsPrestige) .. "
                    "tostring(t.ShouldCheckMap) .. t.MapID .. t.MapDifficulty .. ',' .. t.SortOrder .. t.Artwork .. tostring(t.IsFeatured)"),
        "1Alpha/The firsttruefalsetrue2,64,6,33icon1CHALLENGE_DIFFICULTY_HARD50,50falsefalsefalsetrue302,5art1false"));
    CHECK(Eq(Run(L, "local t = C_Challenge.GetChallengeInfo(1) r = tostring(t.Levels[1] ~= nil) .. tostring(t.Levels[2]) .. tostring(t.Levels[3] ~= nil) .. "
                    "seq(t.Levels[1].Rules) .. '/' .. seq(t.Levels[3].Rules) .. tostring(t.Levels[1].IsCompleted)"),
        "truenil" "trueCHALLENGE_RULES_TYPE_NO_GROUP/CHALLENGE_RULES_TYPE_NO_BANKfalse"));
    CHECK(Eq(Run(L, "local t = C_Challenge.GetChallengeInfo(2) r = seq(t.Levels[1].Rules) .. t.RequiredGameMode .. tostring(t.IsPrestige) .. tostring(t.PvP)"),
        "CHALLENGE_RULES_TYPE_NO_GROUP4096truetrue"));
    CHECK(Eq(Run(L, "local t = C_Challenge.GetChallengeInfo(3) r = t.RequiredGameMode .. '|' .. tostring(t.Levels[1]) .. tostring(t.Levels[2] ~= nil) .. "
                    "tostring(t.Levels[3])"),
        "0|niltruenil"));
    CHECK(Eq(Run(L, "local t = C_Challenge.GetChallengeInfo(4) r = tostring(next(t.Levels))"), "nil"));
    CHECK(Eq(Run(L, "r = tostring(C_Challenge.GetChallengeInfo(1).Levels[5] ~= nil) .. tostring(C_Challenge.GetChallengeInfo(2).Levels[5] ~= nil)"), "truetrue"));
    CHECK(Eq(Run(L, "r = tostring(C_Challenge.GetChallengeInfo(9))"), "nil"));

    // (available, completed, max): a normal challenge is never startable (NOT_TRIAL), so 1.
    CHECK(Eq(Join(CoATest::Call(L, "C_Challenge", "GetChallengeLevels", { "n:1" })), "1|0|5"));
    CHECK(Eq(Join(CoATest::Call(L, "C_Challenge", "GetChallengeLevels", { "n:3" })), "2|0|2"));
    CHECK(Eq(Join(CoATest::Call(L, "C_Challenge", "GetChallengeLevels", { "n:4" })), "1|0|0"));
    CHECK(Eq(Join(CoATest::Call(L, "C_Challenge", "GetChallengeLevels", { "n:99" })), "1|0|0"));
    SetCompleted({ { 1, 1 }, { 1, 3 } });
    CHECK(Eq(Join(CoATest::Call(L, "C_Challenge", "GetChallengeLevels", { "n:1" })), "1|3|5"));
    // Active already (code 3) is ignored for the available level.
    SetActive({ { 3, 2 } });
    CHECK(Eq(Join(CoATest::Call(L, "C_Challenge", "GetChallengeLevels", { "n:3" })), "2|0|2"));

    // Normal, then trials, then dungeon trials; a trial that is also a dungeon trial is listed twice.
    CHECK(Eq(Run(L, "r = seq(C_Challenge.GetChallengesWithGroupID(50))"), "1,2"));
    CHECK(Eq(Run(L, "r = seq(C_Challenge.GetChallengesWithGroupID(0))"), "1,2,4,3,4"));
    CHECK(Eq(Run(L, "r = seq(C_Challenge.GetChallengesWithGroupID(60))"), "3"));
    Done();
}

// Rewards: realm expansion, player class and the achievement gate filter them; first-completion rewards until the level
// is completed, then repeat rewards; group rewards once all but one of the group's count are done; the featured
// multiplier (amount > 1, the reward row's own challenge); all sorted by their sort column.
COA_TEST(Challenge_Rewards) {
    lua_State* L = Setup();
    ChallengeRow a;
    a.id = 1;
    a.trial = 1;
    AddChallenge(a);
    a.id = 2;
    AddChallenge(a);
    AddGroup(1, 1, 50);
    AddGroup(2, 2, 50);
    AddLevel(1, 1, 0, 1);
    AddLevel(2, 2, 0, 1);
    AddReward(1, 1, 0, 1, "EXPANSION_WRATH_OF_THE_LICH_KING", "CLASS_WARRIOR", 1, 100, 1, 0, 3);
    AddReward(2, 1, 0, 1, "EXPANSION_CLASSIC", "CLASS_NONE", 1, 101, 1, 0, 1);
    AddReward(3, 1, 0, 1, "MAX_EXPANSIONS", "CLASS_NONE", 1, 102, 5, 0, 1);
    AddReward(4, 1, 0, 1, "BOGUS", "CLASS_PALADIN", 1, 103, 1, 0, 1);
    AddReward(5, 1, 0, 1, "MAX_EXPANSIONS", "BOGUS", 0, 104, 2, 0, 2);
    AddReward(6, 1, 0, 1, "MAX_EXPANSIONS", "CLASS_NONE", 1, 105, 1, 900, 1);   // no Achievement.dbc row
    AddReward(7, 1, 0, 1, "MAX_EXPANSIONS", "CLASS_NONE", 1, 106, 1, 901, 1);   // flags 0x2
    AddReward(8, 1, 0, 1, "MAX_EXPANSIONS", "CLASS_NONE", 1, 108, 1, 902, 0);   // flags 0x10
    AddReward(9, 1, 0, 1, "MAX_EXPANSIONS", "CLASS_NONE", 1, 109, 1, 903, 1);   // earned
    AddReward(10, 0, 50, 1, "MAX_EXPANSIONS", "CLASS_NONE", 1, 110, 4, 0, 4);   // the group's challenges
    AddReward(11, 9, 0, 1, "MAX_EXPANSIONS", "CLASS_NONE", 1, 111, 1, 0, 0);    // no challenge 9
    AddReward(12, 1, 0, 1, "MAX_EXPANSIONS", "CLASS_NONE", 1, 112, 1, 904, 7);   // flags 0x300
    AddGroupReward(1, 50, 1, 2, "MAX_EXPANSIONS", "CLASS_NONE", 200, 3, 0, 9);
    AddGroupReward(2, 50, 1, 2, "EXPANSION_CLASSIC", "CLASS_NONE", 201, 3, 0, 9);
    AddGroupReward(3, 50, 1, 2, "MAX_EXPANSIONS", "CLASS_MAGE", 202, 3, 0, 9);
    AddGroupReward(4, 50, 1, 2, "MAX_EXPANSIONS", "CLASS_NONE", 203, 3, 900, 9);
    AddGroupReward(5, 50, 1, 1, "MAX_EXPANSIONS", "CLASS_NONE", 204, 3, 0, 8);   // count 1: only while none is done
    C().achievementFlags = { { 901, 0x2 }, { 902, 0x10 }, { 903, 0 }, { 904, 0x300 } };
    C().achievementsDone = { 903 };
    AddFeatured(1, 1, 0, 1, 0);   // week 1 (C().now is in 1970's second week), challenge 1
    Configs({}, {}, { { "CONFIG_CHALLENGE_FEATURED_REWARD_MULTIPLIER", 2.5f } });
    Load();

    const std::string items = "local t = C_Challenge.GetChallengeInfoByLevel(%d, 1) r = seq(t.Rewards, 'ItemID') .. '/' .. seq(t.Rewards, 'ItemAmount') .. '/' .. "
                              "seq(t.Rewards, 'IsFirstCompletion') .. '/' .. seq(t.Rewards, 'IsSpecialReward')";
    auto rewards = [&](int id) {
        char buf[512];
        snprintf(buf, sizeof(buf), items.c_str(), id);
        return Run(L, buf);
    };

    CHECK(Eq(rewards(1), "108,102,100,110,204/1,12,1,4,3/true,true,true,true,false/false,false,false,false,true"));
    CHECK(Eq(rewards(2), "110,204/4,3/true,false/false,true"));
    CHECK(Eq(Run(L, "local t = C_Challenge.GetChallengeInfoByLevel(1, 1) r = t.Rewards[1].Achievement .. tostring(t.IsFeatured)"), "902true"));

    // Challenge 1 done: its repeat rewards; the count-2 group reward appears on both, the count-1 one is gone.
    SetCompleted({ { 1, 1 } });
    Rebuild();
    CHECK(Eq(rewards(1), "104,200/5,3/false,false/false,true"));
    CHECK(Eq(rewards(2), "110,200/4,3/true,false/false,true"));
    CHECK(Eq(Run(L, "r = tostring(C_Challenge.GetChallengeInfoByLevel(1, 1).IsCompleted)"), "true"));
    SetCompleted({ { 1, 1 }, { 2, 1 } });
    Rebuild();
    CHECK(Eq(rewards(2), "///"));

    // Another class and another expansion.
    SetCompleted({});
    C().cls = 2;
    Realm(0);
    Rebuild();
    CHECK(Eq(rewards(1), "108,101,102,103,110,204/1,1,12,1,4,3/true,true,true,true,true,false/false,false,false,false,false,true"));

    // Not featured this week: no multiplier.
    C().now += 7 * 86400;
    Rebuild();
    CHECK(Eq(Run(L, "local t = C_Challenge.GetChallengeInfoByLevel(1, 1) r = seq(t.Rewards, 'ItemAmount') .. tostring(t.IsFeatured)"), "1,1,5,1,4,3false"));
    Done();
}

// The summed modifiers (an unknown type counts as NONE), requirements sorted by +0x20, spells listed unless both are
// hidden or unknown, and the rule strings.
COA_TEST(Challenge_LevelFields) {
    lua_State* L = Setup();
    ChallengeRow a;
    a.id = 1;
    a.trial = 1;
    AddChallenge(a);
    AddGroup(1, 1, 50);
    AddLevel(1, 1, 0, 1);
    AddModifier(1, 1, 0, 1, "CHALLENGE_MODIFIERS_TYPE_MOD_HEALING_DONE_PCT", 1, 2, 3);
    AddModifier(2, 0, 50, 1, "CHALLENGE_MODIFIERS_TYPE_MOD_HEALING_DONE_PCT", 10, 20, 30);
    AddModifier(3, 1, 0, 1, "BOGUS", 2, 0, 0);
    AddModifier(4, 1, 0, 1, "CHALLENGE_MODIFIERS_TYPE_NONE", 0.5f, 0, 0);
    AddRequirement(1, 1, 0, 1, "CHALLENGE_REQUIREMENT_TYPE_DUO", 4, 9);
    AddRequirement(2, 1, 0, 1, "CHALLENGE_REQUIREMENT_TYPE_TRIO", 1, 2);
    AddRequirement(3, 0, 50, 1, "CHALLENGE_REQUIREMENT_TYPE_COMPLETE_WITHOUT_DEATH", 7, 5);
    AddSpell(1, 1, 0, 1, 10, 11, 1, 0, 1);   // 10 hidden, 11 shown
    AddSpell(2, 1, 0, 1, 12, 13);            // both hidden
    AddSpell(3, 1, 0, 1, 14, 0, 0, 1, 0);    // 0 unknown, 14 shown
    AddSpell(4, 1, 0, 1, 15, 16);            // both unknown
    C().spellAttributes = { { 10, 0x80 }, { 11, 0x40 }, { 12, 0x81 }, { 13, 0xFF }, { 14, 0 } };
    Load();

    CHECK(Eq(Run(L, "r = j(C_Challenge.GetChallengeInfoByLevel(1, 1).Modifiers)"),
        "{CHALLENGE_MODIFIERS_TYPE_MOD_HEALING_DONE_PCT={1=11,2=22,3=33},CHALLENGE_MODIFIERS_TYPE_NONE={1=2.5,2=0,3=0}}"));
    CHECK(Eq(Run(L, "local t = C_Challenge.GetChallengeInfoByLevel(1, 1).Requirements r = seq(t, 'RequirementType') .. '/' .. seq(t, 'MiscValue1') .. "
                    "seq(t, 'MiscValue2') .. seq(t, 'MiscValue3')"),
        "CHALLENGE_REQUIREMENT_TYPE_TRIO,CHALLENGE_REQUIREMENT_TYPE_COMPLETE_WITHOUT_DEATH,CHALLENGE_REQUIREMENT_TYPE_DUO/1,7,42,8,53,9,6"));
    CHECK(Eq(Run(L, "r = j(C_Challenge.GetChallengeInfoByLevel(1, 1).Spells)"),
        "{1={IsDungeonBossSpell=false,IsDungeonMinionSpell=true,IsDungeonPlayerSpell=true,PvESpellID=10,PvPSpellID=11},"
        "2={IsDungeonBossSpell=true,IsDungeonMinionSpell=false,IsDungeonPlayerSpell=false,PvESpellID=14,PvPSpellID=0}}"));
    Done();
}

// Every condition type and its negation (FUN_101b7900), evaluated live; unmet conditions sort first.
COA_TEST(Challenge_Conditions) {
    lua_State* L = Setup();
    ChallengeRow a;
    a.id = 1;
    a.trial = 1;
    AddChallenge(a);
    a.trial = 0;
    a.id = 2;
    AddChallenge(a);
    a.id = 3;
    AddChallenge(a);
    AddGroup(1, 2, 50);
    AddGroup(2, 3, 50);
    const struct {
        const char* type;
        float v1;
        float v2;
        float v3;
    } kCases[] = {
        { "CHALLENGE_CONDITIONS_TYPE_AUCTIONHOUSE_INTERACTION", 0, 0, 0 },           // L1 / L2
        { "CHALLENGE_CONDITIONS_TYPE_LEVEL_UP", 0, 0, 0 },                           // L3 / L4
        { "CHALLENGE_CONDITIONS_TYPE_WITHDRAW_BANK_MONEY_OR_ITEM", 0, 0, 0 },        // L5 / L6
        { "CHALLENGE_CONDITIONS_TYPE_WITHDRAW_GUILD_BANK_MONEY_OR_ITEM", 0, 0, 0 },  // L7 / L8
        { "CHALLENGE_CONDITIONS_TYPE_WITHDRAW_REALM_BANK_MONEY_OR_ITEM", 0, 0, 0 },  // L9 / L10
        { "CHALLENGE_CONDITIONS_TYPE_TAKE_MAIL_MONEY_OR_ITEM", 0, 0, 0 },            // L11 / L12
        { "CHALLENGE_CONDITIONS_TYPE_ACCEPT_TRADE", 0, 0, 0 },                       // L13 / L14
        { "CHALLENGE_CONDITIONS_TYPE_VENDOR_INTERACTION", 0, 0, 0 },                 // L15 / L16
        { "CHALLENGE_CONDITIONS_TYPE_LOOT_INTERACTION", 0, 0, 0 },                   // L17 / L18
        { "CHALLENGE_CONDITIONS_TYPE_COMPLETE_CHALLENGE", 2, 1, 0 },                 // L19 / L20
        { "CHALLENGE_CONDITIONS_TYPE_COMPLETE_CHALLENGE_GROUP_COUNT", 50, 1, 2 },    // L21 / L22
        { "CHALLENGE_CONDITIONS_TYPE_PRESTIGE", 3, 0, 0 },                           // L23 / L24
        { "CHALLENGE_CONDITIONS_TYPE_GROUP_SIZE", 1, 0, 0 },                         // L25 / L26
        { "CHALLENGE_CONDITIONS_TYPE_GROUP_SIZE", 3, 0, 0 },                         // L27 / L28
        { "CHALLENGE_CONDITIONS_TYPE_OUTSIDE_INTERACTION", 0, 0, 0 },                // L29 / L30
        { "CHALLENGE_CONDITIONS_TYPE_HAVE_FREE_INVENTORY_SLOTS", 0, 0, 0 },          // L31 / L32
        { "BOGUS", 0, 0, 0 },                                                        // L33 / L34
        { "CHALLENGE_CONDITIONS_TYPE_COMPLETE_CHALLENGE_GROUP_COUNT", 0, 1, 3 },     // L35 / L36: group 0 = every challenge
    };
    uint32_t level = 0;

    for (const auto& k : kCases) {
        for (uint32_t neg = 0; neg < 2; ++neg) {
            ++level;
            AddLevel(level, 1, 0, level);
            AddCondition(level, 1, 0, level, k.type, k.v1, k.v2, k.v3, neg);
        }
    }

    // Level 40: three conditions, sorted unmet first, then by id.
    AddLevel(40, 1, 0, 40);
    AddCondition(50, 1, 0, 40, "CHALLENGE_CONDITIONS_TYPE_PRESTIGE", 0, 0, 0, 0, 7);
    AddCondition(41, 1, 0, 40, "CHALLENGE_CONDITIONS_TYPE_PRESTIGE", 9, 0, 0, 0, 8);
    AddCondition(42, 1, 0, 40, "CHALLENGE_CONDITIONS_TYPE_NONE", 0, 0, 0, 0);
    Load();

    // Both halves of a case as "met,met-negated".
    auto met = [&](uint32_t caseIndex) {
        const uint32_t lvl = caseIndex * 2 + 1;
        return Run(L, "r = tostring(C_Challenge.GetChallengeInfoByLevel(1, " + std::to_string(lvl) + ").Conditions[1].ConditionMet) .. ',' .. "
                      "tostring(C_Challenge.GetChallengeInfoByLevel(1, " + std::to_string(lvl + 1) + ").Conditions[1].ConditionMet)");
    };

    for (uint32_t i : { 0u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 14u }) {
        CHECK(Eq(met(i), "true,false"));
    }

    const uint32_t bits[] = { 0xF, 0, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17 };

    for (uint32_t i : { 0u, 2u, 3u, 4u, 5u, 6u, 7u, 8u }) {
        Mode(1u << bits[i]);
        CHECK(Eq(met(i), "false,true"));
        Mode(0);
    }

    Mode(0x200000);   // a mode in 0x3F8000
    CHECK(Eq(met(14), "false,true"));
    Mode(0);

    // Level up: an active player above level 1, or mode bit 16, answers the negation flag.
    CHECK(Eq(met(1), "false,true"));
    C().level = 1;
    CHECK(Eq(met(1), "true,false"));
    Mode(0x10000);
    CHECK(Eq(met(1), "false,true"));
    Mode(0);
    C().level = 10;
    C().player = false;
    CHECK(Eq(met(1), "true,false"));
    C().player = true;

    // Completion of challenge 2 level 1; two challenges of group 50 at level 1; every challenge (group 0), three needed.
    CHECK(Eq(met(9), "false,true"));
    CHECK(Eq(met(10), "false,true"));
    SetCompleted({ { 2, 1 } });
    CHECK(Eq(met(9), "true,false"));
    CHECK(Eq(met(10), "false,true"));
    SetCompleted({ { 2, 1 }, { 3, 1 } });
    CHECK(Eq(met(10), "true,false"));
    CHECK(Eq(met(17), "false,true"));
    SetCompleted({ { 2, 1 }, { 3, 1 }, { 1, 1 } });
    CHECK(Eq(met(17), "true,false"));

    // Prestige: the player's addon field 2 at least the value.
    CHECK(Eq(met(11), "false,true"));
    CoA::ObjectAddon::Unit(7)[2].value = 3;
    CHECK(Eq(met(11), "true,false"));

    // Group size: alone counts as wanting fewer than two; in a party the size must match.
    CHECK(Eq(met(12), "true,false"));
    CHECK(Eq(met(13), "false,true"));
    C().party = 2;
    CHECK(Eq(met(12), "false,true"));
    CHECK(Eq(met(13), "true,false"));
    C().party = 0;

    // Unhandled and unknown types are met unless negated.
    CHECK(Eq(met(15), "true,false"));
    CHECK(Eq(met(16), "true,false"));

    // Sorted: unmet (41: prestige 9) first, then by id.
    CHECK(Eq(Run(L, "r = seq(C_Challenge.GetChallengeInfoByLevel(1, 40).Conditions, 'ElseGroup') .. '/' .. "
                    "seq(C_Challenge.GetChallengeInfoByLevel(1, 40).Conditions, 'ConditionMet')"),
        "8,0,7/false,true,true"));
    CHECK(Eq(Run(L, "local c = C_Challenge.GetChallengeInfoByLevel(1, 40).Conditions[1] r = c.ConditionType .. c.ConditionValue1 .. c.ConditionValue2 .. "
                    "c.ConditionValue3 .. tostring(c.NegativeCondition)"),
        "CHALLENGE_CONDITIONS_TYPE_PRESTIGE900false"));
    Done();
}

// CanActivateChallenge: every start code in the original's order (FUN_10133dc0), the trial lock and the forced
// challenge after the row checks, and the one-argument form.
COA_TEST(Challenge_CanActivate) {
    lua_State* L = Setup();
    ChallengeRow a;
    a.id = 10;
    a.requiresMode = 1;
    a.mode = "Ironman";
    a.flags = 3;
    a.exclusive = 5;
    a.event = 77;
    a.checkMap = 1;
    a.map = 30;
    a.mapDifficulty = 1;
    AddChallenge(a);
    ChallengeRow b;
    b.id = 11;
    b.exclusive = 5;
    b.trial = 1;
    AddChallenge(b);
    ChallengeRow t;
    t.id = 12;
    t.trial = 1;
    AddChallenge(t);
    ChallengeRow m;
    m.id = 13;
    m.trial = 1;
    m.requiresMode = 1;
    m.mode = "None";
    AddChallenge(m);
    m.id = 14;
    m.mode = "Unknown";
    AddChallenge(m);
    m.id = 15;
    m.requiresMode = 0;
    m.prestige = 1;
    AddChallenge(m);
    AddLevel(6, 15, 0, 1);
    AddLevel(1, 10, 0, 2, 78);
    AddLevel(2, 12, 0, 1);
    AddLevel(3, 13, 0, 1);
    AddLevel(4, 14, 0, 1);
    AddLevel(5, 11, 0, 1);
    AddRule(1, 10, 0, 2, "CHALLENGE_RULES_TYPE_NO_BANK");
    AddCondition(1, 10, 0, 2, "CHALLENGE_CONDITIONS_TYPE_AUCTIONHOUSE_INTERACTION");
    Load();

    CHECK(Eq(Can(L, "C_Challenge.CanActivateChallenge(12, 1)"), "true|nil"));
    CHECK(Eq(Can(L, "C_Challenge.CanActivateChallenge(12)"), "true|nil"));
    CHECK(Eq(Can(L, "C_Challenge.CanActivateChallenge(999, 1)"), "false|CHALLENGE_START_NOT_FOUND"));

    SetActive({ { 10, 2 }, { 11, 1 } });
    Feed(0x59B, CoATest::Bytes().U32(0).U32(1).U32(12).U32(1));   // a trial pending
    C().leader = 99;
    CoA::ObjectAddon::Unit(7)[2].value = 1;
    Mode(0x8000);
    Feed(0x5BA, CoATest::Bytes().U32(1).Name("CHALLENGE_RULES_TYPE_NO_BANK"));
    Feed(0x5B1, CoATest::Bytes().Name("trial-1"));
    Configs({ { "CONFIG_CHALLENGE_FORCED", 5 }, { "CONFIG_CHALLENGE_FORCED_LEVEL", 1 } }, {}, {});
    CHECK(Eq(Can(L, "C_Challenge.CanActivateChallenge(10, 2)"),
        "false|CHALLENGE_START_ALREADY_ACTIVE,CHALLENGE_START_EXCLUSIVE_GROUP_ACTIVE,CHALLENGE_START_GAME_EVENT_NOT_ACTIVE,CHALLENGE_START_DISABLED,"
        "CHALLENGE_START_PREVIOUS_LEVEL_NOT_COMPLETED,CHALLENGE_START_NOT_PARTY_LEADER,CHALLENGE_START_NOT_PRESTIGE,CHALLENGE_START_OUTSIDE_INTERACTION,"
        "CHALLENGE_START_RULE_BROKEN,CHALLENGE_START_WRONG_GAME_MODE,CHALLENGE_START_CONDITIONS_NOT_MET,CHALLENGE_START_PARTICIPATING_IN_TRIAL,"
        "CHALLENGE_START_NOT_TRIAL,CHALLENGE_START_WRONG_MAP_ID,CHALLENGE_START_WRONG_MAP_DIFFICULTY,CHALLENGE_START_BAD_LEVEL,"
        "CHALLENGE_START_PARTICIPATING_IN_COMMUNITY_TRIAL,CHALLENGE_START_FORCED_CHALLENGE"));
    CHECK(Eq(Can(L, "C_Challenge.CanActivateChallenge(999, 1)"),
        "false|CHALLENGE_START_NOT_FOUND,CHALLENGE_START_PARTICIPATING_IN_COMMUNITY_TRIAL,CHALLENGE_START_FORCED_CHALLENGE"));
    // A level without a row: BAD_LEVEL (and no rule / condition checks).
    CHECK(Eq(Can(L, "C_Challenge.CanActivateChallenge(10, 5)"),
        "false|CHALLENGE_START_ALREADY_ACTIVE,CHALLENGE_START_EXCLUSIVE_GROUP_ACTIVE,CHALLENGE_START_GAME_EVENT_NOT_ACTIVE,CHALLENGE_START_DISABLED,"
        "CHALLENGE_START_PREVIOUS_LEVEL_NOT_COMPLETED,CHALLENGE_START_NOT_PARTY_LEADER,CHALLENGE_START_NOT_PRESTIGE,CHALLENGE_START_OUTSIDE_INTERACTION,"
        "CHALLENGE_START_WRONG_GAME_MODE,CHALLENGE_START_PARTICIPATING_IN_TRIAL,CHALLENGE_START_NOT_TRIAL,CHALLENGE_START_WRONG_MAP_ID,"
        "CHALLENGE_START_WRONG_MAP_DIFFICULTY,CHALLENGE_START_BAD_LEVEL,CHALLENGE_START_PARTICIPATING_IN_COMMUNITY_TRIAL,CHALLENGE_START_FORCED_CHALLENGE"));

    // Clear them one by one.
    Feed(0x5B1, CoATest::Bytes().Name(""));
    Configs({ { "CONFIG_CHALLENGE_FORCED", 10 }, { "CONFIG_CHALLENGE_FORCED_LEVEL", 2 } }, {}, {});   // this one: allowed
    SetActive({});
    Feed(0x59B, CoATest::Bytes().U32(0).U32(0));
    C().leader = 7;
    CoA::ObjectAddon::Unit(7)[2].value = 0;
    Mode(2);   // Ironman
    Feed(0x5BA, CoATest::Bytes().U32(0));
    Event(77, true);
    Event(78, true);
    C().map = 30;
    C().difficulty = 1;
    SetCompleted({ { 10, 1 } });
    CHECK(Eq(Can(L, "C_Challenge.CanActivateChallenge(10, 2)"), "false|CHALLENGE_START_DISABLED,CHALLENGE_START_NOT_TRIAL"));
    // Level 1 skips the previous-level check (and has no row here); the forced level is 2.
    SetCompleted({});
    CHECK(Eq(Can(L, "C_Challenge.CanActivateChallenge(10, 1)"),
        "false|CHALLENGE_START_DISABLED,CHALLENGE_START_NOT_TRIAL,CHALLENGE_START_BAD_LEVEL,CHALLENGE_START_FORCED_CHALLENGE"));
    // No party leader at all is fine too.
    C().leader = 0;
    CHECK(Eq(Can(L, "C_Challenge.CanActivateChallenge(10, 2)"),
        "false|CHALLENGE_START_DISABLED,CHALLENGE_START_PREVIOUS_LEVEL_NOT_COMPLETED,CHALLENGE_START_NOT_TRIAL"));
    Configs({ { "CONFIG_CHALLENGE_FORCED", 0 }, { "CONFIG_CHALLENGE_FORCED_LEVEL", 0 } }, {}, {});
    // A prestige challenge ignores prestige and the restricted modes.
    Mode(0x8000);
    CoA::ObjectAddon::Unit(7)[2].value = 1;
    CHECK(Eq(Can(L, "C_Challenge.CanActivateChallenge(12, 1)"), "false|CHALLENGE_START_NOT_PRESTIGE,CHALLENGE_START_OUTSIDE_INTERACTION"));
    CHECK(Eq(Can(L, "C_Challenge.CanActivateChallenge(15, 1)"), "true|nil"));
    Mode(0);
    CoA::ObjectAddon::Unit(7)[2].value = 0;

    // A trial with a normal challenge pending: PARTICIPATING_IN_CHALLENGE; a normal one with a pending trial is code 0xD.
    Feed(0x59B, CoATest::Bytes().U32(0).U32(2).U32(10).U32(1).U32(999).U32(1));
    CHECK(Eq(Can(L, "C_Challenge.CanActivateChallenge(12, 1)"), "false|CHALLENGE_START_PARTICIPATING_IN_CHALLENGE"));
    Feed(0x59B, CoATest::Bytes().U32(0).U32(0));

    // RequiredGameMode "None" / unknown: no restricted mode (0x19EF) may be active.
    CHECK(Eq(Can(L, "C_Challenge.CanActivateChallenge(13, 1)"), "true|nil"));
    Mode(0x10);
    CHECK(Eq(Can(L, "C_Challenge.CanActivateChallenge(14, 1)"), "true|nil"));
    Mode(0x1);
    CHECK(Eq(Can(L, "C_Challenge.CanActivateChallenge(13, 1)"), "false|CHALLENGE_START_WRONG_GAME_MODE"));
    CHECK(Eq(Can(L, "C_Challenge.CanActivateChallenge(14, 1)"), "false|CHALLENGE_START_WRONG_GAME_MODE"));
    Mode(0);

    // Forced challenge: any other id / level is refused; forced id 0 is off.
    Configs({ { "CONFIG_CHALLENGE_FORCED", 10 }, { "CONFIG_CHALLENGE_FORCED_LEVEL", 2 } }, {}, {});
    CHECK(Eq(Can(L, "C_Challenge.CanActivateChallenge(12, 1)"), "false|CHALLENGE_START_FORCED_CHALLENGE"));
    Configs({ { "CONFIG_CHALLENGE_FORCED", 12 }, { "CONFIG_CHALLENGE_FORCED_LEVEL", 1 } }, {}, {});
    CHECK(Eq(Can(L, "C_Challenge.CanActivateChallenge(12, 1)"), "true|nil"));
    Configs({ { "CONFIG_CHALLENGE_FORCED", 0 }, { "CONFIG_CHALLENGE_FORCED_LEVEL", 1 } }, {}, {});
    CHECK(Eq(Can(L, "C_Challenge.CanActivateChallenge(12, 1)"), "true|nil"));
    Configs({ { "CONFIG_CHALLENGE_FORCED", 10 } }, {}, {});   // the level unset: off
    CHECK(Eq(Can(L, "C_Challenge.CanActivateChallenge(12, 1)"), "true|nil"));

    // The exported form with an ignore set.
    SetActive({ { 12, 1 } });
    CHECK(!CoA::Challenge::CanActivate(12, 1, {}));
    CHECK(CoA::Challenge::CanActivate(12, 1, { 3 }));
    Mode(2);
    CHECK(!CoA::Challenge::CanActivate(10, 2, { 6, 0x11 }));   // the previous level
    SetCompleted({ { 10, 1 } });
    CHECK(CoA::Challenge::CanActivate(10, 2, { 6, 0x11 }));
    Done();
}

// CanDeactivateChallenge (FUN_10134a80): not active, a death under permadeath below the level cap, not the leader, the
// trial lock, the forced challenge at its level; CanResurrect.
COA_TEST(Challenge_CanDeactivate) {
    lua_State* L = Setup();
    ChallengeRow a;
    a.id = 10;
    a.trial = 1;
    AddChallenge(a);
    AddLevel(1, 10, 0, 1);
    AddRequirement(1, 10, 0, 1, "CHALLENGE_REQUIREMENT_TYPE_COMPLETE_WITHOUT_DEATH", 2, 0);
    Load();

    CHECK(Eq(Can(L, "C_Challenge.CanDeactivateChallenge(10)"), "false|CHALLENGE_STOP_NOT_ACTIVE"));
    CHECK(Eq(Run(L, "r = tostring(C_Challenge.CanResurrect())"), "true"));
    Feed(0x596, CoATest::Bytes().U32(1).Raw(ActiveRecord(10, 1, 1).data.data(), 0x1A));
    CHECK(Eq(Can(L, "C_Challenge.CanDeactivateChallenge(10)"), "true|nil"));
    Configs({}, { { "CONFIG_CHALLENGE_PERMADEATH_ENABLED", true } }, {});
    CHECK(Eq(Can(L, "C_Challenge.CanDeactivateChallenge(10)"), "false|CHALLENGE_STOP_DEATH_CHALLENGE"));
    CHECK(Eq(Run(L, "r = tostring(C_Challenge.CanResurrect())"), "true"));   // 1 death of 2
    Feed(0x596, CoATest::Bytes().U32(1).Raw(ActiveRecord(10, 1, 2).data.data(), 0x1A));
    CHECK(Eq(Run(L, "r = tostring(C_Challenge.CanResurrect())"), "false"));

    // At the level cap (ruleset 2: 80), on a dev realm at 60 / 70 / 80, or without a player: no permadeath.
    C().level = 80;
    CHECK(Eq(Can(L, "C_Challenge.CanDeactivateChallenge(10)"), "true|nil"));
    C().level = 60;
    CHECK(Eq(Can(L, "C_Challenge.CanDeactivateChallenge(10)"), "false|CHALLENGE_STOP_DEATH_CHALLENGE"));
    Realm(2, 1);
    CHECK(Eq(Can(L, "C_Challenge.CanDeactivateChallenge(10)"), "true|nil"));
    C().level = 61;
    CHECK(Eq(Can(L, "C_Challenge.CanDeactivateChallenge(10)"), "false|CHALLENGE_STOP_DEATH_CHALLENGE"));
    C().player = false;
    CHECK(Eq(Can(L, "C_Challenge.CanDeactivateChallenge(10)"), "true|nil"));
    C().player = true;

    // No deaths: nothing.
    Feed(0x596, CoATest::Bytes().U32(1).Raw(ActiveRecord(10, 1, 0).data.data(), 0x1A));
    CHECK(Eq(Can(L, "C_Challenge.CanDeactivateChallenge(10)"), "true|nil"));
    Feed(0x596, CoATest::Bytes().U32(1).Raw(ActiveRecord(10, 1, 1).data.data(), 0x1A));

    C().leader = 99;
    Feed(0x5B1, CoATest::Bytes().Name("trial-1"));
    Configs({ { "CONFIG_CHALLENGE_FORCED", 10 }, { "CONFIG_CHALLENGE_FORCED_LEVEL", 1 } }, { { "CONFIG_CHALLENGE_PERMADEATH_ENABLED", true } }, {});
    CHECK(Eq(Can(L, "C_Challenge.CanDeactivateChallenge(10)"),
        "false|CHALLENGE_STOP_DEATH_CHALLENGE,CHALLENGE_STOP_NOT_PARTY_LEADER,CHALLENGE_STOP_PARTICIPATING_IN_COMMUNITY_TRIAL,CHALLENGE_STOP_FORCED_CHALLENGE"));
    CHECK(!CoA::Challenge::CanDeactivate(10, { 2, 3, 4 }));
    CHECK(CoA::Challenge::CanDeactivate(10, { 2, 3, 4, 5 }));
    // A forced id that is not active compares its level 0.
    Configs({ { "CONFIG_CHALLENGE_FORCED", 11 }, { "CONFIG_CHALLENGE_FORCED_LEVEL", 0 } }, {}, {});
    CHECK(Eq(Can(L, "C_Challenge.CanDeactivateChallenge(11)"),
        "false|CHALLENGE_STOP_NOT_ACTIVE,CHALLENGE_STOP_NOT_PARTY_LEADER,CHALLENGE_STOP_PARTICIPATING_IN_COMMUNITY_TRIAL,CHALLENGE_STOP_FORCED_CHALLENGE"));
    Configs({ { "CONFIG_CHALLENGE_FORCED", 10 }, { "CONFIG_CHALLENGE_FORCED_LEVEL", 2 } }, {}, {});
    CHECK(Eq(Can(L, "C_Challenge.CanDeactivateChallenge(10)"), "false|CHALLENGE_STOP_NOT_PARTY_LEADER,CHALLENGE_STOP_PARTICIPATING_IN_COMMUNITY_TRIAL"));
    Done();
}

// SMSG 0x596 / 0x5A6 / 0x593 / 0x595: the active list and its events.
COA_TEST(Challenge_ActivePackets) {
    lua_State* L = Setup();
    ChallengeRow a;
    a.id = 10;
    a.trial = 1;
    AddChallenge(a);
    a.id = 11;
    AddChallenge(a);
    AddLevel(1, 10, 0, 1);
    AddLevel(2, 11, 0, 1);
    AddRequirement(1, 10, 0, 1, "CHALLENGE_REQUIREMENT_TYPE_DUO", 9, 0);
    AddRequirement(2, 10, 0, 1, "CHALLENGE_REQUIREMENT_TYPE_COMPLETE_WITHOUT_DEATH", 3, 1);
    Load();

    // The count is trusted only while a 0x1A-byte record is left.
    CoATest::Bytes list;
    list.U32(5).Raw(ActiveRecord(10, 1, 1, 123).data.data(), 0x1A).Raw(ActiveRecord(11, 1).data.data(), 0x1A);
    CHECK(Eq(Join(Feed(0x596, list)), "CHALLENGE_ACTIVE_LIST_CHANGED"));
    CHECK(Eq(Run(L, "local t = C_Challenge.GetActiveChallenges() sort(t, function(a, b) return a.Id < b.Id end) r = #t .. j(t[1])"),
        "2{DeathCount=1,EligibleForCompletion=true,Enabled=true,Id=10,Level=1,StartTime=123}"));

    // A death on a known challenge with a no-death requirement: the deaths left.
    CHECK(Eq(Join(Feed(0x5A6, ActiveRecord(10, 1, 2))), "CHALLENGE_DEATH_UPDATE:10,1,1"));
    CHECK(Eq(Join(Feed(0x5A6, ActiveRecord(10, 1, 5))), "CHALLENGE_DEATH_UPDATE:10,1,0"));
    CHECK(Feed(0x5A6, ActiveRecord(10, 1, 5)).empty());      // unchanged
    CHECK(Feed(0x5A6, ActiveRecord(11, 1, 3)).empty());      // no such requirement
    CHECK(Feed(0x5A6, ActiveRecord(12, 1, 3)).empty());      // not active before
    CHECK(Eq(Run(L, "local t = C_Challenge.GetActiveChallenges() r = #t"), "3"));
    CHECK(Eq(Join(CoATest::Call(L, "C_Challenge", "IsChallengeActive", { "n:10" })), "true|1"));
    CHECK(Eq(Join(CoATest::Call(L, "C_Challenge", "IsChallengeActive", { "n:20" })), "false|0"));

    // Start response: an OK adds the challenge (death count 0); other codes only report.
    CHECK(Eq(Join(Feed(0x593, CoATest::Bytes().U32(20).U32(2).U32(0).U64(555).U8(1))), "CHALLENGE_START_RESPONSE:20,2,CHALLENGE_START_OK"));
    CHECK(Eq(Run(L, "local t = C_Challenge.GetActiveChallenges() for _, a in ipairs(t) do if a.Id == 20 then r = j(a) end end"),
        "{DeathCount=0,EligibleForCompletion=true,Enabled=true,Id=20,Level=2,StartTime=555}"));
    CHECK(Eq(Join(Feed(0x593, CoATest::Bytes().U32(21).U32(1).U32(3).U64(0).U8(0))), "CHALLENGE_START_RESPONSE:21,1,CHALLENGE_START_ALREADY_ACTIVE"));
    CHECK(Eq(Join(Feed(0x593, CoATest::Bytes().U32(21).U32(1).U32(24).U64(0).U8(0))), "CHALLENGE_START_RESPONSE:21,1,UNEXPECTED_ENUM_VALUE_24"));
    CHECK(Eq(Join(CoATest::Call(L, "C_Challenge", "IsChallengeActive", { "n:21" })), "false|0"));
    // No "player" unit: the OK is dropped without an event.
    C().tokenFound = false;
    CHECK(Feed(0x593, CoATest::Bytes().U32(22).U32(1).U32(0).U64(0).U8(0)).empty());
    CHECK(Eq(Join(CoATest::Call(L, "C_Challenge", "IsChallengeActive", { "n:22" })), "false|0"));
    C().tokenFound = true;

    CHECK(Eq(Join(Feed(0x595, CoATest::Bytes().U32(20).U32(2).U32(6))), "CHALLENGE_STOP_RESPONSE:20,2,UNEXPECTED_ENUM_VALUE_6"));
    CHECK(Eq(Join(CoATest::Call(L, "C_Challenge", "IsChallengeActive", { "n:20" })), "true|2"));
    CHECK(Eq(Join(Feed(0x595, CoATest::Bytes().U32(20).U32(2).U32(1))), "CHALLENGE_STOP_RESPONSE:20,2,CHALLENGE_STOP_NOT_ACTIVE"));
    CHECK(Eq(Join(Feed(0x595, CoATest::Bytes().U32(20).U32(2).U32(0))), "CHALLENGE_STOP_RESPONSE:20,2,CHALLENGE_STOP_OK"));
    CHECK(Eq(Join(CoATest::Call(L, "C_Challenge", "IsChallengeActive", { "n:20" })), "false|0"));

    // The list replaces.
    Feed(0x596, CoATest::Bytes().U32(0));
    CHECK(Eq(Run(L, "r = #C_Challenge.GetActiveChallenges()"), "0"));
    Done();
}

// SMSG 0x597 / 0x598 (completed records, a multimap; both schedule a rebuild), 0x599 / 0x59A (criteria and their
// completed / failed events), 0x59B (pending sync).
COA_TEST(Challenge_ProgressPackets) {
    lua_State* L = Setup();
    ChallengeRow a;
    a.id = 10;
    a.trial = 1;
    AddChallenge(a);
    AddLevel(1, 10, 0, 1);
    AddReward(1, 10, 0, 1, "MAX_EXPANSIONS", "CLASS_NONE", 1, 100, 1, 0, 0);
    AddReward(2, 10, 0, 1, "MAX_EXPANSIONS", "CLASS_NONE", 0, 101, 1, 0, 0);
    AddRequirement(5, 10, 0, 1, "CHALLENGE_REQUIREMENT_TYPE_DUO", 4, 0);
    Load();

    CoATest::Bytes done;
    done.U32(3).U32(7).U32(10).U32(1).U64(10).U64(20).U32(7).U32(11).U32(2).U64(30).U64(40);
    CHECK(Eq(Join(Feed(0x597, done)), "CHALLENGE_COMPLETED_LIST_CHANGED"));
    CHECK(Eq(Run(L, "local t = C_Challenge.GetCompletedChallenges() sort(t, function(a, b) return a.Id < b.Id end) r = #t .. j(t[1]) .. j(t[2])"),
        "2{EndTime=20,Id=10,Level=1,StartTime=10}{EndTime=40,Id=11,Level=2,StartTime=30}"));
    // The rebuild a second later swaps in the repeat rewards.
    CHECK(Eq(Run(L, "r = seq(C_Challenge.GetChallengeInfoByLevel(10, 1).Rewards, 'ItemID')"), "100"));
    C().now += 1;
    CoA::Lifecycle::RunAfterIdle();
    CHECK(Eq(Run(L, "r = seq(C_Challenge.GetChallengeInfoByLevel(10, 1).Rewards, 'ItemID')"), "100"));
    C().now += 1;
    CoA::Lifecycle::RunAfterIdle();
    CHECK(Eq(Run(L, "r = seq(C_Challenge.GetChallengeInfoByLevel(10, 1).Rewards, 'ItemID')"), "101"));

    CHECK(Eq(Join(Feed(0x598, CoATest::Bytes().U32(7).U32(10).U32(1).U64(50).U64(60))), "CHALLENGE_COMPLETED:10,1"));
    CHECK(Eq(Run(L, "r = #C_Challenge.GetCompletedChallenges()"), "3"));
    Feed(0x597, CoATest::Bytes().U32(0));
    Rebuild();
    CHECK(Eq(Run(L, "r = seq(C_Challenge.GetChallengeInfoByLevel(10, 1).Rewards, 'ItemID')"), "100"));

    // Criteria.
    CoATest::Bytes crit;
    crit.U32(3).Raw(Criteria(10, 5, 0, true).data.data(), 0x21).Raw(Criteria(10, 6, 9, true).data.data(), 0x21);
    CHECK(Eq(Join(Feed(0x599, crit)), "CHALLENGE_CRITERIA_LIST_CHANGED"));
    CHECK(Eq(Run(L, "local t = C_Challenge.GetChallengeCriterias() sort(t, function(a, b) return a.MiscValue1 > b.MiscValue1 end) r = #t .. j(t[1]) .. j(t[2])"),
        "2{AchieveTime=0,Active=true,Challenge=10,Guid=7,Level=1,MiscValue1=4,MiscValue2=5,MiscValue3=6,RequirementType=CHALLENGE_REQUIREMENT_TYPE_DUO,StartTime=5}"
        "{AchieveTime=9,Active=true,Challenge=10,Guid=7,Level=1,MiscValue1=0,MiscValue2=0,MiscValue3=0,RequirementType=CHALLENGE_REQUIREMENT_TYPE_NONE,StartTime=5}"));
    CHECK(Eq(Run(L, "r = seq(C_Challenge.GetChallengeCriteriaInfo(10, 'CHALLENGE_REQUIREMENT_TYPE_DUO'), 'MiscValue1') .. '/' .. "
                    "#C_Challenge.GetChallengeCriteriaInfo(10, 'CHALLENGE_REQUIREMENT_TYPE_NONE') .. #C_Challenge.GetChallengeCriteriaInfo(11, 'CHALLENGE_REQUIREMENT_TYPE_DUO')"),
        "4/00"));
    CHECK(Eq(Join(Feed(0x59A, Criteria(10, 5, 0, true))), "CHALLENGE_CRITERIA_UPDATED:10,1,CHALLENGE_REQUIREMENT_TYPE_DUO,4,5,6"));
    CHECK(Eq(Join(Feed(0x59A, Criteria(10, 5, 8, true))),
        "CHALLENGE_CRITERIA_UPDATED:10,1,CHALLENGE_REQUIREMENT_TYPE_DUO,4,5,6|CHALLENGE_CRITERIA_COMPLETED:10,1,CHALLENGE_REQUIREMENT_TYPE_DUO,4,5,6"));
    // Achieved and active before; now neither: failed. Still active, or still achieved: just updated.
    CHECK(Eq(Join(Feed(0x59A, Criteria(10, 6, 9, true))), "CHALLENGE_CRITERIA_UPDATED:10,1,CHALLENGE_REQUIREMENT_TYPE_NONE,0,0,0"));
    CHECK(Eq(Join(Feed(0x59A, Criteria(10, 6, 0, false))),
        "CHALLENGE_CRITERIA_UPDATED:10,1,CHALLENGE_REQUIREMENT_TYPE_NONE,0,0,0|CHALLENGE_CRITERIA_FAILED:10,1,CHALLENGE_REQUIREMENT_TYPE_NONE,0,0,0"));
    // Now achieve 0: the next achieved update completes it.
    CHECK(Eq(Join(Feed(0x59A, Criteria(10, 6, 3, false))),
        "CHALLENGE_CRITERIA_UPDATED:10,1,CHALLENGE_REQUIREMENT_TYPE_NONE,0,0,0|CHALLENGE_CRITERIA_COMPLETED:10,1,CHALLENGE_REQUIREMENT_TYPE_NONE,0,0,0"));
    // Achieved, inactive before: no failure.
    CHECK(Eq(Join(Feed(0x59A, Criteria(10, 6, 0, false))), "CHALLENGE_CRITERIA_UPDATED:10,1,CHALLENGE_REQUIREMENT_TYPE_NONE,0,0,0"));
    CHECK(Eq(Join(Feed(0x59A, Criteria(12, 5, 4, true))), "CHALLENGE_CRITERIA_UPDATED:12,1,CHALLENGE_REQUIREMENT_TYPE_DUO,4,5,6"));
    // Achieved and active before, now achieved but inactive: not failed.
    CHECK(Eq(Join(Feed(0x59A, Criteria(12, 5, 4, false))), "CHALLENGE_CRITERIA_UPDATED:12,1,CHALLENGE_REQUIREMENT_TYPE_DUO,4,5,6"));
    CHECK(Eq(Run(L, "r = #C_Challenge.GetChallengeCriterias()"), "3"));

    // Pending sync.
    CHECK(Eq(Join(Feed(0x59B, CoATest::Bytes().U32(4).U32(3).U32(10).U32(1).U32(11).U32(2))), "CHALLENGE_SYNC_REQUEST:4"));
    CHECK(Eq(Run(L, "r = j(C_Challenge.GetPendingChallenges())"), "{1={ChallengeId=10,Level=1},2={ChallengeId=11,Level=2}}"));
    Done();
}

// SMSG 0x5A2 / 0x5A3 (failures; GetChallengeFailure is 0-based, GetChallengeFailures the zone's last 7 days, latest
// first) and 0x5C7 / 0x5C8 (completions, fastest first, grouped by equal time).
COA_TEST(Challenge_HistoryPackets) {
    lua_State* L = Setup();
    const uint64_t now = static_cast<uint64_t>(C().now);
    CoATest::Bytes list;
    list.U32(3).Raw(Failure("Anna", 10, 5, now - 100, 300.0f).data.data(), static_cast<uint32_t>(Failure("Anna", 10, 5, now - 100, 300.0f).data.size()));
    list.Raw(Failure("Bob", 11, 5, now - 8 * 86400, 0).data.data(), static_cast<uint32_t>(Failure("Bob", 11, 5, now - 8 * 86400, 0).data.size()));
    CHECK(Eq(Join(Feed(0x5A2, list)), "CHALLENGE_FAILURE_LIST_CHANGED"));
    CHECK(Eq(Run(L, "r = j(C_Challenge.GetChallengeFailure(0))"),
        "{AreaId=77,Challenge=10,DeathCount=2,Difficulty=3,EndTime=" + std::to_string(now - 100) + ",FailReason=Drowned,KillerDamage=12,KillerEntry=13,KillerLevel=14,"
        "KillerName=Murloc,KillerSpell=11,KillerType=Creature,Level=1,MapId=30,Name=Anna,PlayerClass=Warrior,PlayerGender=Male,PlayerLevel=9,PlayerRace=Human,"
        "Ruleset=1,StartTime=" + std::to_string(now - 150) + ",X=3,Y=32,ZoneId=5}"));
    CHECK(Eq(Run(L, "r = tostring(C_Challenge.GetChallengeFailure(2))"), "nil"));
    CHECK(Eq(Join(Feed(0x5A3, Failure("Cleo", 12, 5, now - 10, 0))), "CHALLENGE_FAILURE_ADDED:12,1,2"));
    Feed(0x5A3, Failure("Dan", 13, 6, now - 5, 0));
    CHECK(Eq(Run(L, "r = seq(C_Challenge.GetChallengeFailures(5, 5), 'Name')"), "Cleo,Anna"));
    CHECK(Eq(Run(L, "r = seq(C_Challenge.GetChallengeFailures(5, 1), 'Name')"), "Cleo"));
    CHECK(Eq(Run(L, "r = seq(C_Challenge.GetChallengeFailures(6, 5), 'Name')"), "Dan"));
    CHECK(Eq(Run(L, "r = #C_Challenge.GetChallengeFailures(7, 5)"), "0"));

    // Completions keyed by each record's own challenge / level.
    CoATest::Bytes c;
    c.U32(10).U32(1).U32(4);
    c.Raw(Completion(10, 1, "Slow", 0, 30).data.data(), static_cast<uint32_t>(Completion(10, 1, "Slow", 0, 30).data.size()));
    c.Raw(Completion(10, 1, "Fast", 100, 110).data.data(), static_cast<uint32_t>(Completion(10, 1, "Fast", 100, 110).data.size()));
    c.Raw(Completion(10, 1, "Tied", 5, 15).data.data(), static_cast<uint32_t>(Completion(10, 1, "Tied", 5, 15).data.size()));
    CHECK(Eq(Join(Feed(0x5C7, c)), "CHALLENGE_COMPLETION_LIST_CHANGED:10,1"));
    CHECK(Eq(Run(L, "r = j(C_Challenge.GetChallengeCompletion(10, 1, 0))"),
        "{Challenge=10,CompleteTime=110,Level=1,PlayerClass=Mage,PlayerGender=Female,PlayerName=Fast,PlayerRace=Orc,StartTime=100}"));
    CHECK(Eq(Run(L, "r = C_Challenge.GetChallengeCompletion(10, 1, 2).PlayerName .. tostring(C_Challenge.GetChallengeCompletion(10, 1, 3))"), "Slownil"));
    CHECK(Eq(Run(L, "local t = C_Challenge.GetChallengeCompletions(10, 1, 5) r = #t .. seq(t[1], 'PlayerName') .. '/' .. seq(t[2], 'PlayerName')"), "2Fast,Tied/Slow"));
    CHECK(Eq(Run(L, "local t = C_Challenge.GetChallengeCompletions(10, 1, 1) r = #t"), "1"));
    CHECK(Eq(Run(L, "r = select('#', C_Challenge.GetChallengeCompletions(10, 1, 0))"), "0"));
    CHECK(Eq(Run(L, "r = #C_Challenge.GetChallengeCompletions(10, 2, 3)"), "0"));
    CHECK(Eq(Join(Feed(0x5C8, Completion(10, 1, "Late", 0, 1))), "CHALLENGE_COMPLETION_ADDED:10,1,3"));
    CHECK(Eq(Run(L, "r = C_Challenge.GetChallengeCompletion(10, 1, 0).PlayerName"), "Late"));
    // A list for (10, 1) whose record says (11, 1): (10, 1) is emptied, the record lands at (11, 1).
    CoATest::Bytes other;
    other.U32(10).U32(1).U32(1).Raw(Completion(11, 1, "Elsewhere", 0, 1).data.data(), static_cast<uint32_t>(Completion(11, 1, "Elsewhere", 0, 1).data.size()));
    CHECK(Eq(Join(Feed(0x5C7, other)), "CHALLENGE_COMPLETION_LIST_CHANGED:10,1"));
    CHECK(Eq(Run(L, "r = tostring(C_Challenge.GetChallengeCompletion(10, 1, 0)) .. C_Challenge.GetChallengeCompletion(11, 1, 0).PlayerName"), "nilElsewhere"));
    Done();
}

// SMSG 0x5BA / 0x5BB (broken rules: no event for the list, CHALLENGE_RULE_BROKEN_UPDATE for one; both rebuild), the rule
// queries and the exported rule tests.
COA_TEST(Challenge_Rules) {
    lua_State* L = Setup();
    ChallengeRow a;
    a.id = 10;
    a.trial = 1;
    AddChallenge(a);
    AddLevel(1, 10, 0, 1);
    AddLevel(2, 10, 0, 2);
    AddRule(1, 10, 0, 1, "CHALLENGE_RULES_TYPE_NO_BANK");
    AddRule(2, 10, 0, 2, "CHALLENGE_RULES_TYPE_NO_GROUP");
    AddRule(3, 10, 0, 2, "CHALLENGE_RULES_TYPE_NO_BANK");
    AddRule(4, 10, 0, 2, "SOMETHING_ELSE");
    AddLevel(3, 10, 0, 3);
    AddRule(5, 10, 0, 3, "BOGUS_RULE");
    Load();

    CHECK(Feed(0x5BA, CoATest::Bytes().U32(3).Name("CHALLENGE_RULES_TYPE_NO_BANK").Name("X")).empty());
    CHECK(Eq(Join(CoATest::Call(L, "C_Challenge", "HasBrokenChallengeRule", { "s:X" })), "true"));
    CHECK(Eq(Join(CoATest::Call(L, "C_Challenge", "HasBrokenChallengeRule", { "s:Y" })), "false"));
    CHECK(Eq(Run(L, "r = seq(C_Challenge.GetBrokenChallengeRules())"), "CHALLENGE_RULES_TYPE_NO_BANK,CHALLENGE_RULES_TYPE_NO_BANK"));
    CHECK(Eq(Join(Feed(0x5BB, CoATest::Bytes().Name("Y").U8(1))), "CHALLENGE_RULE_BROKEN_UPDATE:Y,true"));
    CHECK(Eq(Join(Feed(0x5BB, CoATest::Bytes().Name("X").U8(0))), "CHALLENGE_RULE_BROKEN_UPDATE:X,false"));
    CHECK(Eq(Join(CoATest::Call(L, "C_Challenge", "HasBrokenChallengeRule", { "s:X" })), "false"));
    CHECK(Eq(Join(CoATest::Call(L, "C_Challenge", "HasBrokenChallengeRule", { "s:Y" })), "true"));

    CHECK(Eq(Join(CoATest::Call(L, "C_Challenge", "HasChallengeRule", { "s:CHALLENGE_RULES_TYPE_NO_BANK" })), "false"));
    CHECK(!CoA::Challenge::ActiveHasRule(12));
    SetActive({ { 10, 1 } });
    CHECK(Eq(Join(CoATest::Call(L, "C_Challenge", "HasChallengeRule", { "s:CHALLENGE_RULES_TYPE_NO_BANK" })), "true"));
    CHECK(Eq(Join(CoATest::Call(L, "C_Challenge", "HasChallengeRule", { "s:CHALLENGE_RULES_TYPE_NO_GROUP" })), "false"));
    CHECK(CoA::Challenge::ActiveHasRule(12));   // CHALLENGE_RULES_TYPE_NO_BANK
    CHECK(!CoA::Challenge::ActiveHasRule(2));
    CHECK(!CoA::Challenge::ActiveHasRule(0));
    CHECK(CoA::AttachFixes::Dependencies().challengeHasRule == CoA::Challenge::ActiveHasRule);
    SetActive({ { 10, 2 } });
    CHECK(CoA::Challenge::ActiveHasRule(2));
    CHECK(CoA::Challenge::ActiveHasRule(0));   // an unknown rule name counts as NONE
    CHECK(Eq(Join(CoATest::Call(L, "C_Challenge", "HasChallengeRule", { "s:SOMETHING_ELSE" })), "true"));
    CHECK(CoA::Challenge::HasSoloRule(10, 2));
    CHECK(!CoA::Challenge::HasSoloRule(10, 1));
    CHECK(!CoA::Challenge::HasSoloRule(11, 1));
    CHECK(CoA::Challenge::HasSoloRule(10, 3));   // an unknown rule name parses as NO_GROUP there
    Done();
}

// The CMSGs: StartChallenge 0x592, StopChallenge 0x594, QueryChallengeFailures 0x5A1, QueryChallengeCompletions 0x5C6,
// SendChallengeSyncResponse 0x59C.
COA_TEST(Challenge_Cmsgs) {
    lua_State* L = Setup();
    CoATest::RecordPackets();
    CHECK(Eq(Join(CoATest::Call(L, "C_Challenge", "StartChallenge", { "n:10", "n:2" })), "true"));
    CHECK(Eq(Join(CoATest::Call(L, "C_Challenge", "StopChallenge", { "n:10" })), "true"));
    CHECK(Eq(Join(CoATest::Call(L, "C_Challenge", "QueryChallengeFailures")), "true"));
    CHECK(Eq(Join(CoATest::Call(L, "C_Challenge", "QueryChallengeCompletions", { "n:10", "n:3" })), "true"));
    CHECK(Eq(Join(CoATest::Call(L, "C_Challenge", "SendChallengeSyncResponse", { "b:1" })), "true"));
    CHECK(Eq(Join(CoATest::Call(L, "C_Challenge", "SendChallengeSyncResponse", { "b:0" })), "true"));
    CHECK(CoATest::Call(L, "C_Challenge", "StartChallenge", { "n:10" }).empty());
    CHECK(CoATest::Call(L, "C_Challenge", "SendChallengeSyncResponse", { "s:x" }).empty());
    CoATest::StopRecordingPackets();
    const auto& sent = CoATest::SentPackets();
    CHECK(sent.size() == 6);

    if (sent.size() == 6) {
        CHECK(sent[0] == CoATest::Expected(0x592, CoATest::Bytes().U32(10).U32(2)));
        CHECK(sent[1] == CoATest::Expected(0x594, CoATest::Bytes().U32(10)));
        CHECK(sent[2] == CoATest::Expected(0x5A1, CoATest::Bytes()));
        CHECK(sent[3] == CoATest::Expected(0x5C6, CoATest::Bytes().U32(10).U32(3)));
        CHECK(sent[4] == CoATest::Expected(0x59C, CoATest::Bytes().U8(1)));
        CHECK(sent[5] == CoATest::Expected(0x59C, CoATest::Bytes().U8(0)));
    }

    Done();
}

// The idle tick (rebuild a second after it was asked, CHALLENGE_PATCHED two seconds after a patch), world entry (once
// per glue visit), the achievement hooks, and the SMSG_PATCH_CHALLENGE_* side effects.
COA_TEST(Challenge_TickAndPatches) {
    lua_State* L = Setup();
    ChallengeRow a;
    a.id = 10;
    a.trial = 1;
    a.name = "Ten";
    AddChallenge(a);
    AddLevel(1, 10, 0, 1);
    AddGroup(1, 10, 50);
    // Rows past the patched ids (CoA::Dbc::Table::Upsert drops the old last row when a patch extends the id range).
    AddLevel(9, 99, 0, 1);
    AddGroup(20, 99, 99);
    ChallengeRow z;
    z.id = 12;
    z.flags = 1;
    Unload();

    auto idle = [&](int64_t step) {
        C().now += step;
        CoATest::RecordSignals();
        CoA::Lifecycle::RunAfterIdle();
        CoATest::StopRecordingSignals();
        return CoATest::Signals();
    };
    auto known = [&](int id) { return Run(L, "r = tostring(C_Challenge.GetChallengeInfo(" + std::to_string(id) + ") ~= nil)"); };

    // Nothing scheduled: no rebuild.
    idle(10);
    CHECK(Eq(known(10), "false"));
    CoA::ClientHooks::afterEnterWorld.Run();
    idle(1);   // not yet: strictly after the second
    CHECK(Eq(known(10), "false"));
    idle(1);
    CHECK(Eq(known(10), "true"));
    // A second world entry in the same session schedules nothing.
    Tbl("Challenge.dbc").rows.clear();
    Unload();
    CoA::ClientHooks::afterEnterWorld.Run();
    idle(5);
    CHECK(Eq(known(10), "true"));
    // The achievement packets' hooks do.
    CoA::AttachMisc::achievementsChanged.Run();
    idle(2);
    CHECK(Eq(known(10), "false"));
    // After a glue screen, world entry schedules again.
    AddChallenge(a);
    Unload();
    CoA::Lifecycle::RunGlueScreen();
    CoA::ClientHooks::afterEnterWorld.Run();
    idle(2);
    CHECK(Eq(known(10), "true"));
    // The glue screen drops a pending rebuild.
    CoA::Challenge::ScheduleRebuild();
    CoA::Lifecycle::RunGlueScreen();
    Tbl("Challenge.dbc").rows.clear();
    Unload();
    idle(5);
    CHECK(Eq(known(10), "true"));
    AddChallenge(a);
    AddChallenge(z);
    Unload();
    Rebuild();

    // 0x591 (u32-length strings): the row is stored, the list reset, a rebuild and CHALLENGE_PATCHED scheduled.
    CoATest::Bytes p;
    std::vector<uint32_t> row(21, 0);
    row[0] = 11;
    row[12] = 1;
    p.Raw(row.data(), 0x54);
    p.Name("Ironman").Name("Eleven").Name("desc").Name("icon").Name("CHALLENGE_DIFFICULTY_NORMAL").Name("art");
    CoATest::Call(L, "C_Challenge", "SetChallengeFilter", { "s:" });   // no table: rejected
    CHECK(Eq(Join(CoATest::Call(L, "C_Challenge", "GetNumChallenges")), "0"));
    luaL_dostring(L, "C_Challenge.SetChallengeFilter('', {})");
    CHECK(Eq(Join(CoATest::Call(L, "C_Challenge", "GetNumChallenges")), "1"));
    CHECK(Feed(0x591, p).empty());
    CHECK(Eq(Join(CoATest::Call(L, "C_Challenge", "GetNumChallenges")), "0"));
    CHECK(Eq(known(11), "false"));
    CHECK(idle(1).empty());
    CHECK(Eq(known(11), "false"));
    CHECK(Eq(Join(idle(1)), ""));   // rebuild (now > +1) but CHALLENGE_PATCHED waits for +2
    CHECK(Eq(known(11), "true"));
    CHECK(Eq(Join(idle(1)), "CHALLENGE_PATCHED"));
    CHECK(idle(5).empty());
    CHECK(Eq(Run(L, "local t = C_Challenge.GetChallengeInfo(11) r = t.Name .. t.Description .. t.Icon .. t.DifficultyRating .. t.Artwork .. t.RequiredGameMode"),
        "Elevendescicon" "CHALLENGE_DIFFICULTY_NORMALart2"));

    // 0x5B5 (a group row): marks its challenge.
    CHECK(Feed(0x5B5, CoATest::Bytes().U32(9).U32(11).U32(50)).empty());
    idle(3);
    CHECK(Eq(Run(L, "r = seq(C_Challenge.GetChallengesWithGroupID(50))"), "10,11"));
    // 0x5B2 keyed by group 50 marks every member; keyed by nothing marks nothing (but rebuilds).
    CoATest::Bytes cond;
    std::vector<uint32_t> crow = { 7, 0, 50, 1, 0, 0, 0, 0, 0, 0 };
    cond.Raw(crow.data(), 0x28).Name("CHALLENGE_CONDITIONS_TYPE_PRESTIGE");
    Feed(0x5B2, cond);
    CHECK(Eq(Join(idle(3)), "CHALLENGE_PATCHED"));
    CHECK(Eq(Run(L, "r = C_Challenge.GetChallengeInfoByLevel(10, 1).Conditions[1].ConditionType"), "CHALLENGE_CONDITIONS_TYPE_PRESTIGE"));
    CoATest::Bytes none;
    std::vector<uint32_t> nrow = { 8, 0, 0, 1, 0, 0, 0, 0, 0, 0 };
    none.Raw(nrow.data(), 0x28).Name("CHALLENGE_CONDITIONS_TYPE_NONE");
    Feed(0x5B2, none);
    CHECK(idle(3).empty());
    // 0x5B3 (featured): marks its non-zero ids, no rebuild.
    CHECK(Feed(0x5B3, CoATest::Bytes().U32(1).U32(1).U32(0).U32(0).U32(0)).empty());
    CHECK(idle(3).empty());
    Feed(0x5B3, CoATest::Bytes().U32(2).U32(1).U32(0).U32(10).U32(0));
    CHECK(Eq(Join(idle(3)), "CHALLENGE_PATCHED"));
    // 0x5B4 (group reward) marks the group's members.
    CoATest::Bytes gr;
    std::vector<uint32_t> grow = { 1, 50, 1, 1, 0, 0, 300, 2, 0, 0 };
    gr.Raw(grow.data(), 0x28).Name("MAX_EXPANSIONS").Name("CLASS_NONE");
    Feed(0x5B4, gr);
    CHECK(Eq(Join(idle(3)), "CHALLENGE_PATCHED"));
    CHECK(Eq(Run(L, "r = seq(C_Challenge.GetChallengeInfoByLevel(10, 1).Rewards, 'ItemID')"), "300"));
    // 0x663 (modifiers, C strings), 0x677 (levels), 0x5BC (types: no side effects).
    CoATest::Bytes mod;
    std::vector<uint32_t> mrow = { 1, 10, 0, 1, 0, F(1.5f), 0, 0 };
    mod.Raw(mrow.data(), 0x20).Str("CHALLENGE_MODIFIERS_TYPE_MOD_HEALING_TAKEN_PCT");
    Feed(0x663, mod);
    Feed(0x677, CoATest::Bytes().U32(5).U32(10).U32(0).U32(2).U32(0));
    CHECK(Eq(Join(idle(3)), "CHALLENGE_PATCHED"));
    CHECK(Eq(Run(L, "r = j(C_Challenge.GetChallengeInfoByLevel(10, 1).Modifiers) .. tostring(C_Challenge.GetChallengeInfo(10).Levels[2] ~= nil)"),
        "{CHALLENGE_MODIFIERS_TYPE_MOD_HEALING_TAKEN_PCT={1=1.5,2=0,3=0}}true"));
    CoATest::Bytes rt;
    std::vector<uint32_t> rtrow(9, 0);
    rtrow[0] = 3;
    rt.Raw(rtrow.data(), 0x24).Name("CHALLENGE_REQUIREMENT_TYPE_DUO").Name("a").Name("b").Name("c").Name("d").Name("e").Name("f").Name("g");
    Feed(0x5BC, rt);
    CHECK(idle(3).empty());
    CHECK(Eq(Join(CoATest::Call(L, "C_Challenge", "GetRequirementLocalization", { "s:CHALLENGE_REQUIREMENT_TYPE_DUO" })), "a|b|c|d|e|f|g"));
    Done();
}

// The ChallengeEntryContainer: what is listed (hidden, check-activation and map-bound rows), its order (active, dungeon
// trial, featured, sort order), the argument groups of SetChallengeFilter, the text match, and the map / difficulty
// repopulate; GetAllChallenges / GetAllTrials.
COA_TEST(Challenge_Container) {
    lua_State* L = Setup();
    auto add = [](uint32_t id, const char* name, uint32_t sort, auto tweak) {
        ChallengeRow c;
        c.id = id;
        c.name = name;
        c.sort = sort;
        tweak(c);
        AddChallenge(c);
        AddLevel(id, id, 0, 1);
    };
    add(1, "Alpha", 3, [](ChallengeRow& c) { c.pve = 1; c.desc = "the First one"; });
    add(2, "Bravo", 1, [](ChallengeRow& c) { c.trial = 1; c.pvp = 1; c.prestige = 1; c.difficulty = "CHALLENGE_DIFFICULTY_HARD"; });
    add(3, "Charlie", 9, [](ChallengeRow& c) { c.dungeon = 1; c.difficulty = "CHALLENGE_DIFFICULTY_IMPOSSIBLE"; });
    add(4, "Delta", 0, [](ChallengeRow& c) { c.flags = 1; c.trial = 1; });
    add(5, "Echo", 0, [](ChallengeRow& c) { c.flags = 4; });
    add(6, "Foxtrot", 4, [](ChallengeRow& c) { c.trial = 1; c.checkMap = 1; c.map = 30; });
    add(7, "Golf", 2, [](ChallengeRow& c) { c.trial = 1; });
    add(8, "Hotel", 0, [](ChallengeRow& c) { c.trial = 1; c.difficulty = "CHALLENGE_DIFFICULTY_VERY_HARD"; });
    add(9, "India", 6, [](ChallengeRow& c) { c.trial = 1; c.flags = 4; });
    AddRule(1, 2, 0, 1, "CHALLENGE_RULES_TYPE_NO_GROUP");
    AddFeatured(1, 1, 7, 0, 0);
    Load();
    SetActive({ { 8, 1 } });

    auto filter = [&](const std::string& text, const std::string& flags) {
        luaL_dostring(L, ("C_Challenge.SetChallengeFilter('" + text + "', {" + flags + "})").c_str());
        return Run(L, "local s = '' for i = 1, C_Challenge.GetNumChallenges() do s = s .. (i > 1 and ',' or '') .. C_Challenge.GetChallengeIDByIndex(i) end r = s");
    };

    CHECK(Eq(filter("", ""), "8,3,7,2,1,9"));
    CHECK(Eq(filter("", "FILTER_TRIAL = true"), "8,3,7,2,9"));
    CHECK(Eq(filter("", "FILTER_TRIAL = 'x'"), "8,3,7,2,1,9"));   // FUN_10131480: a bad value empties the map
    CHECK(Eq(filter("", "FILTER_CHALLENGE = true"), "8,3,1"));
    CHECK(Eq(filter("", "FILTER_TRIAL = true, FILTER_CHALLENGE = true"), "8,3,7,2,1,9"));
    CHECK(Eq(filter("", "FILTER_TRIAL = true, FILTER_PRESTIGE = true"), "8,3,2"));
    CHECK(Eq(filter("", "FILTER_NOT_PRESTIGE = true"), "8,3,7,1,9"));
    CHECK(Eq(filter("", "FILTER_PVE = true"), "8,3,1"));
    CHECK(Eq(filter("", "FILTER_PVP = true, FILTER_PVE = true"), "8,3,2,1"));
    CHECK(Eq(filter("", "FILTER_SOLO = true"), "8,3,2"));
    CHECK(Eq(filter("", "FILTER_GROUP = true"), "8,3,7,1,9"));
    CHECK(Eq(filter("", "FILTER_DIFFICULTY_HARD = true"), "8,3,2"));
    CHECK(Eq(filter("", "FILTER_DIFFICULTY_EASY = true, FILTER_DIFFICULTY_HARD = true"), "8,3,7,2,1,9"));
    CHECK(Eq(filter("", "FILTER_CAN_ACTIVATE = true"), "8,3,7,2,9"));
    CHECK(Eq(filter("", "FILTER_NONE = true"), "8,3"));
    CHECK(Eq(filter("", "FILTER_DUNGEON_TRIAL = true"), "8,3"));
    CHECK(Eq(filter("", "FILTER_TRIAL = false, BOGUS = true"), "8,3,7,2,1,9"));
    CHECK(Eq(filter("ALP", ""), "1"));
    CHECK(Eq(filter("first", ""), "1"));
    CHECK(Eq(filter("o", "FILTER_TRIAL = true"), "8,7,2"));
    CHECK(Eq(filter("zzz", ""), ""));

    CHECK(Eq(filter("", ""), "8,3,7,2,1,9"));
    CHECK(Eq(Run(L, "local t = C_Challenge.GetChallengeAtIndex(3, false) r = t.ID .. t.Name .. tostring(t.IsFeatured)"), "7Golftrue"));
    CHECK(Eq(Run(L, "r = tostring(C_Challenge.GetChallengeAtIndex(7, true))"), "nil"));
    CHECK(Eq(Run(L, "r = select('#', C_Challenge.GetChallengeAtIndex(0, true))"), "0"));
    CHECK(Eq(Run(L, "r = tostring(C_Challenge.GetChallengeIDByIndex(7))"), "nil"));
    CHECK(Eq(Run(L, "r = select('#', C_Challenge.GetChallengeIDByIndex(0))"), "0"));

    // The list is populated once: a new active challenge does not reorder it ...
    SetActive({ { 8, 1 }, { 1, 1 } });
    CHECK(Eq(filter("", ""), "8,3,7,2,1,9"));
    // ... until the map or difficulty changes (the map-bound trial now listed).
    C().map = 30;
    CHECK(Eq(filter("", ""), "8,1,3,7,2,6,9"));
    C().difficulty = 1;
    CHECK(Eq(filter("", ""), "8,1,3,7,2,9"));
    C().difficulty = 0;
    // Map-bound and active: listed anywhere.
    C().map = 0;
    SetActive({ { 6, 1 } });
    CHECK(Eq(filter("", ""), "6,3,7,8,2,1,9"));

    // GetAllChallenges / GetAllTrials: listed rows of each kind, in challenge order.
    CHECK(Eq(Run(L, "r = seq(C_Challenge.GetAllChallenges(), 'ID')"), "1"));
    CHECK(Eq(Run(L, "r = seq(C_Challenge.GetAllTrials(), 'ID')"), "6,3,7,8,2,9"));
    SetActive({});
    CHECK(Eq(Run(L, "r = seq(C_Challenge.GetAllTrials(), 'ID')"), "3,7,8,2,9"));
    Done();
}

// The localization lookups: the type row whose name matches, its strings; nil otherwise.
COA_TEST(Challenge_Localization) {
    lua_State* L = Setup();
    TableData& c = Tbl("ChallengeConditionTypes.dbc");
    c.spec = "ddLLLLddd";
    c.rows.push_back({ 1, c.str("CHALLENGE_CONDITIONS_TYPE_PRESTIGE"), c.str("c1"), c.str("c2"), c.str("c3"), c.str("c4"), c.str("c5"), c.str("c6"), c.str("c7") });
    TableData& q = Tbl("ChallengeRequirementTypes.dbc");
    q.spec = "ddLLddddd";
    q.rows.push_back({ 4, q.str("CHALLENGE_REQUIREMENT_TYPE_DUO"), q.str("q1"), q.str("q2"), q.str("q3"), q.str("q4"), q.str("q5"), q.str("q6"), q.str("q7") });
    TableData& m = Tbl("ChallengeModifierTypes.dbc");
    m.spec = "ddLLd";
    m.rows.push_back({ 2, m.str("CHALLENGE_MODIFIERS_TYPE_NONE"), m.str("m1"), m.str("m2"), m.str("m3") });
    TableData& r = Tbl("ChallengeRuleTypes.dbc");
    r.spec = "ddLL";
    r.rows.push_back({ 3, r.str("CHALLENGE_RULES_TYPE_NO_BANK"), r.str("r1"), r.str("r2") });
    Load();

    CHECK(Eq(Join(CoATest::Call(L, "C_Challenge", "GetConditionLocalization", { "s:CHALLENGE_CONDITIONS_TYPE_PRESTIGE" })), "c1|c2|c3|c4|c5|c6|c7"));
    CHECK(Eq(Join(CoATest::Call(L, "C_Challenge", "GetRequirementLocalization", { "s:CHALLENGE_REQUIREMENT_TYPE_DUO" })), "q1|q2|q3|q4|q5|q6|q7"));
    CHECK(Eq(Join(CoATest::Call(L, "C_Challenge", "GetModifierLocalization", { "s:CHALLENGE_MODIFIERS_TYPE_NONE" })), "m1|m2|m3"));
    CHECK(Eq(Join(CoATest::Call(L, "C_Challenge", "GetRuleLocalization", { "s:CHALLENGE_RULES_TYPE_NO_BANK" })), "r1|r2"));
    CHECK(Eq(Join(CoATest::Call(L, "C_Challenge", "GetRuleLocalization", { "s:CHALLENGE_RULES_TYPE_NO_GROUP" })), "nil"));
    CHECK(CoATest::Call(L, "C_Challenge", "GetRuleLocalization", { "n:1" }).size() == 1);
    Done();
}

// The glue screen (FUN_10135010) clears the server state and the list, not the data stores.
COA_TEST(Challenge_GlueReset) {
    lua_State* L = Setup();
    ChallengeRow a;
    a.id = 10;
    a.trial = 1;
    AddChallenge(a);
    AddLevel(1, 10, 0, 1);
    Load();
    SetActive({ { 10, 1 } });
    SetCompleted({ { 10, 1 } });
    Feed(0x599, CoATest::Bytes().U32(1).Raw(Criteria(10, 5, 0, true).data.data(), 0x21));
    Feed(0x59B, CoATest::Bytes().U32(0).U32(1).U32(10).U32(1));
    Feed(0x5A3, Failure("Anna", 10, 5, 10, 0));
    Feed(0x5C8, Completion(10, 1, "Fast", 0, 1));
    Feed(0x5BA, CoATest::Bytes().U32(1).Name("X"));
    luaL_dostring(L, "C_Challenge.SetChallengeFilter('', {})");
    CHECK(Eq(Join(CoATest::Call(L, "C_Challenge", "GetNumChallenges")), "1"));

    CoA::Lifecycle::RunGlueScreen();
    CHECK(Eq(Run(L, "r = #C_Challenge.GetActiveChallenges() .. #C_Challenge.GetCompletedChallenges() .. #C_Challenge.GetChallengeCriterias() .. "
                    "#C_Challenge.GetPendingChallenges() .. tostring(C_Challenge.GetChallengeFailure(0)) .. tostring(C_Challenge.GetChallengeCompletion(10, 1, 0)) .. "
                    "tostring(C_Challenge.HasBrokenChallengeRule('X')) .. C_Challenge.GetNumChallenges()"),
        "0000nilnilfalse0"));
    CHECK(Eq(Run(L, "r = C_Challenge.GetChallengeInfo(10).ID"), "10"));
    Done();
}

// The exported pieces: the row and its difficulty, ActiveIds in the original's (32-bit FNV) hash order.
COA_TEST(Challenge_Exports) {
    Setup();
    ChallengeRow a;
    a.id = 10;
    a.difficulty = "CHALLENGE_DIFFICULTY_VERY_HARD";
    AddChallenge(a);
    a.id = 11;
    a.difficulty = "BOGUS";
    AddChallenge(a);
    Load();
    const uint8_t* row = CoA::Challenge::ChallengeRowOf(10);
    CHECK(row && CoA::Dbc::Table::U32(row, 0) == 10);
    CHECK(row && CoA::Challenge::Difficulty(row) == 3);
    CHECK(CoA::Challenge::Difficulty(CoA::Challenge::ChallengeRowOf(11)) == 0);
    CHECK(CoA::Challenge::ChallengeRowOf(12) == nullptr);

    // Ids whose order differs between the 32-bit and the 64-bit std::hash: the list follows the 32-bit one. (The two
    // hashes agree below bit 5, so the order only differs once more than 8 entries rehash the map to 64 buckets.)
    auto order = [](const std::vector<uint32_t>& ids, bool narrow) {
        std::vector<uint32_t> out;

        if (narrow) {
            std::unordered_map<uint32_t, int, CoA::Challenge::detail::Fnv32> m;

            for (uint32_t id : ids) {
                m[id] = 0;
            }

            for (auto& kv : m) {
                out.push_back(kv.first);
            }
        } else {
            std::unordered_map<uint32_t, int> m;

            for (uint32_t id : ids) {
                m[id] = 0;
            }

            for (auto& kv : m) {
                out.push_back(kv.first);
            }
        }

        return out;
    };

    std::vector<uint32_t> ids;

    for (uint32_t seed = 1; seed < 500; ++seed) {
        std::vector<uint32_t> candidate;

        for (uint32_t i = 1; i <= 12; ++i) {
            candidate.push_back(seed * 7919u + i * 104729u);
        }

        if (order(candidate, true) != order(candidate, false)) {
            ids = candidate;
            break;
        }
    }

    CHECK(!ids.empty());
    std::vector<std::pair<uint32_t, uint32_t>> list;

    for (uint32_t id : ids) {
        list.emplace_back(id, 1);
    }

    SetActive(list);
    CHECK(CoA::Challenge::ActiveIds() == order(ids, true));
    CHECK(CoA::Challenge::ActiveIds() != order(ids, false));
    CHECK(CoA::Challenge::detail::Fnv32()(0) == 0x4B95F515u);
    CHECK(CoA::Challenge::detail::Fnv32()(0x12345678) == 0xA3649785u);
    Done();
}
