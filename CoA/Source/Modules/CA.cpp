// AscCAMgr.cpp (see CA.h): the rows, the entry index, spell tags and SpellCustomAttr, the known-spell sets, Build, the
// manager. The packets, the update and the glue reset are in CAPackets.cpp; the rules in CARules.cpp.
#include "CoA/Source/Modules/CA.h"
#include "CoA/Source/Modules/CAInternal.h"
#include "CoA/Source/Modules/AttachPatches.h"
#include "CoA/Source/Modules/Config.h"
#include "CoA/Source/Modules/GameEvents.h"
#include "CoA/Source/Modules/GameMode.h"
#include "CoA/Source/Modules/RealmInfo.h"
#include "CoA/Source/Runtime/Events.h"
#include "CoA/Source/Runtime/Log.h"
#include <algorithm>
#include <ctime>

namespace {
#include "CoA/Source/Modules/CAEnums.generated.inc"

    CoA::CA::Deps s_deps;
    CoA::CA::detail::Index s_index;
    CoA::CA::detail::Mgr s_mgr;
    std::set<CoA::CA::Build*> s_builds;
    uint64_t s_serial = 0;
    std::unordered_set<uint32_t> s_knownSpells;
    std::unordered_set<uint32_t> s_runeSpells;
    std::unordered_set<uint32_t> s_knownTags;
    uint32_t s_removingSpell = 0;   // 0x10BDEBC4

    using CoA::CA::Build;
    using CoA::CA::Entry;
    using CoA::CA::Row;
    using CoA::CA::RowU32;

    // FUN_10217450 (map 0x10be05d8, built by FUN_10217670 from SpellTags.dbc's records in file order): spell (+4) ->
    // its tags (+8).
    struct SpellTags {
        bool built = false;
        std::unordered_map<uint32_t, std::vector<uint32_t>> map;
    } s_tags;

    std::unordered_map<uint32_t, std::vector<uint32_t>>& SpellTagMap() {
        if (!s_tags.built) {
            s_tags.built = true;
            CoA::Dbc::Table& t = CoA::Dbc::Get("DBFilesClient\\SpellTags.dbc");

            for (uint32_t i = 0; i < t.Count(); ++i) {
                if (Row r = t.RowAt(i)) {
                    s_tags.map[RowU32(r, 4)].push_back(RowU32(r, 8));
                }
            }
        }

        return s_tags.map;
    }

    const std::vector<uint32_t>& SpellTagsOf(uint32_t spell) {
        static const std::vector<uint32_t> none;
        auto it = SpellTagMap().find(spell);
        return it == SpellTagMap().end() ? none : it->second;
    }

    // FUN_102a7f10 over the map FUN_1020b080 builds from SpellCustomAttr.dbc (spell +4 -> row). Its loop reads index
    // slot i for i in [min, max), i.e. ids 2*min .. max+min-1, and that quirk is kept.
    struct CustomAttrs {
        bool built = false;
        std::unordered_map<uint32_t, Row> map;   // 0x10be0618
    } s_attrs;

    void BuildCustomAttr() {   // FUN_1020b080: the map is cleared, then refilled
        s_attrs.built = true;
        s_attrs.map.clear();
        CoA::Dbc::Table& t = CoA::Dbc::Get("DBFilesClient\\SpellCustomAttr.dbc");

        for (uint32_t i = t.MinId(); t.Loaded() && i < t.MaxId(); ++i) {
            if (Row r = t.Row(i + t.MinId())) {
                s_attrs.map[RowU32(r, 4)] = r;
            }
        }
    }

    Row CustomAttr(uint32_t spell) {
        if (!s_attrs.built) {
            BuildCustomAttr();
        }

        auto it = s_attrs.map.find(spell);
        return it == s_attrs.map.end() ? nullptr : it->second;
    }

    // The client regenerates runes (0x728A20) only for the active player whose display power type (UNIT_FIELD_BYTES_0
    // byte 3) matches the operand of `cmp ebx, 6` at 0x728AA1. The original writes 10 there while a rune spell is known
    // (FUN_1016d5b0) and 6 when none is (FUN_10173330). No power type 10 exists (10 is the Hero class ID), so the original
    // never regenerates runes once any rune spell is known, Death Knights included. Kept (the reconstruction departs from
    // it with `cmp ebx, ebx`).
    void SetRuneRegeneration(bool runeSpellKnown) {
        CoA::AttachPatches::SetRuneUiClass(runeSpellKnown ? 10 : 6);
    }

    std::vector<Row>& ListFor(CoA::CA::detail::ClassLists& l, Row r) {
        switch (CoA::CA::RowType(r)) {
            case 1: return l.ability;
            case 2: return l.talent;
            case 4: return l.talentAbility;
            default: return l.other;
        }
    }

    void AddToClass(Row r, uint32_t cls) {   // FUN_10148850
        std::vector<Row>& list = ListFor(s_index.byClass[cls], r);

        for (Row x : list) {
            if (RowU32(x, 0) == RowU32(r, 0)) {
                return;
            }
        }

        list.push_back(r);
    }

    // An unowned, named wildcard build (a build-creator build) is never short of essence.
    bool Unlimited(const Build& b) {
        return b.guid == 0 && b.wildcard && !b.name.empty();
    }

    // The investment sums re-add each counted entry into an emptied copy (FUN_1014eb80(this, 0) + FUN_10157d30(1),
    // then FUN_101510d0 per entry), so a wildcard choice group (+0x74) is charged only for its first entry.
    enum Cost { AE, TE };

