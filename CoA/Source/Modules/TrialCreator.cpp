// AscTrialCreator.cpp (see TrialCreator.h).
#include "CoA/Source/Modules/TrialCreator.h"
#include "CoA/Source/Modules/Challenge.h"
#include "CoA/Source/Modules/Modules.h"
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
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

using CoA::Script::CheckNumber;
using CoA::Script::CheckString;
using CoA::Script::PushBool;
using CoA::Script::PushInt;
using CoA::Script::PushNum;
using CoA::Script::PushStr;
using CoA::Script::ReadString;
using CoA::Script::ToInt;
using CoA::Script::ValidateInput;

namespace {
#include "CoA/Source/Modules/CAEnums.generated.inc"

    struct TrialChallenge {
        uint32_t challenge = 0;
        uint32_t level = 0;
        std::string description;
    };

    struct Trial {
        std::string id;
        bool owned = false;
        std::string author;
        std::string name;
        std::string description;
        std::string icon;
        std::vector<TrialChallenge> challenges;
        bool upvoted = false;
        bool downvoted = false;
        std::vector<uint64_t> upvotes;
        std::vector<uint64_t> downvotes;

        int32_t Score() const {
            return static_cast<int32_t>(this->upvotes.size()) - static_cast<int32_t>(this->downvotes.size());
        }
    };

    struct Completion {
        std::string trial;
        std::string player;
        uint64_t start = 0;
        uint64_t complete = 0;
        std::string race;
        std::string gender;
        std::string cls;
    };

    CoA::TrialCreator::Deps s_deps;
    std::vector<Trial> s_trials;                                             // +0x00
    std::string s_active;                                                    // +0x0C
    std::unordered_map<std::string, std::vector<Completion>> s_completions;   // +0x24

    const Trial* Find(const std::string& id) {   // FUN_10122420
        for (const Trial& t : s_trials) {
            if (t.id == id) {
                return &t;
            }
        }

        return nullptr;
    }

    Trial* FindMutable(const std::string& id) {
        return const_cast<Trial*>(Find(id));
    }

    std::vector<std::pair<uint32_t, uint32_t>> Levels(const Trial& t) {
        std::vector<std::pair<uint32_t, uint32_t>> v;

        for (const TrialChallenge& c : t.challenges) {
            v.emplace_back(c.challenge, c.level);
        }

        return v;
    }

    uint32_t RowU32(const uint8_t* r, uint32_t off) {
        uint32_t v;
        memcpy(&v, r + off, 4);
        return v;
    }

    // FUN_10121d00: why the trial cannot be activated (ACTIVATE_TRIAL_*).
    bool CanActivate(const std::string& id, std::vector<std::string>& errors) {
        if (!s_active.empty()) {
            errors.push_back("ACTIVATE_TRIAL_ALREADY_ACTIVE");
        }

        const Trial* t = Find(id);

        if (!t) {
            errors.push_back("ACTIVATE_TRIAL_NOT_FOUND");
        } else {
            // Each level must be startable (ignoring "not a trial", 0x11) and every running challenge stoppable.
            bool failed = false;

            for (const TrialChallenge& c : t->challenges) {
                if (!CoA::Challenge::CanActivate(c.challenge, c.level, { 0x11 })) {
                    errors.push_back("ACTIVATE_TRIAL_CANNOT_ACTIVATE_CHALLENGE");
                    failed = true;
                    break;
                }
            }

            if (!failed) {
                for (uint32_t active : CoA::Challenge::ActiveIds()) {
                    if (!CoA::Challenge::CanDeactivate(active, { 0x11 })) {
                        errors.push_back("ACTIVATE_TRIAL_CANNOT_ACTIVATE_CHALLENGE");
                        break;
                    }
                }
            }
        }

        return errors.empty();
    }

    // FUN_10125e50's checks: why the active trial cannot be deactivated (DEACTIVATE_TRIAL_*).
    bool CanDeactivate(std::vector<std::string>& errors) {
        if (s_active.empty()) {
            errors.push_back("DEACTIVATE_TRIAL_NO_ACTIVE_CHALLENGE");
        }

        const Trial* t = Find(s_active);

        if (!t) {
            errors.push_back("DEACTIVATE_TRIAL_NOT_FOUND");
        } else {
            for (const TrialChallenge& c : t->challenges) {
                if (!CoA::Challenge::CanDeactivate(c.challenge, { 1, 4 })) {
                    errors.push_back("DEACTIVATE_TRIAL_CANNOT_DEACTIVATE_CHALLENGE");
                    break;
                }
            }
        }

        return errors.empty();
    }

