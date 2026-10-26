// AscCARules.cpp: the CharacterAdvancement learn rules (FUN_1015cbf0 installs them, FUN_10151480 runs them), the unlearn
// and purge rules, the learnable-order pass (FUN_101557d0), mastery removal and apply validation.
//
// Each rule is a static object with a vtable {?, applies(ctx, row), ok(ctx, row)}, stored in slot n of the build's
// unit-rule vector (+0x1A8, ctx = the owner's client object) or build-rule vector (+0x1B4, ctx = the build). Slot n is
// CA_LEARN_* value n; a slot fails when its rule applies and is not ok. Every installed rule's applies() is 0x1015B7C0
// (`mov al,1; ret 8`), so only ok() is transcribed. Slot 29 (a per-build object, FUN_1015b7d0) and slots 7, 32, 33, 34,
// 36 use 0x1015B7C0 for ok() too -- those checks are the server's.
//
// Which slots a build gets depends on FUN_10157450's flags: 1 normal, 2 draft, 4 wildcard (1 only when neither), | 8
// when qualities are on. A named build then loses slots 8, 16 and 19 (FUN_10150a80).
//
// The BUILD_CREATOR slots (learn 18, unlearn 16) find their template through Deps::seedOfBuild (C_BuildCreator).
// Dependencies not ported yet, and what the original does without their data:
//   slot 20 NO_TALENTS_CHALLENGE  FUN_1013b8b0 / FUN_1013b350 (C_Challenge rules): no challenge -> ok.
#include "CoA/Source/Modules/CA.h"
#include "CoA/Source/Modules/CAInternal.h"
#include "CoA/Source/Modules/GameMode.h"
#include "CoA/Source/Modules/RealmInfo.h"
#include "CoA/Source/Runtime/Script.h"
#include <algorithm>
#include <map>

namespace {
#include "CoA/Source/Modules/CAEnums.generated.inc"

    using namespace CoA::CA::detail;
    using CoA::CA::Build;
    using CoA::CA::Entry;
    using CoA::CA::Row;
    using CoA::CA::RowHasFlag;
    using CoA::CA::RowU32;
    using CoA::CA::TokenCost;

    bool IsTalent(Row r) {
        return CoA::CA::RowType(r) == 2;
    }

    bool IsAbilityLike(Row r) {   // FUN_101c6770
        const int t = CoA::CA::RowType(r);
        return t == 1 || t == 4;
    }

    // The mode's required-level column (FUN_10153af0).
    uint32_t RequiredLevel(const Build& b, Row r) {
        if (b.draft) {
            return RowU32(r, 0x6C);
        }

        return b.wildcard ? RowU32(r, 0x70) : RowU32(r, 0x68);
    }

    // ---- qualities (slots 21..24) --------------------------------------------------------------------------------
    int QualityOf(const Build& b, Row r) {   // FUN_101539b0: the mode's quality string -> kQualities index
        const char* s = RowStr(r, b.draft ? 0x50 : b.wildcard ? 0x60 : 0x40);

        for (int i = 0; i < 9; ++i) {
            if (strcmp(s, kQualities[i]) == 0) {
                return i;
            }
        }

        return 0;
    }

    uint32_t QualityCost(const Build& b, Row r) {   // FUN_101539f0
        if (b.wildcard && RowU32(r, 0x74) && b.HasGroup(RowU32(r, 0x74), RowU32(r, 0))) {
            return 0;
        }

        for (uint32_t i = 0; i < 3; ++i) {
            if (const uint32_t m = RowU32(r, 0x1B0 + i * 4)) {
                if (b.Has(m)) {
                    return 0;
                }
            }
        }

        return b.draft ? RowU32(r, 0x54) : b.wildcard ? RowU32(r, 0x64) : RowU32(r, 0x44);
    }

    uint32_t QualitySum(const Build& b, int quality) {   // FUN_10153f60
        uint32_t total = 0;

        for (const Entry& e : b.entries) {
            if (QualityOf(b, e.row) == quality) {
                total += QualityCost(b, e.row);
            }
        }

        return total;
    }

    bool QualityOk(const Build& b, Row r, int quality, const char* stem, int32_t def) {
        if (QualityOf(b, r) != quality) {
            return true;
        }

        const uint32_t ruleset = CoA::RealmInfo::Get().ruleset;
        const char* exp = ruleset == 0 ? "CLASSIC" : ruleset == 1 ? "THE_BURNING_CRUSADE" : "WRATH_OF_THE_LICH_KING";
        const std::string name = std::string("CONFIG_CHARACTER_ADVANCEMENT_QUALITIES_MAX_") + stem + "_ABILITIES_" + exp;
        const uint32_t max = ConfigInt(name.c_str(), def) & 0xFF;   // FUN_10196ab0
        return QualityCost(b, r) + QualitySum(b, quality) <= max;
    }

    // ---- wildcard level pools (slot 28) --------------------------------------------------------------------------
    // FUN_10152b30: per (class, draft, wildcard, specDraft, u2c) key, Essence.dbc's {level, AE, TE} rows in level order.
    struct Pool {
        uint32_t level;
        uint32_t ae;
        uint32_t te;
    };

    uint64_t PoolKey(uint32_t cls, bool f3, bool f4, bool f5, bool f6) {
        return (static_cast<uint64_t>(cls) << 4) | (static_cast<uint64_t>(f3) << 3) | (static_cast<uint64_t>(f4) << 2) |
            (static_cast<uint64_t>(f5) << 1) | static_cast<uint64_t>(f6);
    }

    std::map<uint64_t, std::vector<Pool>>& Pools() {
        static std::map<uint64_t, std::vector<Pool>> pools;
        static uint32_t builtMin = 0;
        static uint32_t builtMax = 0;
        CoA::Dbc::Table& t = Essence();

        if (pools.empty() || builtMin != t.MinId() || builtMax != t.MaxId()) {
            pools.clear();
            builtMin = t.MinId();
            builtMax = t.MaxId();

            for (uint32_t id = t.MinId(); t.Loaded() && id <= t.MaxId(); ++id) {
                if (Row r = t.Row(id)) {
                    pools[PoolKey(RowU32(r, 8), RowU32(r, 0xC) != 0, RowU32(r, 0x10) != 0, RowU32(r, 0x14) != 0, RowU32(r, 0x18) != 0)]
                        .push_back({ RowU32(r, 4), RowU32(r, 0x1C), RowU32(r, 0x20) });
                }
            }

            for (auto& kv : pools) {
                std::sort(kv.second.begin(), kv.second.end(), [](const Pool& a, const Pool& b) { return a.level < b.level; });
            }
        }

        return pools;
    }

    // FUN_10153510 (AE, ability roll) / FUN_10153750 (TE, talent roll): the lowest pool level whose budget covers what
    // is spent plus one roll, capped at the build's level.
    uint32_t PoolLevel(const Build& b, bool talent) {
        const uint32_t need = talent ? b.GlobalTE(0) + ConfigInt("CONFIG_WILDCARD_TALENT_ROLL_COST", 1)
                                     : b.GlobalAE(0) + ConfigInt("CONFIG_WILDCARD_ABILITY_ROLL_COST", 2);
        auto& pools = Pools();
        auto it = pools.find(PoolKey(b.classKey, b.draft, b.wildcard, b.specDraft, b.u2c != 0));

        if (it == pools.end()) {
            return b.level;
        }

        const std::vector<Pool>& v = it->second;
        auto budget = [talent](const Pool& x) { return talent ? x.te : x.ae; };
        auto p = std::lower_bound(v.begin(), v.end(), need, [&](const Pool& x, uint32_t n) { return budget(x) < n; });

        if (p == v.end()) {
            return b.level;
        }

        while (p + 1 != v.end() && budget(*(p + 1)) == budget(*p)) {
            ++p;
        }

        uint32_t level = p->level;

        if (talent) {
            level = std::max(level, ConfigInt("CONFIG_WILDCARD_TALENT_ROLL_POOL_START_LEVEL", 10));
        }

        return std::min(level, b.level);
    }

