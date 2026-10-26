// AscWildcardSets.cpp (see Wildcard.h): C_Wildcard, part 2 -- the WildcardMgr's desired and undesired roll lists
// (FUN_10a349d0, static 0x10D3D318: +0x00 desired, +0x0C undesired) and their entry browsers (+0x18 / +0xF8).
//
// The (id, kind name) argument pair is FUN_100e9180 + FUN_10a29980 (names 0x10B5D160). The lists notify three runtime
// lists, each with the entry's id: 0x10be2c8c on every desired entry removed (nothing registers into it), 0x10be2cac on
// an undesired entry added and 0x10be2ccc on every undesired entry removed (the suggestion manager's dirty mark
// 0x10333950 in both).
#include "CoA/Source/Modules/WildcardInternal.h"
#include "CoA/Source/Modules/CAFilter.h"
#include "CoA/Source/Runtime/Dbc.h"
#include "CoA/Source/Runtime/Events.h"
#include "CoA/Source/Runtime/FilterContainer.h"
#include "CoA/Source/Runtime/Lifecycle.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Script.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <algorithm>
#include <map>

using CoA::CA::Build;
using CoA::CA::Row;
using CoA::CA::RowU32;
using CoA::Script::CheckNumber;
using CoA::Script::CheckString;
using CoA::Script::PushBool;
using CoA::Script::PushStr;
using CoA::Script::ReadNumber;
using CoA::Script::ValidateInput;

namespace {
    const char* const kKinds[7] = { "None", "Ability", "Talent", "Trait", "TalentAbility", "Tag", "Suggestion" };   // 0x10B5D160
    const char* const kAddDesired[5] = { "ADD_WILDCARD_DESIRED_ENTRY_OK", "ADD_WILDCARD_DESIRED_ENTRY_UNKNOWN",
        "ADD_WILDCARD_DESIRED_ENTRY_NOT_FOUND", "ADD_WILDCARD_DESIRED_ENTRY_ALREADY_KNOWN",
        "ADD_WILDCARD_DESIRED_ENTRY_ALREADY_DESIRED" };   // 0x10B1D4DC
    const char* const kAddUndesired[8] = { "ADD_WILDCARD_UNDESIRED_ENTRY_OK", "ADD_WILDCARD_UNDESIRED_ENTRY_UNKNOWN",
        "ADD_WILDCARD_UNDESIRED_ENTRY_NOT_FOUND", "ADD_WILDCARD_UNDESIRED_ENTRY_NOT_KNOWN", "ADD_WILDCARD_UNDESIRED_ENTRY_LOCKED",
        "ADD_WILDCARD_UNDESIRED_ENTRY_ALREADY_UNDESIRED", "ADD_WILDCARD_UNDESIRED_ENTRY_SPELL_TAG",
        "ADD_WILDCARD_UNDESIRED_ENTRY_SUGGESTION" };   // 0x10B22C30

    struct Entry {
        uint32_t id;
        uint8_t kind;
        uint8_t ability;
        uint8_t talent;
    };

    std::vector<Entry> s_desired;     // +0x00
    std::vector<Entry> s_undesired;   // +0x0C

    using IdCallback = void (*)(uint32_t);
    std::vector<IdCallback> s_desiredRemoved;     // 0x10be2c8c (FUN_10278150)
    std::vector<IdCallback> s_undesiredAdded;     // 0x10be2cac (FUN_10278180)
    std::vector<IdCallback> s_undesiredRemoved;   // 0x10be2ccc (FUN_102781b0)

    void Notify(const std::vector<IdCallback>& list, uint32_t id) {
        for (IdCallback cb : list) {
            cb(id);
        }
    }

    bool Contains(const std::vector<Entry>& v, uint32_t id, uint8_t kind) {   // FUN_10a34b20 / FUN_10a34d10
        return std::any_of(v.begin(), v.end(), [&](const Entry& e) { return e.id == id && e.kind == kind; });
    }

    bool KindIndex(const std::string& name, uint8_t& kind) {   // FUN_10a29980
        for (uint8_t i = 0; i < 7; ++i) {
            if (name == kKinds[i]) {
                kind = i;
                return true;
            }
        }

        return false;
    }

    // FUN_100e9180 + FUN_10a29980: (number, kind name); false unless the name is a known kind.
    bool ReadIdKind(lua_State* L, uint32_t& id, uint8_t& kind) {
        if (!ValidateInput(L, { LUA_TNUMBER, LUA_TSTRING })) {
            return false;
        }

        id = static_cast<uint32_t>(static_cast<int64_t>(CheckNumber(L, 1)));
        return KindIndex(CheckString(L, 2), kind);
    }

    CoA::Dbc::Table& TagTypes() {   // 0x10BDF5F4
        return CoA::Dbc::Get("DBFilesClient/SpellTagTypes.dbc");
    }

    // FUN_10a34d40: the row's own kind (type string at +4) equals the requested one.
    bool RowIsKind(Row r, uint8_t kind) {
        return r && CoA::CA::RowType(r) == kind;
    }

    bool IsAbilityRow(Row r) {   // FUN_101c6770
        const int t = CoA::CA::RowType(r);
        return t == 1 || t == 4;
    }

