// AscCAFilter.cpp: the character-advancement browser filters, transcribed from the original Extensions.dll.
//
// Ascension built these on its klib library: a basic_filterable_builder<CharacterAdvancementEntry const*,
// filterable_index<...>> over CharacterAdvancement.dbc with one registered enum filter, whose query is a set of OR groups
// (one per FILTER_* group) evaluated in index order. Four containers, one code shape:
//
//   CharacterAdvancementMgr +0x028  category container      SetFilteredEntriesByCategory, categories outside
//                                                           0x1F..0x34 (FUN_101c6e20)
//   CharacterAdvancementMgr +0x190  category container      categories 0x1F..0x34
//   CharacterAdvancementMgr +0x2F8  entry container         C_CharacterAdvancement.SetFilteredEntries,
//                                                           GetNum/GetFilteredEntryAtIndex, IsFiltered
//   BuildCreator            +0x228  build-creator container C_BuildEditor.SetFilteredEntries & co.
//
//   +0x000  index: every row FUN_101c67c0(0) does not hide whose class type (+0x80) is not 1, in id order
//           (FUN_101c1960). Built on first use; dropped only by the resets.
//   +0x0A0  results                                    +0x0AC  the query as last given
//   +0x0C4  filter ids                                 +0x0D0  '$' tokens {char, int}
//   +0x0DC  a third id list (every caller passes it empty)
//   category containers also: +0x0E8 category -> bucket of result rows, +0x108 / +0x114 spell tag types (current /
//   previous), +0x120 / +0x140 entry-id sets (RebuildRecentSets), +0x160 dirty.
//
// Apply (FUN_101c4bf0 / FUN_101c5070 / FUN_101c47c0): strip the '$' tokens from a copy of the query (FUN_101c4370), then
// either refine the current results or rebuild them from the index (the refine is taken only when the old query is a
// substring of the new one, the old filter list was empty (FUN_101ba9b0 reduces to that) and the old tokens are a
// subset). Either way: any token empties the results; otherwise keep the rows that pass every filter group (OR within
// a group), then the rows FUN_101c1ce0 matches against the query, then sort by the container's 256-bit key. The
// category containers skip all of it when nothing changed since the last call.
#include "CoA/Source/Modules/CAFilter.h"
#include "CoA/Source/Modules/CANativesInternal.h"
#include "CoA/Source/Modules/PatchData.h"
#include "CoA/Source/Runtime/Lifecycle.h"
#include "CoA/Source/Runtime/Log.h"
#include "CoA/Source/Runtime/Script.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <algorithm>
#include <cctype>
#include <iterator>
#include <unordered_map>
#include <unordered_set>

namespace {
#include "CoA/Source/Modules/CAEnums.generated.inc"
}

using namespace CoA::CA;
using namespace CoA::CA::detail;
using CoA::Script::PushBool;
using CoA::Script::PushInt;
using CoA::Script::PushNils;
using CoA::Script::ReadInt2;
using CoA::Script::ReadNumber;
using CoA::Script::ValidateInput;

namespace {
    // ---- the three FILTER_* enums ------------------------------------------------------------------------------
#define COA_CA_FILTER_COMMON                                                                                              \
    "FILTER_NONE", "FILTER_QUALITY_NORMAL", "FILTER_QUALITY_UNCOMMON", "FILTER_QUALITY_RARE", "FILTER_QUALITY_EPIC",       \
        "FILTER_QUALITY_LEGENDARY", "FILTER_CLASS_HUNTER", "FILTER_CLASS_WARRIOR", "FILTER_CLASS_ROGUE", "FILTER_CLASS_MAGE", \
        "FILTER_CLASS_PRIEST", "FILTER_CLASS_WARLOCK", "FILTER_CLASS_DRUID", "FILTER_CLASS_SHAMAN", "FILTER_CLASS_PALADIN",   \
        "FILTER_CLASS_DEATH_KNIGHT", "FILTER_CLASS_GENERAL", "FILTER_CLASS_HERO", "FILTER_CLASS_BARBARIAN",                 \
        "FILTER_CLASS_WITCHDOCTOR", "FILTER_CLASS_DEMONHUNTER", "FILTER_CLASS_WITCHHUNTER", "FILTER_CLASS_STORMBRINGER",    \
        "FILTER_CLASS_KNIGHT_OF_XOROTH", "FILTER_CLASS_GUARDIAN", "FILTER_CLASS_MONK", "FILTER_CLASS_SON_OF_ARUGAL",        \
        "FILTER_CLASS_RANGER", "FILTER_CLASS_CHRONOMANCER", "FILTER_CLASS_NECROMANCER", "FILTER_CLASS_PYROMANCER",          \
        "FILTER_CLASS_CULTIST", "FILTER_CLASS_STARCALLER", "FILTER_CLASS_SUN_CLERIC", "FILTER_CLASS_TINKER",                \
        "FILTER_CLASS_PRIMALIST", "FILTER_CLASS_REAPER", "FILTER_CLASS_VENOMANCER", "FILTER_CLASS_RUNEMASTER",              \
        "FILTER_CLASS_CONQUEST_OF_AZEROTH", "FILTER_CLASS_REBORN_HUNTER", "FILTER_CLASS_REBORN_WARRIOR",                    \
        "FILTER_CLASS_REBORN_ROGUE", "FILTER_CLASS_REBORN_MAGE", "FILTER_CLASS_REBORN_PRIEST", "FILTER_CLASS_REBORN_WARLOCK", \
        "FILTER_CLASS_REBORN_DRUID", "FILTER_CLASS_REBORN_SHAMAN", "FILTER_CLASS_REBORN_PALADIN",                           \
        "FILTER_CLASS_REBORN_DEATH_KNIGHT", "FILTER_CLASS_REBORN_GENERAL", "FILTER_TYPE_ABILITY", "FILTER_TYPE_TALENT",     \
        "FILTER_TYPE_TRAIT"

    // 0x10B34780 (SetFilteredEntriesByCategory)
    const char* const kCategoryFilters[0x3D] = { COA_CA_FILTER_COMMON, "FILTER_CAN_LEARN", "FILTER_CAN_UNLEARN", "FILTER_KNOWN",
        "FILTER_UNKNOWN", "FILTER_REQUIRED_IN_RANGE_2", "FILTER_WCR_TRAITS", "FILTER_SPELL_TAGS" };
    // 0x10B1CE48 (C_CharacterAdvancement.SetFilteredEntries, FUN_1016b410)
    const char* const kEntryFilters[0x3B] = { COA_CA_FILTER_COMMON, "FILTER_CAN_LEARN", "FILTER_CAN_UNLEARN", "FILTER_KNOWN",
        "FILTER_UNKNOWN", "FILTER_WCR_TRAITS" };
    // 0x10B21088 (C_BuildEditor.SetFilteredEntries, FUN_100f3010)
    const char* const kEditorFilters[0x3A] = { COA_CA_FILTER_COMMON, "FILTER_CAN_ADD", "FILTER_CAN_REMOVE", "FILTER_KNOWN",
        "FILTER_UNKNOWN" };
#undef COA_CA_FILTER_COMMON

    enum class Kind { Category, Entry, Editor };

    using Token = std::pair<char, int32_t>;

    struct Container {
        explicit Container(Kind k) : kind(k) {
        }