    // FUN_10160df0 / FUN_101649b0 past their guard: the Build Creator build named like this build (FUN_100fd750 on the
    // +0x4C name; none -> fail), built at this build's level (FUN_1014f0f0(entry, +0x30, 1)), must hold more ranks of the
    // row than this build would after learning one (learn), or fewer than it holds (unlearn).
    bool TemplateAllows(const Build& b, Row r, bool learn) {
        const CoA::CA::Deps& d = CoA::CA::Dependencies();
        CoA::CA::BuildSeed seed;

        if (!d.seedOfBuild || !d.seedOfBuild(b.name, seed)) {
            return false;
        }

        const Build tmpl(seed, b.level, true);
        const uint32_t id = RowU32(r, 0);
        return learn ? tmpl.RankOf(id) >= b.RankOf(id) + 1 : tmpl.RankOf(id) < b.RankOf(id);
    }

    // ---- build rules (ctx = the build) ---------------------------------------------------------------------------
    // Slot 3 (BAD_ABILITY) runs without a row; the others are only reached with one. The row's fields are read only when
    // there is a row (the reconstruction read them up front and faulted on an unknown id).
    bool BuildRuleOk(uint32_t slot, Build& b, Row r) {
        const uint32_t flags = r ? RowU32(r, 0x124) : 0;

        switch (slot) {
            case 3: return r != nullptr;                                        // FUN_10160de0 BAD_ABILITY
            case 4: return !(flags & 0x20000);                                  // FUN_10160f00 DISPLAY_ENTRY
            case 5: return RequiredLevel(b, r) <= b.level;                      // FUN_10160fe0 LOW_LEVEL
            case 6: return b.RankOf(RowU32(r, 0)) < MaxRank(r);                // FUN_10160db0 ALREADY_KNOWN
            case 9: {                                                           // FUN_101644b0 WRONG_EXPANSION
                const char* exp = RowStr(r, 0x120);
                uint32_t index = 0;

                for (uint32_t i = 0; i < 4; ++i) {
                    if (strcmp(exp, kExpansions[i]) == 0) {
                        index = i;
                        break;
                    }
                }

                return index <= CoA::RealmInfo::Get().ruleset;
            }
            case 10: return !(flags & 8);                                       // FUN_10160ef0 DISABLED
            case 11: return CoA::CA::ClassTypeAdmits(r, static_cast<uint8_t>(b.classKey));   // FUN_10164490 WRONG_CLASS
            case 12:                                                            // FUN_10160d20 ALREADY_KNOW_A_STARTING_NODE
                if (!r[0xE0]) {
                    return true;
                }

                for (const Entry& e : b.entries) {
                    if (Row er = CoA::CA::FindRow(e.id)) {
                        if (er[0xE0]) {
                            return false;
                        }
                    }
                }

                return true;
            case 13: {                                                          // FUN_101616c0 MISSING_CONNECTED_ENTRIES
                bool any = false;

                for (uint32_t i = 0; i < 15; ++i) {
                    any = any || RowU32(r, 0xE4 + i * 4) != 0;
                }

                if (!any) {
                    return true;
                }

                for (uint32_t i = 0; i < 15; ++i) {
                    const uint32_t c = RowU32(r, 0xE4 + i * 4);

                    if (!c) {
                        continue;
                    }

                    if (RowHasFlag(r, 0x200000)) {
                        Row cr = CoA::CA::FindRow(c);

                        if (cr && b.RankOf(c) == MaxRank(cr) && b.Has(c)) {   // FUN_10155090(c, max rank)
                            return true;
                        }
                    } else if (b.Has(c)) {
                        return true;
                    }
                }

                return false;
            }
            case 14:                                                            // FUN_10161790 MISSING_REQUIRED_ID
                for (uint32_t i = 0; i < 3; ++i) {
                    if (const uint32_t q = RowU32(r, 8 + i * 4)) {
                        if (!b.Has(q)) {
                            return false;
                        }
                    }
                }

                return true;
            case 15: return Visible(r);                                         // FUN_10164620 WRONG_REALM
            case 17: return !(flags & 1);                                       // FUN_10160ee0 DEPRECATED
            case 18:                                                            // FUN_10160df0 BUILD_CREATOR
                if ((b.specDraft || b.u64 || b.u65) && (b.u65 || RowU32(r, 0x74) != 1)) {
                    return TemplateAllows(b, r, true);
                }

                return true;
            case 21: return QualityOk(b, r, 2, "UNCOMMON", 8);                  // FUN_10163ed0
            case 22: return QualityOk(b, r, 3, "RARE", 8);                      // FUN_10163b90
            case 23: return QualityOk(b, r, 4, "EPIC", 4);                      // FUN_101634f0
            case 24: return QualityOk(b, r, 5, "LEGENDARY", 2);                 // FUN_10163830
            case 25: return !(RowU32(r, 0x74) == 2 && b.HasGroup(2, 0));        // FUN_10164460 WILDCARD_TAME_SPELLS
            case 26: return !(flags & 0x1000);                                  // FUN_10164320 WILDCARD_MASTERIES
            case 27: return !(flags & 0x10000);                                 // FUN_10164250 WILDCARD_DISABLED
            case 28: {                                                          // FUN_10164270 WILDCARD_LOW_LEVEL
                const uint32_t req = RequiredLevel(b, r);

                if (IsAbilityLike(r)) {
                    return req <= b.level && req <= PoolLevel(b, false);
                }

                if (IsTalent(r)) {
                    return req <= b.level && req <= PoolLevel(b, true);
                }

                return req <= b.level;
            }
            case 37: {                                                          // FUN_10160f20 GROUP
                const uint32_t g = RowU32(r, 0x74);
                return !(g != 0 && g != 1 && b.HasGroup(g, 0));
            }
            case 38: {                                                          // FUN_101624e0 NOT_ENOUGH_INVESTED_AE
                const uint32_t cls = RowU32(r, 0x80);
                const uint32_t tab = RowU32(r, 0x84);

                if (const uint32_t n = RowU32(r, 0x88)) {
                    if (b.GlobalAE(n) < n) {
                        return false;
                    }
                }

                if (const uint32_t n = RowU32(r, 0x90)) {
                    if (b.ClassAE(cls, n) < n) {
                        return false;
                    }
                }

                const uint32_t n = RowU32(r, 0x98);
                return !n || n <= b.TabAE(cls, tab, n);
            }
            case 39: {                                                          // FUN_10163020 NOT_ENOUGH_INVESTED_TE
                const uint32_t cls = RowU32(r, 0x80);
                const uint32_t tab = RowU32(r, 0x84);

                if (const uint32_t n = RowU32(r, 0x8C)) {
                    if (b.GlobalTE(n) < n) {
                        return false;
                    }
                }

                if (const uint32_t n = RowU32(r, 0x94)) {
                    if (b.ClassTE(cls, n) < n) {
                        return false;
                    }
                }

                const uint32_t n = RowU32(r, 0x9C);
                return !n || n <= b.TabTE(cls, tab, n);
            }
            case 40: {                                                          // FUN_10162c70 NOT_ENOUGH_INVESTED_POINTS
                const uint32_t n = RowU32(r, 0xA4);
                return !n || n <= b.ClassPoints(RowU32(r, 0x80), n);
            }
            case 41: {                                                          // FUN_10161230 MISSING_AE
                const uint32_t cost = b.AECost(r, 1);
                return cost == 0 || cost <= b.RemainingAE();
            }
            case 42: {                                                          // FUN_101617d0 MISSING_TE
                const uint32_t cost = b.TECost(r, 1);
                return cost == 0 || cost <= b.RemainingTE();
            }
            case 43:                                                            // FUN_10161960 MODE_RESTRICTED
                if (!(flags & 0x1800000)) {
                    return true;
                }

                if ((flags & 0x800000) && b.wildcard) {
                    return true;
                }

                return (flags & 0x1000000) && b.draft;
            case 44: return RowU32(r, 0x74) == 1;                               // FUN_101634e0 NOT_WILDCARD
            default: return true;
        }
    }

