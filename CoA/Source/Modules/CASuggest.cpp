// AscCAMgr.cpp's suggestion manager (0x10bcc618, FUN_103338f0) and inspect: the weighted "what to learn next" lists
// from SpellSpellSuggestions.dbc / SpellStatSuggestions.dbc, their once-a-tick rebuild, SMSG 0x6CE / 0x6D2 (one row
// each), the context-override natives and GetSuggestedStats; CMSG 0x6E1 / SMSG 0x6E2 and the inspect natives.
#include "CoA/Source/Modules/CANativesInternal.h"
#include "CoA/Source/Runtime/Events.h"
#include "CoA/Source/Runtime/Lifecycle.h"
#include "CoA/Source/Runtime/Log.h"
#include "CoA/Source/Runtime/Packet.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Source/Runtime/Read.h"
#include "CoA/Source/Runtime/Script.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <algorithm>

namespace {
#include "CoA/Source/Modules/CAEnums.generated.inc"
}

using namespace CoA::CA;
using namespace CoA::CA::detail;
using CoA::Script::CheckNumber;
using CoA::Script::CheckString;
using CoA::Script::PushBool;
using CoA::Script::PushInt;
using CoA::Script::ReadNumber;
using CoA::Script::ReadString;
using CoA::Script::ToInt;
using CoA::Script::ValidateInput;

namespace {
    const char* const kSpellSuggestions = "DBFilesClient\\SpellSpellSuggestions.dbc";   // +4 source spell, +8 spell, +0xC weight
    const char* const kStatSuggestions = "DBFilesClient\\SpellStatSuggestions.dbc";     // +4 source spell, +8 stat, +0xC weight

    // FUN_10216a60 (map 0x10be20f0) / FUN_10216c60 (map 0x10be2124): the suggestion rows whose source spell the player
    // has, keyed by the entry that grants it.
    std::unordered_map<uint32_t, std::vector<Row>> s_spellSuggestions;
    std::unordered_map<uint32_t, std::vector<Row>> s_statSuggestions;

    void BuildSuggestionMap(std::unordered_map<uint32_t, std::vector<Row>>& map, const char* path) {
        const std::vector<std::pair<uint32_t, Row>>& index = KnownSpellIndex();
        map.clear();
        CoA::Dbc::Table& t = CoA::Dbc::Get(path);

        for (uint32_t id = t.MinId(); t.Loaded() && id <= t.MaxId(); ++id) {
            if (Row s = t.Row(id)) {
                const uint32_t spell = RowU32(s, 4);
                auto it = std::lower_bound(index.begin(), index.end(), spell,
                    [](const std::pair<uint32_t, Row>& p, uint32_t x) { return p.first < x; });

                if (it != index.end() && !(spell < it->first) && it->second) {
                    map[RowU32(it->second, 0)].push_back(s);
                }
            }
        }
    }

    // FUN_10333260: the suggestion context of an entry type name; logs and falls back otherwise.
    const char* const kSuggestionContexts[7] = { "None", "Ability", "Talent", "Trait", "TalentAbility", "Tag", "Suggestion" };

    uint8_t SuggestionContext(const char* name, uint8_t fallback) {
        for (uint8_t i = 0; i < 7; ++i) {
            if (strcmp(name, kSuggestionContexts[i]) == 0) {
                return i;
            }
        }

        CoA::Log::Printf("Unexpected Value: %s", name);
        return fallback;
    }

    // Doubled for a locked source entry, a tenth for one the wildcard roller lists as undesired (single precision,
    // truncated through __ftol like the original).
    uint32_t Weigh(Row src, uint32_t w) {
        if (ActiveBuild()->LockedOf(RowU32(src, 0))) {   // FUN_10155640
            const float f = static_cast<float>(static_cast<double>(w));
            w = static_cast<uint32_t>(static_cast<int64_t>(f + f));
        }

        const Deps& d = Dependencies();

        if (d.wildcardUndesired && d.wildcardUndesired(RowU32(src, 0), SuggestionContext(RowStr(src, 4), 0))) {
            w = static_cast<uint32_t>(static_cast<int64_t>(static_cast<float>(static_cast<double>(w)) * 0.1f));
        }

        return w;
    }

    void AddWeight(std::vector<std::pair<uint32_t, uint32_t>>& list, uint32_t key, uint32_t w) {
        for (auto& p : list) {
            if (p.first == key) {
                p.second += w;
                return;
            }
        }

        list.push_back({ key, w });
    }