        Kind kind;
        std::vector<Row> index;                                   // +0x000
        std::vector<Row> results;                                 // +0x0A0
        std::string query;                                        // +0x0AC
        std::vector<uint32_t> filters;                            // +0x0C4
        std::vector<Token> tokens;                                // +0x0D0
        std::vector<uint32_t> third;                              // +0x0DC
        std::unordered_map<uint32_t, std::vector<Row>> buckets;   // +0x0E8 (category containers)
        std::vector<uint32_t> tags;                               // +0x108
        std::vector<uint32_t> prevTags;                           // +0x114
        std::unordered_set<uint32_t> recent;                      // +0x120
        std::unordered_set<uint32_t> upcoming;                    // +0x140
        bool dirty = true;                                        // +0x160 (category) / +0x0E8 (the others)
    };

    Container s_normal(Kind::Category);   // mgr +0x028
    Container s_classes(Kind::Category);  // mgr +0x190
    Container s_entries(Kind::Entry);     // mgr +0x2F8
    Container s_editor(Kind::Editor);     // BuildCreator +0x228
    uint32_t s_category = 0;              // DAT_10bde3e4: the category of the last SetFilteredEntriesByCategory

    CoA::Dbc::Table& Categories() {
        return CoA::Dbc::Get("DBFilesClient/CharacterAdvancementCategories.dbc");
    }

    CoA::Dbc::Table& TagTypes() {
        return CoA::Dbc::Get("DBFilesClient/SpellTagTypes.dbc");
    }

    bool ClassCategory(uint32_t category) {   // FUN_101c6e20
        return category >= 0x1F && category <= 0x34;
    }

    Container& CategoryContainer(uint32_t category) {
        return ClassCategory(category) ? s_classes : s_normal;
    }

    // ---- row fields --------------------------------------------------------------------------------------------
    // FUN_1014e100 over a string: its index among the nine quality names, -1 when none.
    int QualityOf(const char* s) {
        for (int i = 0; i < 9; ++i) {
            if (strcmp(s, kQualities[i]) == 0) {
                return i;
            }
        }

        return -1;
    }

    uint32_t RankSpell(Row r, uint8_t rank) {   // FUN_101c62e0
        return SpellAtRankOf(r, rank);
    }

    bool HasActivePlayer() {   // 0x4038F0
        return UnitExists(ActivePlayerGuid());
    }

    uint8_t PlayerClass() {
        return HasActivePlayer() ? UnitClass(ActivePlayerGuid()) : 0;
    }

    // ---- FUN_101bf110: ASCII case-insensitive substring, an empty needle always matching ---------------------
    char Fold(char c) {
        return static_cast<uint8_t>(c - 'A') <= 0x19 ? static_cast<char>(c + 0x20) : c;
    }

    bool Contains(const char* hay, size_t hayLen, const std::string& needle) {
        const size_t n = needle.size();

        if (n == 0) {
            return true;
        }

        if (n > hayLen) {
            return false;
        }

        for (size_t i = 0; i <= hayLen - n; ++i) {
            size_t j = 0;

            while (j < n && Fold(hay[i + j]) == Fold(needle[j])) {
                ++j;
            }

            if (j == n) {
                return true;
            }
        }

        return false;
    }

    bool Contains(const std::string& hay, const std::string& needle) {
        return Contains(hay.data(), hay.size(), needle);
    }

    // ---- FUN_101c1ce0: does the row match the query? -----------------------------------------------------------
    // The gates first unless `skipGates` (the build creator's container): then per spell of the chain its name, the
    // forms it requires and its described text; then the row's name (+0xBC) and the strings at +0xC4 / +0xC8.
    bool TextMatch(Row r, const std::string& q, bool skipGates) {
        if (!skipGates && Hidden(r, false)) {
            return false;
        }

        for (uint32_t off = 0x14; off < 0x38; off += 4) {
            const uint32_t spell = RowU32(r, off);

            if (!spell) {
                continue;
            }

            const Deps& d = Dependencies();
            static const std::string empty;
            const std::string& name = d.spellName ? d.spellName(spell) : empty;

            if (!name.empty() && Contains(name, q)) {
                return true;
            }

            const std::string& forms = d.spellForms ? d.spellForms(spell) : empty;

            if (!forms.empty() && Contains(forms, q)) {
                return true;
            }

            if (Contains(d.spellDescription ? d.spellDescription(spell) : std::string(), q)) {
                return true;
            }
        }

        for (uint32_t off : { 0xBCu, 0xC4u, 0xC8u }) {
            const char* s = RowText(r, off);

            if (Contains(s, strlen(s), q)) {
                return true;
            }
        }

        return false;
    }

    // ---- the filter predicates ---------------------------------------------------------------------------------
    // FILTER ids 1..53 mean the same in all three enums.
    bool CommonFilter(uint32_t id, Row r, bool& handled) {
        handled = true;

        if (id >= 1 && id <= 5) {   // FUN_1014e100(+0x40) == id
            return QualityOf(RowText(r, 0x40)) == static_cast<int>(id);
        }

        if (id >= 6 && id <= 0x32) {
            return RowU32(r, 0x80) == id - 4;
        }

        const int type = RowType(r);

        switch (id) {
            case 0x33: return type == 1 || type == 4;
            case 0x34: return type == 2;
            case 0x35: return type == 3;
        }

        handled = false;
        return false;
    }

    uint32_t PendingLearnResult(uint32_t id) {   // FUN_10151480 on the pending build, no include / exclude lists
        Build* p = PendingBuild();
        return p ? p->ValidateLearn(0, id, {}, {}) : 0;
    }

    bool PendingKnows(uint32_t id) {   // FUN_10155060
        Build* p = PendingBuild();
        return p && p->Has(id);
    }

    bool PendingCanUnlearn(uint32_t id) {
        Build* p = PendingBuild();
        return p && p->ValidateUnlearn(0, id, {}, {}) == 0;
    }

    // FUN_101c2c50 (category containers, 0x3D enum). `tags` is the container's +0x108.
    bool CategoryFilter(uint32_t id, Row r, const std::vector<uint32_t>& tags) {
        bool handled;
        const bool common = CommonFilter(id, r, handled);

        if (handled) {
            return common;
        }

        const uint32_t entry = RowU32(r, 0);

        switch (id) {
            case 0x36: {   // FILTER_CAN_LEARN: learnable, or failing only on slot 0x2C
                const uint32_t result = PendingLearnResult(entry);
                return result == 0 || result == 0x2C;
            }
            case 0x37: return PendingCanUnlearn(entry);
            case 0x38: return PendingKnows(entry);
            case 0x39: return !PendingKnows(entry);
            case 0x3A:   // FILTER_REQUIRED_IN_RANGE_2: the mode's level column at most two above the player's
                if (!HasActivePlayer()) {
                    return false;
                }

                return RowU32(r, ModeColumn(0x68, 0x6C, 0x70)) <= UnitLevel(ActivePlayerGuid()) + 2;
            case 0x3B: {   // FILTER_WCR_TRAITS: every non-trait; a trait only when a stock class knows it
                if (RowType(r) != 3) {
                    return true;
                }

                const uint8_t cls = PlayerClass();
                return (cls >= 1 && cls <= 9) || cls == 11 ? PendingKnows(entry) : true;
            }
            case 0x3C:   // FILTER_SPELL_TAGS: some spell of the chain carries every tag type asked for
                for (uint32_t off = 0x14; off < 0x38; off += 4) {
                    if (tags.empty()) {
                        return true;
                    }

                    const uint32_t spell = RowU32(r, off);

                    if (!spell) {
                        continue;
                    }

                    const std::vector<uint32_t>& has = SpellTagTypesOf(spell);

                    if (has.empty()) {
                        continue;
                    }

                    bool all = true;

                    for (uint32_t t : tags) {
                        if (std::find(has.begin(), has.end(), t) == has.end()) {
                            all = false;
                            break;
                        }
                    }

                    if (all) {
                        return true;
                    }
                }

                return false;
        }

        return false;
    }