    // FUN_1011f170 (via FUN_1011cb30): the active trial first, then higher score.
    bool Before(const Trial& a, const Trial& b) {
        const bool aa = a.id == s_active;
        const bool ab = b.id == s_active;

        if (aa != ab) {
            return aa;
        }

        return b.Score() < a.Score();
    }

    // ---- ChallengeCreatorEntryContainer (vtable 0x10B312D0) ----
    // The original keeps pointers into the trial vector (FUN_10128950). Here an entry is the trial's index, which reads
    // the same trial as long as the original's pointer does (an append that reallocates leaves the original's dangling).
    class Container : public CoA::Filter::Container<size_t> {
    protected:
        std::vector<size_t> DoPopulate() override {   // FUN_10128950
            std::vector<size_t> v;

            for (size_t i = 0; i < s_trials.size(); ++i) {
                v.push_back(i);
            }

            return v;
        }

        bool ArgMatch(uint32_t filter, const size_t& e) override {   // FUN_10128cf0
            if (e >= s_trials.size()) {
                return false;
            }

            const Trial* t = &s_trials[e];

            if (t->id == s_active) {
                return true;
            }

            auto any = [&](auto pred) {
                for (const TrialChallenge& c : t->challenges) {
                    if (const uint8_t* r = CoA::Challenge::ChallengeRowOf(c.challenge)) {
                        if (pred(r)) {
                            return true;
                        }
                    }
                }

                return false;
            };

            switch (filter) {
                case 1:
                    return any([](const uint8_t* r) { return RowU32(r, 0x38) != 0; });
                case 2:
                    return any([](const uint8_t* r) { return RowU32(r, 0x38) == 0; });
                case 3: {
                    std::vector<std::string> e2;
                    return CanActivate(t->id, e2);
                }
                case 4:
                    return any([](const uint8_t* r) { return RowU32(r, 4) != 0; });
                case 5:
                    return any([](const uint8_t* r) { return RowU32(r, 8) != 0; });
                case 6:
                    for (const TrialChallenge& c : t->challenges) {
                        if (CoA::Challenge::HasSoloRule(c.challenge, c.level)) {
                            return true;
                        }
                    }

                    return false;
                case 7:
                    for (const TrialChallenge& c : t->challenges) {
                        if (CoA::Challenge::HasSoloRule(c.challenge, c.level)) {
                            return false;
                        }
                    }

                    return true;
                case 8:
                case 9:
                case 10:
                case 11:
                case 12:
                    return any([&](const uint8_t* r) { return CoA::Challenge::Difficulty(r) == static_cast<int>(filter - 8); });
                default:
                    return false;
            }
        }

        uint32_t ArgGroup(uint32_t filter) override {   // FUN_10129370
            switch (filter) {
                case 1:
                case 2:
                    return 1;
                case 4:
                case 5:
                    return 2;
                case 6:
                case 7:
                    return 3;
                case 8:
                case 9:
                case 10:
                case 11:
                case 12:
                    return 4;
                default:
                    return 0;
            }
        }

        bool TextMatch(const std::string& text, const size_t& e) override {   // FUN_10128b80
            if (text.empty()) {
                return true;
            }

            if (e >= s_trials.size()) {
                return false;
            }

            if (CoA::Filter::Lower(s_trials[e].name).find(text) != std::string::npos) {
                return true;
            }

            return CoA::Filter::Lower(s_trials[e].author).find(text) != std::string::npos;
        }
    };

    Container s_entries;

    // ---- Lua ----
    void SetStr(lua_State* L, const char* k, const std::string& v) {
        lua_pushstring(L, k);
        PushStr(L, v.c_str());
        lua_settable(L, -3);
    }

    void SetBool(lua_State* L, const char* k, bool v) {
        lua_pushstring(L, k);
        PushBool(L, v);
        lua_settable(L, -3);
    }

    void SetInt(lua_State* L, const char* k, int32_t v) {
        lua_pushstring(L, k);
        PushInt(L, v);
        lua_settable(L, -3);
    }

    void SetNum(lua_State* L, const char* k, double v) {
        lua_pushstring(L, k);
        PushNum(L, v);
        lua_settable(L, -3);
    }

    void NewTable(lua_State* L) {
        lua_createtable(L, 0, 0);
        lua_checkstack(L, 2);
    }