    uint32_t Sum(const Build& b, Cost cost, uint32_t minOff, uint32_t minimum, int cls, int tab) {
        std::vector<uint32_t> seenGroups;
        std::vector<uint32_t> seenIds;
        uint32_t total = 0;

        for (const Entry& e : b.entries) {
            if (minimum && RowU32(e.row, minOff) >= minimum) {
                continue;
            }

            const uint32_t group = RowU32(e.row, 0x74);
            const bool match = (cls < 0 || RowU32(e.row, 0x80) == static_cast<uint32_t>(cls)) &&
                (tab < 0 || RowU32(e.row, 0x84) == static_cast<uint32_t>(tab));

            if (match) {
                const bool charged = !b.wildcard || !group || std::find(seenGroups.begin(), seenGroups.end(), group) == seenGroups.end();

                if (charged) {
                    total += cost == AE ? b.AEColumn(e.row, e.rank) : b.TEColumn(e.row, e.rank);
                }
            }

            if (std::find(seenIds.begin(), seenIds.end(), e.id) == seenIds.end()) {
                seenIds.push_back(e.id);
                seenGroups.push_back(group);
            }
        }

        return total;
    }
}

// ---- detail ------------------------------------------------------------------------------------------------------
namespace CoA::CA::detail {
    CoA::Dbc::Table& Rows() {
        return CoA::Dbc::Get("DBFilesClient\\CharacterAdvancement.dbc");
    }

    CoA::Dbc::Table& Essence() {
        return CoA::Dbc::Get("DBFilesClient\\CharacterAdvancementEssence.dbc");
    }

    CoA::Dbc::Table& ClassTypes() {
        return CoA::Dbc::Get("DBFilesClient\\CharacterAdvancementClassTypes.dbc");
    }

    CoA::Dbc::Table& TabTypes() {
        return CoA::Dbc::Get("DBFilesClient\\CharacterAdvancementTabTypes.dbc");
    }

    const char* RowStr(Row r, uint32_t off) {
        return Rows().Str(r, off);
    }

    uint64_t ActivePlayerGuid() {
        return s_deps.activePlayerGuid ? s_deps.activePlayerGuid() : 0;
    }

    bool UnitExists(uint64_t guid) {
        return guid && s_deps.unitExists && s_deps.unitExists(guid);
    }

    uint8_t UnitClass(uint64_t guid) {
        return UnitExists(guid) && s_deps.unitClass ? s_deps.unitClass(guid) : 0;
    }

    uint32_t UnitLevel(uint64_t guid) {
        return UnitExists(guid) && s_deps.unitLevel ? s_deps.unitLevel(guid) : 0;
    }

    bool UnitInCombat(uint64_t guid) {
        return UnitExists(guid) && s_deps.unitInCombat && s_deps.unitInCombat(guid);
    }

    bool UnitDead(uint64_t guid) {
        return UnitExists(guid) && s_deps.unitDead && s_deps.unitDead(guid);
    }

    bool UnitSchoolImmune(uint64_t guid) {
        return UnitExists(guid) && s_deps.unitSchoolImmune && s_deps.unitSchoolImmune(guid);
    }

    uint32_t ItemCount(uint64_t guid, uint32_t item) {
        if (!guid || guid != ActivePlayerGuid() || !s_deps.unitItemCount) {
            return 0;
        }

        return s_deps.unitItemCount(guid, item);
    }

    uint32_t Money(uint64_t guid) {
        if (!guid || guid != ActivePlayerGuid() || !s_deps.unitMoney) {
            return 0;
        }

        return s_deps.unitMoney(guid);
    }

    int32_t MapType() {
        return s_deps.mapType ? s_deps.mapType() : -1;
    }

    int32_t BattlegroundState() {
        return s_deps.battlegroundState ? s_deps.battlegroundState() : 0;
    }

    bool SpellExists(uint32_t spell) {
        return s_deps.spellExists && s_deps.spellExists(spell);
    }

    uint32_t FirstRank(uint32_t spell) {
        return s_deps.firstRank ? s_deps.firstRank(spell) : spell;
    }

    const std::vector<uint32_t>& Chain(uint32_t firstRank) {
        static const std::vector<uint32_t> none;
        return s_deps.rankChain ? s_deps.rankChain(firstRank) : none;
    }

    bool StockClass(uint8_t c) {
        return (c >= 1 && c <= 9) || c == 11;
    }

    // FUN_101c6cc0: the row is available on this realm at all.
    bool Visible(Row r) {
        const uint32_t flags = RowU32(r, 0x124);

        if ((flags >> 3) & 1 || flags & 1) {
            return false;
        }

        const uint8_t* g = CoA::RealmInfo::Get().gates;
        const bool live = g[0] != 0;
        const bool seasonal = g[1] != 0;
        const bool league = g[2] != 0;
        const bool ptr = g[3] != 0;
        const bool dev = g[4] != 0;
        const bool g46 = g[6] != 0;
        const bool g47 = g[7] != 0;
        const bool heroRealm = dev || !(g46 || g47);
        const uint8_t* ct = ClassTypes().Row(RowU32(r, 0x80));

        if (!ct) {
            return false;
        }

        if ((RowU32(ct, 0xC) != 0) == heroRealm || (RowU32(ct, 0x10) != 0) == g46 || (RowU32(ct, 0x14) != 0) == g47) {
            if ((r[0x128] && live) || (r[0x129] && seasonal) || (r[0x12A] && league) || (r[0x12B] && ptr)) {
                return true;
            }

            if (r[0x12C]) {
                return dev;
            }
        }

        return false;
    }