    // FUN_101c3860 (entry container, 0x3B enum).
    bool EntryFilter(uint32_t id, Row r) {
        bool handled;
        const bool common = CommonFilter(id, r, handled);

        if (handled) {
            return common;
        }

        const uint32_t entry = RowU32(r, 0);

        switch (id) {
            case 0x36: {
                const uint32_t result = PendingLearnResult(entry);
                return result == 0 || result == 0x2C;
            }
            case 0x37: return PendingCanUnlearn(entry);
            case 0x38: return !InFlight(entry) && PendingKnows(entry);   // FILTER_KNOWN: never while a request is in flight
            case 0x39: return InFlight(entry) || !PendingKnows(entry);   // FILTER_UNKNOWN: always while one is
            case 0x3A: return RowType(r) != 3;                           // FILTER_WCR_TRAITS
        }

        return false;
    }

    // FUN_101c2320 (build creator container, 0x3A enum): the pending build creator entry, asked per spell of the
    // chain.
    bool AnySpell(Row r, bool (*test)(uint32_t)) {
        if (!test) {
            return false;
        }

        for (uint32_t off = 0x14; off < 0x38; off += 4) {
            if (const uint32_t spell = RowU32(r, off)) {
                if (test(spell)) {
                    return true;
                }
            }
        }

        return false;
    }

    bool EditorFilter(uint32_t id, Row r) {
        bool handled;
        const bool common = CommonFilter(id, r, handled);

        if (handled) {
            return common;
        }

        const Deps& d = Dependencies();

        switch (id) {
            case 0x36: return AnySpell(r, d.creatorCanAddSpell);      // FUN_100faa20
            case 0x37: return AnySpell(r, d.creatorCanRemoveSpell);   // FUN_100fb960
            case 0x38: return AnySpell(r, d.creatorHasSpell);         // FUN_10100180
            case 0x39: return !AnySpell(r, d.creatorHasSpell);
        }

        return false;
    }

    bool Filter(const Container& c, uint32_t id, Row r) {
        switch (c.kind) {
            case Kind::Category: return CategoryFilter(id, r, c.tags);
            case Kind::Entry: return EntryFilter(id, r);
            default: return EditorFilter(id, r);
        }
    }

    // FUN_101c5df0 / FUN_101c5ea0 / FUN_101c5d60: the group a filter id ORs within.
    uint32_t GroupOf(Kind kind, uint32_t id) {
        if (id >= 1 && id <= 5) {
            return 1;
        }

        if (id >= 6 && id <= 0x32) {
            return 2;
        }

        if (id >= 0x33 && id <= 0x35) {
            return 3;
        }

        if (id >= 0x36 && id <= 0x37) {
            return 4;
        }

        if (id >= 0x38 && id <= 0x39) {
            return 5;
        }

        if (kind == Kind::Category) {
            return id == 0x3A ? 8 : id == 0x3B ? 6 : id == 0x3C ? 7 : 0;
        }

        if (kind == Kind::Entry) {
            return id == 0x3A ? 6 : 0;
        }

        return 0;
    }

    using Groups = std::vector<std::pair<uint32_t, std::vector<uint32_t>>>;

    // The groups in first-seen order, each with its ids in list order.
    Groups GroupsOf(Kind kind, const std::vector<uint32_t>& ids) {
        Groups groups;

        for (uint32_t id : ids) {
            const uint32_t g = GroupOf(kind, id);
            auto it = std::find_if(groups.begin(), groups.end(), [g](const auto& x) { return x.first == g; });

            if (it == groups.end()) {
                groups.push_back({ g, { id } });
            } else {
                it->second.push_back(id);
            }
        }

        return groups;
    }

    // The klib query (FUN_101becc0 groups, FUN_101cc170 walk): every group has a member that passes.
    bool PassesGroups(const Container& c, const Groups& groups, Row r) {
        for (const auto& g : groups) {
            bool any = false;

            for (uint32_t id : g.second) {
                if (Filter(c, id, r)) {
                    any = true;
                    break;
                }
            }

            if (!any) {
                return false;
            }
        }

        return true;
    }

    // FUN_101bf4d0 / FUN_101bf780 (the refine): the same groups, except that group 0 only asks its first member.
    bool PassesGroupsRefine(const Container& c, const Groups& groups, Row r) {
        for (const auto& g : groups) {
            size_t i = 0;

            while (!Filter(c, g.second[i], r)) {
                if (g.first == 0 || ++i == g.second.size()) {
                    return false;
                }
            }
        }

        return true;
    }

    // ---- the sort keys: 256-bit integers built by shift-and-or (FUN_101b2bb0 / FUN_101b2770) ---------------------
    // A field wider than its shift overlaps the next one up, as in the original.
    struct Key {
        uint32_t w[8] = {};

        void Push(uint32_t bits, uint32_t value) {
            if (bits >= 256) {
                std::fill(std::begin(this->w), std::end(this->w), 0u);
            } else if (bits) {
                const uint32_t words = bits / 32;
                const uint32_t rest = bits % 32;

                for (int i = 7; i >= 0; --i) {
                    uint32_t v = 0;
                    const int src = i - static_cast<int>(words);

                    if (src >= 0) {
                        v = rest ? this->w[src] << rest : this->w[src];

                        if (rest && src >= 1) {
                            v |= this->w[src - 1] >> (32 - rest);
                        }
                    }

                    this->w[i] = v;
                }
            }

            this->w[0] |= value;
        }

        bool operator<(const Key& o) const {
            for (int i = 7; i >= 0; --i) {
                if (this->w[i] != o.w[i]) {
                    return this->w[i] < o.w[i];
                }
            }

            return false;
        }
    };

    uint32_t TypeBits(Row r, Key& k) {   // the four "is not <type>" bits (a missing type counts as not)
        const int type = RowType(r);
        k.Push(1, type != 1);
        k.Push(1, type != 4);
        k.Push(1, type != 2);
        k.Push(1, type != 3);
        // FUN_1014e100 is handed the TYPE string (row +4), never a quality name, so this is 7 - 0.
        const int q = QualityOf(RowText(r, 4));
        return 7u - static_cast<uint8_t>(q < 0 ? 0 : q);
    }