    // FUN_101243b0: the trial's fields into the table on top.
    void PushTrial(lua_State* L, const Trial& t, bool sortConditions) {
        SetStr(L, "ID", t.id);
        SetBool(L, "Owned", t.owned);
        SetStr(L, "Author", t.author);
        SetStr(L, "Name", t.name);
        SetStr(L, "Description", "Created By: " + t.author + "\n" + t.description);
        SetStr(L, "Icon", t.icon);
        lua_pushstring(L, "Challenges");
        NewTable(L);

        for (size_t i = 0; i < t.challenges.size(); ++i) {
            PushInt(L, static_cast<int32_t>(i + 1));
            NewTable(L);
            SetInt(L, "ChallengeId", static_cast<int32_t>(t.challenges[i].challenge));
            SetInt(L, "Level", static_cast<int32_t>(t.challenges[i].level));
            SetStr(L, "Description", t.challenges[i].description);
            lua_settable(L, -3);
        }

        lua_settable(L, -3);
        CoA::Challenge::PushMergedLevels(L, Levels(t), sortConditions);
        SetBool(L, "Upvoted", t.upvoted);
        SetBool(L, "Downvoted", t.downvoted);
        SetInt(L, "Score", t.Score());
    }

    void PushCompletion(lua_State* L, const Completion& c) {   // FUN_10125630
        NewTable(L);
        SetStr(L, "Challenge", c.trial);
        SetStr(L, "PlayerName", c.player);
        SetNum(L, "StartTime", static_cast<int32_t>(c.start));
        SetNum(L, "CompleteTime", static_cast<int32_t>(c.complete));
        SetStr(L, "PlayerRace", c.race);
        SetStr(L, "PlayerGender", c.gender);
        SetStr(L, "PlayerClass", c.cls);
    }

    void PushStrings(lua_State* L, const std::vector<std::string>& v) {   // FUN_100beb90
        lua_createtable(L, 0, static_cast<int>(v.size()));
        lua_checkstack(L, 2);

        for (size_t i = 0; i < v.size(); ++i) {
            PushNum(L, static_cast<double>(i + 1));
            PushStr(L, v[i].c_str());
            lua_settable(L, -3);
        }
    }

    void PutSized(CoA::Packet& p, const std::string& s) {   // u32 length + bytes
        p.U32(static_cast<uint32_t>(s.size()));

        if (!s.empty()) {
            p.Data(s.data(), static_cast<uint32_t>(s.size()));
        }
    }

    void SendId(uint32_t opcode, const std::string& id) {
        CoA::Packet p(opcode);
        PutSized(p, id);
        p.Send();
    }

    void Query() {   // FUN_101257a0
        CoA::Packet(0x5AB).Send();
    }

    // FrameScript::lua_toarray over the table at `idx` (numbers / strings; anything else empties it).
    bool NumberArray(lua_State* L, int idx, std::vector<uint32_t>& out) {   // FUN_1011f230
        out.clear();

        if (lua_type(L, idx) != LUA_TTABLE) {
            CoA::Log::Printf("FrameScript::lua_toarray Value at index %d is not a table", idx);
            return false;
        }

        lua_pushnil(L);

        while (lua_next(L, idx)) {
            if (!lua_isnumber(L, -1)) {
                lua_settop(L, -3);
                CoA::Log::Printf("FrameScript::lua_toarray Invalid lua value type");
                out.clear();
                return false;
            }

            out.push_back(static_cast<uint32_t>(static_cast<int64_t>(CheckNumber(L, -1))));
            lua_settop(L, -2);
        }

        return true;
    }

    bool StringArray(lua_State* L, int idx, std::vector<std::string>& out) {   // FUN_1011f710
        out.clear();

        if (lua_type(L, idx) != LUA_TTABLE) {
            CoA::Log::Printf("FrameScript::lua_toarray Value at index %d is not a table", idx);
            return false;
        }

        lua_pushnil(L);

        while (lua_next(L, idx)) {
            if (!lua_isstring(L, -1)) {
                lua_settop(L, -3);
                CoA::Log::Printf("FrameScript::lua_toarray Invalid lua value type");
                out.clear();
                return false;
            }

            out.push_back(lua_tolstring(L, -1, nullptr));
            lua_settop(L, -2);
        }

        return true;
    }

    struct SaveArgs {
        std::string id;
        std::string name;
        std::string description;
        std::string icon;
        std::vector<uint32_t> challenges;
        std::vector<uint32_t> levels;
        std::vector<std::string> descriptions;
    };

    bool ReadSave(lua_State* L, SaveArgs& a) {   // FUN_10119e50 (string x4, table x3)
        if (!ValidateInput(L, { LUA_TSTRING, LUA_TSTRING, LUA_TSTRING, LUA_TSTRING, LUA_TTABLE, LUA_TTABLE, LUA_TTABLE })) {
            return false;
        }

        a.id = CheckString(L, 1);
        a.name = CheckString(L, 2);
        a.description = CheckString(L, 3);
        a.icon = CheckString(L, 4);
        NumberArray(L, 5, a.challenges);
        NumberArray(L, 6, a.levels);
        StringArray(L, 7, a.descriptions);
        return true;
    }

