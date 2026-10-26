#ifndef COA_SOURCE_MODULES_CA_H
#define COA_SOURCE_MODULES_CA_H

#include <cstdint>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

struct lua_State;

// AscCAMgr.hpp: the original's CharacterAdvancementMgr (static 0x10bde440, accessor FUN_101722e0) and the objects it
// owns. CA.cpp (rows, index, known spells, builds, manager, packets), CARules.cpp (the learn / unlearn / purge rules,
// the reorder pass, apply validation), CAClient.cpp (Deps and the client hooks).
//
//   Row     a DBFilesClient\CharacterAdvancement.dbc record as the DLL holds it: the 0x1FC-byte struct its parser
//           FUN_101c9120 builds (CoA::Dbc applies the layout), not the 0x2B4-byte file row.
//   Build   CharacterAdvancementBuild, 0x278 bytes (ctor FUN_1014f820, copy FUN_1014eb80, from a unit FUN_1014e4d0).
//   Player  the per-GUID record, 0x20 bytes (FUN_10148650): up to 20 spec builds + the active spec.
//
// A unit is passed by GUID; 0 means the build's owner (the original's null unit, resolved with ObjectPtr(guid, 0x10)).
namespace CoA::CA {
    using Row = const uint8_t*;

    Row FindRow(uint32_t id);   // CharacterAdvancement.dbc rows[id - min], nullptr if absent

    inline uint32_t RowU32(Row r, uint32_t off) {
        uint32_t v;
        const uint8_t* p = r + off;
        v = static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) | (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
        return v;
    }

    // FUN_1014e160 over the type string at row +4: 0 None, 1 Ability, 2 Talent, 3 Trait, 4 TalentAbility; -1 when the
    // string is none of them.
    int RowType(Row r);

    inline bool RowHasFlag(Row r, uint32_t mask) {   // FUN_101c64e0
        return (RowU32(r, 0x124) & mask) != 0;
    }

    // CharacterAdvancementBuildEntry (0x20 bytes).
    struct Entry {
        Row row = nullptr;     // +0x00, re-resolved by UpdatePointers (FUN_10158e40)
        uint32_t id = 0;       // +0x04
        uint32_t rank = 0;     // +0x08
        uint32_t u0c = 0;      // +0x0C
        uint8_t locked = 0;    // +0x10
        uint32_t u14 = 0;      // +0x14
        uint32_t u18 = 0;      // +0x18
        uint32_t u1c = 0;      // +0x1C
    };

    // What FUN_1014f0f0 reads from a CBuildCreatorEntry: +0x60 class, +0xF0 category, +0xF8 primary stat, the +0xFC
    // spell list {spell, level} and +0 the id.
    struct BuildSeed {
        uint32_t classKey = 0;
        uint32_t category = 0;
        uint32_t primaryStat = 0;
        std::vector<std::pair<uint32_t, uint32_t>> spells;
        std::string id;
    };

    struct TokenCost {
        uint32_t money;
        uint32_t marks;
        uint32_t item;
        uint32_t count;
    };

    struct ApplyCosts {
        std::vector<TokenCost> tokens;
        uint32_t marks = 0;
        uint32_t money = 0;
    };

    class Build {
    public:
        Build();                                   // FUN_1014f820 (the config-driven defaults)
        Build(const Build& src, bool full);        // FUN_1014eb80
        // FUN_1014f0f0: a build of a Build Creator entry at lvl (0 = the server max level): every spell at or below that
        // level adds a rank of its row; a Hero adds its primary-stat entry.
        Build(const BuildSeed& seed, uint32_t lvl, bool full);
        ~Build();
        Build& operator=(const Build&) = delete;

