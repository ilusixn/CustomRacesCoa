#include "CoA/Source/Modules/GameMode.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Modules/RealmInfo.h"
#include "CoA/Source/Modules/Tutorial.h"
#include "CoA/Source/Runtime/ClientHooks.h"
#include "CoA/Source/Runtime/Dbc.h"
#include "CoA/Source/Runtime/Lifecycle.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Tests/Support.h"
#include "CoA/Tests/TestMain.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <cstring>
#include <set>
#include <string>
#include <vector>

namespace {
    using V = std::vector<std::string>;

    struct Strings {
        std::string block;

        uint32_t operator()(const char* s) {
            const uint32_t at = static_cast<uint32_t>(this->block.size()) + 1;
            this->block.append(s);
            this->block.push_back('\0');
            return at;
        }
    };

    // A file row from the struct dwords: an 'L' column becomes 16 locale slots and a flags dword.
    std::vector<uint32_t> Expand(const char* spec, const std::vector<uint32_t>& dwords) {
        std::vector<uint32_t> out;

        for (size_t i = 0; spec[i]; ++i) {
            const uint32_t v = i < dwords.size() ? dwords[i] : 0;

            if (spec[i] == 'L') {
                out.insert(out.end(), 16, v);
                out.push_back(0);
            } else {
                out.push_back(v);
            }
        }

        return out;
    }

    const char* const kTutorialSpec = "dddddddddddddddddddddddddddddddddddddLLLddddd";
    const char* const kKeywordSpec = "ddddddLLLdddddddd";

    // A tutorial row (struct dwords, 45): id, category, completion achievement, start quest, the realm flag 0 set.
    std::vector<uint32_t> TutorialRow(uint32_t id, uint32_t cat, uint32_t done, uint32_t quest, uint32_t name, uint32_t desc) {
        std::vector<uint32_t> r(45, 0);
        r[0] = id;
        r[1] = cat;
        r[2] = done;
        r[3] = quest;
        r[0x84 / 4] = 3;      // any expansion
        r[0x94 / 4] = name;
        r[0x98 / 4] = desc;
        r[0xA0 / 4] = 1;      // realm flag 0
        return r;
    }

    std::vector<std::vector<uint32_t>> s_tutorials;
    std::vector<std::vector<uint32_t>> s_keywords;
    Strings s_tutStrings;
    Strings s_kwStrings;

    bool s_player = true;
    uint8_t s_race = 1;
    uint8_t s_class = 1;
    uint32_t s_level = 10;
    std::set<uint32_t> s_spells;
    std::set<uint32_t> s_achievements;
    bool s_show = true;

    bool ReadTables(const char* path, std::vector<uint8_t>& out) {
        auto is = [&](const char* name) { return strstr(path, name) != nullptr; };

        if (is("Tutorial.dbc")) {
            std::vector<std::vector<uint32_t>> rows;

            for (const auto& r : s_tutorials) {
                rows.push_back(Expand(kTutorialSpec, r));
            }

            out = CoATest::Wdbc(rows, s_tutStrings.block, static_cast<uint32_t>(strlen(kTutorialSpec)));
            return true;
        }

        if (is("TutorialKeywords.dbc")) {
            std::vector<std::vector<uint32_t>> rows;

            for (const auto& r : s_keywords) {
                rows.push_back(Expand(kKeywordSpec, r));
            }

            out = CoATest::Wdbc(rows, s_kwStrings.block, static_cast<uint32_t>(strlen(kKeywordSpec)));
            return true;
        }

        if (is("TutorialCategories.dbc")) {
            out = CoATest::Wdbc({ Expand("dL", { 1, 1 }), Expand("dL", { 2, 8 }) }, std::string("Basics") + '\0' + "Combat", 2);
            return true;
        }

        if (is("TutorialRewards.dbc")) {
            out = CoATest::Wdbc({ { 1, 1, 500, 2 }, { 2, 1, 500, 3 }, { 3, 1, 0, 9 } }, "");
            return true;
        }

        if (is("MentorSpecializations.dbc")) {
            out = CoATest::Wdbc({ Expand("ddL", { 4, 0, 1 }) }, "Tank", 3);
            return true;
        }

        return false;
    }