    bool AnyRollKind(Row r) {   // FUN_101c6740 / FUN_101c6eb0 / FUN_101c6e80
        const int t = CoA::CA::RowType(r);
        return t == 1 || t == 4 || t == 2;
    }

    // FUN_10a26d10: a row the player may still roll into -- visible, class-allowed, not held, none of the excluding
    // flags, its level within the build's and the wildcard pool's reach, and not a second +0x74 == 2 entry.
    bool Desirable(Row r) {
        if (!r || !AnyRollKind(r)) {
            return false;
        }

        Build* b = CoA::CA::ActiveBuild();

        if (!b || !CoA::CA::RowVisible(r) || !CoA::CA::ClassAllowedForPlayer(r)) {
            return false;
        }

        if (b->Has(RowU32(r, 0))) {
            return false;
        }

        if (CoA::CA::RowHasFlag(r, 0x20000) || CoA::CA::RowHasFlag(r, 8) || CoA::CA::RowHasFlag(r, 0x10000) ||
            CoA::CA::RowHasFlag(r, 0x1000) || CoA::CA::RowHasFlag(r, 0x80000)) {
            return false;
        }

        const uint32_t f = RowU32(r, 0x124);

        if ((f & 0x1800000) && !((f & 0x800000) && b->wildcard) && !((f & 0x1000000) && b->draft)) {
            return false;
        }

        const uint32_t req = CoA::CA::RequiredLevelOf(*b, r);

        if (b->level < req) {
            return false;
        }

        if (IsAbilityRow(r) && CoA::CA::PoolLevelOf(*b, false) < req) {
            return false;
        }

        if (CoA::CA::RowType(r) == 2 && CoA::CA::PoolLevelOf(*b, true) < req) {
            return false;
        }

        return !(RowU32(r, 0x74) == 2 && b->HasGroup(2, 0));
    }

    // FUN_10a26e80: a held, unlocked row the player may mark undesired.
    bool Undesirable(Row r) {
        if (!r || !AnyRollKind(r)) {
            return false;
        }

        Build* b = CoA::CA::ActiveBuild();

        if (!b || !CoA::CA::RowVisible(r) || !CoA::CA::ClassAllowedForPlayer(r)) {
            return false;
        }

        const uint32_t id = RowU32(r, 0);

        if (!b->Has(id) || b->LockedOf(id)) {
            return false;
        }

        if (CoA::CA::RowHasFlag(r, 0x20000) || CoA::CA::RowHasFlag(r, 8) || CoA::CA::RowHasFlag(r, 0x10000) ||
            CoA::CA::RowHasFlag(r, 0x1000) || CoA::CA::RowHasFlag(r, 4)) {
            return false;
        }

        const uint32_t f = RowU32(r, 0x124);

        if (!(f & 0x1800000)) {
            return true;
        }

        return ((f & 0x800000) && b->wildcard) || ((f & 0x1000000) && b->draft);
    }

    // FUN_10a33170: 0 ok, else an ADD_WILDCARD_DESIRED_ENTRY_* index.
    uint8_t CanDesire(uint32_t id, uint8_t kind) {
        switch (kind) {
            case 1:
            case 2:
            case 4: {
                Row r = CoA::CA::FindRow(id);

                if (!r) {
                    return 2;
                }

                if (Build* b = CoA::CA::ActiveBuild()) {
                    if (b->Has(id)) {
                        return 3;
                    }

                    if (RowIsKind(r, kind) && Desirable(r)) {
                        return Contains(s_desired, id, kind) ? 4 : 0;
                    }
                }

                return 1;
            }
            case 5:
                if (TagTypes().Row(id)) {
                    return Contains(s_desired, id, kind) ? 4 : 0;
                }

                return 2;
            case 6:
                if (id == 1 || id == 2) {
                    return Contains(s_desired, id, kind) ? 4 : 0;
                }

                return 2;
            default:
                return 2;
        }
    }

    // FUN_10a33280: 0 ok, else an ADD_WILDCARD_UNDESIRED_ENTRY_* index.
    uint8_t CanUndesire(uint32_t id, uint8_t kind) {
        switch (kind) {
            case 1:
            case 2:
            case 4: {
                Row r = CoA::CA::FindRow(id);
                Build* b = r ? CoA::CA::ActiveBuild() : nullptr;

                if (!b) {
                    return 1;
                }

                if (!b->Has(id)) {
                    return 3;
                }

                if (b->LockedOf(id)) {
                    return 4;
                }

                if (RowIsKind(r, kind) && Undesirable(r)) {
                    return Contains(s_undesired, id, kind) ? 5 : 0;
                }

                return 1;
            }
            case 5:
                return TagTypes().Row(id) ? 6 : 1;
            case 6:
                return (id == 1 || id == 2) ? 7 : 1;
            default:
                return 1;
        }
    }