        uint64_t serial = 0;       // +0x00, registered in the live-build map (DAT_10bdd5c0)
        uint64_t guid = 0;         // +0x08
        int32_t spec = -1;         // +0x10
        uint32_t classKey = 0;     // +0x14  unit class byte
        uint8_t draft = 0;         // +0x18
        uint8_t wildcard = 0;      // +0x19
        uint8_t specDraft = 0;     // +0x1A
        uint8_t qualities = 0;     // +0x1B
        uint8_t hero = 0;          // +0x1C  class 10
        uint8_t coa = 0;           // +0x1D  class 12..32
        uint8_t stock = 0;         // +0x1E  class 1..9, 11
        uint8_t ready = 0;         // +0x1F  set by FUN_10157450
        std::vector<Entry> entries;   // +0x20
        uint32_t u2c = 0;          // +0x2C  unit addon field 2
        uint32_t level = 0;        // +0x30
        uint32_t credit[4] = {};   // +0x34..+0x40
        uint32_t u44 = 0;          // +0x44
        uint32_t u48 = 0;          // +0x48
        std::string name;          // +0x4C
        uint8_t u64 = 0;           // +0x64
        uint8_t u65 = 0;           // +0x65

        // Entry list (FUN_10152920 / FUN_10155060 / FUN_10155640 / FUN_101550c0).
        uint32_t RankOf(uint32_t id) const;
        bool Has(uint32_t id) const;
        bool LockedOf(uint32_t id) const;
        bool HasGroup(uint32_t group, uint32_t exceptId) const;

        // Essence budget from CharacterAdvancementEssence.dbc (FUN_10153190 / FUN_10153340 at the build's level,
        // FUN_101530b0 / FUN_10153260 at an explicit one).
        uint32_t AEBudget(uint32_t lvl) const;
        uint32_t TEBudget(uint32_t lvl) const;

        // Spent essence (FUN_10153b20 & co). A zero minimum disables the "row +off < minimum" filter.
        uint32_t GlobalAE(uint32_t minimum) const;
        uint32_t GlobalTE(uint32_t minimum) const;
        uint32_t ClassAE(uint32_t cls, uint32_t minimum) const;
        uint32_t ClassTE(uint32_t cls, uint32_t minimum) const;
        uint32_t ClassPoints(uint32_t cls, uint32_t minimum) const;
        uint32_t TabAE(uint32_t cls, uint32_t tab, uint32_t minimum) const;
        uint32_t TabTE(uint32_t cls, uint32_t tab, uint32_t minimum) const;
        uint32_t RemainingAE() const;   // FUN_10153a90
        uint32_t RemainingTE() const;   // FUN_10153ac0
        bool MeetsPoints(uint32_t cls, uint32_t tab, uint32_t points, uint32_t minimum) const;   // FUN_101552e0

        // FUN_101525f0 / FUN_10154350: the mode's cost column, waived for a wildcard choice group this build already
        // holds.
        uint32_t AECost(Row r, uint32_t ranks) const;
        uint32_t TECost(Row r, uint32_t ranks) const;
        uint32_t AEColumn(Row r, uint32_t ranks) const;
        uint32_t TEColumn(Row r, uint32_t ranks) const;

        // FUN_10158e40: re-resolve every entry's row; drop entries whose row is gone (kept on a dev realm when
        // keepMissing).
        void UpdatePointers(bool keepMissing);

        // ---- learn validation (CARules.cpp) ------------------------------------------------------------------
        // FUN_10157450: the "full" build preparation -- rule sets and result caches, then ready. A non-full copy has
        // no rules, so it validates everything as learnable (FUN_10151480's guard).
        void Prepare();
        uint64_t learnUnitRules = 0;    // +0x1A8: bit n = a rule in CA_LEARN_* slot n, run on the unit
        uint64_t learnBuildRules = 0;   // +0x1B4: run on the build
        bool learnCacheOn = false;      // +0x250
        std::vector<std::pair<uint32_t, std::pair<uint32_t, uint32_t>>> learnCache;   // +0x230: id -> {rank, result}

        // FUN_10151480: the first failing CA_LEARN_* slot for learning one more rank of id (0 = ok). include empty =
        // every slot; exclude empty = none excluded. unit 0 = the owner's.
        uint32_t ValidateLearn(uint64_t unit, uint32_t id, const std::vector<uint32_t>& include, const std::vector<uint32_t>& exclude);
        uint64_t learnMask = 0;           // +0x1C0 bitset: the slots FUN_1015cbf0 marks as it installs them
        uint32_t unlearnUnitRules = 0;    // +0x1D0
        uint32_t unlearnBuildRules = 0;   // +0x1DC
        std::vector<std::pair<uint32_t, uint32_t>> unlearnCache;   // +0x254 (on with learnCacheOn): id -> result