    // FUN_10333a00.
    void RecomputeSuggestions() {
        Suggestions& g = Sugg();
        g.dirty = false;
        g.entries.clear();
        g.stats.clear();
        std::vector<uint32_t> ids;

        if (M().suggestionOverrides.empty()) {   // FUN_10171f50
            for (const Entry& e : ActiveBuild()->entries) {
                ids.push_back(e.id);
            }
        } else {
            ids = M().suggestionOverrides;
        }

        for (uint32_t id : ids) {
            auto sp = s_spellSuggestions.find(id);

            if (sp != s_spellSuggestions.end()) {
                for (Row s : sp->second) {
                    Row target = RowBySpell(RowU32(s, 8));
                    Row src = target ? RowBySpell(RowU32(s, 4)) : nullptr;

                    if (src) {
                        AddWeight(g.entries, RowU32(target, 0), Weigh(src, RowU32(s, 0xC)));
                    }
                }
            }

            auto st = s_statSuggestions.find(id);

            if (st != s_statSuggestions.end()) {
                for (Row s : st->second) {
                    if (Row src = RowBySpell(RowU32(s, 4))) {
                        AddWeight(g.stats, RowU32(s, 8), Weigh(src, RowU32(s, 0xC)));
                    }
                }
            }
        }

        auto heavier = [](const std::pair<uint32_t, uint32_t>& a, const std::pair<uint32_t, uint32_t>& b) { return a.second > b.second; };
        std::sort(g.entries.begin(), g.entries.end(), heavier);   // FUN_10332ea0
        std::sort(g.stats.begin(), g.stats.end(), heavier);
        CoA::Events::Signal("CHARACTER_ADVANCEMENT_SUGGESTIONS_UPDATED", nullptr);
        CoA::Events::Signal("STAT_SUGGESTIONS_UPDATED", nullptr);
    }

    // 0x10333960, after 0x403340. Its one-second gate compares against DAT_10d3c180, which nothing writes, so it always
    // passes.
    void SuggestionTick() {
        Suggestions& g = Sugg();

        if (g.statsDirty) {
            g.statsDirty = false;
            BuildSuggestionMap(s_statSuggestions, kStatSuggestions);
        }

        if (g.spellsDirty) {
            g.spellsDirty = false;
            BuildSuggestionMap(s_spellSuggestions, kSpellSuggestions);
        }

        if (g.dirty) {
            RecomputeSuggestions();
        }
    }

    void SuggestionsOnGlue() {   // 0x10333560
        Suggestions& g = Sugg();
        g.statsDirty = g.spellsDirty = g.dirty = true;
        g.entries.clear();
    }

    void DirtyOnListChange(uint32_t, uint32_t) {   // 0x10333950
        Sugg().dirty = true;
    }

    // SMSG 0x6CE (FUN_101e3a70) / 0x6D2 (FUN_101e3b20): one SpellSpellSuggestions / SpellStatSuggestions row {u32 id,
    // u32, u32, u32}, patched into the table.
    void UpsertSuggestionRow(const char* path, CDataStore* msg) {
        uint32_t row[4];

        for (uint32_t& v : row) {
            v = CoA::Read::U32(msg);
        }

        CoA::Dbc::Get(path).Upsert(row[0], std::vector<uint8_t>(reinterpret_cast<uint8_t*>(row), reinterpret_cast<uint8_t*>(row) + 16));
    }

    int32_t OnSpellSuggestion(void*, uint32_t, uint32_t, CDataStore* msg) {
        UpsertSuggestionRow(kSpellSuggestions, msg);
        Sugg().spellsDirty = true;
        Sugg().dirty = true;
        return 1;
    }

    int32_t OnStatSuggestion(void*, uint32_t, uint32_t, CDataStore* msg) {
        UpsertSuggestionRow(kStatSuggestions, msg);
        Sugg().statsDirty = true;
        Sugg().dirty = true;
        return 1;
    }

    std::string StatName(uint32_t s) {   // FUN_10169fc0
        return s < 6 ? kStats[s] : "UNEXPECTED_ENUM_VALUE_" + std::to_string(s);
    }