    Entry Make(uint32_t id, uint8_t kind) {   // FUN_10a25fe0 / FUN_10a26020 / FUN_10a25fc0
        if (kind == 5) {
            return { id, 5, 1, 1 };
        }

        if (kind == 6) {
            return { id, 6, static_cast<uint8_t>(id == 1), static_cast<uint8_t>(id == 2) };
        }

        Row r = CoA::CA::FindRow(id);
        const uint8_t own = r ? static_cast<uint8_t>(CoA::CA::RowType(r)) : kind;
        return { id, own, static_cast<uint8_t>(r && IsAbilityRow(r)), static_cast<uint8_t>(r && CoA::CA::RowType(r) == 2) };
    }

    void DesiredChanged() {
        CoA::Events::Signal("WILDCARD_DESIRED_ENTRIES_CHANGED", nullptr);
    }

    void UndesiredChanged() {
        CoA::Events::Signal("WILDCARD_UNDESIRED_ENTRIES_CHANGED", nullptr);
    }

    // FUN_10a32c10 (quiet = a bulk add, no event).
    void AddDesired(uint32_t id, uint8_t kind, bool quiet) {
        if (Contains(s_desired, id, kind) || CanDesire(id, kind) != 0) {
            return;
        }

        s_desired.push_back(Make(id, kind));

        if (!quiet) {
            DesiredChanged();
        }
    }

    // FUN_10a32ee0: only the CA kinds pass (the jump table sends Trait / Tag / Suggestion straight out).
    void AddUndesired(uint32_t id, uint8_t kind, bool quiet) {
        if (Contains(s_undesired, id, kind) || CanUndesire(id, kind) != 0) {
            return;
        }

        s_undesired.push_back(Make(id, kind));
        Notify(s_undesiredAdded, id);

        if (!quiet) {
            UndesiredChanged();
        }
    }

    void Erase(std::vector<Entry>& v, uint32_t id, uint8_t kind) {
        v.erase(std::remove_if(v.begin(), v.end(), [&](const Entry& e) { return e.id == id && e.kind == kind; }), v.end());
    }

    // FUN_10a36010: every matching entry erased, then one notification and the event.
    void RemoveDesired(uint32_t id, uint8_t kind) {
        if (!Contains(s_desired, id, kind)) {
            return;
        }

        Erase(s_desired, id, kind);
        Notify(s_desiredRemoved, id);
        DesiredChanged();
    }

    // FUN_10a36150.
    void RemoveUndesired(uint32_t id, uint8_t kind) {
        if (!Contains(s_undesired, id, kind)) {
            return;
        }

        Erase(s_undesired, id, kind);
        Notify(s_undesiredRemoved, id);
        UndesiredChanged();
    }

    // A whole list swapped out: each entry it held notified (FUN_10a339b0 / FUN_10a33e60 / the spec change).
    void ClearDesired(bool signal) {
        const std::vector<Entry> old = std::move(s_desired);
        s_desired.clear();

        for (const Entry& e : old) {
            Notify(s_desiredRemoved, e.id);
        }

        if (signal) {
            DesiredChanged();
        }
    }

    void ClearUndesired(bool signal) {
        const std::vector<Entry> old = std::move(s_undesired);
        s_undesired.clear();

        for (const Entry& e : old) {
            Notify(s_undesiredRemoved, e.id);
        }

        if (signal) {
            UndesiredChanged();
        }
    }

    int AddDesiredID(lua_State* L) {   // FUN_10a2af20
        uint32_t id;
        uint8_t kind;

        if (ReadIdKind(L, id, kind) && CanDesire(id, kind) == 0) {
            AddDesired(id, kind, false);
        }

        return 0;
    }

    int AddUndesiredID(lua_State* L) {   // FUN_10a2b020
        uint32_t id;
        uint8_t kind;

        if (ReadIdKind(L, id, kind) && CanUndesire(id, kind) == 0) {
            AddUndesired(id, kind, false);
        }

        return 0;
    }

    template <size_t N>
    int PushCan(lua_State* L, uint8_t code, const char* const (&names)[N]) {
        PushBool(L, code == 0);

        if (code == 0) {
            lua_pushnil(L);
        } else {
            PushStr(L, code < N ? names[code] : ("UNEXPECTED_ENUM_VALUE_" + std::to_string(code)).c_str());
        }

        return 2;
    }

    int CanAddDesiredID(lua_State* L) {   // FUN_10a2b120
        uint32_t id;
        uint8_t kind;

        if (!ReadIdKind(L, id, kind)) {
            return 0;
        }

        return PushCan(L, CanDesire(id, kind), kAddDesired);
    }

    int CanAddUndesiredID(lua_State* L) {   // FUN_10a2b340
        uint32_t id;
        uint8_t kind;

        if (!ReadIdKind(L, id, kind)) {
            return 0;
        }

        return PushCan(L, CanUndesire(id, kind), kAddUndesired);
    }

    int IsDesiredID(lua_State* L) {   // FUN_10a2d1c0
        uint32_t id;
        uint8_t kind;

        if (!ReadIdKind(L, id, kind)) {
            return 0;
        }

        PushBool(L, Contains(s_desired, id, kind));
        return 1;
    }

    int IsUndesiredID(lua_State* L) {   // FUN_10a2d360
        uint32_t id;
        uint8_t kind;

        if (!ReadIdKind(L, id, kind)) {
            return 0;
        }

        PushBool(L, Contains(s_undesired, id, kind));
        return 1;
    }