    // FUN_10152fc0 on the pending build: -1 when the entry is absent, else how many entries follow it in (+0x1C, +0x18)
    // order.
    uint32_t OrderAfter(uint32_t id) {
        Build* p = PendingBuild();

        if (!p) {
            return 0xFFFFFFFFu;
        }

        const std::vector<Entry>& v = p->entries;
        size_t it = 0;

        while (it < v.size() && v[it].id != id) {
            ++it;
        }

        if (it == v.size()) {
            return 0xFFFFFFFFu;
        }

        const uint32_t u = v[it].u1c;
        size_t j = 0;

        while (v[j].u1c <= u) {
            if ((u <= v[j].u1c && v[it].u18 < v[j].u18) || ++j == v.size()) {
                break;
            }
        }

        if (j == v.size()) {
            return 0;
        }

        uint32_t count = 0;

        for (;;) {
            ++count;
            size_t k;

            do {
                k = j;
                j = k + 1;

                if (j == v.size() || u < v[k + 1].u1c) {
                    break;
                }
            } while (v[k + 1].u1c < u || v[k + 1].u18 <= v[it].u18);

            if (j == v.size()) {
                return count;
            }
        }
    }

    // FUN_10217220 (DAT_10be0750): the STAT_* names in SpellTagTypes.dbc +0x5C..+0x68, per RECORD INDEX (the vector is
    // looked up by tag type id, as the original does), MAX_STATS dropped.
    std::vector<std::vector<uint32_t>> s_tagTypeStats;
    bool s_tagTypeStatsBuilt = false;

    const std::vector<uint32_t>& StatsOfTagType(uint32_t tagType) {
        static const std::vector<uint32_t> none;

        if (!s_tagTypeStatsBuilt) {
            s_tagTypeStatsBuilt = true;
            s_tagTypeStats.clear();
            CoA::Dbc::Table& t = TagTypes();
            s_tagTypeStats.resize(t.Loaded() ? t.MaxId() + 1 : 0);

            for (uint32_t i = 0; i < t.Count() && i < s_tagTypeStats.size(); ++i) {
                if (const uint8_t* rec = t.RowAt(i)) {
                    for (uint32_t off = 0x5C; off < 0x68; off += 4) {
                        const char* s = t.Str(rec, off);

                        for (uint32_t k = 0; k < 6; ++k) {
                            if (strcmp(s, kStats[k]) == 0) {
                                if (k != 5) {
                                    s_tagTypeStats[i].push_back(k);
                                }

                                break;
                            }
                        }
                    }
                }
            }
        }

        return tagType < s_tagTypeStats.size() ? s_tagTypeStats[tagType] : none;
    }

    // FUN_10172ca0: a pending entry currently grants the first rank of `spell`.
    bool SpellRequirementMet(uint32_t spell) {
        Build* p = PendingBuild();

        if (!p) {
            return false;
        }

        const uint32_t first = FirstRank(spell);

        for (const Entry& e : p->entries) {
            if (e.row && RankSpell(e.row, static_cast<uint8_t>(e.rank)) == first) {
                return true;
            }
        }

        return false;
    }

    // FUN_10172c30: a pending entry's current spell carries the tag type.
    bool TagRequirementMet(uint32_t tag) {
        Build* p = PendingBuild();

        if (!p) {
            return false;
        }

        for (const Entry& e : p->entries) {
            if (!e.row) {
                continue;
            }

            const std::vector<uint32_t>& tags = SpellTagTypesOf(RankSpell(e.row, static_cast<uint8_t>(e.rank)));

            if (std::find(tags.begin(), tags.end(), tag) != tags.end()) {
                return true;
            }
        }

        return false;
    }

    // FUN_101c6560: a tag type on one of the row's spells has a requirement the pending build misses -- a required
    // spell (+0x14..), a required tag (+0x38..) or a primary-stat entry (0x47D..0x480).
    bool UnmetTagRequirement(Row r) {
        CoA::Dbc::Table& t = TagTypes();

        for (uint32_t off = 0x14; off < 0x38; off += 4) {
            const uint32_t spell = RowU32(r, off);

            if (!spell) {
                continue;
            }

            for (uint32_t tagType : SpellTagTypesOf(spell)) {
                const uint8_t* tr = t.Row(tagType);

                if (!tr) {
                    continue;
                }

                auto requirementMet = [tr](uint32_t lo, bool (*met)(uint32_t)) {
                    bool any = false;

                    for (uint32_t o = lo; o < lo + 0x24; o += 4) {
                        if (RowU32(tr, o)) {
                            any = true;

                            if (met(RowU32(tr, o))) {
                                return true;
                            }
                        }
                    }

                    return !any;
                };

                if (!requirementMet(0x14, SpellRequirementMet) || !requirementMet(0x38, TagRequirementMet)) {
                    return true;
                }

                const std::vector<uint32_t>& stats = StatsOfTagType(tagType);

                if (stats.empty()) {
                    continue;
                }

                bool met = false;

                for (uint32_t s : stats) {
                    static const uint32_t kStatEntries[5] = { 0x47D, 0x47E, 0, 0x47F, 0x480 };

                    if (s < 5 && kStatEntries[s] && PendingKnows(kStatEntries[s])) {
                        met = true;
                        break;
                    }
                }

                if (!met) {
                    return true;
                }
            }
        }

        return false;
    }

    // FUN_101c7570 (category containers).
    Key CategoryKey(Row r) {
        const uint32_t id = RowU32(r, 0);
        Build* p = PendingBuild();
        Key k;
        k.Push(1, !PendingKnows(id));
        k.Push(1, PendingLearnResult(id) != 0);

        for (uint32_t off : { 0xA4u, 0x88u, 0x8Cu, 0x90u, 0x94u, 0x98u, 0x9Cu }) {
            k.Push(8, RowU32(r, off));
        }

        k.Push(1, UnmetTagRequirement(r));
        const std::vector<uint32_t> suggested = SuggestedFor(s_category);
        k.Push(8, static_cast<uint32_t>(std::find(suggested.begin(), suggested.end(), id) - suggested.begin()));
        k.Push(8, RowU32(r, 0x1D4));
        k.Push(1, !(p && p->LockedOf(id)));
        k.Push(8, OrderAfter(id));
        uint32_t cls;

        switch (RowU32(r, 0x80)) {
            case 2: case 0x24: cls = 3; break;
            case 3: case 0x25: cls = 1; break;
            case 4: case 0x26: cls = 2; break;
            case 5: case 0x27: cls = 10; break;
            case 6: case 0x28: cls = 7; break;
            case 7: case 0x29: cls = 8; break;
            case 8: case 0x2A: cls = 4; break;
            case 9: case 0x2B: cls = 5; break;
            case 10: case 0x2C: cls = 6; break;
            case 11: case 0x2D: cls = 9; break;
            default: cls = 0xFFFFFFFFu;
        }

        k.Push(8, cls);
        const uint32_t quality = TypeBits(r, k);
        k.Push(8, quality);
        k.Push(32, id);
        return k;
    }

    // FUN_101c8220 (entry container).
    Key EntryKey(Row r) {
        const uint32_t id = RowU32(r, 0);
        Build* p = PendingBuild();
        Key k;
        k.Push(1, !(p && p->LockedOf(id)));
        k.Push(1, !PendingKnows(id));
        k.Push(16, OrderAfter(id));
        k.Push(1, PendingLearnResult(id) != 0);
        const uint32_t quality = TypeBits(r, k);
        k.Push(8, quality);
        k.Push(32, id);
        return k;
    }

    // FUN_101c7120 (build creator container).
    Key EditorKey(Row r) {
        Key k;
        k.Push(1, !AnySpell(r, Dependencies().creatorHasSpell));
        const uint32_t quality = TypeBits(r, k);
        k.Push(8, quality);
        k.Push(32, RowU32(r, 0));
        return k;
    }

