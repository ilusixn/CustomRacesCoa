// AscTutorial.cpp (see Tutorial.h).
#include "CoA/Source/Modules/Tutorial.h"
#include "CoA/Source/Modules/GameMode.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Modules/RealmInfo.h"
#include "CoA/Source/Runtime/ClientHooks.h"
#include "CoA/Source/Runtime/Dbc.h"
#include "CoA/Source/Runtime/DbcPatch.h"
#include "CoA/Source/Runtime/Events.h"
#include "CoA/Source/Runtime/FilterContainer.h"
#include "CoA/Source/Runtime/Lifecycle.h"
#include "CoA/Source/Runtime/Log.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Packet.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Source/Runtime/Read.h"
#include "CoA/Source/Runtime/Script.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <algorithm>
#include <cstdio>
#include <set>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

using CoA::Dbc::Table;

namespace {
    CoA::Tutorial::Deps s_deps;

    Table& Tutorials() {
        return CoA::Dbc::Get("DBFilesClient\\Tutorial.dbc");
    }

    Table& Categories() {
        return CoA::Dbc::Get("DBFilesClient\\TutorialCategories.dbc");
    }

    Table& Keywords() {
        return CoA::Dbc::Get("DBFilesClient\\TutorialKeywords.dbc");
    }

    Table& Objectives() {
        return CoA::Dbc::Get("DBFilesClient\\TutorialObjectives.dbc");
    }

    Table& ObjectiveTypes() {
        return CoA::Dbc::Get("DBFilesClient\\TutorialObjectiveTypes.dbc");
    }

    Table& Rewards() {
        return CoA::Dbc::Get("DBFilesClient\\TutorialRewards.dbc");
    }

    Table& MentorSpecs() {
        return CoA::Dbc::Get("DBFilesClient/MentorSpecializations.dbc");
    }

    // ---- the player, as the applicability checks read it ----
    struct PlayerInfo {
        bool present;
        uint8_t race;
        uint8_t cls;
        uint32_t level;
    };

    PlayerInfo Player() {
        PlayerInfo p { false, 0, 0, 0 };

        if (!s_deps.hasPlayer || !s_deps.hasPlayer()) {
            return p;
        }

        p.present = true;
        p.race = s_deps.playerRace ? s_deps.playerRace() : 0;
        p.cls = s_deps.playerClass ? s_deps.playerClass() : 0;
        p.level = s_deps.playerLevel ? s_deps.playerLevel() : 0;
        return p;
    }

    bool ClassInMask(uint8_t cls, uint32_t lo, uint32_t hi) {
        const uint32_t c = static_cast<uint32_t>(cls) - 1;
        const uint32_t bitLo = c < 32 ? (1u << c) : 0;
        const uint32_t bitHi = (c >= 32 && c < 64) ? (1u << (c - 32)) : 0;
        return (bitLo & lo) || (bitHi & hi);
    }

    // The realm-flag gate both applicability checks share: the first set flag whose realm gate is also set passes;
    // none passing fails.
    bool RealmAllowed(const uint8_t* row, uint32_t firstFlagOff) {
        const CoA::RealmInfo::Info& r = CoA::RealmInfo::Get();

        for (int i = 0; i < 5; ++i) {
            if (Table::U32(row, firstFlagOff + i * 4) && r.gates[i]) {
                return true;
            }
        }

        return false;
    }

    bool AchievementDone(uint32_t id) {   // FUN_103087e0
        return s_deps.achievementDone && s_deps.achievementDone(id);
    }

    // FUN_10a18e10
    bool TutorialComplete(uint32_t id) {
        const uint8_t* row = Tutorials().Row(id);

        if (!row) {
            return false;
        }

        const uint32_t done = Table::U32(row, 8);
        return !done || AchievementDone(done);
    }

    // ---- manager state ----
    struct MentorRecord {
        std::string name;
        uint8_t level = 0;
        std::string s1;
        std::vector<uint32_t> specs;
        std::string s2;
        std::string s3;
    };

    std::vector<uint32_t> s_rewarded;       // +0x174
    std::vector<uint32_t> s_activeSpecs;    // +0x180
    uint32_t s_pendingSpec = 0;             // +0x18C
    std::vector<MentorRecord> s_mentors;    // +0x190
    uint8_t s_mentorStatus = 0;             // +0x19C
    uint8_t s_lastLevel = 0;                // +0x19D
    std::set<uint32_t> s_notApplicable;     // +0x1A0
    std::set<uint32_t> s_incomplete;        // +0x1C0

    bool Rewarded(uint32_t id) {
        return std::find(s_rewarded.begin(), s_rewarded.end(), id) != s_rewarded.end();
    }

    // FUN_102182f0: 0 = applicable, otherwise the TUTORIAL_APPLICABILITY_* index. skipProgress stops after the realm /
    // race / class / expansion checks.
    uint32_t Applicability(const uint8_t* row, bool skipProgress) {
        const PlayerInfo p = Player();

        if (!p.present) {
            return 1;
        }

        if (!RealmAllowed(row, 0xA0)) {
            return 2;
        }

        const uint32_t raceMask = Table::U32(row, 0x3C);

        if (raceMask && !(raceMask & (1u << ((p.race - 1u) & 31)))) {
            return 3;
        }

        const uint32_t classLo = Table::U32(row, 0x40);
        const uint32_t classHi = Table::U32(row, 0x44);

        if ((classLo || classHi) && !ClassInMask(p.cls, classLo, classHi)) {
            return 4;
        }

        const uint32_t expansion = Table::U32(row, 0x84);

        if (expansion != 3 && expansion != CoA::RealmInfo::Get().ruleset) {
            return 0xB;
        }

        if (skipProgress) {
            return 0;
        }

        if (!TutorialComplete(Table::U32(row, 0))) {
            const uint32_t minLevel = Table::U32(row, 0x48);
            const uint32_t maxLevel = Table::U32(row, 0x4C);

            if (minLevel && p.level < minLevel) {
                return 5;
            }

            if (maxLevel && maxLevel < p.level) {
                return 6;
            }
        }

        int first = -1;

        for (int i = 0; i < 10; ++i) {
            if (Table::U32(row, 0x10 + i * 4)) {
                first = i;
                break;
            }
        }

        if (first >= 0) {
            bool any = false;

            for (int i = first; i < 10 && !any; ++i) {
                const uint32_t prev = Table::U32(row, 0x10 + i * 4);
                any = prev && TutorialComplete(prev);
            }

            if (!any) {
                return 7;
            }
        }

        first = -1;

        for (int i = 0; i < 9; ++i) {
            if (Table::U32(row, 0x58 + i * 4)) {
                first = i;
                break;
            }
        }

        if (first >= 0) {
            bool any = false;

            for (int i = first; i < 9 && !any; ++i) {
                any = s_deps.knowsSpell && s_deps.knowsSpell(Table::U32(row, 0x58 + i * 4));
            }

            if (!any) {
                return 8;
            }
        }

        const uint32_t require = Table::U32(row, 0x7C);

        if (require && !(CoA::GameMode::ActiveMask() & require)) {
            return 9;
        }

        const uint32_t exclude = Table::U32(row, 0x80);

        if (exclude && (CoA::GameMode::ActiveMask() & exclude)) {
            return 10;
        }

        return 0;
    }