    // ---- unit rules (ctx = the owner's client object) -----------------------------------------------------------
    bool UnitRuleOk(uint32_t slot, uint64_t unit, Row r) {
        const uint32_t flags = RowU32(r, 0x124);

        switch (slot) {
            case 8: {                                                           // FUN_10163300 NOT_IN_BATTLEGROUNDS
                if (IsTalent(r)) {
                    return true;
                }

                if (CoAClass(UnitClass(unit)) && !(flags & 0x40000)) {
                    return true;
                }

                const int32_t type = MapType();

                if (type < 0) {
                    return true;
                }

                if (type != 4) {
                    if (type != 3) {
                        return true;
                    }

                    Build* b = CoA::CA::BuildOf(unit);

                    if (RowU32(r, 0x74) != 1 || (b && !b->HasGroup(1, 0))) {
                        return BattlegroundState() != 1;
                    }
                }

                return false;
            }
            case 16: {                                                          // FUN_10163410 NOT_IN_COMBAT
                if (!UnitInCombat(unit) || CoAClass(UnitClass(unit))) {
                    return true;
                }

                const int32_t type = MapType();

                if (!IsTalent(r) || type < 0) {
                    return false;
                }

                return type == 1 || type == 2;
            }
            case 19: return !UnitSchoolImmune(unit);                            // FUN_10160f50 INVULNERABLE
            case 20: return true;   // FUN_101619b0 NO_TALENTS_CHALLENGE: no C_Challenge rule held until it is ported
            case 30: return !UnitDead(unit);                                    // FUN_101634c0 NOT_WHILE_DEAD
            case 31: return !UnitInCombat(unit) || RowU32(r, 0x74) != 1;        // FUN_10164430 WILDCARD_NOT_IN_COMBAT
            case 35: {                                                          // FUN_10164340 WILDCARD_NOT_IN_BGS
                Build* b = CoA::CA::BuildOf(unit);

                if (!b) {
                    return false;
                }

                if (CoAClass(UnitClass(unit)) && !(flags & 0x40000)) {
                    return true;
                }

                const int32_t type = MapType();

                if ((type == 4 || type == 3) && RowU32(r, 0x74) == 1) {
                    return !b->HasGroup(1, 0);
                }

                return true;
            }
            default: return true;
        }
    }

    // FUN_1015cbf0's slots per mode (see the header comment).
    const uint32_t kAnyBuild[] = { 3, 4, 6, 9, 10, 17, 43 };
    const uint32_t kAnyUnit[] = { 7, 30 };
    const uint32_t kNormalDraftBuild[] = { 41, 42, 5, 11, 12, 13, 38, 39, 40, 14, 15, 18, 37 };
    const uint32_t kNormalDraftUnit[] = { 8, 16, 19, 20 };
    const uint32_t kWildcardBuild[] = { 44, 25, 26, 27, 28 };
    const uint32_t kWildcardUnit[] = { 29, 31, 32, 33, 34, 35, 36 };
    const uint32_t kQualityBuild[] = { 21, 22, 23, 24 };

    template <size_t N>
    void Set(uint64_t& mask, const uint32_t (&slots)[N]) {
        for (uint32_t s : slots) {
            mask |= 1ull << s;
        }
    }

    bool Contains(const std::vector<uint32_t>& v, uint32_t x) {
        return std::find(v.begin(), v.end(), x) != v.end();
    }

    // The owner's unit when none is given (ObjectPtr(guid, 0x10)).
    uint64_t OwnerUnit(uint64_t unit, uint64_t guid) {
        if (unit) {
            return unit;
        }

        return guid && UnitExists(guid) ? guid : 0;
    }

    void Remember(Build& b, uint32_t id, uint32_t result) {   // FUN_10158a60(id, 1, result)
        if (!b.learnCacheOn) {
            return;
        }

        if (!result) {
            b.ForgetResult(id);
            return;
        }

        for (auto& kv : b.learnCache) {
            if (kv.first == id) {
                kv.second = { 1, result };
                return;
            }
        }

        b.learnCache.push_back({ id, { 1, result } });
    }

    bool HasRequirements(Row r) {
        for (uint32_t i = 0; i < 3; ++i) {
            if (RowU32(r, 8 + i * 4)) {
                return true;
            }
        }

        return false;
    }

    // FUN_1014d470's order: masteries first, then the mode's required level, then id.
    bool NormalOrder(const Build& b, const Entry& x, const Entry& y) {
        const bool mx = RowHasFlag(x.row, 0x1000);
        const bool my = RowHasFlag(y.row, 0x1000);

        if (mx != my) {
            return mx;
        }

        const uint32_t lx = RequiredLevel(b, x.row);
        const uint32_t ly = RequiredLevel(b, y.row);

        if (lx != ly) {
            return lx < ly;
        }

        return x.id < y.id;
    }

    // FUN_10150950 (CoA builds): no required ids first, then free, then AE-costing, then PositionY (larger first),
    // PositionX, id.
    bool CoAOrder(const Build& b, const Entry& x, const Entry& y) {
        const bool rx = HasRequirements(x.row);
        const bool ry = HasRequirements(y.row);

        if (rx != ry) {
            return !rx;
        }

        const bool fx = b.AECost(x.row, 1) == 0 && b.TECost(x.row, 1) == 0;
        const bool fy = b.AECost(y.row, 1) == 0 && b.TECost(y.row, 1) == 0;

        if (fx != fy) {
            return fx;
        }

        const bool ax = b.AECost(x.row, 1) != 0;
        const bool ay = b.AECost(y.row, 1) != 0;

        if (ax != ay) {
            return ax;
        }

        const float yx = RowF32(x.row, 0xD4);
        const float yy = RowF32(y.row, 0xD4);

        if (yx != yy) {
            return yy < yx;
        }

        const float xx = RowF32(x.row, 0xD0);
        const float xy = RowF32(y.row, 0xD0);

        if (xx != xy) {
            return xx < xy;
        }

        return x.id < y.id;
    }

    // One step per rank, sorted in the build's learn order.
    std::vector<Entry> Steps(const Build& b) {
        std::vector<Entry> steps;

        for (const Entry& e : b.entries) {
            for (uint32_t k = 0; k < e.rank; ++k) {
                Entry s = e;
                s.rank = 1;
                steps.push_back(s);
            }
        }

        if (b.coa) {
            std::sort(steps.begin(), steps.end(), [&b](const Entry& x, const Entry& y) { return CoAOrder(b, x, y); });
        } else {
            std::sort(steps.begin(), steps.end(), [&b](const Entry& x, const Entry& y) { return NormalOrder(b, x, y); });
        }

        return steps;
    }

    // ---- unlearn rules (FUN_10151a50) ----------------------------------------------------------------------------
    bool HasItems(uint64_t unit, uint32_t item, uint32_t count) {   // FUN_10308700
        if (!unit || unit != ActivePlayerGuid()) {
            return count == 0;
        }

        return count <= ItemCount(unit, item);
    }