    // FUN_101bae90: sort by the key, computed once per row.
    void SortResults(Container& c) {
        std::vector<std::pair<Key, Row>> keyed;
        keyed.reserve(c.results.size());

        for (Row r : c.results) {
            keyed.push_back({ c.kind == Kind::Category ? CategoryKey(r) : c.kind == Kind::Entry ? EntryKey(r) : EditorKey(r), r });
        }

        std::sort(keyed.begin(), keyed.end(), [](const auto& a, const auto& b) { return a.first < b.first; });

        for (size_t i = 0; i < keyed.size(); ++i) {
            c.results[i] = keyed[i].second;
        }
    }

    // ---- FUN_101c4370: the '$' tokens ---------------------------------------------------------------------------
    // Every "$<c><digits>" up to the next space is cut from the query; the well-formed ones are kept as {c, value} (a
    // value past INT_MAX is kept as 0). Then leading and trailing whitespace go and runs of spaces shrink to one.
    void ExtractTokens(std::string& q, std::vector<Token>& tokens) {
        size_t pos = 0;

        for (;;) {
            if (q.size() <= pos) {
                break;
            }

            pos = q.find('$', pos);

            if (pos == std::string::npos) {
                break;
            }

            size_t end = pos < q.size() ? q.find(' ', pos) : std::string::npos;

            if (end == std::string::npos) {
                end = q.size();
            }

            if (pos + 2 < end) {
                const char c = q[pos + 1];
                const std::string digits = q.substr(pos + 2, std::min(q.size() - (pos + 2), end - pos - 2));

                if (std::all_of(digits.begin(), digits.end(), [](char d) { return isdigit(static_cast<unsigned char>(d)) != 0; })) {
                    uint32_t value = 0;
                    bool overflow = false;

                    for (char d : digits) {
                        const uint32_t v = static_cast<uint32_t>(d - '0');

                        if (value < 0xCCCCCCCu || (value == 0xCCCCCCCu && v <= 7)) {
                            value = value * 10 + v;
                        } else {
                            overflow = true;
                        }
                    }

                    tokens.push_back({ c, overflow ? 0 : static_cast<int32_t>(value) });
                }
            }

            q.erase(pos, std::min(end - pos, q.size() - pos));
        }

        size_t first = 0;

        while (first < q.size() && isspace(static_cast<unsigned char>(q[first]))) {
            ++first;
        }

        q.erase(0, first);
        size_t last = q.size();

        while (last > 0 && isspace(static_cast<unsigned char>(q[last - 1]))) {
            --last;
        }

        q.erase(last);
        q.erase(std::unique(q.begin(), q.end(), [](char a, char b) { return a == b && a == ' '; }), q.end());
    }

    void BuildIndex(Container& c) {   // FUN_101c1960
        CoA::Dbc::Table& t = Rows();

        if (!t.Loaded()) {
            return;
        }

        for (uint32_t id = t.MinId(); id < t.MaxId() + 1; ++id) {
            if (Row r = FindRow(id)) {
                if (!HiddenBase(r, false) && RowU32(r, 0x80) != 1) {
                    c.index.push_back(r);
                }
            }
        }
    }

    void KeepTextMatches(Container& c, const std::string& q) {
        const bool skipGates = c.kind == Kind::Editor;
        c.results.erase(std::remove_if(c.results.begin(), c.results.end(), [&](Row r) { return !TextMatch(r, q, skipGates); }), c.results.end());
    }

    // FUN_101c8be0 / FUN_101c8e80 / FUN_101c8940
    void FullFilter(Container& c, const std::string& q, const std::vector<uint32_t>& filters, const std::vector<Token>& tokens) {
        c.results.clear();

        if (!tokens.empty()) {
            return;
        }

        const Groups groups = GroupsOf(c.kind, filters);

        for (Row r : c.index) {
            if (PassesGroups(c, groups, r)) {
                c.results.push_back(r);
            }
        }

        KeepTextMatches(c, q);
        SortResults(c);
    }

    // FUN_101c59f0 / FUN_101c5bb0 / FUN_101c5850
    void Refine(Container& c, const std::string& q, const std::vector<uint32_t>& filters, const std::vector<Token>& tokens) {
        if (!tokens.empty()) {
            c.results.clear();
            return;
        }

        const Groups groups = GroupsOf(c.kind, filters);
        c.results.erase(std::remove_if(c.results.begin(), c.results.end(), [&](Row r) { return !PassesGroupsRefine(c, groups, r); }),
            c.results.end());
        KeepTextMatches(c, q);
        SortResults(c);
    }

    // FUN_101ba9b0 / FUN_101bac20 / FUN_101ba740: every old id is among the new ones and the two share no group --
    // which only an empty old list satisfies.
    bool FiltersNarrow(Kind kind, const std::vector<uint32_t>& old, const std::vector<uint32_t>& now) {
        if (old.empty()) {
            return true;
        }

        for (uint32_t id : old) {
            if (std::find(now.begin(), now.end(), id) == now.end()) {
                return false;
            }
        }

        std::vector<uint32_t> a;
        std::vector<uint32_t> b;

        for (uint32_t id : old) {
            a.push_back(GroupOf(kind, id));
        }

        for (uint32_t id : now) {
            b.push_back(GroupOf(kind, id));
        }

        std::sort(a.begin(), a.end());
        std::sort(b.begin(), b.end());
        std::vector<uint32_t> both;
        std::set_intersection(a.begin(), a.end(), b.begin(), b.end(), std::back_inserter(both));
        return both.empty();
    }

    bool TokensNarrow(const std::vector<Token>& old, const std::vector<Token>& now) {   // FUN_101c6c70
        for (const Token& t : old) {
            if (std::find(now.begin(), now.end(), t) == now.end()) {
                return false;
            }
        }

        return true;
    }

    void Rebucket(Container& c, uint32_t category);

    // FUN_101c4bf0 / FUN_101c5070 / FUN_101c47c0
    void Apply(Container& c, const std::string& queryIn, const std::vector<uint32_t>& filters, const std::vector<uint32_t>& third, bool refine) {
        std::string q = queryIn;
        std::vector<Token> tokens;
        ExtractTokens(q, tokens);

        if (c.index.empty()) {
            BuildIndex(c);
            // 0x101C5215..0x101C5261: the klib builder over the new index runs an empty query into the results
            // (FUN_101cbd80 into +0xA0), so a first refine works over every indexed row.
            c.results = c.index;
            c.dirty = true;
        }

        if (!c.index.empty()) {
            // FUN_101c6fd0: a category container only works when something changed.
            const bool changed = c.kind != Kind::Category || c.dirty || q != c.query || filters != c.filters || third != c.third ||
                tokens != c.tokens || c.tags != c.prevTags;

            if (changed) {
                bool refined = false;

                if (refine && (!q.empty() || !c.query.empty())) {
                    const bool contains = c.query.empty() || (q.size() >= c.query.size() && q.find(c.query) != std::string::npos);

                    if (contains && FiltersNarrow(c.kind, c.filters, filters) && TokensNarrow(c.tokens, tokens) &&
                        (c.kind != Kind::Category || c.tags == c.prevTags)) {
                        Refine(c, q, filters, tokens);
                        refined = true;
                    }
                }

                if (!refined) {
                    FullFilter(c, q, filters, tokens);
                }
            }

            c.query = queryIn;
            c.filters = filters;
            c.tokens = tokens;
            c.third = third;
        }

        c.dirty = false;

        if (c.kind == Kind::Category) {
            for (auto& kv : c.buckets) {
                Rebucket(c, kv.first);
            }
        }
    }

