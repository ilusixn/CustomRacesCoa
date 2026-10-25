#ifndef COA_SOURCE_MODULES_CAINTERNAL_H
#define COA_SOURCE_MODULES_CAINTERNAL_H

// What CA.cpp, CARules.cpp and the CA natives share (the reconstruction's AscCAMgr.cpp anonymous namespace). Not for
// other modules: they use CA.h.
#include "CoA/Source/Modules/CA.h"
#include "CoA/Source/Runtime/Dbc.h"
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace CoA::CA::detail {
    CoA::Dbc::Table& Rows();         // DBFilesClient\CharacterAdvancement.dbc
    CoA::Dbc::Table& Essence();      // DBFilesClient\CharacterAdvancementEssence.dbc
    CoA::Dbc::Table& ClassTypes();   // DBFilesClient\CharacterAdvancementClassTypes.dbc
    CoA::Dbc::Table& TabTypes();     // DBFilesClient\CharacterAdvancementTabTypes.dbc

    const char* RowStr(Row r, uint32_t off);

    inline float RowF32(Row r, uint32_t off) {
        float v;
        memcpy(&v, r + off, 4);
        return v;
    }

    // ---- the client, through Deps ----------------------------------------------------------------------------
    uint64_t ActivePlayerGuid();
    bool UnitExists(uint64_t guid);
    uint8_t UnitClass(uint64_t guid);
    uint32_t UnitLevel(uint64_t guid);
    bool UnitInCombat(uint64_t guid);
    bool UnitDead(uint64_t guid);
    bool UnitSchoolImmune(uint64_t guid);
    uint32_t ItemCount(uint64_t guid, uint32_t item);   // FUN_10308700's count; 0 for anyone but the active player
    uint32_t Money(uint64_t guid);
    int32_t MapType();
    int32_t BattlegroundState();
    bool SpellExists(uint32_t spell);
    uint32_t FirstRank(uint32_t spell);
    const std::vector<uint32_t>& Chain(uint32_t firstRank);

    // ---- row predicates --------------------------------------------------------------------------------------
    bool StockClass(uint8_t c);   // FUN_100c63d0: 1..9 and 11
    inline bool CoAClass(uint8_t c) {
        return c >= 12 && c <= 32;
    }

    bool Visible(Row r);        // FUN_101c6cc0
    bool ClassAllowed(Row r);   // FUN_101c6af0
    uint32_t MaxRank(Row r);    // FUN_101c6030
    // Mode-dependent columns (FUN_1031ef10 flags): bit 3 draft, bit 6 wildcard.
    uint32_t ModeColumn(uint32_t normal, uint32_t draft, uint32_t wildcard);
    bool Wildcard();   // FUN_1031ef10 bit 6

    template <class Fn>
    void ForEachRow(Fn fn) {
        CoA::Dbc::Table& t = Rows();

        for (uint32_t id = t.MinId(); t.Loaded() && id <= t.MaxId(); ++id) {
            if (Row r = t.Row(id)) {
                fn(r);
            }
        }
    }

    // ---- the entry index (FUN_1014a530) -----------------------------------------------------------------------
    struct ClassLists {   // +0xC / +0x18 / +0x24 / +0x30
        std::vector<Row> other;
        std::vector<Row> ability;
        std::vector<Row> talent;
        std::vector<Row> talentAbility;
    };

    struct Index {
        std::unordered_set<uint32_t> allSpells;                    // 0x10bdd540
        std::unordered_map<uint32_t, std::vector<Row>> bySpell;    // 0x10bdd4c8
        std::unordered_map<uint32_t, std::vector<Row>> byMastery;  // 0x10bdd4e8
        std::vector<Row> visible;                                  // 0x10bdd514
        std::unordered_map<uint32_t, ClassLists> byClass;          // 0x10bdd560
        // FUN_10148bc0 (DAT_10bdd508, rebuilt at most every five seconds): spell -> the first row granting it among
        // the visible, class-allowed rows the active build has.
        std::vector<std::pair<uint32_t, Row>> knownSpellIndex;     // 0x10bdd508
        int64_t knownSpellNext = 0;                                // 0x10bdd580
    };

    Index& Idx();
    std::vector<Row>& ClassListFor(ClassLists& l, Row r);   // the list of the row's type (FUN_10148850's pick)
    void BuildIndex();          // FUN_1014a530
    void IndexRow(Row r);       // FUN_1014a330(row, 1)
    void UnindexRow(Row r);     // FUN_10148fd0
    const std::vector<std::pair<uint32_t, Row>>& KnownSpellIndex();
    void UpdateAllPointers(bool keepMissing);   // FUN_10158750

    // ---- known spells: mgr +0x3E8 known, +0x408 rune spells, +0x428 known tags ----------------------------------
    std::unordered_set<uint32_t>& KnownSpellSet();
    std::unordered_set<uint32_t>& RuneSpellSet();
    std::unordered_set<uint32_t>& KnownTagSet();
    void OnSpellLearned(uint32_t spell, bool silent);   // FUN_1016d7c0
    void OnSpellUnlearned(uint32_t spell);              // FUN_10173550
    bool ChainKnown(uint32_t spell);                    // FUN_10172380
    bool CustomAttrA(uint32_t spell, uint32_t mask);    // +0x10 (FUN_103249d0)
    bool CustomAttrB(uint32_t spell, uint32_t mask);    // +0x14 (FUN_10324a10)
    bool CustomAttrC(uint32_t spell, uint32_t mask);    // +0x18 (FUN_10330f60)

    // ---- the manager ------------------------------------------------------------------------------------------
    struct Player {   // FUN_10148650
        uint64_t guid = 0;            // +0x00
        std::vector<Build*> builds;   // +0x08
        uint32_t spec = 0xFFFFFFFF;   // +0x14
        uint32_t count = 0xFFFFFFFF;  // +0x18

        explicit Player(uint64_t g) : guid(g) {
        }

        ~Player();                    // FUN_1016d170
        Build* Get(uint32_t i) const;   // FUN_10149480
        void Ensure(uint32_t i);        // FUN_10149380
    };

    // FUN_10a315d0 +0x198: learn / unlearn requests sent and not yet answered ({id, ..., rank at +0xC}).
    struct Request {
        uint32_t id = 0;
        uint32_t u4 = 0;
        uint32_t u8 = 0;
        uint32_t rank = 0;
    };

    using PairCallback = void (*)(uint32_t, uint32_t);

    struct Mgr {
        Player* player = nullptr;                          // +0x00
        std::unordered_map<uint64_t, Player*> inspect;     // +0x04
        Build* pending = nullptr;                          // +0x24
        std::vector<uint32_t> suggestionOverrides;         // +0x44C
        bool autoLearn = false;                            // +0x458, set by the pending hooks
        uint64_t pendingVersion = 0;                       // DAT_10bde3d8
        uint64_t pendingSeen = 0;                          // DAT_10bde430
        bool pendingSignal = false;                        // DAT_10bde424
        int64_t lastUpdate = 0;                            // DAT_10bde428
        std::vector<Request> requests;
        std::vector<PairCallback> onSpecChanged;
        std::vector<PairCallback> onLearned;
        std::vector<PairCallback> onRankChanged;
        std::vector<PairCallback> onUnlearned;
        std::map<uint8_t, uint32_t> credits;               // 0x10bded78 (FUN_101a1c90), category -> amount
    };

    // The suggestion manager (0x10bcc618, FUN_103338f0): weighted entries and stats, and its dirty marks. The lists are
    // computed by the natives (7b); the core only marks them dirty.
    struct Suggestions {
        std::vector<std::pair<uint32_t, uint32_t>> entries;   // +0x00 {entry id, weight}
        std::vector<std::pair<uint32_t, uint32_t>> stats;     // +0x0C {stat, weight}
        bool statsDirty = true;                               // +0x18
        bool spellsDirty = true;                              // +0x19
        bool dirty = true;                                    // +0x1A
    };

    Suggestions& Sugg();

    Mgr& M();
    Player* FindPlayer(uint64_t guid);   // FUN_1016f700
    Build* DefaultBuild();               // 0x10bde8a8
    void Fire(const std::vector<PairCallback>& list, uint32_t a, uint32_t b);
    std::set<Build*>& LiveBuilds();      // DAT_10bdd5c0
    uint64_t& Serial();                  // DAT_10bdd5e8

    // The essence row of a build (FUN_10153190 & co).
    Row EssenceRow(const Build& b, uint32_t lvl);

    // ---- CARules.cpp ------------------------------------------------------------------------------------------
    uint32_t ConfigInt(const char* name, int32_t def);
    bool ConfigBool(const char* name);
}

#endif