    // FUN_101c6af0: the row's class type admits the active player's class.
    bool ClassAllowed(Row r) {
        const uint64_t player = ActivePlayerGuid();

        if (!UnitExists(player)) {
            return false;
        }

        return ClassTypeAdmits(r, UnitClass(player));
    }

    uint32_t MaxRank(Row r) {   // FUN_101c6030: spells set in the 9-slot chain, up to the first zero
        uint32_t n = 0;

        while (n < 9 && RowU32(r, 0x14 + n * 4) != 0) {
            ++n;
        }

        return n;
    }

    uint32_t ModeColumn(uint32_t normal, uint32_t draft, uint32_t wildcard) {
        const uint32_t mode = CoA::GameMode::Mode();

        if ((mode >> 3) & 1) {
            return draft;
        }

        return ((mode >> 6) & 1) ? wildcard : normal;
    }

    bool Wildcard() {
        return (CoA::GameMode::Mode() >> 6) & 1;
    }

    Index& Idx() {
        return s_index;
    }

    void BuildIndex() {
        s_index.allSpells.clear();
        s_index.byMastery.clear();
        s_index.visible.clear();
        s_index.byClass.clear();
        s_index.bySpell.clear();
        ForEachRow([](Row r) {
            if (Visible(r) && ClassAllowed(r)) {
                s_index.visible.push_back(r);
                AddToClass(r, 0);

                if (RowU32(r, 0x80) != 0) {
                    AddToClass(r, RowU32(r, 0x80));
                }
            }
        });
        ForEachRow([](Row r) {
            if (!Visible(r)) {
                return;
            }

            for (uint32_t i = 0; i < 9; ++i) {
                if (const uint32_t spell = RowU32(r, 0x14 + i * 4)) {
                    s_index.bySpell[spell].push_back(r);
                }
            }
        });

        // FUN_10147dd0: rows the player's class may use first, then by id.
        for (auto& kv : s_index.bySpell) {
            std::sort(kv.second.begin(), kv.second.end(), [](Row a, Row b) {
                const bool ca = ClassAllowed(a);
                const bool cb = ClassAllowed(b);

                if (ca != cb) {
                    return ca;
                }

                return RowU32(a, 0) < RowU32(b, 0);
            });
        }

        ForEachRow([](Row r) {
            if (!Visible(r)) {
                return;
            }

            for (uint32_t i = 0; i < 3; ++i) {
                const uint32_t m = RowU32(r, 0x1B0 + i * 4);
                Row mr = m ? FindRow(m) : nullptr;

                if (mr && Visible(mr)) {
                    s_index.byMastery[m].push_back(r);
                }
            }
        });
        ForEachRow([](Row r) {
            if (!Visible(r)) {
                return;
            }

            for (uint32_t i = 0; i < 9; ++i) {
                if (const uint32_t spell = RowU32(r, 0x14 + i * 4)) {
                    s_index.allSpells.insert(spell);
                }
            }
        });
        UpdateAllPointers(true);
    }

    // FUN_10148bc0: rebuilt at most every five seconds. Spells past Spell.dbc's max id (0xAD49DC) are left out.
    const std::vector<std::pair<uint32_t, Row>>& KnownSpellIndex() {
        auto& index = s_index.knownSpellIndex;

        if (s_index.knownSpellNext <= _time64(nullptr)) {
            index.clear();
            const uint32_t maxSpell = s_deps.spellMaxId ? s_deps.spellMaxId() : 0;
            const Build* active = ActiveBuild();
            ForEachRow([&](Row r) {
                if (!Visible(r) || !ClassAllowed(r) || !active->Has(RowU32(r, 0))) {
                    return;
                }

                for (uint32_t off = 0x14; off < 0x38; off += 4) {
                    const uint32_t spell = RowU32(r, off);

                    if (!spell || spell > maxSpell) {
                        continue;
                    }

                    auto it = std::lower_bound(index.begin(), index.end(), spell,
                        [](const std::pair<uint32_t, Row>& p, uint32_t s) { return p.first < s; });

                    if (it == index.end() || spell < it->first) {
                        index.insert(it, { spell, r });
                    }
                }
            });
            s_index.knownSpellNext = _time64(nullptr) + 5;
        }

        return index;
    }

    // ---- one row in / out of the entry index (SMSG 0x64A) ----------------------------------------------------------
    // Unlike BuildIndex these do not check Visible for the spell sets, the masteries or the spell lists, and the
    // all-spells set follows the row's 0x1000 flag instead; kept as the original has it.
    template <class Pred>
    void EraseIf(std::vector<Row>& v, Pred pred) {
        v.erase(std::remove_if(v.begin(), v.end(), pred), v.end());
    }