    // FUN_101c54a0: the categories 0x1F..0x34 pair a class type with its reborn one; 0x1F..0x29 need the row in +0x120,
    // 0x2A..0x34 in +0x140 and not learnable by a fresh copy of the pending build. Other categories list the row in
    // +0x1C8..+0x1D0 (up to the first zero).
    bool InCategory(const Container& c, Row r, uint32_t category) {
        if (!ClassCategory(category)) {
            for (uint32_t off = 0x1C8; off < 0x1D4; off += 4) {
                const uint32_t v = RowU32(r, off);

                if (v == 0) {
                    return false;
                }

                if (v == category) {
                    return true;
                }
            }

            return false;
        }

        static const uint32_t kPairs[11][2] = { { 0, 0 }, { 3, 0x25 }, { 4, 0x26 }, { 2, 0x24 }, { 8, 0x2A }, { 9, 0x2B }, { 10, 0x2C },
            { 6, 0x28 }, { 7, 0x29 }, { 11, 0x2D }, { 5, 0x27 } };
        const uint32_t k = (category - 0x1F) % 11;
        const uint32_t cls = RowU32(r, 0x80);

        if (k != 0 && cls != kPairs[k][0] && cls != kPairs[k][1]) {
            return false;
        }

        const uint32_t id = RowU32(r, 0);

        if (category <= 0x29) {
            return c.recent.count(id) != 0;
        }

        if (!c.upcoming.count(id)) {
            return false;
        }

        Build* p = PendingBuild();

        if (!p) {
            return false;
        }

        Build copy;   // FUN_1014f820(1) + the pending build's entries, +0x2C / +0x30 and class
        copy.entries = p->entries;
        copy.u2c = p->u2c;
        copy.level = p->level;
        copy.classKey = p->classKey;
        return copy.ValidateLearn(0, id, {}, {}) != 0;
    }

    void Rebucket(Container& c, uint32_t category) {   // FUN_101c9c40
        std::vector<Row>& bucket = c.buckets[category];
        bucket.clear();

        for (Row r : c.results) {
            if (InCategory(c, r, category)) {
                bucket.push_back(r);
            }
        }
    }

    void ResetContainer(Container& c) {   // FUN_101c9f90 (category) / FUN_101c9df0 (the others)
        c.index.clear();
        c.results.clear();
        c.query.clear();
        c.filters.clear();
        c.tokens.clear();
        c.third.clear();

        if (c.kind == Kind::Category) {
            c.buckets.clear();
            c.prevTags.clear();
            c.tags.clear();
            c.recent.clear();
            c.upcoming.clear();
        }

        c.dirty = true;
    }

    // ---- Lua table readers -------------------------------------------------------------------------------------
    // FUN_1016a740 / FUN_100f3010 (FrameScript::lua_totable into an enum -> bool map): string keys mapped through `names`
    // (an unknown one logs and becomes FILTER_NONE), nil / boolean / number values, the last write winning. A bad key or
    // value logs and yields an empty map. The ids with true come back in the map's order (MSVC unordered_map, the same
    // insertions).
    void ReadEnumFlags(lua_State* L, int idx, const char* const* names, uint32_t count, std::unordered_map<uint32_t, bool>& map) {
        if (lua_type(L, idx) != LUA_TTABLE) {
            CoA::Log::Printf("FrameScript::lua_totable Value at index %d is not a table", idx);
            return;
        }

        lua_pushnil(L);

        while (lua_next(L, idx)) {
            if (!lua_isstring(L, -2)) {
                CoA::Log::Printf("FrameScript::lua_totable Invalid lua key type");
                lua_settop(L, -3);
                map.clear();
                return;
            }

            const int vt = lua_type(L, -1);

            if (vt != LUA_TNIL && vt != LUA_TBOOLEAN && vt != LUA_TNUMBER) {
                CoA::Log::Printf("FrameScript::lua_totable Invalid lua value type");
                lua_settop(L, -3);
                map.clear();
                return;
            }

            const bool value = lua_toboolean(L, -1) != 0;
            const char* key = lua_tolstring(L, -2, nullptr);
            uint32_t id = 0;
            bool found = false;

            for (uint32_t i = 0; i < count && !found; ++i) {
                if (strcmp(key, names[i]) == 0) {
                    id = i;
                    found = true;
                }
            }

            if (!found) {
                CoA::Log::Printf("lua_tovalue<enum>: Unknown enum string '%s'", key);
            }

            map[id] = value;
            lua_settop(L, -2);
        }
    }

    std::vector<uint32_t> TrueIds(const std::unordered_map<uint32_t, bool>& map) {
        std::vector<uint32_t> ids;

        for (const auto& kv : map) {
            if (kv.second) {
                ids.push_back(kv.first);
            }
        }

        return ids;
    }

    // FUN_1011f230 (FrameScript::lua_toarray): the numbers in lua_next order; a non-number logs and yields an empty list.
    std::vector<uint32_t> ReadNumberArray(lua_State* L, int idx) {
        std::vector<uint32_t> out;

        if (lua_type(L, idx) != LUA_TTABLE) {
            CoA::Log::Printf("FrameScript::lua_toarray Value at index %d is not a table", idx);
            return out;
        }

        lua_pushnil(L);

        while (lua_next(L, idx)) {
            if (!lua_isnumber(L, -1)) {
                lua_settop(L, -3);
                CoA::Log::Printf("FrameScript::lua_toarray Invalid lua value type");
                return {};
            }

            out.push_back(static_cast<uint32_t>(static_cast<int64_t>(lua_tonumber(L, -1))));
            lua_settop(L, -2);
        }

        return out;
    }

    // FUN_100bed80: {name = boolean} kept as names, in the map's order.
    std::vector<std::pair<std::string, bool>> ReadNameFlags(lua_State* L, int idx) {
        std::unordered_map<std::string, bool> map;

        if (lua_type(L, idx) != LUA_TTABLE) {
            CoA::Log::Printf("FrameScript::lua_totable Value at index %d is not a table", idx);
            return {};
        }

        lua_pushnil(L);

        while (lua_next(L, idx)) {
            if (!lua_isstring(L, -2)) {
                CoA::Log::Printf("FrameScript::lua_totable Invalid lua key type");
                lua_settop(L, -3);
                return {};
            }

            const int vt = lua_type(L, -1);

            if (vt != LUA_TNIL && vt != LUA_TBOOLEAN && vt != LUA_TNUMBER) {
                CoA::Log::Printf("FrameScript::lua_totable Invalid lua value type");
                lua_settop(L, -3);
                return {};
            }

            map[lua_tolstring(L, -2, nullptr)] = lua_toboolean(L, -1) != 0;
            lua_settop(L, -2);
        }

        return std::vector<std::pair<std::string, bool>>(map.begin(), map.end());
    }