    const char* const kApplicability[] = { "TUTORIAL_APPLICABILITY_OK", "TUTORIAL_APPLICABILITY_NOT_IN_WORLD", "TUTORIAL_APPLICABILITY_NOT_ENABLED_ON_REALM",
        "TUTORIAL_APPLICABILITY_BAD_RACE", "TUTORIAL_APPLICABILITY_BAD_CLASS", "TUTORIAL_APPLICABILITY_LEVEL_TOO_LOW", "TUTORIAL_APPLICABILITY_LEVEL_TOO_HIGH",
        "TUTORIAL_APPLICABILITY_PREV_NOT_COMPLETED", "TUTORIAL_APPLICABILITY_REQUIRED_SPELLS", "TUTORIAL_APPLICABILITY_REQUIRED_GAME_MODES",
        "TUTORIAL_APPLICABILITY_EXCEPT_GAME_MODES", "TUTORIAL_APPLICABILITY_BAD_EXPANSION" };

    // FUN_10a15a90
    std::string ApplicabilityName(uint32_t v) {
        if (v < 12) {
            return kApplicability[v];
        }

        char buf[48];
        snprintf(buf, sizeof(buf), "UNEXPECTED_ENUM_VALUE_%u", v);
        return buf;
    }

    // FUN_10218dc0
    bool KeywordApplicable(const uint8_t* row) {
        const PlayerInfo p = Player();

        if (!p.present || !RealmAllowed(row, 0x24)) {
            return false;
        }

        const uint32_t raceMask = Table::U32(row, 0x38);

        if (raceMask && !(raceMask & (1u << ((p.race - 1u) & 31)))) {
            return false;
        }

        const uint32_t lo = Table::U32(row, 0x3C);
        const uint32_t hi = Table::U32(row, 0x40);
        return !(lo || hi) || ClassInMask(p.cls, lo, hi);
    }

    // ---- the three containers ----
    class TutorialContainer : public CoA::Filter::Container<const uint8_t*> {
    protected:
        std::vector<const uint8_t*> DoPopulate() override {   // FUN_10a1cc10
            std::vector<const uint8_t*> v;
            Table& t = Tutorials();

            for (uint32_t id = t.MinId(); t.Loaded() && id <= t.MaxId(); ++id) {
                if (const uint8_t* row = t.Row(id)) {
                    if (Applicability(row, true) == 0) {
                        v.push_back(row);
                    }
                }
            }

            return v;
        }

        bool TokenMatch(char type, int32_t value, const uint8_t* const& e) override {   // FUN_10a1cf40
            return type != 'c' || Table::I32(e, 4) == value;
        }

        bool TextMatch(const std::string& lower, const uint8_t* const& e) override {   // FUN_10a1cf70
            Table& t = Tutorials();

            if (CoA::Filter::Lower(t.Str(e, 0x94)).find(lower) != std::string::npos) {
                return true;
            }

            if (CoA::Filter::Lower(CoA::Tutorial::FormatKeywords(t.Str(e, 0x98), true, 0)).find(lower) != std::string::npos) {
                return true;
            }

            if (const uint8_t* cat = Categories().Row(Table::U32(e, 4))) {
                if (CoA::Filter::Lower(Categories().Str(cat, 4)).find(lower) != std::string::npos) {
                    return true;
                }
            }

            Table& o = Objectives();

            for (uint32_t id = o.MinId(); o.Loaded() && id <= o.MaxId(); ++id) {
                const uint8_t* obj = o.Row(id);

                if (!obj || Table::U32(obj, 4) != Table::U32(e, 0)) {
                    continue;
                }

                if (CoA::Filter::Lower(o.Str(obj, 0x1C)).find(lower) != std::string::npos) {
                    return true;
                }

                if (const uint8_t* type = ObjectiveTypes().Row(Table::U32(obj, 8))) {
                    if (CoA::Filter::Lower(ObjectiveTypes().Str(type, 8)).find(lower) != std::string::npos) {
                        return true;
                    }
                }
            }

            return false;
        }

        std::string FilterKey(const uint8_t* const& e) override {   // FUN_10a1dbb0
            std::string k;
            CoA::Filter::AppendByte(k, Applicability(e, false) != 0 ? 1 : 0);
            CoA::Filter::AppendU32(k, Table::U32(e, 0x54));
            CoA::Filter::AppendU32(k, Table::U32(e, 0));
            return k;
        }
    };

    class KeywordContainer : public CoA::Filter::Container<const uint8_t*> {
    protected:
        std::vector<const uint8_t*> DoPopulate() override {   // FUN_10a1ccf0
            std::vector<const uint8_t*> v;
            Table& t = Keywords();

            for (uint32_t id = t.MinId(); t.Loaded() && id <= t.MaxId(); ++id) {
                if (const uint8_t* row = t.Row(id)) {
                    if (KeywordApplicable(row)) {
                        v.push_back(row);
                    }
                }
            }

            return v;
        }

        bool TextMatch(const std::string& lower, const uint8_t* const& e) override {   // FUN_10a1d5f0
            if (lower.empty()) {
                return true;
            }

            Table& t = Keywords();
            return CoA::Filter::Lower(t.Str(e, 0x18)).find(lower) != std::string::npos || CoA::Filter::Lower(t.Str(e, 0x1C)).find(lower) != std::string::npos ||
                CoA::Filter::Lower(t.Str(e, 0x20)).find(lower) != std::string::npos;
        }

        std::string PopulateKey(const uint8_t* const& e) override {   // FUN_10a1d980
            std::string k(32, '\0');
            const std::string name = CoA::Filter::Lower(Keywords().Str(e, 0x18));

            for (size_t i = 0; i < name.size() && i < 32; ++i) {
                k[i] = name[i];
            }

            CoA::Filter::AppendU32(k, Table::U32(e, 0));
            return k;
        }
    };

    class MentorContainer : public CoA::Filter::Container<const MentorRecord*> {
    protected:
        std::vector<const MentorRecord*> DoPopulate() override {   // FUN_10a1c960
            std::vector<const MentorRecord*> v;

            for (const MentorRecord& r : s_mentors) {
                v.push_back(&r);
            }

            return v;
        }

        bool TokenMatch(char type, int32_t value, const MentorRecord* const& e) override {   // FUN_10a1cdd0
            if (type != 's' || value == 0) {
                return true;
            }

            return std::find(e->specs.begin(), e->specs.end(), static_cast<uint32_t>(value)) != e->specs.end();
        }

        bool TextMatch(const std::string& lower, const MentorRecord* const& e) override {   // FUN_10a1ce10
            return CoA::Filter::Lower(e->name).find(lower) != std::string::npos;
        }
    };

    TutorialContainer s_tutorials;
    KeywordContainer s_keywords;
    MentorContainer s_mentorContainer;

    // ---- events ----
    // FUN_10a1bfd0: tutorials that have become applicable since the last check -> TUTORIAL_ENTRY_ADDED.
    void RefreshAdded() {
        std::vector<uint32_t> nowApplicable;

        for (uint32_t id : s_notApplicable) {
            if (const uint8_t* row = Tutorials().Row(id)) {
                if (Applicability(row, false) == 0) {
                    CoA::Events::Signal("TUTORIAL_ENTRY_ADDED", "%u%u", Table::U32(row, 0), Table::U32(row, 4));
                    nowApplicable.push_back(Table::U32(row, 0));
                }
            }
        }

        for (uint32_t id : nowApplicable) {
            s_notApplicable.erase(id);
        }
    }