        // FUN_10151a50: the first failing CA_UNLEARN_* slot for removing id (0 = ok).
        uint32_t ValidateUnlearn(uint64_t unit, uint32_t id, const std::vector<uint32_t>& include, const std::vector<uint32_t>& exclude);
        // Purge rules (FUN_1015cbf0): abilities +0x1E8 unit / +0x1F4 build, talents +0x200 / +0x20C.
        uint32_t purgeUnitRules[2] = {};
        uint32_t purgeBuildRules[2] = {};
        // FUN_10151870 (abilities) / FUN_10151960 (talents): the first failing purge slot (0 = ok).
        uint32_t ValidatePurge(bool talents, uint64_t unit) const;
        // FUN_101551a0 / FUN_101554a0: at least amount AE / TE spent (rows whose +0x88 / +0x8C is below below, 0 = all),
        // group-shared wildcard costs counted once.
        bool SpentAtLeast(bool talents, uint32_t amount, uint32_t below) const;
        // FUN_10158050: re-learn every entry on an emptied full copy (in the current order, or the Reorder pass when
        // !inOrder). maskMode also excludes the installed slots the +0x1C0 mask leaves out (and LOW_LEVEL on a draft
        // build). False with the first failing {id, rank, result}.
        bool ValidateAll(bool inOrder, const std::vector<uint32_t>& include, std::vector<uint32_t> exclude, bool maskMode, uint32_t& id,
            uint32_t& rank, uint32_t& result) const;
        // FUN_101731e0: remove an entry together with the mastery bookkeeping it carries.
        void RemoveWithMastery(uint32_t id);
        bool MasteryInvolved(uint32_t id) const;          // FUN_10173c80
        uint32_t MasteryTarget(uint32_t id) const;        // FUN_1016f4f0
        bool ClassFusion() const;                         // FUN_10172490
        std::vector<Entry> TakeMasteryBearers();          // FUN_101738f0
        void ReaddWhileValid(const std::vector<Entry>& saved);   // FUN_10173e20
        // FUN_101560d0 (OptimizeForTraversal): Reorder that reports the first step it could not place.
        bool Optimize(bool restoreOnFail, uint32_t& result, uint32_t& id, uint32_t& rank);
        // FUN_101557d0: reorder the entries into an order they can be learned in. False when some step never
        // validates.
        bool Reorder(bool restoreOnFail);
        // FUN_10156b50: make the build learnable while keeping every keep id (as it is, reordered, or with the fewest
        // rank units outside keep removed). False, with the entries unchanged, when nothing works.
        bool TraverseOrFix(const std::vector<uint32_t>& keep);

        // Entry mutation (FUN_10151190 add at a rank, FUN_10158940 set rank, FUN_10151390 add one rank); each forgets
        // the entry's cached learn result.
        void AddEntry(uint32_t id, uint32_t rank);
        void SetRank(uint32_t id, uint32_t rank);
        void AddRank(uint32_t id);
        void Remove(uint32_t id);         // FUN_10157c30
        void ForgetResult(uint32_t id);   // FUN_10158a60(id, 1, 0)

        // The four std::function hooks ResetPendingBuild installs (+0x68/+0xE0/+0x108/+0x130); only the pending build
        // has them (CARules.cpp: AddEntry / SetRank / Remove).
        bool pendingHooks = false;

    private:
        void Register();
    };

    int ActiveSpecIndex();     // FUN_1016f580: the player record's active spec, -1 without one
    Build* ActiveBuild();      // FUN_1016f390: the player's build for the active spec, else the default build
    // The build's ability rows (types 1 and 4) -- what +0x44 caches.
    uint32_t AbilityCount(const Build& b);
    Build* PendingBuild();     // mgr +0x24, nullptr when none
    void ResetPendingBuild();  // FUN_10173d00