    int RemoveDesiredID(lua_State* L) {   // FUN_10a2d480 -> FUN_10a36010
        uint32_t id;
        uint8_t kind;

        if (ReadIdKind(L, id, kind)) {
            RemoveDesired(id, kind);
        }

        return 0;
    }

    int RemoveUndesiredID(lua_State* L) {   // FUN_10a2d570 -> FUN_10a36150
        uint32_t id;
        uint8_t kind;

        if (ReadIdKind(L, id, kind)) {
            RemoveUndesired(id, kind);
        }

        return 0;
    }

    int ClearDesiredSpells(lua_State*) {   // FUN_10a2bc20 -> FUN_10a339b0: only when non-empty
        if (!s_desired.empty()) {
            ClearDesired(true);
        }

        return 0;
    }

    int ClearUndesiredSpells(lua_State*) {   // FUN_10a2bc30 -> FUN_10a33e60
        if (!s_undesired.empty()) {
            ClearUndesired(true);
        }

        return 0;
    }

    // FUN_10a33ca0 / FUN_10a33fc0: drop the ability (+5) / talent (+6) entries; each removed one notified, then the
    // event when anything went.
    void RemoveAllOf(bool talents) {
        if (s_undesired.empty()) {
            return;
        }

        const std::vector<Entry> before = s_undesired;
        s_undesired.erase(std::remove_if(s_undesired.begin(), s_undesired.end(),
            [&](const Entry& e) { return talents ? e.talent != 0 : e.ability != 0; }), s_undesired.end());

        if (s_undesired.size() == before.size()) {
            return;
        }

        for (const Entry& e : before) {
            if (!Contains(s_undesired, e.id, e.kind)) {
                Notify(s_undesiredRemoved, e.id);
            }
        }

        UndesiredChanged();
    }

    int RemoveAllUndesiredAbilities(lua_State*) {   // FUN_10a2d460
        RemoveAllOf(false);
        return 0;
    }

    int RemoveAllUndesiredTalents(lua_State*) {   // FUN_10a2d470
        RemoveAllOf(true);
        return 0;
    }

    // ---- the entry browsers (WildcardDesired / UndesiredEntryContainer: mgr +0x18 / +0xF8) ---------------------------
    // WildcardEntryContainerBase: +8 type (0 desired, 1 undesired); entries are 0x6C-byte views (FUN_10a25390 CA row /
    // FUN_10a254c0 tag / FUN_10a25cf0 suggestion); the filtered list is +0xD0.
    const char* const kEntryFilters[19] = { "FILTER_NONE", "FILTER_QUALITY_NORMAL", "FILTER_QUALITY_UNCOMMON",
        "FILTER_QUALITY_RARE", "FILTER_QUALITY_EPIC", "FILTER_QUALITY_LEGENDARY", "FILTER_CLASS_HUNTER", "FILTER_CLASS_WARRIOR",
        "FILTER_CLASS_ROGUE", "FILTER_CLASS_MAGE", "FILTER_CLASS_PRIEST", "FILTER_CLASS_WARLOCK", "FILTER_CLASS_DRUID",
        "FILTER_CLASS_SHAMAN", "FILTER_CLASS_PALADIN", "FILTER_CLASS_DEATH_KNIGHT", "FILTER_CLASS_GENERAL",
        "FILTER_TYPE_ABILITY", "FILTER_TYPE_TALENT" };   // 0x10B5FD90

    struct View {
        uint32_t id = 0;
        uint8_t kind = 0;
        std::string name;
        std::string icon;
        uint32_t requiredLevel = 0;
        uint32_t classType = 0;
        uint32_t tab = 0;
        uint32_t spells[9] = {};
        bool ability = false;
        bool talent = false;
    };

    View ViewOfRow(Row r) {   // FUN_10a25390
        View v;
        v.id = RowU32(r, 0);
        v.kind = static_cast<uint8_t>(CoA::CA::RowType(r) < 0 ? 0 : CoA::CA::RowType(r));
        v.name = CoA::CA::RowText(r, 0xBC);
        v.icon = CoA::CA::RowText(r, 0xC0);

        if (Build* b = CoA::CA::ActiveBuild()) {
            v.requiredLevel = CoA::CA::RequiredLevelOf(*b, r);
        }

        v.classType = RowU32(r, 0x80);
        v.tab = RowU32(r, 0x84);

        for (uint32_t k = 0; k < 9; ++k) {
            v.spells[k] = RowU32(r, 0x14 + k * 4);
        }

        v.ability = IsAbilityRow(r);
        v.talent = CoA::CA::RowType(r) == 2;
        return v;
    }