    // handler_0x0258 (stock event 0x258): tutorials whose completion is now done -> TUTORIAL_ENTRY_COMPLETE, and the
    // tutorial list is refiltered with its current filter if any completed.
    void RefreshCompleted(lua_State*) {
        std::vector<uint32_t> done;

        for (uint32_t id : s_incomplete) {
            if (const uint8_t* row = Tutorials().Row(id)) {
                if (TutorialComplete(Table::U32(row, 0))) {
                    done.push_back(Table::U32(row, 0));
                }
            }
        }

        if (done.empty()) {
            return;
        }

        s_tutorials.Reapply();

        for (uint32_t id : done) {
            const uint8_t* row = Tutorials().Row(id);

            if (row && Table::U32(row, 0x88)) {
                CoA::Events::Signal("TUTORIAL_ENTRY_COMPLETE", "%u%u", Table::U32(row, 0), Table::U32(row, 4));
            }

            s_incomplete.erase(id);
        }
    }

    void RefreshAddedOnEvent(lua_State*) {   // handler_0x01c0 (stock event 0x1C0)
        RefreshAdded();
    }

    // 0x10a18e80: after 0x403340, a level gain re-runs the applicability watch.
    void LevelWatch() {
        const PlayerInfo p = Player();

        if (!p.present) {
            return;
        }

        if (s_lastLevel < p.level) {
            RefreshAdded();
            s_lastLevel = static_cast<uint8_t>(p.level);
        }
    }

    // FUN_10a18ed0 (after world entry): seed both watch sets.
    void SeedWatches() {
        s_notApplicable.clear();
        Table& t = Tutorials();

        for (uint32_t id = t.MinId(); t.Loaded() && id <= t.MaxId(); ++id) {
            if (const uint8_t* row = t.Row(id)) {
                if (Applicability(row, false) != 0) {
                    s_notApplicable.insert(id);
                }
            }
        }

        s_incomplete.clear();

        for (uint32_t id = t.MinId(); t.Loaded() && id <= t.MaxId(); ++id) {
            const uint8_t* row = t.Row(id);

            if (!row || Applicability(row, false) != 0) {
                continue;
            }

            if (!TutorialComplete(Table::U32(row, 0))) {
                s_incomplete.insert(id);
            }
        }
    }

    // 0x10a16920 (glue screen)
    void ResetAll() {
        s_tutorials.Reset();
        s_rewarded.clear();
        s_lastLevel = 0;
        s_notApplicable.clear();
        s_incomplete.clear();
        s_activeSpecs.clear();
        s_pendingSpec = 0;
        s_mentors.clear();
        s_mentorContainer.Reset();
        s_mentorStatus = 0;
    }

    // ---- packet handlers ----
    int32_t OnClaimRewardsResult(void*, uint32_t, uint32_t, CDataStore* msg) {   // 0x6AF FUN_10a17910
        CoA::Events::Signal("CLAIM_TUTORIAL_REWARDS_RESULT", "%s", CoA::Read::Str(msg).c_str());
        return 1;
    }

    int32_t OnRewardedList(void*, uint32_t, uint32_t, CDataStore* msg) {   // 0x6B0 handler_0x06b0
        s_rewarded.clear();

        for (uint32_t n = CoA::Read::U32(msg); n && CoA::Read::Remaining(msg) >= 4; --n) {
            s_rewarded.push_back(CoA::Read::U32(msg));
        }

        return 1;
    }

    int32_t OnRewarded(void*, uint32_t, uint32_t, CDataStore* msg) {   // 0x6B1 FUN_10a18aa0
        const uint32_t id = CoA::Read::U32(msg);
        s_rewarded.push_back(id);
        CoA::Events::Signal("TUTORIAL_ENTRY_UPDATE", "%u", id);
        RefreshAdded();
        return 1;
    }

    int32_t OnToggleMentorResult(void*, uint32_t, uint32_t, CDataStore* msg) {   // 0x6C4 FUN_10a18800
        const std::string r = CoA::Read::Str(msg);

        if (r == "ACTIVATE_MENTOR_STATUS_OK") {
            s_mentorStatus = 1;
        } else if (r == "DEACTIVATE_MENTOR_STATUS_OK") {
            s_mentorStatus = 0;
        }

        CoA::Events::Signal("TOGGLE_MENTOR_STATUS_RESULT", "%s", r.c_str());
        return 1;
    }

    int32_t OnAddSpecResult(void*, uint32_t, uint32_t, CDataStore* msg) {   // 0x6C7 FUN_10a17640
        const std::string r = CoA::Read::Str(msg);

        if (r == "ADD_MENTOR_SPECIALIZATION_OK") {
            s_activeSpecs.push_back(s_pendingSpec);
        } else if (r != "ADD_MENTOR_SPECIALIZATION_UNKNOWN") {
            CoA::Log::Printf("Unexpected Value: %s", r.c_str());
        }

        CoA::Events::Signal("ADD_MENTOR_SPECIALIZATION_RESULT", "%s", r.c_str());
        return 1;
    }

    int32_t OnRemoveSpecResult(void*, uint32_t, uint32_t, CDataStore* msg) {   // 0x6C9 FUN_10a184e0
        const std::string r = CoA::Read::Str(msg);

        if (r == "REMOVE_MENTOR_SPECIALIZATION_OK") {
            auto it = std::find(s_activeSpecs.begin(), s_activeSpecs.end(), s_pendingSpec);

            if (it != s_activeSpecs.end()) {
                s_activeSpecs.erase(it);
            }
        } else if (r != "REMOVE_MENTOR_SPECIALIZATION_UNKNOWN") {
            CoA::Log::Printf("Unexpected Value: %s", r.c_str());
        }

        CoA::Events::Signal("REMOVE_MENTOR_SPECIALIZATION_RESULT", "%s", r.c_str());
        return 1;
    }

    // 0x6CA FUN_10a17a70 { u8 status, u32 n, u32 spec[n] } -- appends, as the original does.
    int32_t OnSpecList(void*, uint32_t, uint32_t, CDataStore* msg) {
        s_mentorStatus = CoA::Read::U8(msg);

        for (uint32_t n = CoA::Read::U32(msg); n && CoA::Read::Remaining(msg) >= 4; --n) {
            s_activeSpecs.push_back(CoA::Read::U32(msg));
        }

        return 1;
    }

    // 0x6CC FUN_10a17c00 { str result, [u32 n, n x { str name, u8, str, u32 k, u32 spec[k], str, str }] }
    int32_t OnWhoisResult(void*, uint32_t, uint32_t, CDataStore* msg) {
        s_mentors.clear();
        const std::string r = CoA::Read::Str(msg);

        if (r == "QUERY_MENTOR_WHOIS_OK") {
            for (uint32_t n = CoA::Read::U32(msg); n && CoA::Read::Remaining(msg); --n) {
                MentorRecord m;
                m.name = CoA::Read::Str(msg);
                m.level = CoA::Read::U8(msg);
                m.s1 = CoA::Read::Str(msg);

                for (uint32_t k = CoA::Read::U32(msg); k && CoA::Read::Remaining(msg) >= 4; --k) {
                    m.specs.push_back(CoA::Read::U32(msg));
                }

                m.s2 = CoA::Read::Str(msg);
                m.s3 = CoA::Read::Str(msg);
                s_mentors.push_back(std::move(m));
            }
        }

        s_mentorContainer.Reset();
        CoA::Events::Signal("MENTOR_WHOIS_DATA_CONTAINER_RESET", nullptr);
        CoA::Events::Signal("QUERY_MENTOR_WHOIS_RESULT", "%s", r.c_str());
        return 1;
    }