    // FUN_101498d0: the first entry (class-allowed rows sort first) listing this spell, or nullptr.
    Row RowBySpell(uint32_t spellId);
    bool IsCASpell(uint32_t spellId);   // FUN_101497c0
    // FUN_1008e5b0: the client spellbook or the CA known set has the spell -- every spell of its chain when it heads
    // one -- or, with orBuild, the active build holds the entry listing it.
    bool SpellKnown(uint32_t spellId, bool orBuild);
    const std::unordered_set<uint32_t>& KnownSpells();   // manager +0x3E8

    bool RowVisible(Row r);                     // FUN_101c6cc0
    uint32_t RowMaxRank(Row r);                 // FUN_101c6030
    bool ClassTypeAdmits(Row r, uint8_t cls);   // FUN_101c6a40
    Build* BuildOf(uint64_t guid);              // FUN_1016f700 + FUN_10149480(active spec)
    void RequestAutoLearn();                    // mgr +0x458 = 1 (the pending hooks)

    // FUN_10151df0 + FUN_10151fd0 + FUN_101547e0: can base become entries? CA_UPDATE_ENTRIES_* (0 = ok); on 3/5 the
    // failing {learn result, id, rank}; the marks / money it would cost and token costs.
    uint32_t ValidateApply(uint64_t unit, const Build& base, const std::vector<Entry>& entries, uint32_t& learnResult, uint32_t& id,
        uint32_t& rank, ApplyCosts& costs);
    uint64_t BumpPendingVersion();   // DAT_10bde3d8++

    TokenCost UnlearnCostOf(const Build& b, Row r);       // FUN_10154650
    int QualityIndexOf(const Build& b, Row r);            // FUN_101539b0
    uint32_t QualityCostOf(const Build& b, Row r);        // FUN_101539f0
    uint32_t QualitySumOf(const Build& b, int quality);   // FUN_10153f60
    uint32_t RequiredLevelOf(const Build& b, Row r);      // FUN_10153af0
    uint32_t PoolLevelOf(const Build& b, bool talent);    // FUN_10153510 / FUN_10153750
    bool ClassAllowedForPlayer(Row r);                    // FUN_101c6af0
    TokenCost PurgeCost(const Build& b, bool talents);    // FUN_10152650 / FUN_101543b0
    uint32_t UnitItemCount(uint64_t unit, uint32_t item);   // FUN_10308570 (active player only)
    const uint32_t kMarkOfAscensionItem = 0x5B9D2;

    // The CA manager's lists fired from 0x726 (per entry learned / rank changed / unlearned: id, rank) and 0x725 (a
    // spec change: old, new) -- FUN_102784b0 / FUN_102784d0 / FUN_102784f0.
    void OnEntryLearned(void (*cb)(uint32_t, uint32_t));
    void OnEntryRankChanged(void (*cb)(uint32_t, uint32_t));
    void OnEntryUnlearned(void (*cb)(uint32_t, uint32_t));
    void OnSpecChanged(void (*cb)(uint32_t, uint32_t));

    // FUN_10a315d0 +0x198: the wildcard mode manager's reveal records (0x2C each; the CA natives read them through
    // FUN_10a31720 / FUN_10a31bb0 / FUN_10a31bf0). C_Wildcard owns them (WildcardRolls.cpp: SMSG 0x621 adds one, its
    // 1.5 s reveal timer, the world entry and the glue reset remove them); its own fields (the stop code, the timer)
    // stay there.
    struct Request {
        uint32_t id = 0;     // +0x00 the entry
        uint32_t u4 = 0;     // +0x04 its first-rank spell
        uint32_t u8 = 0;     // +0x08 the ranks gained
        uint32_t rank = 0;   // +0x0C the rank before the roll
    };

    std::vector<Request>& WildcardReveals();