    // FUN_10a26560 (desired): every desirable row, every SpellTagTypes row with +0xC and +0x10 zero, then the two
    // suggestion entries. FUN_10a267e0 (undesired): every undesirable row.
    std::vector<View> Populate(bool undesired) {
        std::vector<View> out;
        CoA::Dbc::Table& ca = CoA::Dbc::Get("DBFilesClient\\CharacterAdvancement.dbc");

        for (uint32_t id = ca.MinId(); ca.Loaded() && ca.Count() && id <= ca.MaxId(); ++id) {
            if (Row r = ca.Row(id)) {
                if (undesired ? Undesirable(r) : Desirable(r)) {
                    out.push_back(ViewOfRow(r));
                }
            }
        }

        if (undesired) {
            return out;
        }

        CoA::Dbc::Table& tags = TagTypes();

        for (uint32_t i = 0; i < tags.Count(); ++i) {
            if (const uint8_t* t = tags.RowAt(i)) {
                if (CoA::Dbc::Table::U32(t, 0xC) == 0 && CoA::Dbc::Table::U32(t, 0x10) == 0) {   // FUN_10a254c0
                    View v;
                    v.id = CoA::Dbc::Table::U32(t, 0);
                    v.kind = 5;
                    v.name = tags.Str(t, 0x70);
                    v.icon = tags.Str(t, 0x68);
                    v.ability = true;
                    v.talent = true;
                    out.push_back(v);
                }
            }
        }

        View a;   // FUN_10a25cf0
        a.id = 1;
        a.kind = 6;
        a.name = "Suggested Abilities";
        a.icon = "Interface\\Icons\\Ability_ThunderBolt";
        a.ability = true;
        out.push_back(a);
        View t;
        t.id = 2;
        t.kind = 6;
        t.name = "Suggested Talents";
        t.icon = "Interface\\Icons\\Warrior_talent_icon_singlemindedfury";
        t.talent = true;
        out.push_back(t);
        return out;
    }

    // FUN_10a26f90: FILTER_* against a CharacterAdvancement view (tags and suggestions never match).
    bool EnumMatch(const View& v, uint32_t f) {
        if (v.kind < 1 || v.kind > 4) {
            return false;
        }

        Row r = CoA::CA::FindRow(v.id);

        if (!r) {
            return false;
        }

        if (f >= 1 && f <= 5) {
            return CoA::CAFilter::RowQualityIndex(r) == static_cast<int>(f);
        }

        if (f >= 6 && f <= 16) {
            return RowU32(r, 0x80) == f - 4;
        }

        if (f == 17) {
            return IsAbilityRow(r);
        }

        if (f == 18) {
            return CoA::CA::RowType(r) == 2;
        }

        return false;
    }

    uint32_t FilterGroup(uint32_t f) {   // FUN_10a27320's grouping
        if (f >= 1 && f <= 5) {
            return 1;
        }

        if (f >= 6 && f <= 16) {
            return 2;
        }

        if (f == 17 || f == 18) {
            return 3;
        }

        return 0;
    }

    // FUN_10a269e0: empty query, or the name contains it, or (a CA view) the row's text search does.
    bool QueryMatch(const View& v, const std::string& lower) {
        if (lower.empty()) {
            return true;
        }

        if (CoA::Filter::Lower(v.name).find(lower) != std::string::npos) {
            return true;
        }

        if (v.kind >= 1 && v.kind <= 4) {
            if (Row r = CoA::CA::FindRow(v.id)) {
                return CoA::CAFilter::RowMatchesQuery(r, lower);
            }
        }

        return false;
    }

    // FUN_10152fc0: how many of the build's entries were learned after this one (+0x18 as a u64), -1 when the build does
    // not hold it -- so the most recent sorts first.
    int64_t LearnedOrder(uint32_t id) {
        Build* b = CoA::CA::ActiveBuild();

        if (!b) {
            return -1;
        }

        const CoA::CA::Entry* mine = nullptr;

        for (const CoA::CA::Entry& e : b->entries) {
            if (e.id == id) {
                mine = &e;
                break;
            }
        }

        if (!mine) {
            return -1;
        }

        const uint64_t key = (static_cast<uint64_t>(mine->u1c) << 32) | mine->u18;
        int64_t later = 0;

        for (const CoA::CA::Entry& e : b->entries) {
            if (((static_cast<uint64_t>(e.u1c) << 32) | e.u18) > key) {
                ++later;
            }
        }

        return later;
    }

    struct Browser {
        bool undesired;
        std::vector<View> filtered;           // +0xD0
        bool built = false;                   // +0xC
        std::string lastText;                 // +0x4
        std::vector<uint32_t> lastFilters;    // +0x28

        // FUN_10a27c80: re-run the last query once one has been made.
        void Refresh() {
            if (this->built) {
                this->Apply(std::string(this->lastText), std::vector<uint32_t>(this->lastFilters));
            }
        }