    // The quality words SetFilteredEntries lifts out of the (lowercased) query: the first occurrence of each becomes
    // FILTER_QUALITY_*, then double spaces shrink and one space is trimmed at each end.
    void LiftQualities(std::string& q, std::unordered_map<uint32_t, bool>& flags) {
        for (char& ch : q) {
            ch = static_cast<char>(tolower(static_cast<unsigned char>(ch)));
        }

        static const std::pair<const char*, uint32_t> kWords[5] = { { "uncommon", 2 }, { "common", 1 }, { "rare", 3 }, { "epic", 4 },
            { "legendary", 5 } };

        for (const auto& w : kWords) {
            const size_t n = strlen(w.first);

            if (q.size() < n) {
                continue;
            }

            const size_t at = q.find(w.first);

            if (at == std::string::npos) {
                continue;
            }

            flags[w.second] = true;
            q.erase(at, std::min(n, q.size() - at));
        }

        for (size_t at; q.size() >= 2 && (at = q.find("  ")) != std::string::npos;) {
            q.erase(at, 1);
        }

        if (!q.empty() && q[0] == ' ') {
            q.erase(0, 1);
        }

        if (!q.empty() && q[q.size() - 1] == ' ') {
            q.erase(q.size() - 1, 1);
        }
    }

    bool ValidCategory(uint32_t category) {
        return Categories().Row(category) != nullptr;
    }

    // ---- C_CharacterAdvancement --------------------------------------------------------------------------------
    int SetFilteredEntriesByCategory(lua_State* L) {   // FUN_1017e6b0
        if (!ValidateInput(L, { LUA_TNUMBER, LUA_TTABLE, LUA_TSTRING, LUA_TTABLE })) {
            return Usage(L,
                "Usage: C_CharacterAdvancement.SetFilteredEntriesByCategory(categoryId, spellTagTypesTable, query, additionalFiltersTable)");
        }

        const uint32_t category = static_cast<uint32_t>(static_cast<int64_t>(lua_tonumber(L, 1)));
        std::vector<uint32_t> tags = ReadNumberArray(L, 2);
        const std::string query = lua_tolstring(L, 3, nullptr);
        const auto named = ReadNameFlags(L, 4);

        if (!ValidCategory(category)) {
            return Usage(L, "SetFilteredEntriesByCategory: invalid categoryId");
        }

        std::vector<uint32_t> filters;

        for (const auto& kv : named) {
            if (!kv.second) {
                continue;
            }

            for (uint32_t i = 0; i < 0x3D; ++i) {
                if (kv.first == kCategoryFilters[i]) {
                    filters.push_back(i);
                    break;
                }
            }
        }

        filters.push_back(0x3C);   // FILTER_SPELL_TAGS and FILTER_REQUIRED_IN_RANGE_2 always apply here
        filters.push_back(0x3A);
        Container& c = CategoryContainer(category);
        s_category = category;
        c.prevTags = c.tags;   // FUN_101ca180
        c.tags = tags;
        Apply(c, query, filters, {}, true);
        Rebucket(c, category);
        return 0;
    }

    Container* CategoryArg(lua_State* L, uint32_t category, const char* invalid) {
        if (!ValidCategory(category)) {
            Usage(L, invalid);
            return nullptr;
        }

        return &CategoryContainer(category);
    }

    int GetNumFilteredEntriesByCategory(lua_State* L) {   // handler_GetNumFilteredEntriesByCategory
        uint32_t category;

        if (!ReadNumber(L, category)) {
            return Usage(L, "Usage: C_CharacterAdvancement.GetNumFilteredEntriesByCategory(categoryId)");
        }

        Container* c = CategoryArg(L, category, "GetNumFilteredEntriesByCategory: invalid categoryId");

        if (!c) {
            return 0;
        }

        auto it = c->buckets.find(category);   // FUN_101c6060
        PushInt(L, it == c->buckets.end() ? 0 : static_cast<int32_t>(it->second.size()));
        return 1;
    }

    // FUN_101774f0: (entry, isSuggested) -- nil, nil past the end.
    int GetFilteredEntryAtIndexByCategory(lua_State* L) {
        int32_t category;
        int32_t index;

        if (!ReadInt2(L, category, index)) {
            return Usage(L, "Usage: C_CharacterAdvancement.GetFilteredEntryAtIndexByCategory(categoryId, index)");
        }

        if (index == 0) {
            return Usage(L, "GetFilteredEntryAtIndexByCategory: index must be >= 1");
        }

        Container* c = CategoryArg(L, static_cast<uint32_t>(category), "GetFilteredEntryAtIndexByCategory: invalid categoryId");

        if (!c) {
            return 0;
        }

        auto it = c->buckets.find(static_cast<uint32_t>(category));   // FUN_101c5f70
        const uint32_t i = static_cast<uint32_t>(index - 1);

        if (it == c->buckets.end() || i >= it->second.size()) {
            return PushNils(L, 2);
        }

        Row r = it->second[i];
        PushEntry(L, r);
        const std::vector<uint32_t> suggested = SuggestedFor(static_cast<uint32_t>(category));
        PushBool(L, std::find(suggested.begin(), suggested.end(), RowU32(r, 0)) != suggested.end());
        return 2;
    }

    int SetFilteredEntries(lua_State* L) {   // FUN_1017dda0
        if (!ValidateInput(L, { LUA_TSTRING, LUA_TTABLE })) {
            return Usage(L, "Usage: C_CharacterAdvancement.SetFilteredEntries(query, additionalFiltersTable)");
        }

        std::string query = lua_tolstring(L, 1, nullptr);
        std::unordered_map<uint32_t, bool> flags;
        ReadEnumFlags(L, 2, kEntryFilters, 0x3B, flags);
        LiftQualities(query, flags);
        std::vector<uint32_t> filters = TrueIds(flags);
        // A stock class only ever sees the traits it knows (FILTER_WCR_TRAITS, added once).
        const uint8_t cls = PlayerClass();

        if (HasActivePlayer() && ((cls >= 1 && cls <= 9) || cls == 11) && std::find(filters.begin(), filters.end(), 0x3Au) == filters.end()) {
            filters.push_back(0x3A);
        }

        Apply(s_entries, query, filters, {}, true);
        return 0;
    }

    int GetNumFilteredEntries(lua_State* L) {   // 0x101794c0
        PushInt(L, static_cast<int32_t>(s_entries.results.size()));
        return 1;
    }

    int GetFilteredEntryAtIndex(lua_State* L) {   // 0x10177460
        uint32_t index;

        if (!ReadNumber(L, index)) {
            return Usage(L, "Usage: C_CharacterAdvancement.GetFilteredEntryAtIndex(index)");
        }

        if (index == 0) {
            return Usage(L, "GetFilteredEntryAtIndex: index must be >= 1");
        }

        if (index - 1 < s_entries.results.size()) {
            PushEntry(L, s_entries.results[index - 1]);
        } else {
            lua_pushnil(L);
        }

        return 1;
    }

    // handler_IsFiltered: false while neither a query nor a filter is set, else whether the id is among the results.
    int IsFiltered(lua_State* L) {
        uint32_t id;

        if (!ReadNumber(L, id)) {
            return Usage(L, "Usage: C_CharacterAdvancement.IsFiltered(id)");
        }

        bool found = false;

        if (!s_entries.query.empty() || !s_entries.filters.empty()) {
            for (Row r : s_entries.results) {
                if (r && RowU32(r, 0) == id) {
                    found = true;
                    break;
                }
            }
        }

        PushBool(L, found);
        return 1;
    }