    // 0x10333950, the suggestion manager's mark registered into the wildcard manager's lists 0x10be2cac (an undesired
    // entry added) and 0x10be2ccc (one removed) by 0x10333880 (FUN_102786c0 / FUN_102786e0).
    void SuggestionsDirty(uint32_t id);
    // FUN_10333770: up to `max` suggested entries whose row is (or is not) an ability, optionally skipping known ones.
    std::vector<uint32_t> SuggestedList(bool abilities, uint32_t max, bool skipKnown);

    const std::vector<Row>& IndexedRows();   // 0x10bdd514: the entry index's visible rows
    const char* RowText(Row r, uint32_t off);   // a string field of the row ("" when none)
    uint32_t SpellAtRankOf(Row r, uint32_t rank);   // FUN_101c62e0
    uint32_t EntryCurrentSpell(const Build& b, uint32_t id);   // FUN_101528d0 (last of FUN_10152dc0)
    std::string QualityName(int q);   // 0x10b2d710, UNEXPECTED_ENUM_VALUE_ past 8

    // SpellCustomAttr.dbc (FUN_1020b080 / FUN_102a7f10) and SpellTags.dbc (FUN_10217450 / FUN_10217510 / FUN_102175b0).
    const uint8_t* SpellCustomAttrRow(uint32_t spell);
    bool SpellCustomAttr18(uint32_t spell, uint32_t mask);
    void RebuildSpellCustomAttr();
    const std::vector<uint32_t>& SpellTagTypesOf(uint32_t spell);
    void SpellTagRemove(uint32_t spell, uint32_t tag);
    void SpellTagAdd(uint32_t spell, uint32_t tag);
    void RebuildKnownTags();   // FUN_10182ff0
    const std::unordered_set<uint32_t>& KnownTags();   // manager +0x428

    // The client hooks CAClient.cpp installs, as the original's detours on the spell packets: the spells around the
    // client's own handling (FUN_10172660 / 10172850 / 101729d0 / 101728e0 / 10172b50).
    void OnInitialSpells(const std::vector<uint32_t>& spells);
    bool LearnedSpellIsQuiet(uint32_t spell);   // FUN_10172850's test: the client's learn message is NOPed
    void OnLearnedSpell(uint32_t spell);
    void OnRemovedSpells(const std::vector<uint32_t>& spells);
    bool RemovedSpellLeavesKnownSet(uint32_t spell);   // FUN_101728e0's test, before the client's handling
    void OnSupersededSpell(uint32_t oldSpell, uint32_t newSpell);
    bool SupersedeIsQuiet(uint32_t oldSpell, uint32_t newSpell);
    void SetRemovingSpell(uint32_t spell);   // 0x10BDEBC4
    uint32_t RemovingSpell();
    // 0x1016FDE0 (after 0x80B5D0): a player's cast of a spell whose CA entry lists point requirements must meet them.
    bool CastRequirementsMet(uint32_t spell);