    bool HasMoney(uint64_t unit, uint32_t money) {   // FUN_103087a0
        return unit && unit == ActivePlayerGuid() && money <= Money(unit);
    }

    const uint32_t kMarkOfAscension = 0x5B9D2;

    // FUN_10154650: what unlearning r costs this build.
    TokenCost UnlearnCost(const Build& b, Row r) {
        TokenCost c { 0, 0, 0, 0 };

        if (b.level <= 10 || RowHasFlag(r, 0x2000)) {
            return c;
        }

        const double level = static_cast<double>(b.level);
        const uint32_t tier = level <= 19.0 ? 32
            : (level >= 20.0 && level <= 29.0) ? 71
            : (level >= 30.0 && level <= 49.0) ? 521
            : (level >= 50.0 && level <= 59.0) ? 1107
            : level >= 60.0 ? 2221 : 0;
        double base = static_cast<double>(tier) * level;
        double extra = 0.0;

        if (b.coa) {
            base *= 0.25;
            extra = static_cast<double>(static_cast<int32_t>(b.credit[1] + b.credit[2])) * 4.16666666666667 * level * 0.25;
        }

        c.money = static_cast<uint32_t>(CoA::Script::ToInt(std::min(std::max(2147483647.0 - base, 0.0), extra) + base));
        c.marks = b.coa ? b.level * 4 : b.stock ? b.level * 5 : 250;
        return c;
    }

    bool CanPay(uint64_t unit, const TokenCost& c) {   // FUN_10151400
        if (!c.count && !c.marks && !c.money) {
            return true;
        }

        if (c.item && HasItems(unit, c.item, c.count)) {
            return true;
        }

        if (c.marks && HasItems(unit, kMarkOfAscension, c.marks)) {
            return true;
        }

        return c.money && HasMoney(unit, c.money);
    }

    bool UnlearnBuildRuleOk(uint32_t slot, Build& b, Row r) {
        if (!r) {
            return slot != 3;   // BAD_ABILITY without a row (the others are not reached without one)
        }

        const uint32_t id = RowU32(r, 0);
        const uint32_t flags = RowU32(r, 0x124);

        switch (slot) {
            case 3:                                                             // FUN_10164940 BAD_ABILITY
                for (uint32_t i = 0; i < 9; ++i) {
                    if (const uint32_t spell = RowU32(r, 0x14 + i * 4)) {
                        if (!SpellExists(spell)) {
                            return false;
                        }
                    }
                }

                return true;
            case 5: return !(flags & 4);                                        // FUN_10165450 NOT_THIS_ABILITY
            case 7: return b.Has(id);                                           // FUN_10165430 NOT_KNOWN
            case 9: {                                                           // FUN_10164b10 MISSING_CONNECTED_ENTRIES
                Build copy(b, true);
                copy.Remove(id);
                uint32_t x;
                uint32_t y;
                uint32_t z;
                return copy.ValidateAll(true, { 0xD }, {}, false, x, y, z);
            }
            case 10:                                                            // FUN_10164ac0 MASTERY_ID
                for (const Entry& e : b.entries) {
                    for (uint32_t i = 0; i < 3; ++i) {
                        if (RowU32(e.row, 0x1B0 + i * 4) == id) {
                            return false;
                        }
                    }
                }

                return true;
            case 11:                                                            // FUN_10164c90 MISSING_REQUIRED_ID
                for (const Entry& e : b.entries) {
                    if (e.id != id) {
                        for (uint32_t i = 0; i < 3; ++i) {
                            if (RowU32(e.row, 8 + i * 4) == id) {
                                return false;
                            }
                        }
                    }
                }

                return true;
            case 13: return !b.LockedOf(id);                                    // FUN_10164aa0 LOCKED
            case 14:                                                            // FUN_10165670 WILDCARD_UNSPENT_AE
            case 15: {                                                          // FUN_101657d0 WILDCARD_UNSPENT_TE
                if ((b.u44 & 0xFF) && b.GlobalTE(0) == 0) {
                    return true;
                }

                const bool ae = slot == 14;

                if (!(ae ? IsAbilityLike(r) : IsTalent(r))) {
                    return true;
                }

                // FUN_10155120 / FUN_10155160: one more roll still fits the budget.
                const uint32_t spent = ae ? b.GlobalAE(0) + ConfigInt("CONFIG_WILDCARD_ABILITY_ROLL_COST", 2)
                                          : b.GlobalTE(0) + ConfigInt("CONFIG_WILDCARD_TALENT_ROLL_COST", 1);
                return !(spent <= (ae ? b.AEBudget(b.level) : b.TEBudget(b.level)));
            }
            case 16:                                                            // FUN_101649b0 BUILD_CREATOR
                if ((b.specDraft || b.u64 || b.u65) && (b.u65 || RowU32(r, 0x74) != 1)) {
                    return TemplateAllows(b, r, false);
                }

                return true;
            case 17:
            case 18:
            case 19:
                // FUN_10164e80 / 10165180 / 10165000 revalidate a FRESH default build (FUN_1014f820), which is empty, so
                // FUN_10158050 succeeds at once -- the original's slots never fail.
                return true;
            case 21: return false;                                              // FUN_100cc240 NOT_WILDCARD
            default: return true;
        }
    }

    // FUN_101656c0: the unspent-essence slots only apply without this config.
    bool UnlearnApplies(uint32_t slot) {
        if (slot != 14 && slot != 15) {
            return true;
        }

        return !ConfigBool("CONFIG_WILDCARD_SCROLL_OF_FORTUNE_ALLOW_UNLEARN_WITH_UNSPENT_ESSENCE");
    }

    bool UnlearnUnitRuleOk(uint32_t slot, uint64_t unit, Row r) {
        const uint32_t flags = RowU32(r, 0x124);

        switch (slot) {
            case 4: {                                                           // FUN_10165300 NOT_IN_BATTLEGROUNDS
                Build* b = CoA::CA::BuildOf(unit);

                if (!b) {
                    return false;
                }

                if (IsTalent(r) || (CoAClass(UnitClass(unit)) && !(flags & 0x40000))) {
                    return true;
                }

                const int32_t type = MapType();

                if (type < 0) {
                    return true;
                }

                if (type == 4) {
                    return false;
                }

                if (type == 3) {
                    if (RowU32(r, 0x74) == 1 && b->HasGroup(1, 0)) {
                        return false;
                    }

                    return BattlegroundState() != 1;
                }

                return true;
            }
            case 6: return !UnitInCombat(unit);                                 // FUN_10165410 NOT_IN_COMBAT
            case 8: {                                                           // FUN_10164de0 NO_UNLEARN_ITEM
                if (flags & 0x2000 || UnitLevel(unit) <= 9) {
                    return true;
                }

                Build* b = CoA::CA::BuildOf(unit);
                return b && CanPay(unit, UnlearnCost(*b, r));
            }
            default:
                // 12 NO_SCROLL_OF_FORTUNE / 20 SCROLL_OF_FORTUNE_LIMIT (wildcard only) count Scroll of Fortune items
                // through a manager not ported yet; slot 21 fails every wildcard unlearn regardless.
                return true;
        }
    }

    void RememberUnlearn(Build& b, uint32_t id, uint32_t result) {   // FUN_10158f50(id, 1, result)
        if (!b.learnCacheOn) {
            return;
        }

        for (size_t i = 0; i < b.unlearnCache.size(); ++i) {
            if (b.unlearnCache[i].first == id) {
                if (result) {
                    b.unlearnCache[i].second = result;
                } else {
                    b.unlearnCache.erase(b.unlearnCache.begin() + static_cast<std::ptrdiff_t>(i));
                }

                return;
            }
        }

        if (result) {
            b.unlearnCache.push_back({ id, result });
        }
    }