    // 0x6B2 handler_0x06b2 { u32 id, u32 a, u32 b, u32 c } -> TutorialRewards row upsert, then TUTORIAL_ENTRY_UPDATE for
    // field a (FUN_10a1c850).
    int32_t OnPatchRewards(void*, uint32_t, uint32_t, CDataStore* msg) {
        std::vector<uint8_t> row(16);
        uint32_t f[4];

        for (uint32_t& v : f) {
            v = CoA::Read::U32(msg);
        }

        memcpy(row.data(), f, 16);
        Rewards().Upsert(f[0], row);
        CoA::Events::Signal("TUTORIAL_ENTRY_UPDATE", "%u", f[1]);
        return 1;
    }

    // ---- bindings ----
    // FUN_10a19040
    int PushTutorial(lua_State* L, const uint8_t* e) {
        if (!e) {
            return CoA::Script::PushNils(L, 8);
        }

        Table& t = Tutorials();
        auto strOrNil = [&](uint32_t off) {
            const char* s = t.Str(e, off);

            if (*s) {
                CoA::Script::PushStr(L, s);
            } else {
                lua_pushnil(L);
            }
        };

        CoA::Script::PushInt(L, Table::I32(e, 0));
        CoA::Script::PushStr(L, t.Str(e, 0x94));

        if (Table::U32(e, 0xC)) {
            CoA::Script::PushInt(L, Table::I32(e, 0xC));
        } else {
            lua_pushnil(L);
        }

        CoA::Script::PushInt(L, Table::I32(e, 0x50));
        strOrNil(0x8C);

        if (Table::U32(e, 8)) {
            CoA::Script::PushInt(L, Table::I32(e, 8));
        } else {
            lua_pushnil(L);
        }

        strOrNil(0x90);
        strOrNil(0x9C);
        return 8;
    }

    // FUN_10a19310
    int PushKeyword(lua_State* L, const uint8_t* e, bool forInfo) {
        if (!e) {
            return CoA::Script::PushNils(L, 4);
        }

        Table& t = Keywords();
        const std::string text = CoA::Tutorial::FormatKeywords(t.Str(e, forInfo ? 0x1C : 0x20), false, Table::U32(e, 0));
        CoA::Script::PushStr(L, t.Str(e, 0x18));
        CoA::Script::PushStr(L, text.c_str());
        CoA::Script::PushStr(L, t.Str(e, 0x10));
        CoA::Script::PushStr(L, t.Str(e, 0x14));
        return 4;
    }

    int PushIntTable(lua_State* L, const std::vector<uint32_t>& v) {   // FUN_1009a460
        lua_createtable(L, 0, static_cast<int>(v.size()));
        lua_checkstack(L, 2);

        for (size_t i = 0; i < v.size(); ++i) {
            CoA::Script::PushNum(L, static_cast<double>(i + 1));
            CoA::Script::PushInt(L, static_cast<int32_t>(v[i]));
            lua_settable(L, -3);
        }

        return 1;
    }

    // FUN_10a17510
    void CategoryProgress(uint32_t cat, int& unclaimed, int& claimed, int& total, std::vector<uint32_t>& ids) {
        unclaimed = claimed = total = 0;
        ids.clear();
        Table& t = Tutorials();

        for (uint32_t id = t.MinId(); t.Loaded() && id <= t.MaxId(); ++id) {
            const uint8_t* row = t.Row(id);

            if (!row || Table::U32(row, 4) != cat || Applicability(row, false) != 0) {
                continue;
            }

            if (Rewarded(Table::U32(row, 0))) {
                ++claimed;
            } else if (TutorialComplete(Table::U32(row, 0))) {
                ++unclaimed;
                ids.push_back(Table::U32(row, 0));
            }

            ++total;
        }
    }

    int GetNumTutorials(lua_State* L) {
        CoA::Script::PushInt(L, static_cast<int32_t>(s_tutorials.Size()));
        return 1;
    }

    int GetNumKeywords(lua_State* L) {
        CoA::Script::PushInt(L, static_cast<int32_t>(s_keywords.Size()));
        return 1;
    }

    int GetNumAvailableMentors(lua_State* L) {
        CoA::Script::PushInt(L, static_cast<int32_t>(s_mentorContainer.Size()));
        return 1;
    }

    int IsMentorStatusActive(lua_State* L) {
        CoA::Script::PushBool(L, s_mentorStatus != 0);
        return 1;
    }

    int GetTutorialAtIndex(lua_State* L) {
        uint32_t i;

        if (!CoA::Script::ReadNumber(L, i) || i == 0) {
            return 0;
        }

        return PushTutorial(L, i - 1 < s_tutorials.Size() ? s_tutorials.At(i - 1) : nullptr);
    }

    int GetTutorialByID(lua_State* L) {
        uint32_t id;

        if (!CoA::Script::ReadNumber(L, id)) {
            return 0;
        }

        return PushTutorial(L, Tutorials().Row(id));
    }

    int GetTutorialDisplay(lua_State* L) {   // FUN_10a1af90
        uint32_t id;

        if (!CoA::Script::ReadNumber(L, id)) {
            return 0;
        }

        const uint8_t* e = Tutorials().Row(id);

        if (!e) {
            return CoA::Script::PushNils(L, 3);
        }

        CoA::Script::PushStr(L, Tutorials().Str(e, 0x94));
        CoA::Script::PushStr(L, CoA::Tutorial::FormatKeywords(Tutorials().Str(e, 0x98), false, 0).c_str());

        if (Table::U32(e, 0xC)) {
            CoA::Script::PushInt(L, Table::I32(e, 0xC));
        } else {
            lua_pushnil(L);
        }

        return 3;
    }

    int IsTutorialComplete(lua_State* L) {
        uint32_t id;

        if (!CoA::Script::ReadNumber(L, id)) {
            return 0;
        }

        CoA::Script::PushBool(L, TutorialComplete(id));
        return 1;
    }

    int IsTutorialAvailable(lua_State* L) {   // FUN_10a1b420
        uint32_t id;

        if (!CoA::Script::ReadNumber(L, id)) {
            return 0;
        }

        const uint8_t* e = Tutorials().Row(id);

        if (!e) {
            CoA::Script::PushBool(L, false);
            return 1;
        }

        const uint32_t r = Applicability(e, false);
        CoA::Script::PushBool(L, r == 0);

        if (r) {
            CoA::Script::PushStr(L, ApplicabilityName(r).c_str());
        } else {
            lua_pushnil(L);
        }

        return 2;
    }

    int DebugTutorialAppliccability(lua_State* L) {   // FUN_10a19c50
        uint32_t id;

        if (!CoA::Script::ReadNumber(L, id)) {
            return 0;
        }

        const uint8_t* e = Tutorials().Row(id);

        if (!e) {
            lua_pushnil(L);
        } else {
            CoA::Script::PushStr(L, ApplicabilityName(Applicability(e, true)).c_str());
        }

        return 1;
    }

    int CanCollectReward(lua_State* L) {   // handler_CanCollectReward
        uint32_t id;

        if (!CoA::Script::ReadNumber(L, id)) {
            return 0;
        }

        CoA::Script::PushBool(L, Tutorials().Row(id) && TutorialComplete(id) && !Rewarded(id));
        return 1;
    }