    bool IconExists(const std::string& icon) {   // SpellIcon (client container 0xAD488C, +4 path)
        return s_deps.iconExists && s_deps.iconExists(icon.c_str());
    }

    std::string TrialName(std::string s) {   // "CHALLENGE" -> "TRIAL", once (FUN_100ee5d0)
        if (s.size() > 8) {
            const size_t at = s.find("CHALLENGE");

            if (at != std::string::npos) {
                s.replace(at, 9, "TRIAL");
            }
        }

        return s;
    }

    int64_t Now() {
        return s_deps.now ? s_deps.now() : 0;
    }

    // ---- bindings ----
    int SaveTrial(lua_State* L) {   // FUN_101280e0
        SaveArgs a;

        if (!ReadSave(L, a)) {
            return 0;
        }

        if (a.challenges.size() != a.levels.size()) {
            PushBool(L, false);
            return 1;
        }

        CoA::Packet p(0x5A7);

        for (const std::string* s : { &a.id, &a.name, &a.description, &a.icon }) {
            PutSized(p, *s);
        }

        p.U32(static_cast<uint32_t>(a.challenges.size()));

        for (size_t i = 0; i < a.challenges.size(); ++i) {
            p.U32(a.challenges[i]);
            p.U32(a.levels[i]);
            PutSized(p, i < a.descriptions.size() ? a.descriptions[i] : std::string());
        }

        p.Send();
        PushBool(L, true);
        return 1;
    }

    // FUN_101263f0: (true, {SAVE_TRIAL_*} or nil); false, nil when the two id tables differ in length.
    int CanSaveTrial(lua_State* L) {
        SaveArgs a;

        if (!ReadSave(L, a)) {
            return 0;
        }

        if (a.challenges.size() != a.levels.size()) {
            PushBool(L, false);
            lua_pushnil(L);
            return 2;
        }

        std::vector<uint32_t> codes;

        if (!a.id.empty()) {
            const Trial* t = Find(a.id);

            if (!t) {
                codes.push_back(1);
            } else {
                if (!t->owned) {
                    codes.push_back(2);
                }

                std::vector<uint32_t> ids = a.challenges;
                std::vector<uint32_t> levels = a.levels;
                std::vector<uint32_t> oldIds;
                std::vector<uint32_t> oldLevels;

                for (const TrialChallenge& c : t->challenges) {
                    oldIds.push_back(c.challenge);
                    oldLevels.push_back(c.level);
                }

                std::sort(ids.begin(), ids.end());
                std::sort(levels.begin(), levels.end());
                std::sort(oldIds.begin(), oldIds.end());
                std::sort(oldLevels.begin(), oldLevels.end());

                if (ids != oldIds || levels != oldLevels) {
                    codes.push_back(0xB);
                }
            }
        }

        if (a.name.empty()) {
            codes.push_back(3);
        }

        if (a.challenges.empty()) {
            codes.push_back(4);
        }

        if (!IconExists(a.icon)) {
            codes.push_back(5);
        }

        if (a.id.size() > 0x24) {
            codes.push_back(6);
        }

        if (a.name.size() > 0x80) {
            codes.push_back(7);
        }

        if (a.description.size() > 0x2000) {
            codes.push_back(8);
        }

        if (a.icon.size() > 0x80) {
            codes.push_back(9);
        }

        for (const std::string& d : a.descriptions) {
            if (d.size() > 0x400) {
                codes.push_back(10);
                break;
            }
        }

        std::vector<std::string> names;

        for (uint32_t c : codes) {
            names.push_back(TrialName(c < 12 ? kTrialSaveResults[c] : "UNEXPECTED_ENUM_VALUE_" + std::to_string(c)));
        }

        PushBool(L, true);

        if (names.empty()) {
            lua_pushnil(L);
        } else {
            PushStrings(L, names);
        }

        return 2;
    }

    int DeleteTrial(lua_State* L) {   // FUN_10126f20
        std::string id;

        if (!ReadString(L, id)) {
            return 0;
        }

        SendId(0x5A9, id);
        PushBool(L, true);
        return 1;
    }

    int ActivateTrial(lua_State* L) {   // FUN_10125b10
        std::string id;

        if (!ReadString(L, id)) {
            return 0;
        }

        SendId(0x5AD, id);
        PushBool(L, true);
        return 1;
    }