    bool PurgeUnitRuleOk(uint32_t slot, bool talents, uint64_t unit) {
        switch (slot) {
            case 3: {                                                           // FUN_101647f0 / FUN_101648f0
                const int32_t type = MapType();

                if (type < 0) {
                    return true;
                }

                if (type == 4) {
                    return false;
                }

                if (type == 3) {
                    return BattlegroundState() != 1;
                }

                return true;
            }
            case 4: return !UnitInCombat(unit);                                 // FUN_10164840
            case 5: {                                                           // FUN_10164780 / FUN_10164880
                Build* b = CoA::CA::BuildOf(unit);
                return b && CanPay(unit, CoA::CA::PurgeCost(*b, talents));
            }
            default: return true;
        }
    }

    bool PurgeBuildRuleOk(uint32_t slot, bool talents, const Build& b) {
        switch (slot) {
            case 6: return b.SpentAtLeast(talents, 1, 0);                       // FUN_10164770 / FUN_10164870
            case 7:                                                             // FUN_10164630
                return ConfigBool("CONFIG_CHARACTER_ADVANCEMENT_HERO_ABILITY_TALENT_PURGE_ENABLED") || b.classKey != 10;
            default: return true;
        }
    }

    // ---- apply validation ----------------------------------------------------------------------------------------
    struct DiffItem {
        uint32_t id;
        int32_t delta;
        bool added;
        bool removed;
        bool modified;
    };

    std::vector<DiffItem> Diff(const Build& now, const Build& base) {   // FUN_101523d0 (this = now)
        std::vector<DiffItem> out;

        for (const Entry& e : now.entries) {
            if (!base.Has(e.id)) {
                out.push_back({ e.id, static_cast<int32_t>(e.rank), true, false, false });
            }
        }

        for (const Entry& e : base.entries) {
            if (!now.Has(e.id)) {
                out.push_back({ e.id, -static_cast<int32_t>(e.rank), false, true, false });
            }
        }

        for (const Entry& e : now.entries) {
            bool listed = false;

            for (const DiffItem& d : out) {
                listed = listed || d.id == e.id;
            }

            if (!listed && e.rank != base.RankOf(e.id)) {
                out.push_back({ e.id, static_cast<int32_t>(e.rank) - static_cast<int32_t>(base.RankOf(e.id)), false, false, true });
            }
        }

        return out;
    }

    // FUN_1014bb50: the same {id, rank} multiset.
    bool SameEntries(const std::vector<Entry>& a, const std::vector<Entry>& b) {
        if (a.size() != b.size()) {
            return false;
        }

        std::vector<std::pair<uint32_t, uint32_t>> x;
        std::vector<std::pair<uint32_t, uint32_t>> y;

        for (const Entry& e : a) {
            x.push_back({ e.id, e.rank });
        }

        for (const Entry& e : b) {
            y.push_back({ e.id, e.rank });
        }

        std::sort(x.begin(), x.end());
        std::sort(y.begin(), y.end());
        return x == y;
    }

    // FUN_101547e0: every removal costs its unlearn price, in Marks of Ascension while they last, else money.
    bool PayForRemovals(uint64_t unit, const Build& base, const std::vector<DiffItem>& diff, CoA::CA::ApplyCosts& costs) {
        costs = CoA::CA::ApplyCosts();
        Build temp(base, false);
        std::vector<TokenCost> prices;

        for (const DiffItem& d : diff) {
            if (d.removed) {
                if (Row r = CoA::CA::FindRow(d.id)) {
                    prices.push_back(UnlearnCost(temp, r));
                    temp.Remove(d.id);
                }
            }
        }

        uint32_t marksLeft = ItemCount(unit, kMarkOfAscension);

        for (const TokenCost& p : prices) {
            if (!(p.item && p.count) && !p.marks && !p.money) {
                continue;
            }

            if (!p.marks || marksLeft < p.marks) {
                if (!p.money || !HasMoney(unit, costs.money + p.money)) {
                    return false;
                }

                costs.money += p.money;
            } else {
                marksLeft -= p.marks;
                costs.marks += p.marks;
            }
        }

        return true;
    }
}

uint32_t CoA::CA::PoolLevelOf(const Build& b, bool talent) {
    return PoolLevel(b, talent);
}

void CoA::CA::Build::Prepare() {
    uint32_t mode = (this->draft ? 2 : 0) | (this->wildcard ? 4 : 0);

    if (!mode) {
        mode = 1;
    }

    if (this->qualities) {
        mode |= 8;
    }

    this->learnUnitRules = 0;
    this->learnBuildRules = 0;

    if (mode & 7) {
        Set(this->learnBuildRules, kAnyBuild);
        Set(this->learnUnitRules, kAnyUnit);
    }

    if (mode & 3) {
        Set(this->learnBuildRules, kNormalDraftBuild);
        Set(this->learnUnitRules, kNormalDraftUnit);
    }

    if (mode & 4) {
        Set(this->learnBuildRules, kWildcardBuild);
        Set(this->learnUnitRules, kWildcardUnit);
    }

    if (mode & 8) {
        Set(this->learnBuildRules, kQualityBuild);
    }

    // The installer marks these slots in the +0x1C0 bitset as it installs them (the helpers mark none).
    this->learnMask = 0;

    for (uint32_t s = 3; s <= 30; ++s) {
        if ((s <= 20 || (s >= 25 && s <= 27) || s == 30) && (((this->learnUnitRules | this->learnBuildRules) >> s) & 1)) {
            this->learnMask |= 1ull << s;
        }
    }

    this->unlearnUnitRules = 0;
    this->unlearnBuildRules = 0;

    if (mode & 7) {
        for (uint32_t s : { 3u, 7u, 13u }) {
            this->unlearnBuildRules |= 1u << s;
        }
    }

    if (mode & 3) {
        for (uint32_t s : { 4u, 8u, 6u }) {
            this->unlearnUnitRules |= 1u << s;
        }

        for (uint32_t s : { 9u, 17u, 18u, 19u, 11u, 5u, 10u, 16u }) {
            this->unlearnBuildRules |= 1u << s;
        }
    }

    if (mode & 4) {
        for (uint32_t s : { 12u, 20u }) {
            this->unlearnUnitRules |= 1u << s;
        }

        for (uint32_t s : { 21u, 14u, 15u }) {
            this->unlearnBuildRules |= 1u << s;
        }
    }

    // Purge slots: 3 NOT_IN_BATTLEGROUNDS, 4 NOT_IN_COMBAT, 5 NO_PURGE_ITEM on the unit; 6 NO_KNOWN_*, 7 HERO_CLASS on
    // the build. Same layout for abilities and talents.
    for (int t = 0; t < 2; ++t) {
        this->purgeUnitRules[t] = 0;
        this->purgeBuildRules[t] = 0;

        if (mode & 3) {
            this->purgeBuildRules[t] |= 1u << 6;
            this->purgeUnitRules[t] |= (1u << 5) | (1u << 3) | (1u << 4);
        }

        if (mode & 7) {
            this->purgeBuildRules[t] |= 1u << 7;
        }
    }

    this->unlearnCache.clear();

    if (!this->name.empty()) {
        for (uint32_t s : { 8u, 16u, 19u }) {
            this->learnUnitRules &= ~(1ull << s);
            this->learnBuildRules &= ~(1ull << s);
        }
    }

    this->learnCache.clear();
    this->learnCacheOn = true;
    this->ready = 1;
}