    void UnindexRow(Row r) {   // FUN_10148fd0
        const uint32_t id = RowU32(r, 0);
        auto same = [id](Row x) { return RowU32(x, 0) == id; };

        if (RowHasFlag(r, 0x1000)) {
            for (uint32_t off = 0x14; off < 0x38; off += 4) {
                if (const uint32_t spell = RowU32(r, off)) {
                    s_index.allSpells.erase(spell);
                }
            }
        }

        for (uint32_t off = 0x1B0; off < 0x1BC; off += 4) {
            if (const uint32_t m = RowU32(r, off)) {
                EraseIf(s_index.byMastery[m], same);
            }
        }

        EraseIf(s_index.visible, same);

        for (auto it = s_index.byClass.begin(); it != s_index.byClass.end();) {   // FUN_101473c0
            ClassLists& l = it->second;

            for (std::vector<Row>* v : { &l.other, &l.ability, &l.talent, &l.talentAbility }) {
                EraseIf(*v, same);
            }

            if (l.other.empty() && l.ability.empty() && l.talent.empty() && l.talentAbility.empty()) {
                it = s_index.byClass.erase(it);
            } else {
                ++it;
            }
        }

        for (uint32_t off = 0x14; off < 0x38; off += 4) {
            const uint32_t spell = RowU32(r, off);

            if (!spell) {
                continue;
            }

            EraseIf(s_index.bySpell[spell], same);
            auto& index = s_index.knownSpellIndex;
            auto it = std::lower_bound(index.begin(), index.end(), spell,
                [](const std::pair<uint32_t, Row>& p, uint32_t s) { return p.first < s; });

            if (it != index.end() && it->first == spell && RowU32(it->second, 0) == id) {
                index.erase(it);
            }
        }
    }

    void IndexRow(Row r) {   // FUN_1014a330(row, 1)
        if (RowHasFlag(r, 0x1000)) {
            for (uint32_t off = 0x14; off < 0x38; off += 4) {
                if (const uint32_t spell = RowU32(r, off)) {
                    s_index.allSpells.insert(spell);
                }
            }
        }

        for (uint32_t off = 0x1B0; off < 0x1BC; off += 4) {
            if (const uint32_t m = RowU32(r, off)) {
                s_index.byMastery[m].push_back(r);
            }
        }

        if (Visible(r) && ClassAllowed(r)) {
            const uint32_t id = RowU32(r, 0);

            if (std::none_of(s_index.visible.begin(), s_index.visible.end(), [id](Row x) { return RowU32(x, 0) == id; })) {
                s_index.visible.push_back(r);
            }

            AddToClass(r, 0);

            if (RowU32(r, 0x80) != 0) {
                AddToClass(r, RowU32(r, 0x80));
            }
        }

        for (uint32_t off = 0x14; off < 0x38; off += 4) {
            if (const uint32_t spell = RowU32(r, off)) {
                s_index.bySpell[spell].push_back(r);
            }
        }

        if (Visible(r) && ClassAllowed(r)) {
            const uint32_t maxSpell = s_deps.spellMaxId ? s_deps.spellMaxId() : 0;   // FUN_100b1020
            auto& index = s_index.knownSpellIndex;

            for (uint32_t off = 0x14; off < 0x38; off += 4) {
                const uint32_t spell = RowU32(r, off);

                if (!spell || !s_index.knownSpellNext || spell > maxSpell) {
                    continue;
                }

                auto it = std::lower_bound(index.begin(), index.end(), spell,
                    [](const std::pair<uint32_t, Row>& p, uint32_t s) { return p.first < s; });

                if (it == index.end() || it->first != spell) {
                    index.insert(it, { spell, r });
                } else {
                    it->second = r;   // FUN_10148520: operator[]
                }
            }
        }
    }

    void UpdateAllPointers(bool keepMissing) {   // FUN_10158750
        for (Build* b : s_builds) {
            b->UpdatePointers(keepMissing);
        }
    }

    // ---- known spells ------------------------------------------------------------------------------------------
    std::unordered_set<uint32_t>& KnownSpellSet() {
        return s_knownSpells;
    }

    std::unordered_set<uint32_t>& RuneSpellSet() {
        return s_runeSpells;
    }

    std::unordered_set<uint32_t>& KnownTagSet() {
        return s_knownTags;
    }

    bool CustomAttrA(uint32_t spell, uint32_t mask) {
        Row r = CustomAttr(spell);
        return r && (RowU32(r, 0x10) & mask);
    }

    bool CustomAttrB(uint32_t spell, uint32_t mask) {
        Row r = CustomAttr(spell);
        return r && (RowU32(r, 0x14) & mask);
    }

    bool CustomAttrC(uint32_t spell, uint32_t mask) {
        Row r = CustomAttr(spell);
        return r && (RowU32(r, 0x18) & mask);
    }

    // FUN_1016d7c0
    void OnSpellLearned(uint32_t spell, bool silent) {
        s_knownSpells.insert(spell);

        if (SpellExists(spell) && s_deps.spellRuneCost && s_deps.spellRuneCost(spell)) {
            s_runeSpells.insert(spell);
            SetRuneRegeneration(true);
        }

        for (uint32_t tag : SpellTagsOf(spell)) {
            s_knownTags.insert(tag);
        }

        if (silent) {
            return;
        }

        if (CustomAttrA(spell, 0x400)) {
            CoA::Events::Signal("NOTABLE_SPELL_LEARNED", "%u", spell);
        }

        if (SpellExists(spell) && s_deps.spellHidden && s_deps.spellHidden(spell)) {
            CoA::Events::Signal("ASCENSION_HIDDEN_SPELL_LEARNED", "%u", spell);
        }

        CoA::Events::Signal("ASCENSION_SPELLS_UPDATED", nullptr);
    }