    int DeactivateTrial(lua_State* L) {   // FUN_10126e30
        CoA::Packet(0x5AF).Send();
        PushBool(L, true);
        return 1;
    }

    int QueryTrials(lua_State* L) {   // handler_QueryTrials
        Query();
        PushBool(L, true);
        return 1;
    }

    int QueryTrialCompletions(lua_State* L) {   // FUN_10127b90
        std::string id;

        if (!ReadString(L, id)) {
            return 0;
        }

        SendId(0x5C9, id);
        PushBool(L, true);
        return 1;
    }

    int CanEditTrial(lua_State* L) {   // FUN_101262e0
        std::string id;

        if (!ReadString(L, id)) {
            return 0;
        }

        const Trial* t = Find(id);
        PushBool(L, t && t->owned);
        return 1;
    }

    int CanActivateTrial(lua_State* L) {   // FUN_10125c90
        std::string id;

        if (!ReadString(L, id)) {
            return 0;
        }

        std::vector<std::string> errors;
        PushBool(L, CanActivate(id, errors));
        PushStrings(L, errors);
        return 2;
    }

    int CanDeactivateTrial(lua_State* L) {   // FUN_10125e50
        std::vector<std::string> errors;
        PushBool(L, CanDeactivate(errors));
        PushStrings(L, errors);
        return 2;
    }

    // FUN_10127d30 (id, upvote, downvote): a changed, non-contradictory vote is recorded and sent (0x5BF).
    int RateTrial(lua_State* L) {
        if (!ValidateInput(L, { LUA_TSTRING, LUA_TBOOLEAN, LUA_TBOOLEAN })) {
            return 0;
        }

        const std::string id = CheckString(L, 1);
        const bool up = lua_toboolean(L, 2) != 0;
        const bool down = lua_toboolean(L, 3) != 0;
        Trial* t = FindMutable(id);

        if (!t || (t->upvoted == up && t->downvoted == down) || (up && down)) {
            PushBool(L, false);
            return 1;
        }

        if (t->upvoted && !t->upvotes.empty()) {
            t->upvotes.pop_back();
        }

        if (t->downvoted && !t->downvotes.empty()) {
            t->downvotes.pop_back();
        }

        t->upvoted = up;
        t->downvoted = down;

        if (up) {
            t->upvotes.push_back(static_cast<uint64_t>(Now()));
        }

        if (down) {
            t->downvotes.push_back(static_cast<uint64_t>(Now()));
        }

        CoA::Packet p(0x5BF);
        PutSized(p, id);
        p.U8(up ? 1 : 0).U8(down ? 1 : 0).Send();
        PushBool(L, true);
        return 1;
    }

    std::vector<const Trial*> Sorted() {   // FUN_101204f0 copy + FUN_1011e400 sort
        std::vector<const Trial*> v;

        for (const Trial& t : s_trials) {
            v.push_back(&t);
        }

        std::sort(v.begin(), v.end(), [](const Trial* a, const Trial* b) { return Before(*a, *b); });
        return v;
    }

    // FUN_101270d0: every trial except the archive's (unless it is the active one).
    int GetAllTrials(lua_State* L) {
        NewTable(L);
        int i = 0;

        for (const Trial* t : Sorted()) {
            if (t->author != "Project Ascension Archive" || t->id == s_active) {
                PushInt(L, ++i);
                NewTable(L);
                PushTrial(L, *t, true);
                lua_settable(L, -3);
            }
        }

        return 1;
    }

    int GetOwnedTrials(lua_State* L) {   // FUN_10127830
        NewTable(L);
        int i = 0;

        for (const Trial* t : Sorted()) {
            if (t->owned) {
                PushInt(L, ++i);
                NewTable(L);
                PushTrial(L, *t, true);
                lua_settable(L, -3);
            }
        }

        return 1;
    }

    int GetTrialInfo(lua_State* L) {   // FUN_101276d0
        std::string id;

        if (!ReadString(L, id)) {
            return 0;
        }

        if (const Trial* t = Find(id)) {
            NewTable(L);
            PushTrial(L, *t, true);
        } else {
            lua_pushnil(L);
        }

        return 1;
    }

    int GetActiveTrial(lua_State* L) {   // handler_GetActiveTrial
        PushStr(L, s_active.c_str());
        return 1;
    }

    int GetNumTrials(lua_State* L) {
        PushInt(L, static_cast<int32_t>(s_entries.Size()));
        return 1;
    }