    int IsRewardCollected(lua_State* L) {
        uint32_t id;

        if (!CoA::Script::ReadNumber(L, id)) {
            return 0;
        }

        CoA::Script::PushBool(L, Rewarded(id));
        return 1;
    }

    int CollectReward(lua_State* L) {   // FUN_10a19b50: CMSG 0x6A8 { u32 }
        uint32_t id;

        if (!CoA::Script::ReadNumber(L, id)) {
            return 0;
        }

        CoA::Packet(0x6A8).U32(id).Send();
        CoA::Script::PushBool(L, true);
        return 1;
    }

    int GetCategoryProgress(lua_State* L) {   // FUN_10a1a070
        uint32_t cat;

        if (!CoA::Script::ReadNumber(L, cat)) {
            return 0;
        }

        int unclaimed;
        int claimed;
        int total;
        std::vector<uint32_t> ids;
        CategoryProgress(cat, unclaimed, claimed, total, ids);
        CoA::Script::PushInt(L, unclaimed + claimed);
        CoA::Script::PushInt(L, claimed);
        CoA::Script::PushInt(L, total);
        return 3;
    }

    int IsCategoryEnabled(lua_State* L) {   // FUN_10a1b270
        uint32_t cat;

        if (!CoA::Script::ReadNumber(L, cat)) {
            return 0;
        }

        const PlayerInfo p = Player();

        if (!p.present || p.level < 8) {
            CoA::Script::PushBool(L, false);
            return 1;
        }

        int unclaimed;
        int claimed;
        int total;
        std::vector<uint32_t> ids;
        CategoryProgress(cat, unclaimed, claimed, total, ids);
        CoA::Script::PushBool(L, total != 0);
        return 1;
    }

    int AnyUnclaimedRewards(lua_State* L) {   // FUN_10a19680
        if (!s_deps.showTutorials || s_deps.showTutorials()) {
            Table& c = Categories();

            for (uint32_t cat = c.MinId(); c.Loaded() && cat <= c.MaxId(); ++cat) {
                if (!c.Row(cat)) {
                    continue;
                }

                int unclaimed;
                int claimed;
                int total;
                std::vector<uint32_t> ids;
                CategoryProgress(cat, unclaimed, claimed, total, ids);

                if (unclaimed) {
                    CoA::Script::PushBool(L, true);
                    CoA::Script::PushInt(L, static_cast<int32_t>(cat));
                    return 1 + 1 + PushIntTable(L, ids);
                }
            }
        }

        CoA::Script::PushBool(L, false);
        return 1 + CoA::Script::PushNils(L, 2);
    }

    int GetTutorialsRequiredForMentorStatus(lua_State* L) {   // FUN_10a1b150
        std::vector<uint32_t> ids;
        Table& t = Tutorials();

        for (uint32_t id = t.MinId(); t.Loaded() && id <= t.MaxId(); ++id) {
            if (const uint8_t* row = t.Row(id)) {
                if (Table::U32(row, 0x88) && !Rewarded(Table::U32(row, 0))) {
                    ids.push_back(Table::U32(row, 0));
                }
            }
        }

        return PushIntTable(L, ids);
    }

    int StartQuest(lua_State* L) {   // FUN_10a1bd00: CMSG_QUESTGIVER_ACCEPT_QUEST with no giver
        uint32_t quest;

        if (!CoA::Script::ReadNumber(L, quest)) {
            return 0;
        }

        Table& t = Tutorials();

        for (uint32_t id = t.MinId(); t.Loaded() && id <= t.MaxId(); ++id) {
            const uint8_t* row = t.Row(id);

            if (row && Table::U32(row, 0xC) == quest) {
                CoA::Packet(0x189).U64(0).U32(quest).U32(0).Send();
                CoA::Script::PushBool(L, true);
                return 1;
            }
        }

        CoA::Script::PushBool(L, false);
        return 1;
    }

    int GetObjectives(lua_State* L) {   // FUN_10a1aac0
        uint32_t tut;

        if (!CoA::Script::ReadNumber(L, tut)) {
            return 0;
        }

        std::vector<uint32_t> ids;
        Table& o = Objectives();

        for (uint32_t id = o.MinId(); o.Loaded() && id <= o.MaxId(); ++id) {
            const uint8_t* obj = o.Row(id);

            if (!obj || Table::U32(obj, 4) != tut) {
                continue;
            }

            const uint8_t* row = Tutorials().Row(Table::U32(obj, 4));   // FUN_10219ea0

            if (row && Applicability(row, false) != 0) {
                continue;
            }

            ids.push_back(id);
        }

        std::stable_sort(ids.begin(), ids.end(), [&](uint32_t a, uint32_t b) { return Table::U32(o.Row(a), 0x18) < Table::U32(o.Row(b), 0x18); });
        return PushIntTable(L, ids);
    }

    int GetObjectiveInfo(lua_State* L) {   // FUN_10a1a970
        uint32_t id;

        if (!CoA::Script::ReadNumber(L, id)) {
            return 0;
        }

        const uint8_t* obj = Objectives().Row(id);

        if (!obj) {
            lua_pushnil(L);
        } else {
            CoA::Script::PushStr(L, CoA::Tutorial::FormatKeywords(Objectives().Str(obj, 0x1C), false, 0).c_str());
        }

        return 1;
    }

    int SetTutorialFilter(lua_State* L) {   // FUN_10a1bb50 -> "$c<category> <text>"
        if (!CoA::Script::ValidateInput(L, { LUA_TNUMBER, LUA_TSTRING })) {
            return 0;
        }

        char buf[16];
        snprintf(buf, sizeof(buf), "$c%d ", CoA::Script::ToInt(CoA::Script::CheckNumber(L, 1)));
        s_tutorials.ApplyFilter(buf + std::string(CoA::Script::CheckString(L, 2)));
        return 0;
    }

    int SetKeywordFilter(lua_State* L) {   // FUN_10a1b9c0
        std::string text;

        if (CoA::Script::ReadString(L, text)) {
            s_keywords.ApplyFilter(text);
        }

        return 0;
    }

    int GetKeywordAtIndex(lua_State* L) {   // handler_GetKeywordAtIndex
        uint32_t i;

        if (!CoA::Script::ReadNumber(L, i) || i == 0) {
            return 0;
        }

        return PushKeyword(L, i - 1 < s_keywords.Size() ? s_keywords.At(i - 1) : nullptr, false);
    }

    int GetKeywordInfo(lua_State* L) {   // handler_GetKeywordInfo
        uint32_t id;

        if (!CoA::Script::ReadNumber(L, id)) {
            return 0;
        }

        return PushKeyword(L, Keywords().Row(id), true);
    }

    int GetIndexByKeywordID(lua_State* L) {   // handler_GetIndexByKeywordID
        uint32_t id;

        if (!CoA::Script::ReadNumber(L, id)) {
            return 0;
        }

        for (size_t i = 0; i < s_keywords.Size(); ++i) {
            if (Table::U32(s_keywords.At(i), 0) == id) {
                CoA::Script::PushInt(L, static_cast<int32_t>(i + 1));
                return 1;
            }
        }

        lua_pushnil(L);
        return 1;
    }