    // FUN_10173550
    void OnSpellUnlearned(uint32_t spell) {
        s_knownSpells.erase(spell);

        if (SpellExists(spell)) {
            if (s_deps.spellHidden && s_deps.spellHidden(spell)) {
                CoA::Events::Signal("ASCENSION_HIDDEN_SPELL_UNLEARNED", "%u", spell);
            }

            if (s_deps.spellRuneCost && s_deps.spellRuneCost(spell)) {
                s_runeSpells.erase(spell);

                if (s_runeSpells.empty()) {
                    SetRuneRegeneration(false);
                }
            }
        }

        std::unordered_set<uint32_t> stillTagged;

        for (uint32_t s : s_knownSpells) {
            for (uint32_t tag : SpellTagsOf(s)) {
                stillTagged.insert(tag);
            }
        }

        for (uint32_t tag : SpellTagsOf(spell)) {
            if (!stillTagged.count(tag)) {
                s_knownTags.erase(tag);
            }
        }

        CoA::Events::Signal("ASCENSION_SPELLS_UPDATED", nullptr);
    }

    // FUN_10172380: the spell's first rank, or any rank of that chain, is known.
    bool ChainKnown(uint32_t spell) {
        const uint32_t first = FirstRank(spell);

        if (s_knownSpells.count(first)) {
            return true;
        }

        for (uint32_t s : Chain(first)) {
            if (s_knownSpells.count(s)) {
                return true;
            }
        }

        return false;
    }

    // ---- the manager ---------------------------------------------------------------------------------------------
    Player::~Player() {
        for (Build* b : this->builds) {
            delete b;
        }
    }

    Build* Player::Get(uint32_t i) const {
        return (i < this->builds.size() && i < this->count) ? this->builds[i] : nullptr;
    }

    void Player::Ensure(uint32_t i) {
        if (i >= 0x14) {
            return;
        }

        if (this->builds.size() <= i) {
            this->builds.resize(i + 1, nullptr);
        }

        if (!this->builds[i]) {
            this->builds[i] = new Build();
        }
    }

    Mgr& M() {
        return s_mgr;
    }

    Player* FindPlayer(uint64_t guid) {   // FUN_1016f700
        if (!guid) {
            return nullptr;
        }

        if (guid == ActivePlayerGuid()) {
            return s_mgr.player;
        }

        auto it = s_mgr.inspect.find(guid);
        return it == s_mgr.inspect.end() ? nullptr : it->second;
    }

    Build* DefaultBuild() {   // 0x10bde8a8, built once on first use (FUN_1014f820(1))
        static Build* b = new Build();
        return b;
    }

    void Fire(const std::vector<PairCallback>& list, uint32_t a, uint32_t b) {
        for (PairCallback cb : list) {
            cb(a, b);
        }
    }

    std::set<Build*>& LiveBuilds() {
        return s_builds;
    }

    uint64_t& Serial() {
        return s_serial;
    }

    // FUN_10153190 & co: the Essence.dbc row for (level, class, the build's mode flags).
    Row EssenceRow(const Build& b, uint32_t lvl) {
        CoA::Dbc::Table& t = Essence();

        for (uint32_t id = t.MinId(); t.Loaded() && id <= t.MaxId(); ++id) {
            Row r = t.Row(id);

            if (!r || RowU32(r, 4) != lvl || RowU32(r, 8) != b.classKey) {
                continue;
            }

            if ((RowU32(r, 0xC) != 0) == (b.draft != 0) && (RowU32(r, 0x10) != 0) == (b.wildcard != 0) &&
                (RowU32(r, 0x14) != 0) == (b.specDraft != 0) && (RowU32(r, 0x18) != 0) == (b.u2c != 0)) {
                return r;
            }
        }

        return nullptr;
    }

    uint32_t ConfigInt(const char* name, int32_t def) {
        const int32_t* v = CoA::Config::Int(name);
        return static_cast<uint32_t>(v ? *v : def);
    }

    bool ConfigBool(const char* name) {
        const bool* v = CoA::Config::Bool(name);
        return v && *v;
    }
}

// ---- rows ----------------------------------------------------------------------------------------------------------
using namespace CoA::CA::detail;

CoA::CA::Row CoA::CA::FindRow(uint32_t id) {
    return Rows().Row(id);
}

int CoA::CA::RowType(Row r) {
    const char* s = RowStr(r, 4);

    for (int i = 0; i < 5; ++i) {
        if (strcmp(s, kEntryTypes[i]) == 0) {
            return i;
        }
    }

    return -1;
}

CoA::CA::Row CoA::CA::RowBySpell(uint32_t spellId) {
    auto it = s_index.bySpell.find(spellId);

    if (it == s_index.bySpell.end() || it->second.empty()) {
        return nullptr;
    }

    return it->second.front();
}

bool CoA::CA::RowVisible(Row r) {
    return Visible(r);
}

uint32_t CoA::CA::RowMaxRank(Row r) {
    return MaxRank(r);
}

bool CoA::CA::ClassAllowedForPlayer(Row r) {
    return ClassAllowed(r);
}

bool CoA::CA::IsCASpell(uint32_t spellId) {
    return s_index.allSpells.count(spellId) != 0;
}

const std::unordered_set<uint32_t>& CoA::CA::KnownSpells() {
    return s_knownSpells;
}

const std::unordered_set<uint32_t>& CoA::CA::KnownTags() {
    return s_knownTags;
}

bool CoA::CA::SpellKnown(uint32_t spellId, bool orBuild) {
    auto knows = [](uint32_t s) {
        return (UnitExists(ActivePlayerGuid()) && s_deps.clientKnowsSpell && s_deps.clientKnowsSpell(s)) || s_knownSpells.count(s) != 0;
    };
    const std::vector<uint32_t>& chain = Chain(spellId);

    if (chain.empty()) {
        if (knows(spellId)) {
            return true;
        }
    } else {
        for (uint32_t s : chain) {
            if (knows(s)) {
                return true;
            }
        }
    }

    if (orBuild) {
        if (Row r = RowBySpell(spellId)) {
            if (const Build* b = ActiveBuild()) {
                return b->Has(RowU32(r, 0));
            }
        }
    }

    return false;
}