    // What CA reads from the client (CAClient.cpp) and from modules not ported yet.
    struct Deps {
        uint64_t (*activePlayerGuid)() = nullptr;
        bool (*unitExists)(uint64_t guid) = nullptr;           // ObjectPtr(guid, 0x10)
        uint8_t (*unitClass)(uint64_t guid) = nullptr;         // unit / player: UNIT_FIELD_BYTES_0 byte 1, else 0
        uint32_t (*unitLevel)(uint64_t guid) = nullptr;        // descriptor +0xD8
        bool (*unitInCombat)(uint64_t guid) = nullptr;         // UNIT_FIELD_FLAGS (+0xEC) bit 19
        bool (*unitDead)(uint64_t guid) = nullptr;             // 0x6DAC10
        bool (*unitSchoolImmune)(uint64_t guid) = nullptr;     // an aura whose spell applies aura 0x27
        uint32_t (*unitItemCount)(uint64_t guid, uint32_t item) = nullptr;   // the currency-token slots
        uint32_t (*unitMoney)(uint64_t guid) = nullptr;        // PLAYER_FIELD_COINAGE
        int32_t (*mapType)() = nullptr;                        // Map.dbc +8 of the current map, -1 without one
        int32_t (*battlegroundState)() = nullptr;              // 0x548D10(0x1097)
        bool (*clientKnowsSpell)(uint32_t spell) = nullptr;    // the active player's spellbook (0x7260E0)
        bool (*spellExists)(uint32_t spell) = nullptr;         // FetchSpell
        bool (*spellRuneCost)(uint32_t spell) = nullptr;       // Spell +0x288 (RuneCostID) != 0
        bool (*spellHidden)(uint32_t spell) = nullptr;         // Spell +0x10 (Attributes) bit 7
        uint32_t (*spellMaxId)() = nullptr;                    // 0xAD49DC
        uint32_t (*firstRank)(uint32_t spell) = nullptr;       // AscSpellRank::FirstRank
        const std::vector<uint32_t>& (*rankChain)(uint32_t firstRank) = nullptr;   // AscSpellRank::Chain
        bool (*wildcardUndesired)(uint32_t id, uint8_t context) = nullptr;   // WildcardSets::IsUndesired (CoA::Wildcard::Init)
        bool (*unitTokenGuid)(const char* token, uint64_t& guid) = nullptr;   // a valid unit token's GUID
        uint32_t (*rankNumber)(uint32_t spell) = nullptr;                      // AscSpellRank::RankNumber
        // The spellbook slot holding the spell picked up (0xBE8D98 / 0xBE6D88, 0x53BAF0(slot, 0)); false if none.
        bool (*pickupFromSpellbook)(uint32_t spell) = nullptr;
        bool (*tokenAtLeast)(uint32_t token, uint32_t count) = nullptr;     // AscToken::AtLeast(token, count, 0)
        // C_BuildCreator (CoA::BuildCreator::Init installs them; unset answers as the original does with no data).
        bool (*specHasCreatorBuild)(int32_t spec) = nullptr;                     // BuildCreator::SpecHasActiveBuild
        std::string (*exportBuild)(const Build& b, bool includeREs) = nullptr;   // BuildCreator::ExportBuildOf
        std::string (*decodeLink)(const std::string& url) = nullptr;             // BuildCreator::DecodeLink
        BuildSeed (*seedFromLink)(const std::string& url) = nullptr;             // BuildCreator::SeedFromLink
        // BuildCreator::SeedOfBuild; also the BUILD_CREATOR learn / unlearn slots' template (CARules.cpp).
        bool (*seedOfBuild)(const std::string& id, BuildSeed& seed) = nullptr;
        // The Build Creator's pending entry, asked per spell (FUN_100faa20 / FUN_100fb960 / FUN_10100180).
        bool (*creatorCanAddSpell)(uint32_t spell) = nullptr;                    // BuildCreator::PendingCanAddSpell
        bool (*creatorCanRemoveSpell)(uint32_t spell) = nullptr;                 // BuildCreator::PendingCanRemoveSpell
        bool (*creatorHasSpell)(uint32_t spell) = nullptr;                       // BuildCreator::PendingHasSpell
        // C_Wildcard (CoA::Wildcard::Init installs them; unset answers as the original does with no data).
        bool (*wildcardRollActive)() = nullptr;                             // WildcardRapid::Active
        bool (*wildcardRollInFlight)() = nullptr;                           // WildcardRapid::InFlight
        void (*wildcardMarkRerollPending)() = nullptr;                      // WildcardRapid::MarkRerollPending
        void (*wildcardArmPoll)() = nullptr;                                // WildcardRolls::ArmPoll
        // AscSpellText (client): a spell's name, the forms it requires, its described text (Description(s, 1, 1, 0)).
        const std::string& (*spellName)(uint32_t spell) = nullptr;
        const std::string& (*spellForms)(uint32_t spell) = nullptr;
        std::string (*spellDescription)(uint32_t spell) = nullptr;
    };

    Deps& Dependencies();

    // 0x10172650 (ClientHooks::enterWorld): the entry index rebuilt (FUN_1014a530).
    void EnterWorld();
    // CA_GetCreditAmount's map (0x10bded78, FUN_101a1c90): category -> amount.
    uint32_t Credit(uint8_t category);

    void Init();
    // Tests only: the manager, the index and the known sets forgotten.
    void Reset();
}

#endif