    const char* const kTables[] = { "DBFilesClient\\Tutorial.dbc", "DBFilesClient\\TutorialCategories.dbc", "DBFilesClient\\TutorialKeywords.dbc",
        "DBFilesClient\\TutorialObjectives.dbc", "DBFilesClient\\TutorialObjectiveTypes.dbc", "DBFilesClient\\TutorialRewards.dbc",
        "DBFilesClient/MentorSpecializations.dbc" };

    void Unload() {
        for (const char* t : kTables) {
            CoA::Dbc::Get(t).Unload();
        }
    }

    void Realm() {
        CoATest::Bytes b;
        b.U32(1).U32(0).F32(0).F32(0).F32(0).U32(0).F32(0).F32(0).U32(0);
        b.U8(1).U8(0).U8(0).U8(0).U8(0).U8(0).U8(0).U8(0);
        b.Str("").Str("Realm").U8(0);
        CoATest::Deliver(CoA::RealmInfo::OnRealmInfo, 0x9BC, b);
    }

    // Tutorials: 1 (category 1, done by achievement 100, quest 7), 2 (needs 1 first), 3 (level 20+), 4 (class mask past
    // 32 bits), 5 (spell 900). Keywords: 1 "Rage", 2 "Rage Bar" (bracketed), 3 "Fury" (class 2 only).
    lua_State* Setup() {
        s_tutStrings = Strings();
        s_kwStrings = Strings();
        s_tutorials.clear();
        s_keywords.clear();
        const uint32_t t1 = s_tutStrings("First Steps");
        const uint32_t d1 = s_tutStrings("Use Rage wisely.");
        s_tutorials.push_back(TutorialRow(1, 1, 100, 7, t1, d1));
        s_tutorials.back()[0x88 / 4] = 1;   // required for mentor status
        s_tutorials.back()[0x54 / 4] = 5;
        s_tutorials.push_back(TutorialRow(2, 1, 101, 0, s_tutStrings("Second"), 0));
        s_tutorials.back()[0x10 / 4 + 3] = 1;   // previous: tutorial 1 (in slot 3)
        s_tutorials.back()[0x54 / 4] = 1;
        s_tutorials.push_back(TutorialRow(3, 2, 102, 0, s_tutStrings("Third"), 0));
        s_tutorials.back()[0x48 / 4] = 20;
        s_tutorials.push_back(TutorialRow(4, 2, 0, 0, s_tutStrings("Fourth"), 0));
        s_tutorials.back()[0x44 / 4] = 1;   // class 33 only
        s_tutorials.push_back(TutorialRow(5, 2, 0, 0, s_tutStrings("Fifth"), 0));
        s_tutorials.back()[0x58 / 4 + 4] = 900;

        auto keyword = [](uint32_t id, uint32_t bracket, const char* colour, const char* key, const char* name, const char* info, uint32_t classLo) {
            std::vector<uint32_t> r(17, 0);
            r[0] = id;
            r[1] = bracket;
            r[2] = s_kwStrings(colour);
            r[3] = s_kwStrings(key);
            r[6] = s_kwStrings(name);
            r[7] = s_kwStrings(info);
            r[8] = s_kwStrings("index");
            r[9] = 1;   // realm flag 0 (+0x24)
            r[15] = classLo;
            s_keywords.push_back(r);
        };

        keyword(1, 0, "ffff0000", "RAGE", "Rage", "Rage fuels Rage Bar.", 0);
        keyword(2, 1, "ff00ff00", "RAGEBAR", "Rage Bar", "the bar", 0);
        keyword(3, 0, "ff0000ff", "FURY", "Fury", "furious", 2);

        CoA::LuaReg::Reset();
        CoA::Packets::Clear();
        CoA::Lifecycle::Reset();
        CoA::ClientHooks::Reset();
        CoA::ClientHooks::afterEnterWorld.Clear();
        CoA::RealmInfo::Reset();
        CoA::GameMode::Reset();
        CoA::GameMode::Init();
        CoA::Dbc::SetFileReader(ReadTables);
        Unload();
        Realm();
        s_player = true;
        s_race = 1;
        s_class = 1;
        s_level = 10;
        s_spells.clear();
        s_achievements.clear();
        s_show = true;
        CoA::Tutorial::Reset();
        CoA::Tutorial::Init();
        CoA::Tutorial::Deps& d = CoA::Tutorial::Dependencies();
        d.hasPlayer = [] { return s_player; };
        d.playerRace = [] { return s_race; };
        d.playerClass = [] { return s_class; };
        d.playerLevel = [] { return s_level; };
        d.knowsSpell = [](uint32_t s) { return s_spells.count(s) != 0; };
        d.achievementDone = [](uint32_t id) { return s_achievements.count(id) != 0; };
        d.showTutorials = [] { return s_show; };
        return CoATest::NewState(false);
    }