        // FUN_10a27320: groups AND, filters in a group OR; then the selected sort (entries already on the list first),
        // suggestions first (desired) or learned order (undesired), then id.
        void Apply(const std::string& text, const std::vector<uint32_t>& filters) {
            this->built = true;
            this->lastText = text;
            this->lastFilters = filters;
            const std::string lower = CoA::Filter::Lower(text);
            std::vector<std::pair<uint32_t, std::vector<uint32_t>>> groups;

            for (uint32_t f : filters) {
                const uint32_t g = FilterGroup(f);
                auto it = std::find_if(groups.begin(), groups.end(), [g](const auto& p) { return p.first == g; });

                if (it == groups.end()) {
                    groups.emplace_back(g, std::vector<uint32_t> { f });
                } else {
                    it->second.push_back(f);
                }
            }

            this->filtered.clear();

            for (const View& v : Populate(this->undesired)) {
                bool keep = QueryMatch(v, lower);

                for (const auto& g : groups) {
                    keep = keep && std::any_of(g.second.begin(), g.second.end(), [&](uint32_t f) { return EnumMatch(v, f); });
                }

                if (keep) {
                    this->filtered.push_back(v);
                }
            }

            const std::vector<Entry>& list = this->undesired ? s_undesired : s_desired;
            auto selected = [&](const View& v) { return Contains(list, v.id, v.kind) ? 0 : 1; };
            const bool undesired = this->undesired;
            std::stable_sort(this->filtered.begin(), this->filtered.end(), [&](const View& a, const View& b) {
                const int sa = selected(a);
                const int sb = selected(b);

                if (sa != sb) {
                    return sa < sb;
                }

                if (!undesired) {
                    const int ga = a.kind != 6;
                    const int gb = b.kind != 6;

                    if (ga != gb) {
                        return ga < gb;
                    }
                } else {
                    const uint64_t la = static_cast<uint64_t>(LearnedOrder(a.id));
                    const uint64_t lb = static_cast<uint64_t>(LearnedOrder(b.id));

                    if (la != lb) {
                        return la < lb;
                    }
                }

                return a.id < b.id;
            });
        }

        void Reset() {   // FUN_10a27d00
            this->filtered.clear();
            this->built = false;
            this->lastText.clear();
            this->lastFilters.clear();
        }
    };

    Browser s_desiredView { false };
    Browser s_undesiredView { true };

    // FUN_102dd650: (text, {FILTER_* = true}).
    bool ReadFilterArgs(lua_State* L, std::string& text, std::vector<uint32_t>& filters) {
        if (!ValidateInput(L, { LUA_TSTRING, LUA_TTABLE })) {
            return false;
        }

        text = CheckString(L, 1);
        std::map<std::string, bool> names;
        lua_pushnil(L);

        while (lua_next(L, 2)) {
            if (lua_isstring(L, -2)) {
                names[lua_tolstring(L, -2, nullptr)] = lua_toboolean(L, -1) != 0;
            }

            lua_settop(L, -2);
        }

        for (const auto& n : names) {
            if (n.second) {
                for (uint32_t i = 0; i < 19; ++i) {
                    if (n.first == kEntryFilters[i]) {
                        filters.push_back(i);
                    }
                }
            }
        }

        return true;
    }

    int SetFilteredDesiredEntries(lua_State* L) {   // FUN_10a2dd60
        std::string text;
        std::vector<uint32_t> filters;

        if (ReadFilterArgs(L, text, filters)) {
            s_desiredView.Apply(text, filters);
        }

        return 0;
    }

    int SetFilteredUndesiredEntries(lua_State* L) {   // FUN_10a2e190
        std::string text;
        std::vector<uint32_t> filters;

        if (ReadFilterArgs(L, text, filters)) {
            s_undesiredView.Apply(text, filters);
        }

        return 0;
    }

    int GetNumFilteredDesiredEntries(lua_State* L) {   // FUN_10a2bfb0
        lua_pushinteger(L, static_cast<int32_t>(s_desiredView.filtered.size()));
        return 1;
    }

    int GetNumFilteredUndesiredEntries(lua_State* L) {   // FUN_10a2bfe0
        lua_pushinteger(L, static_cast<int32_t>(s_undesiredView.filtered.size()));
        return 1;
    }

    void SetString(lua_State* L, const char* k, const std::string& s) {
        lua_pushstring(L, k);
        PushStr(L, s.c_str());
        lua_settable(L, -3);
    }

    // func_0x10a27190 -> FUN_10a257b0 / FUN_10a255c0.
    int PushView(lua_State* L, const Browser& br) {
        uint32_t index;

        if (!ReadNumber(L, index) || index == 0 || index - 1 >= br.filtered.size()) {
            return 0;
        }

        const View& v = br.filtered[index - 1];
        lua_createtable(L, 0, 8);
        lua_checkstack(L, 2);
        lua_pushstring(L, "ID");
        lua_pushinteger(L, static_cast<int32_t>(v.id));
        lua_settable(L, -3);
        SetString(L, "Type", v.kind < 7 ? kKinds[v.kind] : "");
        lua_pushstring(L, "Spells");   // the non-zero spells (FUN_10169a80)
        std::vector<uint32_t> spells;

        for (uint32_t s : v.spells) {
            if (s) {
                spells.push_back(s);
            }
        }

        lua_createtable(L, 0, static_cast<int>(spells.size()));

        for (size_t i = 0; i < spells.size(); ++i) {
            lua_pushnumber(L, static_cast<double>(i + 1));
            lua_pushinteger(L, static_cast<int32_t>(spells[i]));
            lua_settable(L, -3);
        }

        lua_settable(L, -3);
        CoA::Dbc::Table& classTypes = CoA::Dbc::Get("DBFilesClient\\CharacterAdvancementClassTypes.dbc");
        const uint8_t* ct = classTypes.Row(v.classType);
        SetString(L, "Class", ct ? std::string(classTypes.Str(ct, 4)) : "INVALID_CLASS_TYPE_" + std::to_string(v.classType));
        CoA::Dbc::Table& tabTypes = CoA::Dbc::Get("DBFilesClient\\CharacterAdvancementTabTypes.dbc");
        const uint8_t* tb = tabTypes.Row(v.tab);
        SetString(L, "Tab", tb ? std::string(tabTypes.Str(tb, 4)) : "INVALID_TAB_TYPE_" + std::to_string(v.tab));
        SetString(L, "Name", v.name);
        lua_pushstring(L, "RequiredLevel");
        lua_pushinteger(L, static_cast<int32_t>(v.requiredLevel));
        lua_settable(L, -3);
        SetString(L, "Icon", v.icon);
        return 1;
    }