    int GetTrialAtIndex(lua_State* L) {   // FUN_10127930 (index, sortConditions)
        if (!ValidateInput(L, { LUA_TNUMBER, LUA_TBOOLEAN })) {
            return 0;
        }

        const int32_t index = ToInt(CheckNumber(L, 1));
        const bool sort = lua_toboolean(L, 2) != 0;

        if (index == 0) {
            return 0;
        }

        const std::vector<size_t>& f = s_entries.Filtered();

        if (static_cast<uint32_t>(index - 1) < f.size() && f[index - 1] < s_trials.size()) {
            NewTable(L);
            PushTrial(L, s_trials[f[index - 1]], sort);
        } else {
            lua_pushnil(L);
        }

        return 1;
    }

    int GetTrialIDByIndex(lua_State* L) {   // FUN_10127a70
        if (!ValidateInput(L, { LUA_TNUMBER })) {
            return 0;
        }

        const int32_t index = ToInt(CheckNumber(L, 1));

        if (index == 0) {
            return 0;
        }

        const std::vector<size_t>& f = s_entries.Filtered();

        if (static_cast<uint32_t>(index - 1) < f.size() && f[index - 1] < s_trials.size()) {
            PushStr(L, s_trials[f[index - 1]].id.c_str());
        } else {
            lua_pushnil(L);
        }

        return 1;
    }

    bool ReadIdNumber(lua_State* L, std::string& id, uint32_t& n) {   // FUN_100e96d0
        if (!ValidateInput(L, { LUA_TSTRING, LUA_TNUMBER })) {
            return false;
        }

        id = CheckString(L, 1);
        n = static_cast<uint32_t>(ToInt(CheckNumber(L, 2)));
        return true;
    }

    int GetTrialCompletion(lua_State* L) {   // FUN_10127220 (id, 1-based index)
        std::string id;
        uint32_t index;

        if (!ReadIdNumber(L, id, index) || index == 0) {
            return 0;
        }

        const std::vector<Completion>& v = s_completions[id];

        if (index - 1 < v.size()) {
            PushCompletion(L, v[index - 1]);
        } else {
            lua_pushnil(L);
        }

        return 1;
    }

    // FUN_10127350 (id, ranks): completions grouped by equal time taken, fastest first.
    int GetTrialCompletions(lua_State* L) {
        std::string id;
        uint32_t ranks;

        if (!ReadIdNumber(L, id, ranks)) {
            return 0;
        }

        NewTable(L);
        std::vector<std::vector<const Completion*>> groups;

        for (const Completion& c : s_completions[id]) {
            const uint64_t d = c.complete - c.start;

            if (groups.empty() || groups.back().back()->complete - groups.back().back()->start != d) {
                groups.emplace_back();
            }

            groups.back().push_back(&c);
        }

        for (uint32_t g = 0; g < ranks && g < groups.size(); ++g) {
            PushNum(L, g + 1);
            NewTable(L);

            for (size_t j = 0; j < groups[g].size(); ++j) {
                PushNum(L, static_cast<double>(j + 1));
                PushCompletion(L, *groups[g][j]);
                lua_settable(L, -3);
            }

            lua_settable(L, -3);
        }

        return 1;
    }

    int SetTrialFilter(lua_State* L) {   // FUN_101286e0 (text, {FILTER_* = bool})
        if (!ValidateInput(L, { LUA_TSTRING, LUA_TTABLE })) {
            return 0;
        }

        const std::string text = CheckString(L, 1);
        std::map<std::string, bool> names;
        lua_pushnil(L);

        while (lua_next(L, 2)) {
            if (lua_isstring(L, -2)) {
                names[lua_tolstring(L, -2, nullptr)] = lua_toboolean(L, -1) != 0;
            }

            lua_settop(L, -2);
        }

        std::vector<uint32_t> filters;

        for (auto& n : names) {
            if (n.second) {
                for (uint32_t i = 0; i < 13; ++i) {
                    if (n.first == kTrialFilters[i]) {
                        filters.push_back(i);
                        break;
                    }
                }
            }
        }

        s_entries.ApplyFilter(text, filters, {});
        return 0;
    }

    // ---- packets ----
    // The 7a convention: a list's count is trusted only while a record's minimum size (its strings empty) is left.
    constexpr uint32_t kTrialMinSize = 35;
    constexpr uint32_t kTrialChallengeMinSize = 12;
    constexpr uint32_t kVoteSize = 8;
    constexpr uint32_t kCompletionMinSize = 36;

    Completion ReadCompletion(CDataStore* p) {   // FUN_10125860
        Completion c;
        c.trial = CoA::Read::Name(p);
        c.player = CoA::Read::Name(p);
        c.start = CoA::Read::U64(p);
        c.complete = CoA::Read::U64(p);
        c.race = CoA::Read::Name(p);
        c.gender = CoA::Read::Name(p);
        c.cls = CoA::Read::Name(p);
        return c;
    }