    int GetKeywordID(lua_State* L) {   // FUN_10a1a290: exact match on the key column
        std::string key;

        if (!CoA::Script::ReadString(L, key)) {
            return 0;
        }

        Table& t = Keywords();

        for (uint32_t id = t.MinId(); t.Loaded() && id <= t.MaxId(); ++id) {
            const uint8_t* row = t.Row(id);

            if (row && KeywordApplicable(row) && key == t.Str(row, 0xC)) {
                CoA::Script::PushInt(L, Table::I32(row, 0));
                return 1;
            }
        }

        lua_pushnil(L);
        return 1;
    }

    // ---- mentors ----
    // FUN_10a16870: 0 ok, 2 not in world, 3 already active, 4 required tutorials not rewarded.
    uint32_t CanActivateMentor() {
        if (!Player().present) {
            return 2;
        }

        if (s_mentorStatus) {
            return 3;
        }

        Table& t = Tutorials();

        for (uint32_t id = t.MinId(); t.Loaded() && id <= t.MaxId(); ++id) {
            if (const uint8_t* row = t.Row(id)) {
                if (Table::U32(row, 0x88) && !Rewarded(Table::U32(row, 0))) {
                    return 4;
                }
            }
        }

        return 0;
    }

    int CanToggleMentorStatus(lua_State* L) {   // FUN_10a19910
        bool activate;

        if (!CoA::Script::ReadBool(L, activate)) {
            return 0;
        }

        static const char* const kActivate[] = { "ACTIVATE_MENTOR_STATUS_OK", "ACTIVATE_MENTOR_STATUS_UNKNOWN", "ACTIVATE_MENTOR_STATUS_NOT_IN_WORLD",
            "ACTIVATE_MENTOR_STATUS_ALREADY_ACTIVE", "ACTIVATE_MENTOR_STATUS_TUTORIALS_NOT_COMPLETED" };
        static const char* const kDeactivate[] = { "DEACTIVATE_MENTOR_STATUS_OK", "DEACTIVATE_MENTOR_STATUS_UNKNOWN", "DEACTIVATE_MENTOR_STATUS_NOT_IN_WORLD",
            "DEACTIVATE_MENTOR_STATUS_NOT_ACTIVE" };
        std::string reason;

        if (activate) {
            const uint32_t r = CanActivateMentor();

            if (r) {
                reason = r < 5 ? kActivate[r] : "UNEXPECTED_ENUM_VALUE_" + std::to_string(r);
            }
        } else {
            const uint32_t r = !Player().present ? 2 : (s_mentorStatus ? 0 : 3);

            if (r) {
                reason = kDeactivate[r];
            }
        }

        CoA::Script::PushBool(L, reason.empty());

        if (reason.empty()) {
            lua_pushnil(L);
        } else {
            CoA::Script::PushStr(L, reason.c_str());
        }

        return 2;
    }

    int ToggleMentorStatus(lua_State* L) {   // FUN_10a1be90: CMSG 0x6C3 { u8 activate }
        bool activate;

        if (!CoA::Script::ReadBool(L, activate)) {
            return 0;
        }

        const bool ok = activate ? CanActivateMentor() == 0 : (Player().present && s_mentorStatus);

        if (ok) {
            CoA::Packet(0x6C3).U8(activate ? 1 : 0).Send();
        }

        CoA::Script::PushBool(L, ok);
        return 1;
    }

    int AddMentorSpecialization(lua_State* L) {   // FUN_10a19590: CMSG 0x6C6 { u32 }
        uint32_t id;

        if (CoA::Script::ReadNumber(L, id)) {
            CoA::Packet(0x6C6).U32(id).Send();
            s_pendingSpec = id;
        }

        return 0;
    }

    int RemoveMentorSpecialization(lua_State* L) {   // FUN_10a1b710: CMSG 0x6C8 { u32 }
        uint32_t id;

        if (CoA::Script::ReadNumber(L, id)) {
            CoA::Packet(0x6C8).U32(id).Send();
            s_pendingSpec = id;
        }

        return 0;
    }

    int QueryActiveMentors(lua_State* L) {   // FUN_10a1b650: empty CMSG 0x6CB
        CoA::Packet(0x6CB).Send();
        CoA::Script::PushBool(L, true);
        return 1;
    }

    int GetMentorSpecializationActive(lua_State* L) {
        uint32_t id;

        if (!CoA::Script::ReadNumber(L, id)) {
            return 0;
        }

        CoA::Script::PushBool(L, std::find(s_activeSpecs.begin(), s_activeSpecs.end(), id) != s_activeSpecs.end());
        return 1;
    }

    int GetMentorSpecializations(lua_State* L) {   // FUN_10a1a6c0
        std::vector<uint32_t> ids;
        Table& t = MentorSpecs();

        for (uint32_t id = t.MinId(); t.Loaded() && id <= t.MaxId(); ++id) {
            if (t.Row(id)) {
                ids.push_back(id);
            }
        }

        return PushIntTable(L, ids);
    }

    int GetMentorSpecializationInfo(lua_State* L) {   // handler_GetMentorSpecializationInfo
        uint32_t id;

        if (!CoA::Script::ReadNumber(L, id)) {
            return 0;
        }

        const uint8_t* row = MentorSpecs().Row(id);

        if (!row) {
            return CoA::Script::PushNils(L, 2);
        }

        CoA::Script::PushStr(L, MentorSpecs().Str(row, 8));
        const char* name = MentorSpecs().Str(row, 4);

        if (*name) {
            CoA::Script::PushStr(L, name);
        } else {
            lua_pushnil(L);
        }

        return 2;
    }

    int GetAvailableMentorAtIndex(lua_State* L) {   // handler_GetAvailableMentorAtIndex
        uint32_t i;

        if (!CoA::Script::ReadNumber(L, i) || i == 0) {
            return 0;
        }

        const MentorRecord* m = i - 1 < s_mentorContainer.Size() ? s_mentorContainer.At(i - 1) : nullptr;

        if (!m) {
            return CoA::Script::PushNils(L, 6);
        }

        CoA::Script::PushStr(L, m->name.c_str());
        CoA::Script::PushInt(L, m->level);
        CoA::Script::PushStr(L, m->s1.c_str());
        PushIntTable(L, m->specs);
        CoA::Script::PushStr(L, m->s2.c_str());
        CoA::Script::PushStr(L, m->s3.c_str());
        return 6;
    }

    int SetAvailableMentorFilter(lua_State* L) {   // FUN_10a1b800 -> "$s<spec> <text>"
        if (!CoA::Script::ValidateInput(L, { LUA_TSTRING, LUA_TNUMBER })) {
            return 0;
        }

        const std::string text = CoA::Script::CheckString(L, 1);
        char buf[16];
        snprintf(buf, sizeof(buf), "$s%d ", CoA::Script::ToInt(CoA::Script::CheckNumber(L, 2)));
        s_mentorContainer.ApplyFilter(buf + text);
        return 0;
    }

    // GetCategories() (0x10a19e50): every TutorialCategories id, in id order.
    int GetCategories(lua_State* L) {
        Table& t = Categories();
        std::vector<uint32_t> ids;

        for (uint32_t id = t.MinId(); t.Loaded() && id <= t.MaxId(); ++id) {
            if (t.Row(id)) {
                ids.push_back(id);
            }
        }

        return PushIntTable(L, ids);
    }