    // FUN_1017b0d0: (first stat, {stats}) with Strength, Agility, Intellect and Spirit appended when missing; nil, nil
    // before anything is suggested.
    int GetSuggestedStats(lua_State* L) {
        std::vector<uint32_t> stats;

        for (const auto& s : Sugg().stats) {   // FUN_10333590
            stats.push_back(s.first);
        }

        if (stats.empty()) {
            lua_pushnil(L);
            lua_pushnil(L);
            return 2;
        }

        for (uint32_t s : { 0u, 1u, 3u, 4u }) {
            if (std::find(stats.begin(), stats.end(), s) == stats.end()) {
                stats.push_back(s);
            }
        }

        lua_pushstring(L, StatName(stats[0]).c_str());
        lua_createtable(L, 0, static_cast<int>(stats.size()));   // FUN_10169890
        lua_checkstack(L, 2);

        for (size_t i = 0; i < stats.size(); ++i) {
            lua_pushnumber(L, static_cast<double>(i + 1));
            lua_pushstring(L, StatName(stats[i]).c_str());
            lua_settable(L, -3);
        }

        return 2;
    }

    int HasAnySuggestionContextOverrides(lua_State* L) {
        PushBool(L, !M().suggestionOverrides.empty());
        return 1;
    }

    int IsSuggestionContextOverride(lua_State* L) {
        uint32_t id;

        if (!ReadNumber(L, id)) {
            return Usage(L, "Usage: C_CharacterAdvancement.IsSuggestionContextOverride(entryId)");
        }

        const std::vector<uint32_t>& v = M().suggestionOverrides;
        PushBool(L, std::find(v.begin(), v.end(), id) != v.end());
        return 1;
    }

    int AddSuggestionContextOverride(lua_State* L) {
        uint32_t id;

        if (!ReadNumber(L, id)) {
            return Usage(L, "Usage: C_CharacterAdvancement.AddSuggestionContextOverride(entryId)");
        }

        std::vector<uint32_t>& v = M().suggestionOverrides;

        if (std::find(v.begin(), v.end(), id) == v.end()) {
            v.push_back(id);
        }

        Sugg().dirty = true;   // FUN_103339f0(1)
        return 0;
    }

    int RemoveSuggestionContextOverride(lua_State* L) {
        uint32_t id;

        if (!ReadNumber(L, id)) {
            return Usage(L, "Usage: C_CharacterAdvancement.RemoveSuggestionContextOverride(entryId)");
        }

        std::vector<uint32_t>& v = M().suggestionOverrides;
        v.erase(std::remove(v.begin(), v.end(), id), v.end());
        Sugg().dirty = true;
        return 0;
    }

    int ClearSuggestionContextOverrides(lua_State*) {
        M().suggestionOverrides.clear();
        Sugg().dirty = true;
        return 0;
    }

    // ---- inspect -----------------------------------------------------------------------------------------------
    // FUN_1016fe70: SMSG 0x6E2 -- a CA_INSPECT_* string; on CA_INSPECT_OK u64 guid, u32 active spec, u32 spec count and
    // each spec's entries (the 0x726 list). The record goes into the inspect map (or the active player's own);
    // INSPECT_CHARACTER_ADVANCEMENT_RESULT carries the result as sent.
    int32_t OnInspectResult(void*, uint32_t, uint32_t, CDataStore* msg) {
        const std::string result = CoA::Read::Str(msg);
        int index = -1;

        for (int i = 0; i < 6 && index < 0; ++i) {
            if (result == kInspectResults[i]) {
                index = i;
            }
        }

        if (index < 0) {
            CoA::Log::Printf("Unexpected Value: %s", result.c_str());
        } else if (index == 0) {
            const uint64_t guid = CoA::Read::U64(msg);
            const uint32_t spec = CoA::Read::U32(msg);
            const uint32_t specs = CoA::Read::U32(msg);
            Player* player = FindPlayer(guid);

            if (!player) {
                Player*& slot = M().inspect[guid];   // FUN_10168540
                delete slot;
                slot = new Player(guid);
                player = FindPlayer(guid);
            }

            if (!player) {
                return 1;   // the original returns before the event
            }

            player->spec = spec;   // FUN_101499e0

            for (uint32_t i = 0; i < specs; ++i) {
                Build* b = player->Get(i);

                if (!b) {
                    player->Ensure(i);
                    b = player->Get(i);
                }

                if (!b) {
                    return 1;
                }

                std::vector<Entry> fresh;   // FUN_10166760
                const uint32_t n = CoA::Read::U32(msg);

                if (n > CoA::Read::Remaining(msg) / 21) {   // the 0x726 guard: a count past the bytes left reads nothing
                    break;
                }

                for (uint32_t k = 0; k < n; ++k) {
                    Entry e;
                    e.id = CoA::Read::U32(msg);
                    e.rank = CoA::Read::U32(msg);
                    e.u0c = CoA::Read::U32(msg);
                    e.locked = CoA::Read::U8(msg);
                    e.u18 = CoA::Read::U32(msg);
                    e.u1c = CoA::Read::U32(msg);
                    fresh.push_back(e);
                }

                b->entries = fresh;   // FUN_10158010
                b->UpdatePointers(false);
            }

            if (guid == ActivePlayerGuid()) {
                ResetPendingBuild();
            }
        }

        CoA::Events::Signal("INSPECT_CHARACTER_ADVANCEMENT_RESULT", "%s", result.c_str());
        return 1;
    }