    void SortCompletions(const std::string& id) {   // FUN_101288b0
        auto it = s_completions.find(id);

        if (it != s_completions.end()) {
            std::sort(it->second.begin(), it->second.end(), [](const Completion& a, const Completion& b) { return a.complete - a.start < b.complete - b.start; });
        }
    }

    int32_t OnSaveResult(void*, uint32_t, uint32_t, CDataStore* p) {   // 0x5A8
        const std::string a = CoA::Read::Name(p);
        const std::string b = CoA::Read::Name(p);
        CoA::Events::Signal("TRIAL_SAVE_RESULT", "%s%s", a.c_str(), b.c_str());
        return 1;
    }

    int32_t OnDeleteResult(void*, uint32_t, uint32_t, CDataStore* p) {   // 0x5AA
        const std::string a = CoA::Read::Name(p);
        const std::string b = CoA::Read::Name(p);
        CoA::Events::Signal("TRIAL_DELETE_RESULT", "%s%s", a.c_str(), b.c_str());
        return 1;
    }

    // 0x5AC { u8 clear, u8 notify, u32 n, n x trial }: appends unless `clear`; the entry container is reset only with
    // `clear` (0x10122DD5), so after an append it keeps listing the trials it had.
    int32_t OnQueryResult(void*, uint32_t, uint32_t, CDataStore* p) {
        if (CoA::Read::U8(p)) {
            s_trials.clear();
            s_entries.Reset();
        }

        const bool notify = CoA::Read::U8(p) != 0;

        for (uint32_t n = CoA::Read::U32(p); n && CoA::Read::Remaining(p) >= kTrialMinSize; --n) {
            Trial t;
            t.id = CoA::Read::Name(p);
            t.owned = CoA::Read::U8(p) != 0;
            t.author = CoA::Read::Name(p);
            t.name = CoA::Read::Name(p);
            t.description = CoA::Read::Name(p);
            t.icon = CoA::Read::Name(p);

            for (uint32_t m = CoA::Read::U32(p); m && CoA::Read::Remaining(p) >= kTrialChallengeMinSize; --m) {
                TrialChallenge c;
                c.challenge = CoA::Read::U32(p);
                c.level = CoA::Read::U32(p);
                c.description = CoA::Read::Name(p);
                t.challenges.push_back(std::move(c));
            }

            t.upvoted = CoA::Read::U8(p) != 0;
            t.downvoted = CoA::Read::U8(p) != 0;

            for (uint32_t m = CoA::Read::U32(p); m && CoA::Read::Remaining(p) >= kVoteSize; --m) {
                t.upvotes.push_back(CoA::Read::U64(p));
            }

            for (uint32_t m = CoA::Read::U32(p); m && CoA::Read::Remaining(p) >= kVoteSize; --m) {
                t.downvotes.push_back(CoA::Read::U64(p));
            }

            s_trials.push_back(std::move(t));
        }

        if (notify) {
            CoA::Events::Signal("TRIAL_QUERY_RESULT", nullptr);
        }

        return 1;
    }

    int32_t OnActivateResult(void*, uint32_t, uint32_t, CDataStore* p) {   // 0x5AE
        const std::string id = CoA::Read::Name(p);
        const std::string result = CoA::Read::Name(p);

        if (result == "ACTIVATE_CHALLENGE_OK") {
            s_active = id;
            Query();
        }

        CoA::Events::Signal("TRIAL_ACTIVATE_RESULT", "%s%s", id.c_str(), TrialName(result).c_str());
        return 1;
    }

    int32_t OnDeactivateResult(void*, uint32_t, uint32_t, CDataStore* p) {   // 0x5B0
        const std::string id = CoA::Read::Name(p);
        const std::string result = CoA::Read::Name(p);

        if (result == "DEACTIVATE_CHALLENGE_OK") {
            s_active.clear();
            Query();
        }

        CoA::Events::Signal("TRIAL_DEACTIVATE_RESULT", "%s%s", id.c_str(), TrialName(result).c_str());
        return 1;
    }

    int32_t OnActiveUpdate(void*, uint32_t, uint32_t, CDataStore* p) {   // 0x5B1
        s_active = CoA::Read::Name(p);
        return 1;
    }