    int GetFilteredDesiredEntryAtIndex(lua_State* L) {   // FUN_10a2bd30
        return PushView(L, s_desiredView);
    }

    int GetFilteredUndesiredEntryAtIndex(lua_State* L) {   // FUN_10a2bd70
        return PushView(L, s_undesiredView);
    }

    // FUN_10a329c0 / FUN_10a32af0: every filtered entry added quietly; one event if the list grew.
    int AddAllDesiredFilter(lua_State*) {   // FUN_10a2af00
        const size_t before = s_desired.size();

        for (const View& v : s_desiredView.filtered) {
            AddDesired(v.id, v.kind, true);
        }

        if (s_desired.size() != before) {
            DesiredChanged();
        }

        return 0;
    }

    int AddAllUndesiredFilter(lua_State*) {   // FUN_10a2af10
        const size_t before = s_undesired.size();

        for (const View& v : s_undesiredView.filtered) {
            AddUndesired(v.id, v.kind, true);
        }

        if (s_undesired.size() != before) {
            UndesiredChanged();
        }

        return 0;
    }

    // 0x10A33940 (this file's part): the browsers reset (FUN_10a27d00), both lists emptied without notifying.
    void OnGlueScreen() {
        s_desiredView.Reset();
        s_undesiredView.Reset();
        s_desired.clear();
        s_undesired.clear();
    }

    // The row's kind (FUN_103331b0 on its type string), 0 without a row or for an unknown string.
    uint8_t KindOf(uint32_t id) {
        Row r = CoA::CA::FindRow(id);
        const int t = r ? CoA::CA::RowType(r) : -1;
        return static_cast<uint8_t>(t < 0 ? 0 : t);
    }

    // FUN_10a34e80 (CA list 0x10be2c0c, an entry learned): the browsers re-run, and a learned entry leaves the desired
    // list.
    void OnEntryLearned(uint32_t id, uint32_t) {
        s_desiredView.Refresh();
        s_undesiredView.Refresh();
        RemoveDesired(id, KindOf(id));
    }

    // FUN_10a35080 (CA list 0x10be2c4c, an entry unlearned): an unlearned entry leaves the undesired list.
    void OnEntryUnlearned(uint32_t id, uint32_t) {
        s_desiredView.Refresh();
        s_undesiredView.Refresh();
        RemoveUndesired(id, KindOf(id));
    }

    const CoA::Binding s_bindings[] = {
        COA_NATIVE("C_Wildcard", "AddDesiredID", AddDesiredID),
        COA_NATIVE("C_Wildcard", "AddUndesiredID", AddUndesiredID),
        COA_NATIVE("C_Wildcard", "CanAddDesiredID", CanAddDesiredID),
        COA_NATIVE("C_Wildcard", "CanAddUndesiredID", CanAddUndesiredID),
        COA_NATIVE("C_Wildcard", "IsDesiredID", IsDesiredID),
        COA_NATIVE("C_Wildcard", "IsUndesiredID", IsUndesiredID),
        COA_NATIVE("C_Wildcard", "RemoveDesiredID", RemoveDesiredID),
        COA_NATIVE("C_Wildcard", "RemoveUndesiredID", RemoveUndesiredID),
        COA_NATIVE("C_Wildcard", "ClearDesiredSpells", ClearDesiredSpells),
        COA_NATIVE("C_Wildcard", "ClearUndesiredSpells", ClearUndesiredSpells),
        COA_NATIVE("C_Wildcard", "RemoveAllUndesiredAbilities", RemoveAllUndesiredAbilities),
        COA_NATIVE("C_Wildcard", "RemoveAllUndesiredTalents", RemoveAllUndesiredTalents),
        COA_NATIVE("C_Wildcard", "SetFilteredDesiredEntries", SetFilteredDesiredEntries),
        COA_NATIVE("C_Wildcard", "SetFilteredUndesiredEntries", SetFilteredUndesiredEntries),
        COA_NATIVE("C_Wildcard", "GetNumFilteredDesiredEntries", GetNumFilteredDesiredEntries),
        COA_NATIVE("C_Wildcard", "GetNumFilteredUndesiredEntries", GetNumFilteredUndesiredEntries),
        COA_NATIVE("C_Wildcard", "GetFilteredDesiredEntryAtIndex", GetFilteredDesiredEntryAtIndex),
        COA_NATIVE("C_Wildcard", "GetFilteredUndesiredEntryAtIndex", GetFilteredUndesiredEntryAtIndex),
        COA_NATIVE("C_Wildcard", "AddAllDesiredFilter", AddAllDesiredFilter),
        COA_NATIVE("C_Wildcard", "AddAllUndesiredFilter", AddAllUndesiredFilter),
    };
}