    void Done() {
        Unload();
        CoA::Dbc::SetFileReader(nullptr);
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

    std::string Avail(lua_State* L, uint32_t id) {
        const V v = CoATest::Call(L, "C_Tutorial", "IsTutorialAvailable", { "n:" + std::to_string(id) });
        return v.size() == 2 ? v[0] + "," + v[1] : "?";
    }
}

// Applicability: not in world, realm flag, race, class (incl. the high mask word), expansion, then level, previous
// tutorials, required spells and game modes; a completed tutorial skips the level checks.
COA_TEST(Tutorial_Applicability) {
    lua_State* L = Setup();
    CHECK(Avail(L, 1) == "true,nil");
    CHECK(Avail(L, 2) == "false,TUTORIAL_APPLICABILITY_PREV_NOT_COMPLETED");
    s_achievements.insert(100);
    CHECK(Avail(L, 2) == "true,nil");
    CHECK(Avail(L, 3) == "false,TUTORIAL_APPLICABILITY_LEVEL_TOO_LOW");
    s_achievements.insert(102);   // completed: the level checks are skipped
    CHECK(Avail(L, 3) == "true,nil");
    s_achievements.erase(102);
    s_level = 20;
    CHECK(Avail(L, 3) == "true,nil");
    CHECK(Avail(L, 4) == "false,TUTORIAL_APPLICABILITY_BAD_CLASS");
    s_class = 33;
    CHECK(Avail(L, 4) == "true,nil");
    CHECK(Avail(L, 5) == "false,TUTORIAL_APPLICABILITY_REQUIRED_SPELLS");
    s_spells.insert(900);
    CHECK(Avail(L, 5) == "true,nil");
    CHECK(CoATest::Call(L, "C_Tutorial", "IsTutorialAvailable", { "n:99" }) == V{ "false" });
    s_player = false;
    CHECK(Avail(L, 1) == "false,TUTORIAL_APPLICABILITY_NOT_IN_WORLD");
    CHECK(CoATest::Call(L, "C_Tutorial", "DebugTutorialAppliccability", { "n:1" }) == V{ "TUTORIAL_APPLICABILITY_NOT_IN_WORLD" });
    s_player = true;
    s_level = 1;
    CHECK(CoATest::Call(L, "C_Tutorial", "DebugTutorialAppliccability", { "n:3" }) == V{ "TUTORIAL_APPLICABILITY_OK" });
    CoA::RealmInfo::Reset();
    CHECK(Avail(L, 1) == "false,TUTORIAL_APPLICABILITY_NOT_ENABLED_ON_REALM");
    CHECK(CoATest::Call(L, "C_Tutorial", "IsTutorialComplete", { "n:1" }) == V{ "true" });
    CHECK(CoATest::Call(L, "C_Tutorial", "IsTutorialComplete", { "n:4" }) == V{ "true" });   // no completion id
    Done();
}

// FormatKeywords: whole words only, case-insensitive, longest match wins, never inside an existing link, not at the
// very start of the text, class-gated keywords skipped, plain mode spells out name / info / index.
COA_TEST(Tutorial_FormatKeywords) {
    Setup();
    CHECK(CoA::Tutorial::FormatKeywords("Use rage now.", false, 0) == "Use |cffff0000|Hkeyword:1|hRage|h|r now.");
    CHECK(CoA::Tutorial::FormatKeywords("Fill the rage bar!", false, 0) == "Fill the |cff00ff00|Hkeyword:2|h[Rage Bar]|h|r!");
    CHECK(CoA::Tutorial::FormatKeywords("Rage first", false, 0) == "Rage first");
    CHECK(CoA::Tutorial::FormatKeywords("outrage and ragers", false, 0) == "outrage and ragers");
    CHECK(CoA::Tutorial::FormatKeywords("see |Hitem:1|h[the Rage]|h here", false, 0) == "see |Hitem:1|h[the Rage]|h here");
    CHECK(CoA::Tutorial::FormatKeywords("see |Hitem:1|h the Rage |h here", false, 0) == "see |Hitem:1|h the Rage |h here");
    CHECK(CoA::Tutorial::FormatKeywords("a fury b", false, 0) == "a fury b");
    CHECK(CoA::Tutorial::FormatKeywords("a rage b", false, 1) == "a rage b");
    CHECK(CoA::Tutorial::FormatKeywords("a rage b", true, 0) == "a Rage Rage fuels Rage Bar. index b");
    s_class = 2;
    CHECK(CoA::Tutorial::FormatKeywords("a fury b", false, 0) == "a |cff0000ff|Hkeyword:3|hFury|h|r b");
    Done();
}

// The tutorial and keyword containers, their filters, and the keyword getters.
COA_TEST(Tutorial_Containers) {
    lua_State* L = Setup();
    CoATest::Call(L, "C_Tutorial", "SetTutorialFilter", { "n:1", "s:" });
    CHECK(CoATest::Call(L, "C_Tutorial", "GetNumTutorials") == V{ "2" });
    // Sorted by "not applicable", then sort order: 1 is applicable, 2 is not.
    CHECK(Run(L, "local id = C_Tutorial.GetTutorialAtIndex(1) r = id") == "1");
    CHECK(Run(L, "local a, b, c, d, e, f = C_Tutorial.GetTutorialAtIndex(1) r = b .. ',' .. c .. ',' .. f") == "First Steps,7,100");
    CoATest::Call(L, "C_Tutorial", "SetTutorialFilter", { "n:2", "s:fourth" });
    CHECK(CoATest::Call(L, "C_Tutorial", "GetNumTutorials") == V{ "0" });   // class-gated out of the list
    CoATest::Call(L, "C_Tutorial", "SetTutorialFilter", { "n:1", "s:wisely" });
    CHECK(CoATest::Call(L, "C_Tutorial", "GetNumTutorials") == V{ "1" });

    CoATest::Call(L, "C_Tutorial", "SetKeywordFilter", { "s:" });
    CHECK(CoATest::Call(L, "C_Tutorial", "GetNumKeywords") == V{ "2" });
    CHECK(CoATest::Call(L, "C_Tutorial", "GetIndexByKeywordID", { "n:2" }) == V{ "2" });
    CHECK(CoATest::Call(L, "C_Tutorial", "GetIndexByKeywordID", { "n:3" }) == V{ "nil" });
    CHECK(CoATest::Call(L, "C_Tutorial", "GetKeywordID", { "s:RAGEBAR" }) == V{ "2" });
    CHECK(CoATest::Call(L, "C_Tutorial", "GetKeywordID", { "s:FURY" }) == V{ "nil" });
    CHECK((CoATest::Call(L, "C_Tutorial", "GetKeywordInfo", { "n:1" }) ==
        V{ "Rage", "Rage fuels |cff00ff00|Hkeyword:2|h[Rage Bar]|h|r.", "", "" }));
    CHECK(Run(L, "local a, b = C_Tutorial.GetTutorialDisplay(1) r = b") == "Use |cffff0000|Hkeyword:1|hRage|h|r wisely.");
    CHECK(Run(L, "r = #C_Tutorial.GetCategories()") == "2");
    CHECK(CoATest::Call(L, "C_Tutorial", "GetCategoryInfo", { "n:2" }) == V{ "Combat" });
    Done();
}

// Rewards: 0x6B0 / 0x6B1 and the claim events, category progress, AnyUnclaimedRewards (and the showTutorials gate),
// GetRewards sums, the 0x6B2 rewards patch, CollectReward's CMSG 0x6A8 and StartQuest's CMSG 0x189.
COA_TEST(Tutorial_Rewards) {
    lua_State* L = Setup();
    s_achievements.insert(100);
    CHECK(CoATest::Call(L, "C_Tutorial", "CanCollectReward", { "n:1" }) == V{ "true" });
    CHECK(Run(L, "local a, b, c = C_Tutorial.GetCategoryProgress(1) r = a .. b .. c") == "102");
    CHECK(Run(L, "local a, b, c = C_Tutorial.AnyUnclaimedRewards() r = tostring(a) .. b .. c[1]") == "true11");
    s_show = false;
    CHECK(Run(L, "local a, b, c = C_Tutorial.AnyUnclaimedRewards() r = tostring(a) .. tostring(b) .. tostring(c)") == "falsenilnil");
    CHECK(Run(L, "r = C_Tutorial.GetRewards(1)[500]") == "5");
    CHECK(Run(L, "r = C_Tutorial.GetRewards(2)") == "nil");
    CHECK(Feed(0x6B2, CoATest::Bytes().U32(4).U32(2).U32(600).U32(1)) == V{ "TUTORIAL_ENTRY_UPDATE:2" });
    CHECK(Run(L, "r = C_Tutorial.GetRewards(2)[600]") == "1");

    CHECK(Feed(0x6B0, CoATest::Bytes().U32(1).U32(1)).empty());
    CHECK(CoATest::Call(L, "C_Tutorial", "IsRewardCollected", { "n:1" }) == V{ "true" });
    CHECK(CoATest::Call(L, "C_Tutorial", "CanCollectReward", { "n:1" }) == V{ "false" });
    CHECK(Run(L, "local a, b, c = C_Tutorial.GetCategoryProgress(1) r = a .. b .. c") == "112");
    CHECK(Feed(0x6B1, CoATest::Bytes().U32(5)) == V{ "TUTORIAL_ENTRY_UPDATE:5" });
    CHECK(Feed(0x6AF, CoATest::Bytes().Str("CLAIM_OK")) == V{ "CLAIM_TUTORIAL_REWARDS_RESULT:CLAIM_OK" });

    CoATest::RecordPackets();
    CHECK(CoATest::Call(L, "C_Tutorial", "CollectReward", { "n:2" }) == V{ "true" });
    CHECK(CoATest::Call(L, "C_Tutorial", "StartQuest", { "n:7" }) == V{ "true" });
    CHECK(CoATest::Call(L, "C_Tutorial", "StartQuest", { "n:8" }) == V{ "false" });
    CoATest::StopRecordingPackets();
    CHECK(CoATest::SentPackets().size() == 2);
    CHECK(CoATest::SentPackets()[0] == CoATest::Expected(0x6A8, CoATest::Bytes().U32(2)));
    CHECK(CoATest::SentPackets()[1] == CoATest::Expected(0x189, CoATest::Bytes().U64(0).U32(7).U32(0)));
    Done();
}

// Watches: world entry seeds them; a level gain (idle tick) fires ENTRY_ADDED; stock event 0x258 fires ENTRY_COMPLETE
// for a mentor-required tutorial; a reward (0x6B1) re-runs the added watch.
COA_TEST(Tutorial_Watches) {
    lua_State* L = Setup();
    CoA::ClientHooks::afterEnterWorld.Run();
    CoATest::RecordSignals();
    CoA::Lifecycle::RunAfterIdle();   // last level 0 -> 10: tutorial 3 still too low
    CoATest::StopRecordingSignals();
    CHECK(CoATest::Signals().empty());
    s_level = 25;
    CoATest::RecordSignals();
    CoA::Lifecycle::RunAfterIdle();
    CoATest::StopRecordingSignals();
    CHECK(CoATest::Signals() == V{ "TUTORIAL_ENTRY_ADDED:3,2" });

    // Only a level gain re-runs the watch: tutorial 5 becoming applicable at the same level adds nothing.
    s_spells.insert(900);
    CoATest::RecordSignals();
    CoA::Lifecycle::RunAfterIdle();
    CoATest::StopRecordingSignals();
    CHECK(CoATest::Signals().empty());

    // Reseeded at level 25, tutorial 3 is watched too; it completes without the mentor flag, so no event for it.
    CoA::ClientHooks::afterEnterWorld.Run();
    s_achievements.insert(102);
    s_achievements.insert(100);
    CoATest::RecordSignals();
    for (const auto& e : CoA::ClientHooks::stockEvents) {
        if (e.first == 0x258) {
            e.second(L);
        }
    }
    CoATest::StopRecordingSignals();
    CHECK(CoATest::Signals() == V{ "TUTORIAL_ENTRY_COMPLETE:1,1" });

    // Tutorial 2 (needs 1 complete) became applicable: the 0x1C0 stock event adds it.
    CoATest::RecordSignals();
    for (const auto& e : CoA::ClientHooks::stockEvents) {
        if (e.first == 0x1C0) {
            e.second(L);
        }
    }
    CoATest::StopRecordingSignals();
    CHECK(CoATest::Signals() == V{ "TUTORIAL_ENTRY_ADDED:2,1" });
    Done();
}

// Mentors: toggle checks and CMSG 0x6C3, the result packets, spec add / remove (pending spec), the appended spec list,
// whois records and their filter, the spec table getters, and the glue reset.
COA_TEST(Tutorial_Mentors) {
    lua_State* L = Setup();
    CHECK(CoATest::Call(L, "C_Tutorial", "CanToggleMentorStatus", { "b:1" }) == (V{ "false", "ACTIVATE_MENTOR_STATUS_TUTORIALS_NOT_COMPLETED" }));
    CHECK(CoATest::Call(L, "C_Tutorial", "CanToggleMentorStatus", { "b:0" }) == (V{ "false", "DEACTIVATE_MENTOR_STATUS_NOT_ACTIVE" }));
    Feed(0x6B0, CoATest::Bytes().U32(1).U32(1));
    CHECK(CoATest::Call(L, "C_Tutorial", "CanToggleMentorStatus", { "b:1" }) == (V{ "true", "nil" }));
    CHECK(Run(L, "r = #C_Tutorial.GetTutorialsRequiredForMentorStatus()") == "0");
    CHECK(CoATest::Call(L, "C_Tutorial", "IsTutorialRequiredForMentorStatus", { "n:1" }) == V{ "true" });
    CoATest::RecordPackets();
    CHECK(CoATest::Call(L, "C_Tutorial", "ToggleMentorStatus", { "b:1" }) == V{ "true" });
    CHECK(CoATest::Call(L, "C_Tutorial", "ToggleMentorStatus", { "b:0" }) == V{ "false" });
    CoATest::Call(L, "C_Tutorial", "AddMentorSpecialization", { "n:4" });
    CHECK(CoATest::Call(L, "C_Tutorial", "QueryActiveMentors") == V{ "true" });
    CoATest::StopRecordingPackets();
    CHECK(CoATest::SentPackets().size() == 3);
    CHECK(CoATest::SentPackets()[0] == CoATest::Expected(0x6C3, CoATest::Bytes().U8(1)));
    CHECK(CoATest::SentPackets()[1] == CoATest::Expected(0x6C6, CoATest::Bytes().U32(4)));
    CHECK(CoATest::SentPackets()[2] == CoATest::Expected(0x6CB, CoATest::Bytes()));

    CHECK(Feed(0x6C4, CoATest::Bytes().Str("ACTIVATE_MENTOR_STATUS_OK")) == V{ "TOGGLE_MENTOR_STATUS_RESULT:ACTIVATE_MENTOR_STATUS_OK" });
    CHECK(CoATest::Call(L, "C_Tutorial", "IsMentorStatusActive") == V{ "true" });
    CHECK(Feed(0x6C7, CoATest::Bytes().Str("ADD_MENTOR_SPECIALIZATION_OK")) == V{ "ADD_MENTOR_SPECIALIZATION_RESULT:ADD_MENTOR_SPECIALIZATION_OK" });
    CHECK(CoATest::Call(L, "C_Tutorial", "GetMentorSpecializationActive", { "n:4" }) == V{ "true" });
    Feed(0x6CA, CoATest::Bytes().U8(1).U32(1).U32(9));   // appends
    CHECK(CoATest::Call(L, "C_Tutorial", "GetMentorSpecializationActive", { "n:4" }) == V{ "true" });
    CHECK(CoATest::Call(L, "C_Tutorial", "GetMentorSpecializationActive", { "n:9" }) == V{ "true" });
    CoATest::Call(L, "C_Tutorial", "RemoveMentorSpecialization", { "n:4" });
    Feed(0x6C9, CoATest::Bytes().Str("REMOVE_MENTOR_SPECIALIZATION_OK"));
    CHECK(CoATest::Call(L, "C_Tutorial", "GetMentorSpecializationActive", { "n:4" }) == V{ "false" });

    CoATest::Bytes whois;
    whois.Str("QUERY_MENTOR_WHOIS_OK").U32(2);
    whois.Str("Anna").U8(30).Str("x").U32(1).U32(4).Str("y").Str("z");
    whois.Str("Bob").U8(40).Str("x").U32(1).U32(9).Str("y").Str("z");
    CHECK((Feed(0x6CC, whois) == V{ "MENTOR_WHOIS_DATA_CONTAINER_RESET", "QUERY_MENTOR_WHOIS_RESULT:QUERY_MENTOR_WHOIS_OK" }));
    CoATest::Call(L, "C_Tutorial", "SetAvailableMentorFilter", { "s:", "n:9" });
    CHECK(CoATest::Call(L, "C_Tutorial", "GetNumAvailableMentors") == V{ "1" });
    CHECK(Run(L, "local a, b, c, d = C_Tutorial.GetAvailableMentorAtIndex(1) r = a .. b .. d[1]") == "Bob409");
    CHECK(Run(L, "r = C_Tutorial.GetMentorSpecializations()[1]") == "4");
    CHECK(CoATest::Call(L, "C_Tutorial", "GetMentorSpecializationInfo", { "n:4" }) == (V{ "Tank", "nil" }));

    CoA::Lifecycle::RunGlueScreen();
    CHECK(CoATest::Call(L, "C_Tutorial", "IsMentorStatusActive") == V{ "false" });
    CHECK(CoATest::Call(L, "C_Tutorial", "GetMentorSpecializationActive", { "n:9" }) == V{ "false" });
    CHECK(CoATest::Call(L, "C_Tutorial", "IsRewardCollected", { "n:1" }) == V{ "false" });
    Done();
}

// The table patches: 0x6A5 stores the row and resets the tutorial list; 0x6AC resets the keywords.
COA_TEST(Tutorial_Patches) {
    lua_State* L = Setup();
    CoATest::Bytes b;
    std::vector<uint32_t> row(45, 0);
    row[0] = 9;
    row[1] = 1;
    row[0x84 / 4] = 3;
    row[0xA0 / 4] = 1;
    b.Raw(row.data(), 0xB4);
    b.Str("a").Str("b").Str("Ninth").Str("d").Str("e");
    CoATest::Call(L, "C_Tutorial", "SetTutorialFilter", { "n:1", "s:" });
    CHECK(CoATest::Call(L, "C_Tutorial", "GetNumTutorials") == V{ "2" });
    CHECK((Feed(0x6A5, b) == V{ "TUTORIAL_CONTAINER_RESET", "TUTORIAL_ENTRY_UPDATE:9" }));
    CHECK(CoATest::Call(L, "C_Tutorial", "GetNumTutorials") == V{ "0" });
    CHECK(Run(L, "local a, b = C_Tutorial.GetTutorialByID(9) r = b") == "Ninth");
    CoATest::Bytes k;
    std::vector<uint32_t> kw(17, 0);
    kw[0] = 7;
    k.Raw(kw.data(), 0x44);
    k.Str("c").Str("K").Str("s").Str("t").Str("Seven").Str("i").Str("x");
    CHECK((Feed(0x6AC, k) == V{ "TUTORIAL_KEYWORD_CONTAINER_RESET", "TUTORIAL_ENTRY_UPDATE:0" }));
    Done();
}