bool CoA::CA::ClassTypeAdmits(Row r, uint8_t cls) {
    const uint8_t* ct = ClassTypes().Row(RowU32(r, 0x80));

    if (!ct) {
        return false;
    }

    if (RowU32(ct, 8) != 0) {
        return cls == RowU32(ct, 8);
    }

    if (RowU32(ct, 0xC) != 0) {
        return cls == 10;
    }

    if (RowU32(ct, 0x10) != 0) {
        return cls >= 12 && cls <= 32;
    }

    if (RowU32(ct, 0x14) != 0) {
        return StockClass(cls);
    }

    return false;
}

CoA::CA::Build* CoA::CA::BuildOf(uint64_t guid) {
    Player* p = FindPlayer(guid);
    return p ? p->Get(p->spec) : nullptr;
}

const std::vector<CoA::CA::Row>& CoA::CA::IndexedRows() {
    return s_index.visible;
}

const char* CoA::CA::RowText(Row r, uint32_t off) {
    return RowStr(r, off);
}

uint32_t CoA::CA::SpellAtRankOf(Row r, uint32_t rank) {   // FUN_101c62e0
    if (rank == 0 || rank > 9) {
        return 0;
    }

    return RowU32(r, 0x10 + rank * 4);
}

std::string CoA::CA::QualityName(int q) {
    const uint8_t i = static_cast<uint8_t>(q);
    return i < 9 ? std::string(kQualities[i]) : "UNEXPECTED_ENUM_VALUE_" + std::to_string(i);
}

// FUN_10152dc0: the spells an entry currently gives -- its rank's spell, or that spell's rank chain cut to the learned
// rank (+0xC) -- and FUN_101528d0 takes the last.
uint32_t CoA::CA::EntryCurrentSpell(const Build& b, uint32_t id) {
    const Entry* e = nullptr;

    for (const Entry& x : b.entries) {
        if (x.id == id) {
            e = &x;
            break;
        }
    }

    Row r = e ? FindRow(id) : nullptr;

    if (!r || e->rank == 0) {
        return 0;
    }

    if (e->rank > MaxRank(r)) {
        return 0;
    }

    const uint32_t spell = RowU32(r, 0x10 + e->rank * 4);
    const uint32_t learned = e->u0c;   // FUN_10152f90

    if (!spell || !learned) {
        return 0;
    }

    const std::vector<uint32_t>& chain = Chain(spell);

    if (chain.empty()) {
        return spell;
    }

    const size_t n = learned <= chain.size() ? learned : chain.size();
    return n ? chain[n - 1] : 0;
}

const uint8_t* CoA::CA::SpellCustomAttrRow(uint32_t spell) {
    return CustomAttr(spell);
}

bool CoA::CA::SpellCustomAttr18(uint32_t spell, uint32_t mask) {
    return CustomAttrC(spell, mask);
}

void CoA::CA::RebuildSpellCustomAttr() {
    BuildCustomAttr();
}

const std::vector<uint32_t>& CoA::CA::SpellTagTypesOf(uint32_t spell) {
    return SpellTagsOf(spell);
}

// FUN_10217510: every occurrence of tag leaves the spell's list (the list itself stays).
void CoA::CA::SpellTagRemove(uint32_t spell, uint32_t tag) {
    auto it = SpellTagMap().find(spell);

    if (it != SpellTagMap().end()) {
        it->second.erase(std::remove(it->second.begin(), it->second.end(), tag), it->second.end());
    }
}

// FUN_102175b0: appended unless the spell already lists it.
void CoA::CA::SpellTagAdd(uint32_t spell, uint32_t tag) {
    auto it = SpellTagMap().find(spell);

    if (it != SpellTagMap().end() && std::find(it->second.begin(), it->second.end(), tag) != it->second.end()) {
        return;
    }

    SpellTagMap()[spell].push_back(tag);
}

void CoA::CA::RebuildKnownTags() {
    s_knownTags.clear();

    for (uint32_t spell : s_knownSpells) {
        for (uint32_t tag : SpellTagsOf(spell)) {
            s_knownTags.insert(tag);
        }
    }
}

// ---- Build ---------------------------------------------------------------------------------------------------------
void CoA::CA::Build::Register() {
    this->serial = ++s_serial;
    s_builds.insert(this);
}

CoA::CA::Build::Build() {
    this->Register();
    this->draft = ConfigBool("CONFIG_ENABLE_FULL_DRAFT_MODE");
    this->wildcard = ConfigBool("CONFIG_ENABLE_FULL_WILDCARD_MODE");
    this->qualities = ConfigBool("CONFIG_CHARACTER_ADVANCEMENT_QUALITIES_FORCED");
    this->Prepare();
}