// ---- Init / Reset -------------------------------------------------------------------------------------------------
void CoA::Wildcard::detail::InitSets() {
    CoA::Lifecycle::OnGlueScreen(OnGlueScreen);
    CoA::CA::OnEntryLearned(OnEntryLearned);
    CoA::CA::OnEntryUnlearned(OnEntryUnlearned);
    // The suggestion manager's registrations (0x10333880: FUN_102786c0 / FUN_102786e0).
    s_undesiredAdded.push_back(CoA::CA::SuggestionsDirty);
    s_undesiredRemoved.push_back(CoA::CA::SuggestionsDirty);
    COA_EVENT("WILDCARD_DESIRED_ENTRIES_CHANGED");
    COA_EVENT("WILDCARD_UNDESIRED_ENTRIES_CHANGED");
    CoA::LuaReg::AddModule(s_bindings, sizeof(s_bindings) / sizeof(s_bindings[0]));
}

void CoA::Wildcard::detail::ResetSets() {
    OnGlueScreen();
    s_desiredRemoved.clear();
    s_undesiredAdded.clear();
    s_undesiredRemoved.clear();
}

// ---- exports ------------------------------------------------------------------------------------------------------
bool CoA::WildcardSets::IsUndesired(uint32_t id, uint8_t kind) {
    return Contains(s_undesired, id, kind);
}

size_t CoA::WildcardSets::DesiredCount() {
    return s_desired.size();
}

size_t CoA::WildcardSets::UndesiredCount() {
    return s_undesired.size();
}

bool CoA::WildcardSets::HasDesiredWith(bool ability) {
    return std::any_of(s_desired.begin(), s_desired.end(), [&](const Entry& e) { return ability ? e.ability != 0 : e.talent != 0; });
}

void CoA::WildcardSets::ClearForSpecChange() {   // FUN_10a35270's second half
    if (!s_desired.empty()) {
        ClearDesired(true);
    }

    if (!s_undesired.empty()) {
        ClearUndesired(true);
    }
}

void CoA::WildcardSets::DesiredForRoll(std::vector<uint32_t>& ids, std::vector<uint32_t>& tags) {   // FUN_10a36d00's walk
    for (const Entry& e : s_desired) {
        switch (e.kind) {
            case 1:
            case 2:
            case 4:
                ids.push_back(e.id);
                break;
            case 5:
                tags.push_back(e.id);
                break;
            case 6:
                if (e.id == 1 || e.id == 2) {
                    for (uint32_t s : CoA::CA::SuggestedList(e.id == 1, 0xFA, true)) {   // FUN_10a32940
                        if (Desirable(CoA::CA::FindRow(s))) {
                            ids.push_back(s);
                        }
                    }
                }

                break;
            default:
                break;
        }
    }
}

bool CoA::WildcardSets::KindIndex(const std::string& name, uint8_t& kind) {
    return ::KindIndex(name, kind);
}

// FUN_10a368b0's list swap: each list emptied (every undesired entry it held notified) and refilled through the quiet
// adds; an event only when the list really changed (FUN_10a31ea0).
void CoA::WildcardSets::ApplySelections(const std::vector<Selection>& desired, const std::vector<Selection>& undesired) {
    auto same = [](const std::vector<Entry>& a, const std::vector<Entry>& b) {
        return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](const Entry& x, const Entry& y) {
            return x.id == y.id && x.kind == y.kind && x.ability == y.ability && x.talent == y.talent;
        });
    };
    const std::vector<Entry> oldDesired = s_desired;
    ClearDesired(false);

    for (const Selection& e : desired) {
        AddDesired(e.id, e.kind, true);
    }

    if (!same(oldDesired, s_desired)) {
        DesiredChanged();
    }

    const std::vector<Entry> oldUndesired = s_undesired;
    ClearUndesired(false);

    for (const Selection& e : undesired) {
        AddUndesired(e.id, e.kind, true);
    }

    if (!same(oldUndesired, s_undesired)) {
        UndesiredChanged();
    }
}

// FUN_10a344c0: the newest undesired Ability / Talent / TalentAbility entry with a CharacterAdvancement row whose view
// `enough` accepts; failing that, the newest one with a row at all.
bool CoA::WildcardSets::NewestUnlearnable(Next& out, bool (*enough)(const Next&)) {
    bool have = false;

    for (auto it = s_undesired.rbegin(); it != s_undesired.rend(); ++it) {
        if (it->kind != 1 && it->kind != 2 && it->kind != 4) {
            continue;
        }

        Row r = CoA::CA::FindRow(it->id);

        if (!r) {
            continue;
        }

        const View v = ViewOfRow(r);
        const Next n { v.id, v.kind, v.ability, v.talent };

        if (!have) {
            out = n;
            have = true;
        }

        if (enough(n)) {
            out = n;
            return true;
        }
    }

    return have;
}