void CoA::CA::Build::ForgetResult(uint32_t id) {
    if (!this->learnCacheOn) {
        return;
    }

    for (size_t i = 0; i < this->learnCache.size(); ++i) {
        if (this->learnCache[i].first == id) {
            this->learnCache.erase(this->learnCache.begin() + static_cast<std::ptrdiff_t>(i));
            return;
        }
    }
}

uint32_t CoA::CA::Build::ValidateLearn(uint64_t unitIn, uint32_t id, const std::vector<uint32_t>& include, const std::vector<uint32_t>& exclude) {
    if (!this->ready || (!this->learnUnitRules && !this->learnBuildRules)) {
        return 0;
    }

    Row r = FindRow(id);
    const uint64_t unit = OwnerUnit(unitIn, this->guid);
    auto fails = [&](uint32_t slot) {
        if ((this->learnBuildRules >> slot) & 1 && !BuildRuleOk(slot, *this, r)) {
            return true;
        }

        return unit && ((this->learnUnitRules >> slot) & 1) && !UnitRuleOk(slot, unit, r);
    };

    // A remembered failure is returned while it still fails.
    if (r && this->learnCacheOn) {
        for (auto& kv : this->learnCache) {
            if (kv.first == id && kv.second.first == 1) {
                const uint32_t cached = kv.second.second;

                if (cached && cached != 0x2C && (include.empty() || Contains(include, cached)) && (exclude.empty() || !Contains(exclude, cached)) &&
                    fails(cached)) {
                    return cached;
                }

                break;
            }
        }
    }

    for (uint32_t slot = 0; slot < 45; ++slot) {
        if (!include.empty() && !Contains(include, slot)) {
            continue;
        }

        if (!exclude.empty() && Contains(exclude, slot)) {
            continue;
        }

        if (!r && slot != 3) {
            continue;   // every rule but BAD_ABILITY reads the row
        }

        if (fails(slot)) {
            Remember(*this, id, slot);
            return slot;
        }
    }

    Remember(*this, id, 0);
    return 0;
}

void CoA::CA::Build::AddEntry(uint32_t id, uint32_t rank) {   // FUN_10151190 (+ FUN_1014ff10)
    if (this->Has(id)) {
        return;
    }

    Entry e;
    e.id = id;
    e.rank = rank;
    e.row = FindRow(id);

    // lambda_1 (FUN_10183190): a choice-group entry evicts the build's other entries of its group.
    if (this->pendingHooks && e.row) {
        if (const uint32_t group = RowU32(e.row, 0x74)) {
            std::vector<uint32_t> members;   // FUN_10152960(group, 0)

            for (const Entry& x : this->entries) {
                if (RowU32(x.row, 0x74) == group && x.id != 0) {
                    members.push_back(x.id);
                }
            }

            for (uint32_t m : members) {
                this->Remove(m);
            }
        }
    }

    this->entries.push_back(e);

    if (this->pendingHooks) {
        RequestAutoLearn();   // lambda_2
    }

    this->ForgetResult(id);
}

void CoA::CA::Build::SetRank(uint32_t id, uint32_t rank) {   // FUN_10158940
    for (Entry& e : this->entries) {
        if (e.id == id) {
            if (e.rank != rank) {
                e.rank = rank;

                if (this->pendingHooks) {
                    RequestAutoLearn();   // lambda_3
                }

                this->ForgetResult(id);
            }

            return;
        }
    }
}

void CoA::CA::Build::Remove(uint32_t id) {   // FUN_10157c30
    for (size_t i = 0; i < this->entries.size(); ++i) {
        if (this->entries[i].id == id) {
            const Entry removed = this->entries[i];
            this->entries.erase(this->entries.begin() + static_cast<std::ptrdiff_t>(i));

            // lambda_4: removing anything but a 0x100000 (auto-learned) row re-runs the auto-learn pass.
            if (this->pendingHooks && !(removed.row && RowHasFlag(removed.row, 0x100000))) {
                RequestAutoLearn();
            }

            this->ForgetResult(id);
            return;
        }
    }
}

void CoA::CA::Build::AddRank(uint32_t id) {   // FUN_10151390
    if (this->Has(id)) {
        this->SetRank(id, this->RankOf(id) + 1);
    } else {
        this->AddEntry(id, 1);
    }
}

bool CoA::CA::Build::Optimize(bool restoreOnFail, uint32_t& outResult, uint32_t& outId, uint32_t& outRank) {
    outResult = 0;
    outId = 0;
    outRank = 0;
    std::vector<Entry> steps = Steps(*this);
    Build temp(*this, true);
    temp.entries.clear();
    const std::vector<uint32_t> none;
    const std::vector<uint32_t> notWildcard = { 0x2C };
    std::vector<Entry> placed;

    while (!steps.empty()) {
        bool progress = false;

        for (size_t i = 0; i < steps.size();) {
            if (temp.ValidateLearn(0, steps[i].id, none, notWildcard) == 0) {
                placed.push_back(steps[i]);
                temp.AddRank(steps[i].id);
                steps.erase(steps.begin() + static_cast<std::ptrdiff_t>(i));
                progress = true;
            } else {
                ++i;
            }
        }

        if (!progress) {
            // FUN_101560d0 reports the first step left; the entries are unchanged either way.
            outId = steps.front().id;
            outRank = steps.front().rank;
            outResult = temp.ValidateLearn(0, outId, none, notWildcard);
            (void)restoreOnFail;
            return false;
        }
    }

    std::vector<Entry> merged;

    for (const Entry& p : placed) {
        auto it = std::find_if(merged.begin(), merged.end(), [&](const Entry& m) { return m.id == p.id; });

        if (it == merged.end()) {
            merged.push_back(p);
        } else {
            ++it->rank;
        }
    }

    this->entries = merged;
    return true;
}

bool CoA::CA::Build::Reorder(bool restoreOnFail) {   // FUN_101557d0: the same pass without the report
    uint32_t r;
    uint32_t i;
    uint32_t k;
    return this->Optimize(restoreOnFail, r, i, k);
}