    int InspectUnit(lua_State* L) {   // FUN_1017c380: CMSG 0x6E1 u64 guid
        std::string token;

        if (!ReadString(L, token)) {
            return Usage(L, "Usage: C_CharacterAdvancement.InspectUnit(unitToken)");
        }

        uint64_t guid;
        UnitTokenGuid(token.c_str(), guid);
        CoA::Packet(0x6E1).U64(guid).Send();
        PushBool(L, true);
        return 1;
    }

    // FUN_10177940: (active spec + 1, {specs with entries, 1-based}); nil, nil for an unknown unit.
    int GetInspectInfo(lua_State* L) {
        std::string token;

        if (!ReadString(L, token)) {
            return Usage(L, "Usage: C_CharacterAdvancement.GetInspectInfo(unitToken)");
        }

        uint64_t guid;
        UnitTokenGuid(token.c_str(), guid);
        Player* player = FindPlayer(guid);

        if (!player) {
            lua_pushnil(L);
            lua_pushnil(L);
            return 2;
        }

        PushInt(L, static_cast<int32_t>(player->spec + 1));
        std::vector<uint32_t> specs;

        for (uint32_t i = 0; i < 0x14; ++i) {
            if (Build* b = player->Get(i)) {
                if (!b->entries.empty()) {
                    specs.push_back(i + 1);
                }
            }
        }

        PushIntArray(L, specs);
        return 2;
    }

    // LAB_101803c0: abilities first, then talents, then by class type (+0x80), then higher rank first.
    bool InspectedOrder(const Entry& a, const Entry& b) {
        Row ra = FindRow(a.id);
        Row rb = FindRow(b.id);

        if (!ra || !rb) {
            return false;
        }

        const bool aa = KindSpell(ra);
        const bool ab = KindSpell(rb);

        if (aa != ab) {
            return aa;
        }

        const bool ta = KindTalent(ra);
        const bool tb = KindTalent(rb);

        if (ta != tb) {
            return ta;
        }

        if (RowU32(ra, 0x80) != RowU32(rb, 0x80)) {
            return RowU32(ra, 0x80) < RowU32(rb, 0x80);
        }

        return a.rank > b.rank;
    }

    // FUN_10177ae0: {{EntryId, Rank, Locked}, ...} for one of the unit's specs.
    int GetInspectedBuild(lua_State* L) {
        if (!ValidateInput(L, { LUA_TSTRING, LUA_TNUMBER })) {
            return Usage(L, "Usage: C_CharacterAdvancement.GetInspectedBuild(unitToken, specialization)");
        }

        const std::string token = CheckString(L, 1);
        const uint32_t spec = static_cast<uint32_t>(ToInt(CheckNumber(L, 2)));

        if (spec == 0) {
            return Usage(L, "GetInspectedBuild: specialization must be >= 1");
        }

        uint64_t guid;
        UnitTokenGuid(token.c_str(), guid);
        Player* player = FindPlayer(guid);
        Build* b = player ? player->Get(spec - 1) : nullptr;

        if (!b) {
            lua_pushnil(L);
            return 1;
        }

        std::vector<Entry> entries = b->entries;                       // FUN_1014e460
        std::sort(entries.begin(), entries.end(), InspectedOrder);     // FUN_101681b0
        lua_createtable(L, 0, 0);
        lua_checkstack(L, 2);

        for (size_t i = 0; i < entries.size(); ++i) {
            lua_pushinteger(L, static_cast<int32_t>(i + 1));
            lua_createtable(L, 0, 0);
            lua_checkstack(L, 2);
            SetInt(L, "EntryId", static_cast<int32_t>(entries[i].id));
            SetInt(L, "Rank", static_cast<int32_t>(entries[i].rank));
            SetInt(L, "Locked", entries[i].locked);
            lua_settable(L, -3);
        }

        return 1;
    }