CoA::CA::Build::Build(const Build& src, bool full) {
    this->Register();
    this->guid = src.guid;
    this->spec = -1;
    this->classKey = src.classKey;
    this->draft = src.draft;
    this->wildcard = src.wildcard;
    this->specDraft = src.specDraft;
    this->qualities = src.qualities;
    this->hero = src.hero;
    this->coa = src.coa;
    this->stock = src.stock;
    this->ready = src.ready;
    this->u2c = src.u2c;
    this->level = src.level;
    memcpy(this->credit, src.credit, sizeof(this->credit));
    this->u44 = src.u44;
    this->u48 = src.u48;
    this->name = src.name;
    this->entries = src.entries;

    if (full) {
        this->learnCacheOn = src.learnCacheOn;
        this->learnCache = src.learnCache;
        this->Prepare();
    }
}

CoA::CA::Build::Build(const BuildSeed& seed, uint32_t lvl, bool full) {
    this->Register();
    this->classKey = seed.classKey;
    this->draft = ConfigBool("CONFIG_ENABLE_FULL_DRAFT_MODE");
    this->wildcard = ConfigBool("CONFIG_ENABLE_FULL_WILDCARD_MODE");
    this->specDraft = seed.category == 8 || seed.category == 10 || seed.category == 11;
    this->qualities = ConfigBool("CONFIG_CHARACTER_ADVANCEMENT_QUALITIES_FORCED");
    const uint8_t cls = static_cast<uint8_t>(seed.classKey);
    this->hero = cls == 10;
    this->coa = cls >= 12 && cls <= 32;
    this->stock = StockClass(cls);
    this->level = lvl ? lvl : CoA::GameEvents::ServerMaxLevel();
    this->name = seed.id;   // FUN_10089ef0
    this->entries.reserve(200);

    for (const auto& s : seed.spells) {
        if (s.second <= this->level) {
            if (Row r = RowBySpell(s.first)) {
                this->AddRank(RowU32(r, 0));
            }
        }
    }

    if (this->hero) {
        uint32_t id = 0;

        switch (seed.primaryStat) {
            case 0: id = 0x47D; break;
            case 1: id = 0x47E; break;
            case 3: id = 0x47F; break;
            case 4: id = 0x480; break;
        }

        if (id) {
            this->AddEntry(id, 1);
        }
    }

    if (full) {
        this->Prepare();
    }
}

CoA::CA::Build::~Build() {
    s_builds.erase(this);
}

uint32_t CoA::CA::Build::RankOf(uint32_t id) const {
    for (const Entry& e : this->entries) {
        if (e.id == id) {
            return e.rank;
        }
    }

    return 0;
}

bool CoA::CA::Build::Has(uint32_t id) const {
    for (const Entry& e : this->entries) {
        if (e.id == id) {
            return true;
        }
    }

    return false;
}

bool CoA::CA::Build::LockedOf(uint32_t id) const {
    for (const Entry& e : this->entries) {
        if (e.id == id) {
            return e.locked != 0;
        }
    }

    return false;
}

bool CoA::CA::Build::HasGroup(uint32_t group, uint32_t exceptId) const {
    if (!group) {
        return false;
    }

    for (const Entry& e : this->entries) {
        if (RowU32(e.row, 0x74) == group && e.id != exceptId) {
            return true;
        }
    }

    return false;
}

uint32_t CoA::CA::Build::AEBudget(uint32_t lvl) const {
    if (Unlimited(*this)) {
        return 999;
    }

    Row r = EssenceRow(*this, lvl);
    return r ? RowU32(r, 0x1C) : 0;
}

uint32_t CoA::CA::Build::TEBudget(uint32_t lvl) const {
    if (Unlimited(*this)) {
        return 999;
    }

    Row r = EssenceRow(*this, lvl);
    return r ? RowU32(r, 0x20) : 0;
}

uint32_t CoA::CA::Build::AEColumn(Row r, uint32_t ranks) const {
    if (this->draft) {
        return RowU32(r, 0x48) * ranks;
    }

    return (this->wildcard ? RowU32(r, 0x58) : RowU32(r, 0x38)) * ranks;
}

uint32_t CoA::CA::Build::TEColumn(Row r, uint32_t ranks) const {   // the wildcard TE cost is flat
    if (this->draft) {
        return RowU32(r, 0x4C) * ranks;
    }

    return this->wildcard ? RowU32(r, 0x5C) : RowU32(r, 0x3C) * ranks;
}

uint32_t CoA::CA::Build::AECost(Row r, uint32_t ranks) const {
    if (this->wildcard && RowU32(r, 0x74) && this->HasGroup(RowU32(r, 0x74), 0)) {
        return 0;
    }

    return this->AEColumn(r, ranks);
}

uint32_t CoA::CA::Build::TECost(Row r, uint32_t ranks) const {
    if (this->wildcard && RowU32(r, 0x74) && this->HasGroup(RowU32(r, 0x74), 0)) {
        return 0;
    }

    return this->TEColumn(r, ranks);
}

uint32_t CoA::CA::Build::GlobalAE(uint32_t minimum) const {
    return Sum(*this, AE, 0x88, minimum, -1, -1);
}

uint32_t CoA::CA::Build::GlobalTE(uint32_t minimum) const {
    return Sum(*this, TE, 0x8C, minimum, -1, -1);
}

uint32_t CoA::CA::Build::ClassAE(uint32_t cls, uint32_t minimum) const {
    return Sum(*this, AE, 0x90, minimum, static_cast<int>(cls), -1);
}

uint32_t CoA::CA::Build::ClassTE(uint32_t cls, uint32_t minimum) const {
    return Sum(*this, TE, 0x94, minimum, static_cast<int>(cls), -1);
}

uint32_t CoA::CA::Build::TabAE(uint32_t cls, uint32_t tab, uint32_t minimum) const {
    return Sum(*this, AE, 0x98, minimum, static_cast<int>(cls), static_cast<int>(tab));
}