bool CoA::CA::Build::TraverseOrFix(const std::vector<uint32_t>& keep) {
    const std::vector<uint32_t> none;
    const std::vector<uint32_t> notWildcard = { 0x2C };
    auto learnable = [&](const Build& b) {
        uint32_t id;
        uint32_t rank;
        uint32_t result;

        if (!b.ValidateAll(true, none, notWildcard, false, id, rank, result)) {
            return false;
        }

        for (uint32_t k : keep) {
            if (!b.Has(k)) {
                return false;
            }
        }

        return true;
    };

    if (learnable(*this)) {
        return true;
    }

    const std::vector<Entry> saved = this->entries;   // FUN_1014e460

    if (this->Reorder(false)) {
        if (learnable(*this)) {
            return true;
        }

        this->entries = saved;   // FUN_1014b040
    }

    // One unit per rank, split into kept and removable.
    std::vector<uint32_t> kept;
    std::vector<uint32_t> removable;

    for (const Entry& e : saved) {
        for (uint32_t k = 0; k < e.rank; ++k) {
            (std::find(keep.begin(), keep.end(), e.id) != keep.end() ? kept : removable).push_back(e.id);
        }
    }

    for (uint32_t k : keep) {
        if (!this->Has(k)) {
            return false;
        }
    }

    const size_t n = removable.size();

    if (!n) {
        return false;
    }

    // FUN_10150af0: rebuild from the kept units plus the removable ones the mask leaves in, reorder and validate; on
    // success the build takes the result.
    auto attempt = [&](const std::vector<uint8_t>& mask) {
        std::vector<std::pair<uint32_t, uint32_t>> counts;   // id -> units, in first-seen order
        auto count = [&](uint32_t id) {
            for (auto& c : counts) {
                if (c.first == id) {
                    ++c.second;
                    return;
                }
            }

            counts.push_back({ id, 1 });
        };

        for (uint32_t id : kept) {
            count(id);
        }

        for (size_t i = 0; i < n; ++i) {
            if (mask[i]) {
                count(removable[i]);
            }
        }

        Build trial(*this, true);
        trial.entries.clear();   // FUN_10157d30(1)

        for (const auto& c : counts) {
            Entry e;
            e.id = c.first;
            e.rank = c.second;
            trial.entries.push_back(e);
        }

        trial.UpdatePointers(false);
        trial.Reorder(false);

        if (!learnable(trial)) {
            return false;
        }

        this->entries = trial.entries;
        return true;
    };

    std::vector<uint8_t> mask(n, 1);

    if (attempt(mask)) {
        return true;
    }

    for (size_t size = 1; size <= n; ++size) {
        std::vector<size_t> pick(size);

        for (size_t i = 0; i < size; ++i) {
            pick[i] = i;
        }

        for (;;) {
            std::fill(mask.begin(), mask.end(), 1);

            for (size_t p : pick) {
                mask[p] = 0;
            }

            if (attempt(mask)) {
                return true;
            }

            size_t i = size;

            while (i > 0 && pick[i - 1] == i - 1 + n - size) {
                --i;
            }

            if (i == 0) {
                break;
            }

            ++pick[i - 1];

            for (size_t j = i; j < size; ++j) {
                pick[j] = pick[j - 1] + 1;
            }
        }
    }

    this->entries = saved;
    return false;
}

// ---- ValidateAll (FUN_10158050) ------------------------------------------------------------------------------------
bool CoA::CA::Build::ValidateAll(bool inOrder, const std::vector<uint32_t>& include, std::vector<uint32_t> exclude, bool maskMode,
    uint32_t& outId, uint32_t& outRank, uint32_t& outResult) const {
    outId = 0;
    outRank = 0;
    outResult = 0;

    if (this->entries.empty()) {
        return true;
    }

    Build temp(*this, true);
    temp.entries.clear();

    if (maskMode) {
        for (uint32_t s = 0; s < 45; ++s) {
            if (!((temp.learnMask >> s) & 1) && (((temp.learnUnitRules | temp.learnBuildRules) >> s) & 1)) {
                exclude.push_back(s);
            }
        }

        if (temp.draft) {
            exclude.push_back(5);
        }
    }

    if (inOrder) {
        for (const Entry& e : this->entries) {
            for (uint32_t k = 0; k < e.rank; ++k) {
                const uint32_t result = temp.ValidateLearn(0, e.id, include, exclude);

                if (result) {
                    outId = e.id;
                    outRank = e.rank;
                    outResult = result;
                    return false;
                }

                temp.AddRank(e.id);
            }
        }

        return true;
    }

    // The Reorder pass with these sets; the first step left over is reported.
    std::vector<Entry> steps = Steps(*this);

    while (!steps.empty()) {
        bool progress = false;

        for (size_t i = 0; i < steps.size();) {
            if (temp.ValidateLearn(0, steps[i].id, include, exclude) == 0) {
                temp.AddRank(steps[i].id);
                steps.erase(steps.begin() + static_cast<std::ptrdiff_t>(i));
                progress = true;
            } else {
                ++i;
            }
        }

        if (!progress) {
            outId = steps.front().id;
            outRank = steps.front().rank;
            outResult = temp.ValidateLearn(0, outId, include, exclude);
            return false;
        }
    }

    return true;
}

CoA::CA::TokenCost CoA::CA::UnlearnCostOf(const Build& b, Row r) {
    return UnlearnCost(b, r);
}

int CoA::CA::QualityIndexOf(const Build& b, Row r) {
    return QualityOf(b, r);
}

uint32_t CoA::CA::QualitySumOf(const Build& b, int quality) {
    return QualitySum(b, quality);
}

uint32_t CoA::CA::QualityCostOf(const Build& b, Row r) {
    return QualityCost(b, r);
}

uint32_t CoA::CA::RequiredLevelOf(const Build& b, Row r) {
    return RequiredLevel(b, r);
}

uint32_t CoA::CA::UnitItemCount(uint64_t unit, uint32_t item) {
    return ItemCount(unit, item);
}

// ---- purge (unlearn all) -------------------------------------------------------------------------------------------
// FUN_10152650 (abilities, E = +0x34) / FUN_101543b0 (talents, E = +0x38). Free below level 11 except for the token
// entry; the gold cost is a cubic in the level, capped at 0xFFFFFFFF and truncated through __dtol3 (so a negative value
// wraps, as in the original).
CoA::CA::TokenCost CoA::CA::PurgeCost(const Build& b, bool talents) {
    TokenCost c { 0, 0, 0, 0 };
    const uint32_t e = talents ? b.credit[1] : b.credit[0];
    const bool paid = b.level > 10;

    if (paid) {
        const double lvl = static_cast<double>(b.level);
        double x;

        if (b.coa) {
            x = lvl * lvl * 0x1.29867c3eca801p+0 + static_cast<double>(e) * 0x1.555555546ac55p-5 * 10000.0;
            x = (x + lvl * 0x1.2b42e6bdc5bb1p+4 - 359.025) * lvl * 0.25;
        } else {
            x = lvl * lvl * 139.465 + static_cast<double>(e * 50000u) + lvl * 2244.46 - 43083.0;
        }

        c.money = static_cast<uint32_t>(static_cast<int64_t>(4294967295.0 < x ? 4294967295.0 : x));
    }

    if (b.coa) {
        c.marks = paid ? b.level * (talents ? 0x7D : 0xA7) : 0;
        c.item = talents ? 0xE06FB : 0xE06FA;
        c.count = paid ? 1 : 0;
        return c;
    }

    if (paid) {
        c.marks = b.stock ? b.level * 25 : (talents ? 7500 : 10000);
    }

    if (!talents) {
        c.item = 0x5D86A;   // the abilities variant reads the config and ignores it
        c.count = paid ? 1 : 0;
    } else if (ConfigBool("CONFIG_CHARACTER_ADVANCEMENT_HERO_ABILITY_TALENT_PURGE_ENABLED") || b.stock) {
        c.item = 0x5D86B;
        c.count = paid ? 1 : 0;
    }

    return c;
}

bool CoA::CA::Build::SpentAtLeast(bool talents, uint32_t amount, uint32_t below) const {
    if (!amount) {
        return true;
    }

    Build walk(*this, false);   // FUN_10157d30(1) on a hookless copy: emptied, then re-added entry by entry
    walk.entries.clear();
    uint32_t total = 0;

    for (const Entry& e : this->entries) {
        if (below && RowU32(e.row, talents ? 0x8C : 0x88) >= below) {
            continue;
        }

        total += talents ? walk.TECost(e.row, e.rank) : walk.AECost(e.row, e.rank);

        if (!walk.Has(e.id)) {   // FUN_101510d0
            walk.entries.push_back(e);
        }

        if (total >= amount) {
            return true;
        }
    }

    return false;
}

uint32_t CoA::CA::Build::ValidatePurge(bool talents, uint64_t unitIn) const {
    const int t = talents ? 1 : 0;

    if (!this->ready || !this->purgeUnitRules[t] || !this->purgeBuildRules[t]) {
        return 0;
    }

    const uint64_t unit = OwnerUnit(unitIn, this->guid);

    for (uint32_t slot = 0; slot < 8; ++slot) {   // each rule's gate (FUN_1015a580) always admits
        if (((this->purgeBuildRules[t] >> slot) & 1) && !PurgeBuildRuleOk(slot, talents, *this)) {
            return slot;
        }

        if (unit && ((this->purgeUnitRules[t] >> slot) & 1) && !PurgeUnitRuleOk(slot, talents, unit)) {
            return slot;
        }
    }

    return 0;
}