    // GetCategoryInfo(id) (0x10a17490): the category's +4 name, else nil.
    int GetCategoryInfo(lua_State* L) {
        uint32_t id;

        if (!CoA::Script::ReadNumber(L, id)) {
            return 0;
        }

        Table& t = Categories();

        if (const uint8_t* row = t.Row(id)) {
            CoA::Script::PushStr(L, t.Str(row, 4));
        } else {
            lua_pushnil(L);
        }

        return 1;
    }

    // GetRewards(id) (0x10a1ac20): {item = count} summed over the tutorial's TutorialRewards rows (+4 tutorial, +8 item,
    // +0xC count; zero item or count skipped); nil for an unknown tutorial or none.
    int GetRewards(lua_State* L) {
        uint32_t id;

        if (!CoA::Script::ReadNumber(L, id)) {
            return 0;
        }

        const uint8_t* row = Tutorials().Row(id);

        if (!row) {
            lua_pushnil(L);
            return 1;
        }

        std::unordered_map<uint32_t, uint32_t> items;
        Table& r = Rewards();

        for (uint32_t rid = r.MinId(); r.Loaded() && rid <= r.MaxId(); ++rid) {
            const uint8_t* rr = r.Row(rid);

            if (!rr || Table::U32(rr, 4) != Table::U32(row, 0) || !Table::U32(rr, 8) || !Table::U32(rr, 0xC)) {
                continue;
            }

            items[Table::U32(rr, 8)] += Table::U32(rr, 0xC);
        }

        if (items.empty()) {
            lua_pushnil(L);
            return 1;
        }

        lua_createtable(L, 0, static_cast<int>(items.size()));
        lua_checkstack(L, 2);

        for (const auto& e : items) {
            CoA::Script::PushInt(L, static_cast<int32_t>(e.first));
            CoA::Script::PushInt(L, static_cast<int32_t>(e.second));
            lua_settable(L, -3);
        }

        return 1;
    }

    // IsTutorialRequiredForMentorStatus(id) (0x10a1b5d0): Tutorial +0x88 != 0, else nil.
    int IsTutorialRequiredForMentorStatus(lua_State* L) {
        uint32_t id;

        if (!CoA::Script::ReadNumber(L, id)) {
            return 0;
        }

        Table& t = Tutorials();

        if (const uint8_t* row = t.Row(id)) {
            CoA::Script::PushBool(L, Table::U32(row, 0x88) != 0);
        } else {
            lua_pushnil(L);
        }

        return 1;
    }

    // SMSG_PATCH_TUTORIAL (0x6A5, 0x101e4ea0): 0xB4 struct, strings in fields 35..39; then the tutorial container is
    // reset (FUN_10a19510) and TUTORIAL_ENTRY_UPDATE fires for the id.
    void AfterTutorialPatch(const uint8_t* row) {
        s_tutorials.Reset();
        CoA::Events::Signal("TUTORIAL_CONTAINER_RESET", nullptr);
        CoA::Events::Signal("TUTORIAL_ENTRY_UPDATE", "%u", row ? Table::U32(row, 0) : 0);
    }

    // SMSG_PATCH_TUTORIAL_OBJECTIVES (0x6A7, 0x101e4cf0): TUTORIAL_ENTRY_UPDATE for the tutorial (+4).
    void AfterObjectivePatch(const uint8_t* row) {
        CoA::Events::Signal("TUTORIAL_ENTRY_UPDATE", "%u", row ? Table::U32(row, 4) : 0);
    }

    // SMSG_PATCH_TUTORIAL_KEYWORDS (0x6AC, 0x101e44c0): keyword container reset (FUN_10a19480), then
    // TUTORIAL_ENTRY_UPDATE with 0.
    void AfterKeywordPatch(const uint8_t*) {
        s_keywords.Reset();
        CoA::Events::Signal("TUTORIAL_KEYWORD_CONTAINER_RESET", nullptr);
        CoA::Events::Signal("TUTORIAL_ENTRY_UPDATE", "%u", 0u);
    }

    const CoA::Binding s_bindings[] = {
        COA_NATIVE("C_Tutorial", "GetCategories", GetCategories),
        COA_NATIVE("C_Tutorial", "GetCategoryInfo", GetCategoryInfo),
        COA_NATIVE("C_Tutorial", "GetRewards", GetRewards),
        COA_NATIVE("C_Tutorial", "IsTutorialRequiredForMentorStatus", IsTutorialRequiredForMentorStatus),
        COA_NATIVE("C_Tutorial", "AddMentorSpecialization", AddMentorSpecialization),
        COA_NATIVE("C_Tutorial", "AnyUnclaimedRewards", AnyUnclaimedRewards),
        COA_NATIVE("C_Tutorial", "CanCollectReward", CanCollectReward),
        COA_NATIVE("C_Tutorial", "CanToggleMentorStatus", CanToggleMentorStatus),
        COA_NATIVE("C_Tutorial", "CollectReward", CollectReward),
        COA_NATIVE("C_Tutorial", "DebugTutorialAppliccability", DebugTutorialAppliccability),
        COA_NATIVE("C_Tutorial", "GetAvailableMentorAtIndex", GetAvailableMentorAtIndex),
        COA_NATIVE("C_Tutorial", "GetCategoryProgress", GetCategoryProgress),
        COA_NATIVE("C_Tutorial", "GetIndexByKeywordID", GetIndexByKeywordID),
        COA_NATIVE("C_Tutorial", "GetKeywordAtIndex", GetKeywordAtIndex),
        COA_NATIVE("C_Tutorial", "GetKeywordID", GetKeywordID),
        COA_NATIVE("C_Tutorial", "GetKeywordInfo", GetKeywordInfo),
        COA_NATIVE("C_Tutorial", "GetMentorSpecializationActive", GetMentorSpecializationActive),
        COA_NATIVE("C_Tutorial", "GetMentorSpecializationInfo", GetMentorSpecializationInfo),
        COA_NATIVE("C_Tutorial", "GetMentorSpecializations", GetMentorSpecializations),
        COA_NATIVE("C_Tutorial", "GetNumAvailableMentors", GetNumAvailableMentors),
        COA_NATIVE("C_Tutorial", "GetNumKeywords", GetNumKeywords),
        COA_NATIVE("C_Tutorial", "GetNumTutorials", GetNumTutorials),
        COA_NATIVE("C_Tutorial", "GetObjectiveInfo", GetObjectiveInfo),
        COA_NATIVE("C_Tutorial", "GetObjectives", GetObjectives),
        COA_NATIVE("C_Tutorial", "GetTutorialAtIndex", GetTutorialAtIndex),
        COA_NATIVE("C_Tutorial", "GetTutorialByID", GetTutorialByID),
        COA_NATIVE("C_Tutorial", "GetTutorialDisplay", GetTutorialDisplay),
        COA_NATIVE("C_Tutorial", "GetTutorialsRequiredForMentorStatus", GetTutorialsRequiredForMentorStatus),
        COA_NATIVE("C_Tutorial", "IsCategoryEnabled", IsCategoryEnabled),
        COA_NATIVE("C_Tutorial", "IsMentorStatusActive", IsMentorStatusActive),
        COA_NATIVE("C_Tutorial", "IsRewardCollected", IsRewardCollected),
        COA_NATIVE("C_Tutorial", "IsTutorialAvailable", IsTutorialAvailable),
        COA_NATIVE("C_Tutorial", "IsTutorialComplete", IsTutorialComplete),
        COA_NATIVE("C_Tutorial", "QueryActiveMentors", QueryActiveMentors),
        COA_NATIVE("C_Tutorial", "RemoveMentorSpecialization", RemoveMentorSpecialization),
        COA_NATIVE("C_Tutorial", "SetAvailableMentorFilter", SetAvailableMentorFilter),
        COA_NATIVE("C_Tutorial", "SetKeywordFilter", SetKeywordFilter),
        COA_NATIVE("C_Tutorial", "SetTutorialFilter", SetTutorialFilter),
        COA_NATIVE("C_Tutorial", "StartQuest", StartQuest),
        COA_NATIVE("C_Tutorial", "ToggleMentorStatus", ToggleMentorStatus),
    };
}