    const CoA::Binding s_bindings[] = {
        COA_NATIVE("C_CharacterAdvancement", "HasAnySuggestionContextOverrides", HasAnySuggestionContextOverrides),
        COA_NATIVE("C_CharacterAdvancement", "IsSuggestionContextOverride", IsSuggestionContextOverride),
        COA_NATIVE("C_CharacterAdvancement", "AddSuggestionContextOverride", AddSuggestionContextOverride),
        COA_NATIVE("C_CharacterAdvancement", "RemoveSuggestionContextOverride", RemoveSuggestionContextOverride),
        COA_NATIVE("C_CharacterAdvancement", "ClearSuggestionContextOverrides", ClearSuggestionContextOverrides),
        COA_NATIVE("C_CharacterAdvancement", "GetSuggestedStats", GetSuggestedStats),
        COA_NATIVE("C_CharacterAdvancement", "InspectUnit", InspectUnit),
        COA_NATIVE("C_CharacterAdvancement", "GetInspectInfo", GetInspectInfo),
        COA_NATIVE("C_CharacterAdvancement", "GetInspectedBuild", GetInspectedBuild),
    };
}

std::vector<uint32_t> CoA::CA::detail::SuggestedFor(uint32_t category) {
    std::vector<uint32_t> out;

    for (const auto& s : Sugg().entries) {
        Row r = FindRow(s.first);

        if (!r) {
            continue;
        }

        bool inCategory = category == 0;

        for (uint32_t off = 0x1C8; off < 0x1D4 && !inCategory; off += 4) {
            inCategory = RowU32(r, off) == category;
        }

        if (!inCategory || ActiveBuild()->Has(RowU32(r, 0))) {
            continue;
        }

        out.push_back(s.first);

        if (out.size() >= 10) {
            break;
        }
    }

    return out;
}

std::vector<uint32_t> CoA::CA::detail::SuggestedEntries(bool abilities, uint32_t max, bool skipKnown) {
    std::vector<uint32_t> out;

    for (const auto& s : Sugg().entries) {
        Row r = FindRow(s.first);

        if (!r || KindSpell(r) != abilities || (skipKnown && ActiveBuild()->Has(RowU32(r, 0)))) {   // FUN_101c6770
            continue;
        }

        out.push_back(s.first);

        if (max <= out.size()) {
            break;
        }
    }

    return out;
}

void CoA::CA::detail::ResetSuggest() {
    Sugg() = Suggestions();
    s_spellSuggestions.clear();
    s_statSuggestions.clear();
}

void CoA::CA::SuggestionsDirty(uint32_t) {   // 0x10333950
    Sugg().dirty = true;
}

std::vector<uint32_t> CoA::CA::SuggestedList(bool abilities, uint32_t max, bool skipKnown) {   // FUN_10333770
    return detail::SuggestedEntries(abilities, max, skipKnown);
}

// The suggestion manager's registration (0x10333880): glue reset, the tick, and a dirty mark on every CA list change.
// Its marks in the wildcard manager's two lists (0x10be2cac / 0x10be2ccc) are registered by CoA::Wildcard::Init, which
// owns the lists (CoA::CA::SuggestionsDirty).
void CoA::CA::detail::InitSuggest() {
    COA_PACKET(0x6CE, OnSpellSuggestion);
    COA_PACKET(0x6E2, OnInspectResult);
    COA_PACKET(0x6D2, OnStatSuggestion);
    CoA::Lifecycle::OnGlueScreen(SuggestionsOnGlue);
    CoA::Lifecycle::OnAfterIdle(SuggestionTick);
    Mgr& m = M();

    for (std::vector<PairCallback>* list : { &m.onLearned, &m.onRankChanged, &m.onUnlearned, &m.onSpecChanged }) {
        list->push_back(DirtyOnListChange);
    }

    COA_EVENT("CHARACTER_ADVANCEMENT_SUGGESTIONS_UPDATED");
    COA_EVENT("STAT_SUGGESTIONS_UPDATED");
    COA_EVENT("INSPECT_CHARACTER_ADVANCEMENT_RESULT");
    CoA::LuaReg::AddModule(s_bindings, sizeof(s_bindings) / sizeof(s_bindings[0]));
}