    int32_t OnCompletionList(void*, uint32_t, uint32_t, CDataStore* p) {   // 0x5CA
        const std::string id = CoA::Read::Name(p);
        s_completions.erase(id);

        for (uint32_t n = CoA::Read::U32(p); n && CoA::Read::Remaining(p) >= kCompletionMinSize; --n) {
            s_completions[id].push_back(ReadCompletion(p));
        }

        SortCompletions(id);
        CoA::Events::Signal("TRIAL_COMPLETION_LIST_CHANGED", "%s", id.c_str());
        return 1;
    }

    int32_t OnCompletion(void*, uint32_t, uint32_t, CDataStore* p) {   // 0x5CB
        Completion c = ReadCompletion(p);
        const std::string id = c.trial;
        s_completions[id].push_back(std::move(c));
        SortCompletions(id);
        CoA::Events::Signal("TRIAL_COMPLETION_ADDED", "%s%u", id.c_str(), static_cast<uint32_t>(s_completions[id].size() - 1));
        return 1;
    }

    void OnGlueScreen() {   // FUN_10119df0
        s_trials.clear();
        s_active.clear();
        s_completions.clear();
        s_entries.Reset();
    }

    const CoA::Binding s_bindings[] = {
        COA_NATIVE("C_TrialCreator", "SaveTrial", SaveTrial),
        COA_NATIVE("C_TrialCreator", "CanSaveTrial", CanSaveTrial),
        COA_NATIVE("C_TrialCreator", "DeleteTrial", DeleteTrial),
        COA_NATIVE("C_TrialCreator", "GetAllTrials", GetAllTrials),
        COA_NATIVE("C_TrialCreator", "GetOwnedTrials", GetOwnedTrials),
        COA_NATIVE("C_TrialCreator", "QueryTrials", QueryTrials),
        COA_NATIVE("C_TrialCreator", "CanEditTrial", CanEditTrial),
        COA_NATIVE("C_TrialCreator", "GetTrialInfo", GetTrialInfo),
        COA_NATIVE("C_TrialCreator", "GetActiveTrial", GetActiveTrial),
        COA_NATIVE("C_TrialCreator", "ActivateTrial", ActivateTrial),
        COA_NATIVE("C_TrialCreator", "DeactivateTrial", DeactivateTrial),
        COA_NATIVE("C_TrialCreator", "CanActivateTrial", CanActivateTrial),
        COA_NATIVE("C_TrialCreator", "CanDeactivateTrial", CanDeactivateTrial),
        COA_NATIVE("C_TrialCreator", "RateTrial", RateTrial),
        COA_NATIVE("C_TrialCreator", "QueryTrialCompletions", QueryTrialCompletions),
        COA_NATIVE("C_TrialCreator", "GetTrialCompletions", GetTrialCompletions),
        COA_NATIVE("C_TrialCreator", "GetTrialCompletion", GetTrialCompletion),
        COA_NATIVE("C_TrialCreator", "GetNumTrials", GetNumTrials),
        COA_NATIVE("C_TrialCreator", "GetTrialAtIndex", GetTrialAtIndex),
        COA_NATIVE("C_TrialCreator", "SetTrialFilter", SetTrialFilter),
        COA_NATIVE("C_TrialCreator", "GetTrialIDByIndex", GetTrialIDByIndex),
    };
}

CoA::TrialCreator::Deps& CoA::TrialCreator::Dependencies() {
    return s_deps;
}

bool CoA::TrialCreator::HasActiveTrial() {
    return !s_active.empty();
}

void CoA::TrialCreator::Init() {
    CoA::LuaReg::AddModule(s_bindings, sizeof(s_bindings) / sizeof(s_bindings[0]));
    COA_PACKET(0x5A8, OnSaveResult);
    COA_PACKET(0x5AA, OnDeleteResult);
    COA_PACKET(0x5AC, OnQueryResult);
    COA_PACKET(0x5AE, OnActivateResult);
    COA_PACKET(0x5B0, OnDeactivateResult);
    COA_PACKET(0x5B1, OnActiveUpdate);
    COA_PACKET(0x5CA, OnCompletionList);
    COA_PACKET(0x5CB, OnCompletion);
    CoA::Lifecycle::OnGlueScreen(OnGlueScreen);
    COA_EVENT("TRIAL_SAVE_RESULT");
    COA_EVENT("TRIAL_DELETE_RESULT");
    COA_EVENT("TRIAL_QUERY_RESULT");
    COA_EVENT("TRIAL_ACTIVATE_RESULT");
    COA_EVENT("TRIAL_DEACTIVATE_RESULT");
    COA_EVENT("TRIAL_COMPLETION_LIST_CHANGED");
    COA_EVENT("TRIAL_COMPLETION_ADDED");
}

void CoA::TrialCreator::Reset() {
    OnGlueScreen();
}