// FUN_10219110: case-insensitive, whole-word (the delimiter set below, or any byte >= 0x80), never inside an existing
// |H...|h...|h link, longest match wins, `exclude` (a keyword's own id) is never linked. The links are found in the text
// as given (the keyword matches in its lower-cased copy). The left-boundary test at position 0 reads the keyword's own
// first character, as the original's does, so a keyword at the very start of a text is not linked.
std::string CoA::Tutorial::FormatKeywords(std::string text, bool plain, uint32_t exclude) {
    if (text.empty()) {
        return text;
    }

    static const std::string kDelims(" \n\r\t.,!?;:-()<>'\"");
    auto isDelim = [](char c) { return static_cast<signed char>(c) < 0 || kDelims.find(c) != std::string::npos; };
    const std::string lower = CoA::Filter::Lower(text);
    std::vector<std::pair<size_t, size_t>> links;

    for (size_t pos = 0;;) {
        const size_t h = text.size() >= 2 ? text.find("|H", pos) : std::string::npos;

        if (h == std::string::npos) {
            break;
        }

        const size_t m = text.find("|h", h + 2);

        if (m == std::string::npos) {
            break;
        }

        const size_t e = text.find("|h", m + 2);

        if (e == std::string::npos) {
            break;
        }

        links.emplace_back(h, e + 2);
        pos = e + 2;
    }

    struct Match {
        size_t start;
        size_t end;
        uint32_t id;
    };

    std::vector<Match> matches;
    Table& kw = Keywords();

    for (uint32_t id = kw.MinId(); kw.Loaded() && id <= kw.MaxId(); ++id) {
        const uint8_t* row = kw.Row(id);

        if (id == exclude || !row || !KeywordApplicable(row)) {
            continue;
        }

        const std::string name = CoA::Filter::Lower(kw.Str(row, 0x18));

        if (name.empty()) {
            continue;
        }

        for (size_t from = 0; from <= lower.size();) {
            const size_t p = lower.find(name, from);

            if (p == std::string::npos) {
                break;
            }

            const size_t end = p + name.size();
            from = end;
            const size_t left = p ? p - 1 : 0;

            if (!isDelim(lower[left])) {
                continue;
            }

            if (end < lower.size() && !isDelim(lower[end])) {
                continue;
            }

            matches.push_back({ p, end, id });
        }
    }

    matches.erase(std::remove_if(matches.begin(), matches.end(),
                      [&](const Match& m) {
                          for (auto& l : links) {
                              if (l.first <= m.start && m.end <= l.second) {
                                  return true;
                              }
                          }

                          return false;
                      }),
        matches.end());
    std::sort(matches.begin(), matches.end(), [](const Match& a, const Match& b) { return std::tie(a.start, a.end, a.id) < std::tie(b.start, b.end, b.id); });
    std::vector<Match> chosen;

    for (const Match& m : matches) {
        if (chosen.empty() || chosen.back().end <= m.start) {
            chosen.push_back(m);
        } else if (chosen.back().end - chosen.back().start < m.end - m.start) {
            chosen.back() = m;
        }
    }

    for (auto it = chosen.rbegin(); it != chosen.rend(); ++it) {
        const uint8_t* row = kw.Row(it->id);

        if (!row || !KeywordApplicable(row)) {
            continue;
        }

        std::string rep;

        if (!plain) {
            const bool bracket = Table::U32(row, 4) != 0;
            rep = std::string("|c") + kw.Str(row, 8) + "|Hkeyword:" + std::to_string(it->id) + "|h" + (bracket ? "[" : "") + kw.Str(row, 0x18) +
                (bracket ? "]" : "") + "|h|r";
        } else {
            rep = std::string(kw.Str(row, 0x18)) + " " + kw.Str(row, 0x1C) + " " + kw.Str(row, 0x20);
        }

        text.replace(it->start, it->end - it->start, rep);
    }

    return text;
}

CoA::Tutorial::Deps& CoA::Tutorial::Dependencies() {
    return s_deps;
}

void CoA::Tutorial::Init() {
    CoA::LuaReg::AddModule(s_bindings, sizeof(s_bindings) / sizeof(s_bindings[0]));
    CoA::DbcPatch::Register(0x6A5, "DBFilesClient\\Tutorial.dbc", 0xB4, 0x1Full << 35, AfterTutorialPatch);
    CoA::DbcPatch::Register(0x6A7, "DBFilesClient\\TutorialObjectives.dbc", 0x20, 1ull << 7, AfterObjectivePatch);
    CoA::DbcPatch::Register(0x6AC, "DBFilesClient\\TutorialKeywords.dbc", 0x44, 0x1FCull, AfterKeywordPatch);
    CoA::Lifecycle::OnGlueScreen(ResetAll);
    CoA::ClientHooks::afterEnterWorld.Add(SeedWatches);
    CoA::Lifecycle::OnAfterIdle(LevelWatch);
    CoA::ClientHooks::stockEvents.emplace_back(0x258, RefreshCompleted);
    CoA::ClientHooks::stockEvents.emplace_back(0x1C0, RefreshAddedOnEvent);
    COA_PACKET(0x6AF, OnClaimRewardsResult);
    COA_PACKET(0x6B0, OnRewardedList);
    COA_PACKET(0x6B1, OnRewarded);
    COA_PACKET(0x6B2, OnPatchRewards);
    COA_PACKET(0x6C4, OnToggleMentorResult);
    COA_PACKET(0x6C7, OnAddSpecResult);
    COA_PACKET(0x6C9, OnRemoveSpecResult);
    COA_PACKET(0x6CA, OnSpecList);
    COA_PACKET(0x6CC, OnWhoisResult);
    COA_EVENT("TUTORIAL_ENTRY_ADDED");
    COA_EVENT("TUTORIAL_ENTRY_COMPLETE");
    COA_EVENT("TUTORIAL_ENTRY_UPDATE");
    COA_EVENT("TUTORIAL_CONTAINER_RESET");
    COA_EVENT("TUTORIAL_KEYWORD_CONTAINER_RESET");
    COA_EVENT("CLAIM_TUTORIAL_REWARDS_RESULT");
    COA_EVENT("TOGGLE_MENTOR_STATUS_RESULT");
    COA_EVENT("ADD_MENTOR_SPECIALIZATION_RESULT");
    COA_EVENT("REMOVE_MENTOR_SPECIALIZATION_RESULT");
    COA_EVENT("MENTOR_WHOIS_DATA_CONTAINER_RESET");
    COA_EVENT("QUERY_MENTOR_WHOIS_RESULT");
}

void CoA::Tutorial::Reset() {
    ResetAll();
    s_keywords.Reset();
}