uint32_t CoA::CA::Build::TabTE(uint32_t cls, uint32_t tab, uint32_t minimum) const {
    return Sum(*this, TE, 0x9C, minimum, static_cast<int>(cls), static_cast<int>(tab));
}

uint32_t CoA::CA::Build::ClassPoints(uint32_t cls, uint32_t minimum) const {   // FUN_10153e90
    uint32_t total = 0;

    for (const Entry& e : this->entries) {
        if ((!minimum || RowU32(e.row, 0xA4) < minimum) && RowU32(e.row, 0x80) == cls) {
            total += RowU32(e.row, 0xA0) * e.rank;
        }
    }

    return total;
}

uint32_t CoA::CA::Build::RemainingAE() const {
    const uint32_t spent = this->GlobalAE(0);
    const uint32_t budget = this->AEBudget(this->level);
    return spent < budget ? budget - spent : 0;
}

uint32_t CoA::CA::Build::RemainingTE() const {
    const uint32_t spent = this->GlobalTE(0);
    const uint32_t budget = this->TEBudget(this->level);
    return spent < budget ? budget - spent : 0;
}

// FUN_101552e0: a tab of 1 means "any tab of the class"; no requirement is always met.
bool CoA::CA::Build::MeetsPoints(uint32_t cls, uint32_t tab, uint32_t points, uint32_t minimum) const {
    if (!points) {
        return true;
    }

    uint32_t total = 0;

    for (const Entry& e : this->entries) {
        if (minimum && RowU32(e.row, 0xA4) >= minimum) {
            continue;
        }

        if (RowU32(e.row, 0x80) == cls && (tab == 1 || RowU32(e.row, 0x84) == tab)) {
            total += RowU32(e.row, 0xA0) * e.rank;
        }

        if (points <= total) {
            return true;
        }
    }

    return false;
}

void CoA::CA::Build::UpdatePointers(bool keepMissing) {
    for (size_t i = 0; i < this->entries.size();) {
        Row r = FindRow(this->entries[i].id);

        if (!r && !(CoA::RealmInfo::Get().gates[4] && keepMissing)) {
            CoA::Log::Printf("CharacterAdvancementBuildEntry::UpdatePointers: entry %u not found", this->entries[i].id);
        }

        this->entries[i].row = r;

        if (r) {
            ++i;
        } else {
            this->entries.erase(this->entries.begin() + static_cast<std::ptrdiff_t>(i));
        }
    }
}

// ---- manager -------------------------------------------------------------------------------------------------------
int CoA::CA::ActiveSpecIndex() {
    Player* p = FindPlayer(ActivePlayerGuid());
    return p ? static_cast<int>(p->spec) : -1;
}

CoA::CA::Build* CoA::CA::ActiveBuild() {
    if (Player* p = FindPlayer(ActivePlayerGuid())) {
        if (Build* b = p->Get(p->spec)) {
            return b;
        }
    }

    return DefaultBuild();
}

CoA::CA::Build* CoA::CA::PendingBuild() {
    return s_mgr.pending;
}

void CoA::CA::RequestAutoLearn() {
    s_mgr.autoLearn = true;
}

uint64_t CoA::CA::BumpPendingVersion() {
    return ++s_mgr.pendingVersion;
}

uint32_t CoA::CA::AbilityCount(const Build& b) {
    uint32_t count = 0;

    for (const Entry& e : b.entries) {
        if (Row r = FindRow(e.id)) {
            const int type = RowType(r);
            count += type == 1 || type == 4;
        }
    }

    return count;
}

void CoA::CA::ResetPendingBuild() {   // FUN_10173d00
    Build* next = new Build(*ActiveBuild(), true);
    Build* old = s_mgr.pending;
    s_mgr.pending = next;
    delete old;
    next->Reorder(false);        // FUN_101557d0(0)
    next->pendingHooks = true;   // lambdas 1..4 (FUN_10157fb0 / 10157e60 / 10157f40 / 10157ed0)
    ++s_mgr.pendingVersion;
}

void CoA::CA::OnEntryLearned(void (*cb)(uint32_t, uint32_t)) {
    s_mgr.onLearned.push_back(cb);
}

void CoA::CA::OnEntryRankChanged(void (*cb)(uint32_t, uint32_t)) {
    s_mgr.onRankChanged.push_back(cb);
}

void CoA::CA::OnEntryUnlearned(void (*cb)(uint32_t, uint32_t)) {
    s_mgr.onUnlearned.push_back(cb);
}

void CoA::CA::OnSpecChanged(void (*cb)(uint32_t, uint32_t)) {
    s_mgr.onSpecChanged.push_back(cb);
}

void CoA::CA::SetRemovingSpell(uint32_t spell) {
    s_removingSpell = spell;
}

uint32_t CoA::CA::RemovingSpell() {
    return s_removingSpell;
}

CoA::CA::Deps& CoA::CA::Dependencies() {
    return s_deps;
}

void CoA::CA::Reset() {
    for (auto& kv : s_mgr.inspect) {
        delete kv.second;
    }

    delete s_mgr.player;
    delete s_mgr.pending;
    s_mgr = detail::Mgr();
    s_index = detail::Index();
    s_knownSpells.clear();
    s_runeSpells.clear();
    s_knownTags.clear();
    s_tags = SpellTags();
    s_attrs = CustomAttrs();
    s_removingSpell = 0;
    s_deps = Deps();
}