    // ---- C_BuildEditor -----------------------------------------------------------------------------------------
    int EditorSetFilteredEntries(lua_State* L) {   // FUN_10105ee0: no usage error on bad arguments
        if (!ValidateInput(L, { LUA_TSTRING, LUA_TTABLE })) {
            return 0;
        }

        std::string query = lua_tolstring(L, 1, nullptr);
        std::unordered_map<uint32_t, bool> flags;
        ReadEnumFlags(L, 2, kEditorFilters, 0x3A, flags);
        LiftQualities(query, flags);
        Apply(s_editor, query, TrueIds(flags), {}, true);
        return 0;
    }

    int EditorGetNumFilteredEntries(lua_State* L) {   // handler_GetNumFilteredEntries
        PushInt(L, static_cast<int32_t>(s_editor.results.size()));
        return 1;
    }

    int EditorGetFilteredEntryAtIndex(lua_State* L) {   // handler_GetFilteredEntryAtIndex
        uint32_t index;

        if (!ReadNumber(L, index) || index == 0) {
            return 0;
        }

        if (index - 1 < s_editor.results.size()) {
            PushEntry(L, s_editor.results[index - 1]);
        } else {
            lua_pushnil(L);
        }

        return 1;
    }

    void ResetOnGlue() {
        ResetContainer(s_normal);    // FUN_1016ec70: mgr +0x28, +0x190, +0x2F8
        ResetContainer(s_classes);
        ResetContainer(s_entries);
        ResetContainer(s_editor);    // FUN_100fc070: BuildCreator +0x228
    }

    const CoA::Binding s_bindings[] = {
        COA_NATIVE("C_CharacterAdvancement", "SetFilteredEntriesByCategory", SetFilteredEntriesByCategory),
        COA_NATIVE("C_CharacterAdvancement", "GetNumFilteredEntriesByCategory", GetNumFilteredEntriesByCategory),
        COA_NATIVE("C_CharacterAdvancement", "GetFilteredEntryAtIndexByCategory", GetFilteredEntryAtIndexByCategory),
        COA_NATIVE("C_CharacterAdvancement", "SetFilteredEntries", SetFilteredEntries),
        COA_NATIVE("C_CharacterAdvancement", "GetNumFilteredEntries", GetNumFilteredEntries),
        COA_NATIVE("C_CharacterAdvancement", "GetFilteredEntryAtIndex", GetFilteredEntryAtIndex),
        COA_NATIVE("C_CharacterAdvancement", "IsFiltered", IsFiltered),
        COA_NATIVE("C_BuildEditor", "SetFilteredEntries", EditorSetFilteredEntries),
        COA_NATIVE("C_BuildEditor", "GetNumFilteredEntries", EditorGetNumFilteredEntries),
        COA_NATIVE("C_BuildEditor", "GetFilteredEntryAtIndex", EditorGetFilteredEntryAtIndex),
    };
}

bool CoA::CAFilter::RowMatchesQuery(Row r, const std::string& lowerQuery) {
    return TextMatch(r, lowerQuery, false);
}

int CoA::CAFilter::RowQualityIndex(Row r) {
    return QualityOf(RowText(r, 0x60));
}

// FUN_101ca1d0 / FUN_101c4360 / FUN_101ca500 on mgr +0x190, with a fresh prepared build carrying the pending build's
// entries, +0x2C / +0x30 and class. Over the Ability / TalentAbility rows:
//   +0x120 gains the rows learnable (slots 6 and 14 excluded) whose requirement the build passed only just now: class
//          points within 9 of +0xA4, or AE / class AE / tab AE within 16 of +0x88 / +0x90 / +0x98. Nothing clears it
//          until a reset.
//   +0x140 is rebuilt with the rows NOT learnable whose requirement is close: class points under +0xA4 by less than 5,
//          or AE / class AE / tab AE under +0x88 / +0x90 / +0x98 by less than 9.
void CoA::CAFilter::RebuildRecentSets() {
    Build* p = PendingBuild();

    if (!p) {
        return;
    }

    auto freshCopy = [p](Build& b) {
        b.entries = p->entries;
        b.u2c = p->u2c;
        b.level = p->level;
        b.classKey = p->classKey;
    };
    auto forEachSpellRow = [](auto fn) {
        ForEachRow([&](Row r) {
            if (KindSpell(r)) {
                fn(r);
            }
        });
    };
    {
        Build b;
        freshCopy(b);
        forEachSpellRow([&](Row r) {
            if (b.ValidateLearn(0, RowU32(r, 0), {}, { 6, 14 }) != 0) {
                return;
            }

            const uint32_t cls = RowU32(r, 0x80);
            const uint32_t tab = RowU32(r, 0x84);
            bool add = b.ClassPoints(cls, 0) - RowU32(r, 0xA4) < 9;

            if (!add && RowU32(r, 0x88) && b.GlobalAE(0) - RowU32(r, 0x88) <= 16) {
                add = true;
            }

            if (!add && RowU32(r, 0x90) && b.ClassAE(cls, 0) - RowU32(r, 0x90) <= 16) {
                add = true;
            }

            if (!add && RowU32(r, 0x98) && b.TabAE(cls, tab, 0) - RowU32(r, 0x98) <= 16) {
                add = true;
            }

            if (add) {
                s_classes.recent.insert(RowU32(r, 0));
            }
        });
    }
    s_classes.upcoming.clear();
    {
        Build b;
        freshCopy(b);
        forEachSpellRow([&](Row r) {
            if (b.ValidateLearn(0, RowU32(r, 0), {}, {}) == 0) {
                return;
            }

            const uint32_t cls = RowU32(r, 0x80);
            const uint32_t tab = RowU32(r, 0x84);
            auto close = [](uint32_t have, uint32_t need, uint32_t within) { return have < need && need - have < within; };
            bool add = close(b.ClassPoints(cls, 0), RowU32(r, 0xA4), 5);

            if (!add && RowU32(r, 0x88)) {
                add = close(b.GlobalAE(0), RowU32(r, 0x88), 9);
            }

            if (!add && RowU32(r, 0x90)) {
                add = close(b.ClassAE(cls, 0), RowU32(r, 0x90), 9);
            }

            if (!add && RowU32(r, 0x98)) {
                add = close(b.TabAE(cls, tab, 0), RowU32(r, 0x98), 9);
            }

            if (add) {
                s_classes.upcoming.insert(RowU32(r, 0));
            }
        });
    }
}

void CoA::CAFilter::TagTypesPatched() {   // SMSG 0x6C0 re-runs FUN_10217220 after patching SpellTagTypes.dbc
    s_tagTypeStatsBuilt = false;
}

void CoA::CAFilter::ResetEditor() {
    ResetContainer(s_editor);
}

void CoA::CAFilter::ResetEntries() {
    ResetContainer(s_entries);
}

void CoA::CAFilter::ResetCategories(bool classes) {
    ResetContainer(classes ? s_classes : s_normal);
}

void CoA::CAFilter::Init() {
    CoA::Lifecycle::OnGlueScreen(ResetOnGlue);
    CoA::PatchData::Dependencies().tagTypesPatched = TagTypesPatched;
    CoA::LuaReg::AddModule(s_bindings, sizeof(s_bindings) / sizeof(s_bindings[0]));
}

void CoA::CAFilter::Reset() {
    ResetOnGlue();
    s_category = 0;
    s_tagTypeStats.clear();
    s_tagTypeStatsBuilt = false;
}