uint32_t CoA::CA::Build::ValidateUnlearn(uint64_t unitIn, uint32_t id, const std::vector<uint32_t>& include, const std::vector<uint32_t>& exclude) {
    if (!this->ready || (!this->unlearnUnitRules && !this->unlearnBuildRules)) {
        return 0;
    }

    Row r = FindRow(id);
    const uint64_t unit = OwnerUnit(unitIn, this->guid);
    auto fails = [&](uint32_t slot) {
        if (!UnlearnApplies(slot)) {
            return false;
        }

        if (((this->unlearnBuildRules >> slot) & 1) && !UnlearnBuildRuleOk(slot, *this, r)) {
            return true;
        }

        return unit && ((this->unlearnUnitRules >> slot) & 1) && !UnlearnUnitRuleOk(slot, unit, r);
    };

    if (r && this->learnCacheOn) {
        for (auto& kv : this->unlearnCache) {
            if (kv.first == id) {
                const uint32_t cached = kv.second;

                if ((include.empty() || Contains(include, cached)) && (exclude.empty() || !Contains(exclude, cached)) && fails(cached)) {
                    return cached;
                }

                break;
            }
        }
    }

    for (uint32_t slot = 0; slot < 22; ++slot) {
        if (!include.empty() && !Contains(include, slot)) {
            continue;
        }

        if (!exclude.empty() && Contains(exclude, slot)) {
            continue;
        }

        if (!r && slot != 3) {
            continue;
        }

        if (fails(slot)) {
            RememberUnlearn(*this, id, slot);
            return slot;
        }
    }

    RememberUnlearn(*this, id, 0);
    return 0;
}

// ---- mastery removal (FUN_101731e0) --------------------------------------------------------------------------------
bool CoA::CA::Build::MasteryInvolved(uint32_t id) const {   // FUN_10173c80: known, and a mastery or carrying a known one
    Row r = FindRow(id);

    if (!this->Has(id) || !r) {
        return false;
    }

    if (RowHasFlag(r, 0x1000)) {
        return true;
    }

    for (uint32_t i = 0; i < 3; ++i) {
        if (const uint32_t m = RowU32(r, 0x1B0 + i * 4)) {
            if (this->Has(m)) {
                return true;
            }
        }
    }

    return false;
}

uint32_t CoA::CA::Build::MasteryTarget(uint32_t id) const {   // FUN_1016f4f0
    Row r = FindRow(id);

    if (!r) {
        return 0;
    }

    if (RowHasFlag(r, 0x1000) && this->Has(id)) {
        return id;
    }

    for (uint32_t i = 0; i < 3; ++i) {
        if (const uint32_t m = RowU32(r, 0x1B0 + i * 4)) {
            if (this->Has(m)) {
                return m;
            }
        }
    }

    return 0;
}

bool CoA::CA::Build::ClassFusion() const {   // FUN_10172490: class fusion on a Hero build in a plain mode
    return ConfigBool("CONFIG_CLASS_FUSION_ENABLED") && this->hero && !this->coa && !this->stock && (CoA::GameMode::Mode() & 0x1DEF) == 0;
}

std::vector<CoA::CA::Entry> CoA::CA::Build::TakeMasteryBearers() {   // FUN_101738f0
    std::vector<Entry> saved;

    for (const Entry& e : this->entries) {
        for (uint32_t i = 0; i < 3; ++i) {
            if (RowU32(e.row, 0x1B0 + i * 4)) {
                saved.push_back(e);
                break;
            }
        }
    }

    for (const Entry& e : saved) {
        this->Remove(e.id);
    }

    return saved;
}

void CoA::CA::Build::ReaddWhileValid(const std::vector<Entry>& saved) {   // FUN_10173e20
    for (bool progress = true; progress;) {
        progress = false;

        for (const Entry& e : saved) {
            if (this->RankOf(e.id) < e.rank && this->ValidateLearn(0, e.id, {}, {}) == 0) {
                this->AddRank(e.id);
                progress = true;
            }
        }
    }
}

void CoA::CA::Build::RemoveWithMastery(uint32_t id) {   // FUN_101731e0
    if (!this->MasteryInvolved(id)) {
        this->Remove(id);
        return;
    }

    if (this->ClassFusion()) {
        const uint32_t mastery = this->MasteryTarget(id);
        const std::vector<Entry> saved = this->TakeMasteryBearers();
        this->Remove(mastery);
        this->ReaddWhileValid(saved);
        return;
    }

    // FUN_101488f0 with rank 0: the mastery and every entry that lists it.
    Row r = FindRow(id);
    uint32_t mastery = RowHasFlag(r, 0x1000) ? id : 0;

    for (uint32_t i = 0; i < 3 && !mastery; ++i) {
        if (const uint32_t m = RowU32(r, 0x1B0 + i * 4)) {
            if (this->Has(m)) {
                mastery = m;
            }
        }
    }

    if (!mastery) {
        this->Remove(id);
        return;
    }

    std::vector<uint32_t> doomed = { mastery };

    for (const Entry& e : this->entries) {
        for (uint32_t i = 0; i < 3; ++i) {
            if (RowU32(e.row, 0x1B0 + i * 4) == mastery) {
                doomed.push_back(e.id);
                break;
            }
        }
    }

    for (uint32_t d : doomed) {
        this->Remove(d);
    }
}

uint32_t CoA::CA::ValidateApply(uint64_t unit, const Build& base, const std::vector<Entry>& entries, uint32_t& learnResult, uint32_t& id,
    uint32_t& rank, ApplyCosts& costs) {
    learnResult = 0;
    id = 0;
    rank = 0;
    costs = ApplyCosts();

    for (const Entry& e : entries) {   // FUN_10151df0
        if (!FindRow(e.id)) {
            id = e.id;
            rank = e.rank;
            return 3;
        }
    }

    Build now(base, false);
    now.entries = entries;
    now.UpdatePointers(false);
    const std::vector<DiffItem> diff = Diff(now, base);   // FUN_10151fd0

    if (diff.empty()) {
        return 4;
    }

    for (const DiffItem& d : diff) {
        Row r = FindRow(d.id);

        if (r && RowU32(r, 0x74) != 1 && (now.wildcard || (now.draft && !IsTalent(r)))) {
            return 8;
        }
    }

    std::vector<Entry> kept;

    for (const Entry& e : base.entries) {
        if (RowU32(e.row, 0x74) == 1 || RowHasFlag(e.row, 2)) {
            kept.push_back(e);
        }
    }

    bool allRemovals = true;

    for (const DiffItem& d : diff) {
        allRemovals = allRemovals && d.removed;
    }

    if (!(allRemovals && SameEntries(now.entries, kept))) {
        if (!now.ValidateAll(true, {}, { 0x2C }, true, id, rank, learnResult)) {
            return 5;
        }
    }

    if (!PayForRemovals(unit, base, diff, costs)) {
        return 6;
    }

    if (costs.marks && !HasItems(unit, kMarkOfAscension, costs.marks)) {
        return 7;
    }

    if (costs.money && !HasMoney(unit, costs.money)) {
        return 7;
    }

    for (const TokenCost& t : costs.tokens) {
        if (!HasItems(unit, t.item, t.count)) {
            return 7;
        }
    }

    return 0;
}
