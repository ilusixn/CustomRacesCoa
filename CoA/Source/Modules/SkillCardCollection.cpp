// AscSkillCardCollection.cpp (see SkillCard.h): C_SkillCardCollection and the skill-card data it stands on.
#include "CoA/Source/Modules/SkillCard.h"
#include "CoA/Source/Modules/CA.h"
#include "CoA/Source/Modules/Config.h"
#include "CoA/Source/Modules/CustomStore.h"
#include "CoA/Source/Modules/GameMode.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Modules/RealmInfo.h"
#include "CoA/Source/Modules/Wildcard.h"
#include "CoA/Source/Runtime/Dbc.h"
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
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {
    using CoA::SkillCard::Card;
    using CoA::Script::CheckNumber;
    using CoA::Script::CheckString;
    using CoA::Script::PushBool;
    using CoA::Script::PushStr;
    using CoA::Script::ToInt;

    CoA::SkillCard::Deps s_deps;

    const char* const kTypes[9] = { "SKILL_CARD_DEFAULT_NORMAL", "SKILL_CARD_DEFAULT_GOLDEN", "SKILL_CARD_STARTER_NORMAL",
        "SKILL_CARD_STARTER_GOLDEN", "SKILL_CARD_LUCKY_NORMAL", "SKILL_CARD_LUCKY_GOLDEN", "SKILL_CARD_TALENT_NORMAL",
        "SKILL_CARD_TALENT_GOLDEN", "SKILL_CARD_MAX" };   // 0x10B3CA48 (FUN_10214060 parses all nine)
    const char* const kQualities[6] = { "SKILL_CARD_COMMON", "SKILL_CARD_UNCOMMON", "SKILL_CARD_RARE", "SKILL_CARD_EPIC",
        "SKILL_CARD_LEGENDARY", "SKILL_CARD_MAX" };   // 0x10B3A348
    const char* const kExpansions[4] = { "EXPANSION_CLASSIC", "EXPANSION_THE_BURNING_CRUSADE",
        "EXPANSION_WRATH_OF_THE_LICH_KING", "MAX_EXPANSIONS" };   // 0x10B3C9C0
    const char* const kClasses[0x21] = { "CLASS_NONE", "CLASS_WARRIOR", "CLASS_PALADIN", "CLASS_HUNTER", "CLASS_ROGUE",
        "CLASS_PRIEST", "CLASS_DEATH_KNIGHT", "CLASS_SHAMAN", "CLASS_MAGE", "CLASS_WARLOCK", "CLASS_HERO", "CLASS_DRUID",
        "CLASS_BARBARIAN", "CLASS_WITCH_DOCTOR", "CLASS_DEMON_HUNTER", "CLASS_WITCH_HUNTER", "CLASS_STORMBRINGER",
        "CLASS_KNIGHT_OF_XOROTH", "CLASS_GUARDIAN", "CLASS_MONK", "CLASS_SON_OF_ARUGAL", "CLASS_RANGER",
        "CLASS_CHRONOMANCER", "CLASS_NECROMANCER", "CLASS_PYROMANCER", "CLASS_CULTIST", "CLASS_STARCALLER",
        "CLASS_SUN_CLERIC", "CLASS_TINKER", "CLASS_VENOMANCER", "CLASS_REAPER", "CLASS_PRIMALIST",
        "CLASS_RUNEMASTER" };   // 0x10B31920
    const char* const kSealedTypes[25] = { "PURCHASE_SEALED_CARD_TYPE_NORMAL_LEGENDARY_ABILITIES",
        "PURCHASE_SEALED_CARD_TYPE_NORMAL_EPIC_ABILITIES", "PURCHASE_SEALED_CARD_TYPE_NORMAL_RARE_ABILITIES",
        "PURCHASE_SEALED_CARD_TYPE_NORMAL_UNCOMMON_ABILITIES", "PURCHASE_SEALED_CARD_TYPE_NORMAL_COMMON_ABILITIES",
        "PURCHASE_SEALED_CARD_TYPE_NORMAL_LEGENDARY_TALENTS", "PURCHASE_SEALED_CARD_TYPE_NORMAL_EPIC_TALENTS",
        "PURCHASE_SEALED_CARD_TYPE_NORMAL_RARE_TALENTS", "PURCHASE_SEALED_CARD_TYPE_NORMAL_UNCOMMON_TALENTS",
        "PURCHASE_SEALED_CARD_TYPE_NORMAL_COMMON_TALENTS", "PURCHASE_SEALED_CARD_TYPE_GOLDEN_LEGENDARY_ABILITIES",
        "PURCHASE_SEALED_CARD_TYPE_GOLDEN_EPIC_ABILITIES", "PURCHASE_SEALED_CARD_TYPE_GOLDEN_RARE_ABILITIES",
        "PURCHASE_SEALED_CARD_TYPE_GOLDEN_UNCOMMON_ABILITIES", "PURCHASE_SEALED_CARD_TYPE_GOLDEN_COMMON_ABILITIES",
        "PURCHASE_SEALED_CARD_TYPE_GOLDEN_LEGENDARY_TALENTS", "PURCHASE_SEALED_CARD_TYPE_GOLDEN_EPIC_TALENTS",
        "PURCHASE_SEALED_CARD_TYPE_GOLDEN_RARE_TALENTS", "PURCHASE_SEALED_CARD_TYPE_GOLDEN_UNCOMMON_TALENTS",
        "PURCHASE_SEALED_CARD_TYPE_GOLDEN_COMMON_TALENTS", "PURCHASE_SEALED_CARD_TYPE_DEFAULT_PACK",
        "PURCHASE_SEALED_CARD_TYPE_DEFAULT_GOLDEN_PACK", "PURCHASE_SEALED_CARD_TYPE_DEFAULT_TALENT_PACK",
        "PURCHASE_SEALED_CARD_TYPE_DEFAULT_GOLDEN_TALENT_PACK", "PURCHASE_SEALED_CARD_TYPE_MAX" };   // 0x10B5BB70
    const char* const kPurchase[10] = { "PURCHASE_SEALED_CARD_OK", "PURCHASE_SEALED_CARD_UNKNOWN",
        "PURCHASE_SEALED_CARD_RANDOM_MODE_NOT_ENABLED", "PURCHASE_SEALED_CARD_BAD_TYPE",
        "PURCHASE_SEALED_CARD_NO_COST_ENTRY", "PURCHASE_SEALED_CARD_BAD_COST", "PURCHASE_SEALED_CARD_NO_TOKEN",
        "PURCHASE_SEALED_CARD_NO_UNCOLLECTED_CARDS_OF_QUALITY", "PURCHASE_SEALED_CARD_BAD_AMOUNT",
        "PURCHASE_SEALED_CARD_UNCLAIMED_CARDS" };   // 0x10B1EAC8
    const char* const kBooster[6] = { "PURCHASE_SEALED_CARD_BOOSTER_PACK_OK", "PURCHASE_SEALED_CARD_BOOSTER_PACK_UNKNOWN",
        "PURCHASE_SEALED_CARD_BOOSTER_PACK_RANDOM_MODE_NOT_ENABLED", "PURCHASE_SEALED_CARD_BOOSTER_PACK_TOO_FEW",
        "PURCHASE_SEALED_CARD_BOOSTER_PACK_TOO_MANY", "PURCHASE_SEALED_CARD_BOOSTER_PACK_NO_MONEY" };   // 0x10B1C81C
    const char* const kFilters[16] = { "FILTER_NONE", "FILTER_DEFAULT_NORMAL", "FILTER_DEFAULT_GOLDEN",
        "FILTER_LUCKY_NORMAL", "FILTER_LUCKY_GOLDEN", "FILTER_TALENT_NORMAL", "FILTER_TALENT_GOLDEN",
        "FILTER_ACTIVE_CLASS", "FILTER_COMMON", "FILTER_UNCOMMON", "FILTER_RARE", "FILTER_EPIC", "FILTER_LEGENDARY",
        "FILTER_COLLECTED", "FILTER_NOT_COLLECTED", "FILTER_STARTER" };   // 0x10B21EB0

    template <size_t N>
    int IndexIn(const std::string& s, const char* const (&names)[N]) {
        for (size_t i = 0; i < N; ++i) {
            if (s == names[i]) {
                return static_cast<int>(i);
            }
        }

        return -1;
    }

    // FUN_10196ab0: a config integer with its default.
    int32_t ConfigInt(const char* name, int32_t fallback) {
        const int32_t* v = CoA::Config::Int(name);
        return v ? *v : fallback;
    }

    bool HasPlayer() {
        return s_deps.hasPlayer && s_deps.hasPlayer();
    }

    uint8_t PlayerClass() {
        return s_deps.playerClass ? s_deps.playerClass() : 0;
    }

    uint32_t Money() {   // player +0x1248
        return s_deps.playerMoney ? s_deps.playerMoney() : 0;
    }

    bool SpellName(uint32_t spell, std::string& name) {   // FetchSpell, then +0x220
        return s_deps.spellName && s_deps.spellName(spell, name);
    }

    // ---- the cards ------------------------------------------------------------------------------------------------
    struct Store {
        bool loaded = false;
        std::map<uint32_t, std::unique_ptr<Card>> byId;                        // the DBC container, id order
        std::map<std::pair<uint32_t, uint32_t>, const Card*> byCardRank;       // 0x10BE0678
    };

    Store s_store;

    Store& Cards() {
        if (s_store.loaded) {
            return s_store;
        }

        s_store.loaded = true;
        CoA::Dbc::Table& t = CoA::Dbc::Get("DBFilesClient\\SkillCard.dbc");

        for (uint32_t i = 0; i < t.Count(); ++i) {
            const uint8_t* r = t.RowAt(i);

            if (!r) {
                continue;
            }

            auto c = std::make_unique<Card>();
            c->id = CoA::Dbc::Table::U32(r, 0x00);
            c->cardId = CoA::Dbc::Table::U32(r, 0x04);
            c->rank = CoA::Dbc::Table::U32(r, 0x08);
            c->item = CoA::Dbc::Table::U32(r, 0x0C);
            c->spell = CoA::Dbc::Table::U32(r, 0x10);
            c->type = t.Str(r, 0x14);
            c->expansion = t.Str(r, 0x18);
            c->cls = t.Str(r, 0x1C);
            c->quality = t.Str(r, 0x20);
            c->wildcard = CoA::Dbc::Table::U32(r, 0x24);
            c->draft = CoA::Dbc::Table::U32(r, 0x28);
            c->starter = CoA::Dbc::Table::U32(r, 0x2C);
            s_store.byCardRank[{ c->cardId, c->rank }] = c.get();
            s_store.byId[c->id] = std::move(c);
        }

        return s_store;
    }

    // Card rows in id order (the containers' populate walks the DBC min..max).
    std::vector<const Card*> AllCards() {
        std::vector<const Card*> out;

        for (auto& e : Cards().byId) {
            out.push_back(e.second.get());
        }

        return out;
    }

    uint32_t MaxRank(uint32_t cardId) {   // the consecutive ranks from 1 in the (card, rank) map
        uint32_t r = 0;

        while (CoA::SkillCard::ByCardRank(cardId, r + 1)) {
            ++r;
        }

        return r;
    }

    // FUN_10214310 / FUN_102143a0 / FUN_10214350: the card belongs to this mode, expansion and class.
    bool ForMode(const Card& c) {
        const uint32_t m = CoA::GameMode::Mode();
        return (c.draft && ((m >> 3) & 1)) || (c.wildcard && ((m >> 6) & 1));
    }

    bool ForExpansion(const Card& c) {
        int e = IndexIn(c.expansion, kExpansions);

        if (e < 0) {
            CoA::Log::Printf("Unexpected Value: %s", c.expansion.c_str());
            e = 3;
        }

        return static_cast<uint32_t>(e) <= CoA::RealmInfo::Get().ruleset;   // DAT_10bdb140
    }

    int ClassIndex(const std::string& s) {   // FUN_10130fa0(s, 0)
        const int i = IndexIn(s, kClasses);
        return i < 0 ? 0 : i;
    }

    bool ForClass(const Card& c) {
        return ClassIndex(c.cls) == PlayerClass();
    }

    // FUN_10319c40: progress to the next rank, by quality (0xFFFFFFFF for legendary / unknown).
    uint32_t Required(const Card& c) {
        switch (IndexIn(c.quality, kQualities)) {
            case 0: return static_cast<uint32_t>(ConfigInt("CONFIG_SKILL_CARD_UNCOMMON_PROGRESS_REQUIRED", 5));
            case 1: return static_cast<uint32_t>(ConfigInt("CONFIG_SKILL_CARD_RARE_PROGRESS_REQUIRED", 10));
            case 2: return static_cast<uint32_t>(ConfigInt("CONFIG_SKILL_CARD_EPIC_PROGRESS_REQUIRED", 15));
            case 3: return static_cast<uint32_t>(ConfigInt("CONFIG_SKILL_CARD_LEGENDARY_PROGRESS_REQUIRED", 25));
            default: return 0xFFFFFFFF;
        }
    }

    // FUN_10319860: progress a duplicate grants, by quality.
    uint32_t Granted(const Card& c) {
        switch (IndexIn(c.quality, kQualities)) {
            case 0: return static_cast<uint32_t>(ConfigInt("CONFIG_SKILL_CARD_COMMON_PROGRESS_GRANTED", 1));
            case 1: return static_cast<uint32_t>(ConfigInt("CONFIG_SKILL_CARD_UNCOMMON_PROGRESS_GRANTED", 2));
            case 2: return static_cast<uint32_t>(ConfigInt("CONFIG_SKILL_CARD_RARE_PROGRESS_GRANTED", 4));
            case 3: return static_cast<uint32_t>(ConfigInt("CONFIG_SKILL_CARD_EPIC_PROGRESS_GRANTED", 8));
            case 4: return static_cast<uint32_t>(ConfigInt("CONFIG_SKILL_CARD_LEGENDARY_PROGRESS_GRANTED", 10));
            default: return 0;
        }
    }

    // ---- SealedCardCosts (0x10BE01C4) with SMSG 0x66C row patches -------------------------------------------------
    std::map<uint32_t, std::vector<uint32_t>> s_costPatches;

    // FUN_10213d50: the row's 25 u32, empty when absent.
    std::vector<uint32_t> CostRow(uint32_t id) {
        auto it = s_costPatches.find(id);

        if (it != s_costPatches.end()) {
            return it->second;
        }

        const uint8_t* r = CoA::Dbc::Get("DBFilesClient\\SealedCardCosts.dbc").Row(id);

        if (!r) {
            return {};
        }

        std::vector<uint32_t> v(25);
        memcpy(v.data(), r, 100);
        return v;
    }

    // ---- the collection -------------------------------------------------------------------------------------------
    struct Collected {
        uint32_t cardId;
        uint32_t progress;
        uint32_t rank;
    };

    struct Pending {
        std::string name;
        uint32_t a;
        uint32_t b;
    };

    class CardContainer : public CoA::Filter::Container<const Card*> {
    protected:
        std::vector<const Card*> DoPopulate() override {   // FUN_102148b0
            return AllCards();
        }

        // FUN_10214bf0
        bool ArgMatch(uint32_t f, const Card* const& c) override {
            static const int kTypeOf[7] = { -1, 0, 1, 4, 5, 6, 7 };

            if (f >= 1 && f <= 6) {
                return CoA::SkillCard::TypeIndex(c->type) == kTypeOf[f];
            }

            if (f == 7) {
                return HasPlayer() && static_cast<uint32_t>(ClassIndex(c->cls)) == PlayerClass();
            }

            if (f >= 8 && f <= 12) {
                return CoA::SkillCard::QualityIndex(c->quality, 0) == static_cast<int>(f - 8);
            }

            if (f == 13) {
                return CoA::SkillCard::CollectedRank(c->cardId) == c->rank;
            }

            if (f == 14) {
                return CoA::SkillCard::CollectedRank(c->cardId) < c->rank;
            }

            if (f == 15) {
                return c->starter != 0;
            }

            return false;
        }

        uint32_t ArgGroup(uint32_t f) override {   // FUN_10214e00
            if (f >= 1 && f <= 6) {
                return 1;
            }

            if (f >= 8 && f <= 12) {
                return 2;
            }

            return 0;
        }

        // FUN_102149a0: the mode's cards only; with text, the spell's name or description contains it (a spell the
        // client does not have never matches).
        bool TextMatch(const std::string& lower, const Card* const& c) override {
            if (!ForMode(*c)) {
                return false;
            }

            if (lower.empty()) {
                return true;
            }

            std::string name;

            if (!SpellName(c->spell, name)) {
                return false;
            }

            if (CoA::Filter::Lower(name).find(lower) != std::string::npos) {
                return true;
            }

            const std::string description = s_deps.spellDescription ? s_deps.spellDescription(c->spell) : std::string();
            return CoA::Filter::Lower(description).find(lower) != std::string::npos;
        }

        // FUN_10214e50: the lowercase spell name as a big integer (bytes shifted in), 0 without the spell. Its numeric
        // order is the length first, then the bytes.
        std::string PopulateKey(const Card* const& c) override {
            std::string k;
            std::string name;

            if (!SpellName(c->spell, name)) {
                return k;
            }

            name = CoA::Filter::Lower(name);
            CoA::Filter::AppendU32(k, static_cast<uint32_t>(name.size()));
            k += name;
            return k;
        }
    };

    struct Manager {
        uint32_t bonusProgress = 0;                  // +0x00
        std::vector<Collected> collected;            // +0x04
        std::vector<Pending> pending;                // +0x10
        std::map<int, CardContainer> containers;     // +0x20: types 0 1 4 5 6 7 (FUN_102f42e0)
        uint32_t purchased[24] = {};                 // +0x3C
        uint32_t pendingType = 0x18;                 // +0x9C
        uint32_t pendingAmount = 0;                  // +0xA0

        Manager() {
            for (int t : { 0, 1, 4, 5, 6, 7 }) {
                this->containers[t];
            }
        }
    };

    std::unique_ptr<Manager> s_mgr;

    Manager& Mgr() {   // FUN_1031bb20
        if (!s_mgr) {
            s_mgr = std::make_unique<Manager>();
        }

        return *s_mgr;
    }

    // FUN_10319f80's function-local SkillCardEntryContainer (0x10BE4088): what a type without a container (starter, MAX)
    // reads and filters. Neither the mode change nor the glue reset touches it.
    CardContainer s_shared;

    // FUN_10319f80: the type's container, or the shared one.
    CardContainer& ContainerOf(int type) {
        auto it = Mgr().containers.find(type);

        if (it != Mgr().containers.end()) {
            return it->second;
        }

        return s_shared;
    }

    void ResetContainers() {   // FUN_1031bd50
        for (auto& c : Mgr().containers) {
            c.second.Reset();
        }
    }

    Collected* Find(uint32_t cardId) {
        for (Collected& c : Mgr().collected) {
            if (c.cardId == cardId) {
                return &c;
            }
        }

        return nullptr;
    }

    // FUN_10319390: 1 at max rank, else progress / required rounded to hundredths; 0 without ranks.
    double Progress(uint32_t cardId) {
        const uint32_t max = MaxRank(cardId);

        if (max == 0) {
            return 0;
        }

        const Collected* e = Find(cardId);
        const uint32_t rank = e ? e->rank : 0;

        if (max <= rank) {
            return 1;
        }

        const Card* c = CoA::SkillCard::ByCardRank(cardId, rank);

        if (!c) {
            return 0;
        }

        const uint32_t progress = e ? e->progress : 0;
        const uint32_t req = Required(*c);
        return std::round(static_cast<double>(progress) * 100.0 / static_cast<double>(static_cast<int32_t>(req))) / 100.0;
    }

    // FUN_10317ef0: a card was collected -- new, upgraded, or a duplicate's progress.
    void Collect(const Card& card) {
        Collected* e = Find(card.cardId);

        if (!e) {
            Mgr().collected.push_back({ card.cardId, 0, card.rank });   // FUN_10314430
            CoA::Events::Signal("SKILL_CARD_ADDED", "%u%u", card.cardId, card.rank);
            return;
        }

        if (e->rank < card.rank) {
            e->rank = card.rank;
            e->progress = 0;
            CoA::Events::Signal("SKILL_CARD_UPGRADED", "%u%u", card.cardId, card.rank);
            return;
        }

        const uint32_t granted = Granted(card);

        if (granted == 0) {
            return;
        }

        const Card* current = CoA::SkillCard::ByCardRank(e->cardId, e->rank);

        if (current && MaxRank(current->cardId) == current->rank) {   // FUN_102145f0: at max rank
            const uint32_t old = Mgr().bonusProgress;
            Mgr().bonusProgress += granted;

            // The comparison is unsigned (the reconstruction's is signed).
            while (Mgr().bonusProgress >= static_cast<uint32_t>(ConfigInt("CONFIG_BONUS_SEALED_CARD_PACKS_PROGRESS_REQUIRED", 100))) {
                Mgr().bonusProgress -= static_cast<uint32_t>(ConfigInt("CONFIG_BONUS_SEALED_CARD_PACKS_PROGRESS_REQUIRED", 100));
                CoA::Events::Signal("BONUS_SEALED_CARD_PACK_REWARDED", nullptr);
            }

            CoA::Events::Signal("BONUS_SEALED_CARD_PACK_PROGRESS_UPDATE", "%u%u%u%u", card.cardId, card.rank, old, Mgr().bonusProgress);
            return;
        }

        e->progress += granted;
        const Card* at = CoA::SkillCard::ByCardRank(e->cardId, e->rank);

        if (!at) {
            return;
        }

        uint32_t req = Required(*at);
        bool upgraded = false;

        while (req != 0 && req <= e->progress) {
            if (!CoA::SkillCard::ByCardRank(e->cardId, e->rank + 1)) {
                break;
            }

            ++e->rank;
            e->progress -= req;
            at = CoA::SkillCard::ByCardRank(e->cardId, e->rank);
            req = at ? Required(*at) : 0;
            upgraded = true;
            CoA::Events::Signal("SKILL_CARD_UPGRADED", "%u%u", e->cardId, e->rank);

            if (req == 0) {
                return;
            }
        }

        if (upgraded) {
            return;
        }

        CoA::Events::Signal("SKILL_CARD_PROGRESS_UPDATED", "%u%u", at->cardId, at->rank);
    }

    // FUN_103195d0: the SealedCardCosts sum for `amount` purchases of `type` after those already made.
    uint32_t SealedCost(uint32_t type, uint32_t amount) {
        if (type == 0x18) {
            return 0;
        }

        uint32_t sum = 0;

        for (uint32_t i = 0; i < amount; ++i) {
            const std::vector<uint32_t> row = CostRow(Mgr().purchased[type] + 1 + i);

            if (!row.empty()) {
                sum += row[1 + type];
            }
        }

        return sum;
    }

    uint32_t SealedToken(uint32_t type) {   // FUN_10319630
        if (type < 10) {
            return 0x3C1AE;
        }

        if (type < 20) {
            return 0x17C77;
        }

        switch (type) {
            case 20: return 0x17E5D;
            case 21: return 0xBE2F6;
            case 22: return 0x17E5E;
            case 23: return 0x17E4E;
            default: return 0;
        }
    }

    bool RandomModeOn() {
        const uint32_t m = CoA::GameMode::Mode();
        return ((m >> 3) & 1) || ((m >> 6) & 1);
    }

    // FUN_103177c0: PURCHASE_SEALED_CARD_* reasons (empty = allowed).
    std::vector<uint32_t> CanPurchase(uint32_t type, uint32_t amount) {
        std::vector<uint32_t> out;

        if (!RandomModeOn()) {
            out.push_back(2);
        }

        if (type == 0x18) {
            out.push_back(3);
        } else {
            if (CostRow(Mgr().purchased[type] + 1).empty()) {
                out.push_back(4);
            } else {
                const uint32_t cost = SealedCost(type, amount);

                if (cost == 0) {
                    out.push_back(5);
                } else {
                    const uint32_t token = SealedToken(type);

                    if ((s_deps.heldCount ? s_deps.heldCount(token) : 0) < cost) {
                        out.push_back(6);
                    }
                }
            }

            if (type < 20) {
                static const int kCategory[4] = { 0, 6, 1, 7 };   // normal / talent normal / golden / talent golden
                const int category = kCategory[type / 5];
                const int quality = 4 - static_cast<int>(type % 5);
                bool found = false;

                for (const Card* c : AllCards()) {
                    const int t = CoA::SkillCard::TypeIndex(c->type);

                    if (t < 0) {
                        CoA::Log::Printf("Unexpected Value: %s", c->type.c_str());
                        continue;
                    }

                    if (t != category) {
                        continue;
                    }

                    const int q = CoA::SkillCard::QualityIndex(c->quality, -1);

                    if (q < 0) {
                        continue;
                    }

                    if (q != quality) {
                        continue;
                    }

                    if (!ForExpansion(*c) || !ForClass(*c) || !ForMode(*c)) {
                        continue;
                    }

                    if (!CoA::SkillCard::CollectedAtLeast(c->cardId, c->rank)) {
                        found = true;
                        break;
                    }
                }

                if (!found) {
                    out.push_back(7);
                }
            }
        }

        const uint32_t limit = (type >= 20 && type <= 23) ? 200 : 1;

        if (amount == 0 || limit < amount) {
            out.push_back(8);
        }

        if (!Mgr().pending.empty()) {
            out.push_back(9);
        }

        return out;
    }

    // FUN_103196c0 body: PURCHASE_SEALED_CARD_BOOSTER_PACK_* reasons.
    std::vector<uint32_t> CanPurchaseBooster(uint32_t count) {
        std::vector<uint32_t> out;

        if (!RandomModeOn()) {
            out.push_back(2);
        }

        if (count == 0) {
            out.push_back(3);
        } else if (count >= 0x10C7) {
            out.push_back(4);
        }

        if (HasPlayer() && count != 0) {
            const uint32_t price = count < 0x10C7 ? count * 500000 : 0xFFFFFFFF;

            if (Money() < price) {
                out.push_back(5);
            }
        }

        return out;
    }

    template <size_t N>
    void PushNames(lua_State* L, const std::vector<uint32_t>& codes, const char* const (&names)[N]) {   // FUN_103160e0 / 10315ef0
        lua_createtable(L, static_cast<int>(codes.size()), 0);
        lua_checkstack(L, 2);

        for (size_t i = 0; i < codes.size(); ++i) {
            lua_pushnumber(L, static_cast<double>(i + 1));

            if (codes[i] < N) {
                PushStr(L, names[codes[i]]);
            } else {
                PushStr(L, ("UNEXPECTED_ENUM_VALUE_" + std::to_string(codes[i])).c_str());
            }

            lua_settable(L, -3);
        }
    }

    // FUN_10315d50: the PURCHASE_SEALED_CARD_TYPE_* index (0x18 when unknown, logged).
    uint32_t SealedType(const std::string& s) {
        const int i = IndexIn(s, kSealedTypes);

        if (i < 0) {
            CoA::Log::Printf("Unexpected Value: %s", s.c_str());
            return 0x18;
        }

        return static_cast<uint32_t>(i);
    }

    void SetNum(lua_State* L, const char* k, uint32_t v) {
        lua_pushstring(L, k);
        lua_pushinteger(L, static_cast<int32_t>(v));
        lua_settable(L, -3);
    }

    void SetStrField(lua_State* L, const char* k, const std::string& v) {
        lua_pushstring(L, k);
        PushStr(L, v.c_str());
        lua_settable(L, -3);
    }

    void SetBoolField(lua_State* L, const char* k, bool v) {
        lua_pushstring(L, k);
        PushBool(L, v);
        lua_settable(L, -3);
    }

    // FUN_1009bc60: a card as a table.
    void PushCardTable(lua_State* L, const Card& c) {
        lua_createtable(L, 0, 0);
        lua_checkstack(L, 2);
        SetNum(L, "CardID", c.cardId);
        SetNum(L, "Rank", c.rank);
        SetNum(L, "ItemID", c.item);
        SetNum(L, "SpellID", c.spell);
        SetStrField(L, "Type", c.type);
        SetStrField(L, "Expansion", c.expansion);
        SetStrField(L, "Class", c.cls);
        SetStrField(L, "Quality", c.quality);
        SetBoolField(L, "IsWildcard", c.wildcard != 0);
        SetBoolField(L, "IsDraftMode", c.draft != 0);
        SetBoolField(L, "IsStarterCard", c.starter != 0);
        SetBoolField(L, "IsCollected", CoA::SkillCard::CollectedRank(c.cardId) != 0);   // FUN_1031bbc0
        lua_pushstring(L, "CollectedProgress");
        lua_pushnumber(L, Progress(c.cardId));
        lua_settable(L, -3);
        SetNum(L, "CollectedRank", CoA::SkillCard::CollectedRank(c.cardId));
        lua_pushstring(L, "QualityCost");
        CoA::CA::Row r = CoA::CA::RowBySpell(c.spell);   // FUN_101498d0
        CoA::CA::Build* b = CoA::CA::ActiveBuild();

        if (r && b) {
            lua_pushinteger(L, static_cast<int32_t>(CoA::CA::QualityCostOf(*b, r)));   // FUN_101539f0
        } else {
            lua_pushnil(L);
        }

        lua_settable(L, -3);
        SetNum(L, "MaxRank", MaxRank(c.cardId));
    }

    // ---- bindings -------------------------------------------------------------------------------------------------
    int GetProgress(lua_State* L) {   // FUN_1031d330
        uint32_t id;

        if (!CoA::Script::ReadNumber(L, id)) {
            return 0;
        }

        const Collected* e = Find(id);
        lua_pushinteger(L, static_cast<int32_t>(e ? e->rank : 0));
        lua_pushnumber(L, Progress(id));
        return 2;
    }

    int IsCollected(lua_State* L) {   // FUN_1031d4d0: (id [, rank]), rank 1 by default
        uint32_t id;
        uint32_t rank = 1;
        int32_t a;
        int32_t b;

        if (CoA::Script::ReadInt2(L, a, b)) {
            id = static_cast<uint32_t>(a);
            rank = static_cast<uint32_t>(b);
        } else if (!CoA::Script::ReadNumber(L, id)) {
            return 0;
        }

        PushBool(L, CoA::SkillCard::CollectedAtLeast(id, rank));
        return 1;
    }

    // FUN_1031c930: progress / required, fixed to four decimals (an ostringstream round trip).
    int GetBonusSealedCardPacksProgress(lua_State* L) {
        const double required = ConfigInt("CONFIG_BONUS_SEALED_CARD_PACKS_PROGRESS_REQUIRED", 100);
        char buf[64];
        snprintf(buf, sizeof(buf), "%.4f", static_cast<double>(static_cast<int32_t>(Mgr().bonusProgress)) / required);
        lua_pushnumber(L, strtod(buf, nullptr));
        return 1;
    }

    int GetNumSkillCards(lua_State* L) {   // FUN_1031ced0
        std::string s;

        if (!CoA::Script::ReadString(L, s)) {
            return 0;
        }

        const int t = CoA::SkillCard::TypeIndex(s);

        if (t < 0) {
            return 0;
        }

        lua_pushinteger(L, static_cast<int32_t>(ContainerOf(t).Size()));
        return 1;
    }

    int GetSkillCardAtIndex(lua_State* L) {   // FUN_1031d200: (type, 1-based index)
        if (!CoA::Script::ValidateInput(L, { LUA_TSTRING, LUA_TNUMBER })) {
            return 0;
        }

        const std::string s = CheckString(L, 1);
        const uint32_t i = static_cast<uint32_t>(ToInt(CheckNumber(L, 2)));
        const int t = CoA::SkillCard::TypeIndex(s);

        if (t < 0 || i == 0) {
            return 0;
        }

        CardContainer& c = ContainerOf(t);

        if (i - 1 < c.Size() && c.At(i - 1)) {
            PushCardTable(L, *c.At(i - 1));
        } else {
            lua_pushnil(L);
        }

        return 1;
    }

    // FUN_103162d0 (FrameScript::lua_toarray<FILTER_*>): each value's FILTER_* index (FUN_103166c0: an unknown name
    // logs and is FILTER_NONE); a value that is not a string empties the list.
    void ReadFilterArray(lua_State* L, int idx, std::vector<uint32_t>& filters) {
        filters.clear();
        lua_pushnil(L);

        while (lua_next(L, idx)) {
            if (!lua_isstring(L, -1)) {
                lua_settop(L, -3);
                CoA::Log::Printf("FrameScript::lua_toarray Invalid lua value type");
                filters.clear();
                return;
            }

            const std::string name = lua_tolstring(L, -1, nullptr);
            const int i = IndexIn(name, kFilters);

            if (i < 0) {
                CoA::Log::Printf("lua_tovalue<enum>: Unknown enum string '%s'", name.c_str());
            }

            filters.push_back(i < 0 ? 0 : static_cast<uint32_t>(i));   // FUN_100b9d70
            lua_settop(L, -2);
        }
    }

    void AddFilter(std::vector<uint32_t>& v, uint32_t f) {   // FUN_100b9d70 behind a find
        if (std::find(v.begin(), v.end(), f) == v.end()) {
            v.push_back(f);
        }
    }

    // FUN_1031dc30: (type, text, {FILTER_* names}). Quality words in the text become filters and leave it; the type's
    // own filter and FILTER_ACTIVE_CLASS are always applied (FUN_103185e0).
    int SetSkillCardFilter(lua_State* L) {
        if (!CoA::Script::ValidateInput(L, { LUA_TSTRING, LUA_TSTRING, LUA_TTABLE })) {   // FUN_10314150
            return 0;
        }

        const std::string typeName = CheckString(L, 1);
        const std::string text = CheckString(L, 2);
        std::vector<uint32_t> filters;
        ReadFilterArray(L, 3, filters);
        const int t = CoA::SkillCard::TypeIndex(typeName);

        if (t < 0) {
            return 0;
        }

        std::string lower = CoA::Filter::Lower(text);

        auto take = [&](const char* word, uint32_t f) {
            const size_t len = strlen(word);

            if (lower.size() < len) {
                return;
            }

            const size_t at = lower.find(word);

            if (at == std::string::npos) {
                return;
            }

            AddFilter(filters, f);
            lower.erase(at, len);
        };

        take("uncommon", 9);
        take("common", 8);
        take("rare", 10);
        take("epic", 11);
        take("legendary", 12);

        for (size_t at; lower.size() >= 2 && (at = lower.find("  ")) != std::string::npos;) {
            lower.erase(at, 1);
        }

        if (!lower.empty() && lower[0] == ' ') {
            lower.erase(0, 1);
        }

        if (!lower.empty() && lower.back() == ' ') {
            lower.pop_back();
        }

        static const int kOwn[9] = { 1, 2, 0, 0, 3, 4, 5, 6, 0 };

        if (kOwn[t]) {
            AddFilter(filters, static_cast<uint32_t>(kOwn[t]));
        }

        AddFilter(filters, 7);
        ContainerOf(t).ApplyFilter(lower, filters, {});
        return 0;
    }

    int HasAnySkillCardsCollected(lua_State* L) {   // FUN_1031d3c0: ([type])
        std::string s;
        bool any = false;

        if (!CoA::Script::ReadString(L, s)) {
            any = !Mgr().collected.empty();
        } else {
            const int t = CoA::SkillCard::TypeIndex(s);

            if (t >= 0) {
                for (const Collected& e : Mgr().collected) {   // FUN_1031b530
                    const Card* c = CoA::SkillCard::ByCardRank(e.cardId, e.rank);

                    if (c && CoA::SkillCard::TypeIndex(c->type) == t) {
                        any = true;
                        break;
                    }
                }
            }
        }

        PushBool(L, any);
        return 1;
    }

    int GetMaxRank(lua_State* L) {
        uint32_t id;

        if (!CoA::Script::ReadNumber(L, id)) {
            return 0;
        }

        lua_pushinteger(L, static_cast<int32_t>(MaxRank(id)));
        return 1;
    }

    int GetNumPendingSkillCards(lua_State* L) {
        lua_pushinteger(L, static_cast<int32_t>(Mgr().pending.size()));
        return 1;
    }

    int GetPendingSkillCardAtIndex(lua_State* L) {   // (name, a, b) or nil, nil
        uint32_t i;

        if (!CoA::Script::ReadNumber(L, i) || i == 0) {
            return 0;
        }

        if (i - 1 < Mgr().pending.size()) {
            const Pending& p = Mgr().pending[i - 1];
            PushStr(L, p.name.c_str());
            lua_pushinteger(L, static_cast<int32_t>(p.a));
            lua_pushinteger(L, static_cast<int32_t>(p.b));
            return 3;
        }

        lua_pushnil(L);
        lua_pushnil(L);
        return 2;
    }

    // CMSG_CLAIM_PENDING_SKILL_CARD 0x656: u32 n, n x {str name, u32 a, u32 b}.
    void SendClaim(const std::vector<const Pending*>& list) {
        CoA::Packet p(0x656);
        p.U32(static_cast<uint32_t>(list.size()));

        for (const Pending* e : list) {
            p.Str(e->name.c_str()).U32(e->a).U32(e->b);
        }

        p.Send();
    }

    int ClaimPendingSkillCard(lua_State* L) {   // FUN_1031c4f0: ({names})
        if (!CoA::Script::ValidateInput(L, { LUA_TTABLE })) {
            return 0;
        }

        std::vector<std::string> names;
        CoA::Script::ToStringArray(L, 1, names);   // FUN_1011f710
        std::vector<const Pending*> list;

        for (const std::string& n : names) {
            for (const Pending& p : Mgr().pending) {
                if (p.name == n) {
                    list.push_back(&p);
                    break;
                }
            }
        }

        SendClaim(list);
        PushBool(L, true);
        return 1;
    }

    int CanClaimPendingSkillCardAtIndex(lua_State* L) {   // true, nil
        uint32_t i;

        if (!CoA::Script::ReadNumber(L, i)) {
            return 0;
        }

        PushBool(L, true);
        lua_pushnil(L);
        return 2;
    }

    // FUN_1031d930: (type, amount) -> CMSG_PURCHASE_SEALED_CARD 0x66A {str, u32}.
    int PurchaseSealedCard(lua_State* L) {
        if (!CoA::Script::ValidateInput(L, { LUA_TSTRING, LUA_TNUMBER })) {
            return 0;
        }

        const std::string s = CheckString(L, 1);
        const uint32_t amount = static_cast<uint32_t>(ToInt(CheckNumber(L, 2)));
        const int t = IndexIn(s, kSealedTypes);   // FUN_10315d00

        if (t < 0) {
            return 0;
        }

        CoA::Packet(0x66A).Str(s.c_str()).U32(amount).Send();
        Mgr().pendingType = static_cast<uint32_t>(t);
        Mgr().pendingAmount = amount;
        PushBool(L, true);
        return 1;
    }

    int CanPurchaseSealedCard(lua_State* L) {   // FUN_1031be10
        if (!CoA::Script::ValidateInput(L, { LUA_TSTRING, LUA_TNUMBER })) {
            return 0;
        }

        const uint32_t type = SealedType(CheckString(L, 1));
        const std::vector<uint32_t> codes = CanPurchase(type, static_cast<uint32_t>(ToInt(CheckNumber(L, 2))));
        PushBool(L, codes.empty());

        if (codes.empty()) {
            lua_pushnil(L);
        } else {
            PushNames(L, codes, kPurchase);
        }

        return 2;
    }

    int PurchaseSealedCardBoosterPack(lua_State* L) {   // FUN_1031db30: CMSG 0x6DD {u32}
        uint32_t count;

        if (!CoA::Script::ReadNumber(L, count)) {
            return 0;
        }

        CoA::Packet(0x6DD).U32(count).Send();
        PushBool(L, true);
        return 1;
    }

    int CanPurchaseSealedCardBoosterPack(lua_State* L) {   // FUN_1031bfa0
        uint32_t count;

        if (!CoA::Script::ReadNumber(L, count)) {
            return 0;
        }

        const std::vector<uint32_t> codes = CanPurchaseBooster(count);
        PushBool(L, codes.empty());

        if (codes.empty()) {
            lua_pushnil(L);
        } else {
            PushNames(L, codes, kBooster);
        }

        return 2;
    }

    int GetSealedCardCost(lua_State* L) {   // FUN_1031d0f0: token item, cost
        if (!CoA::Script::ValidateInput(L, { LUA_TSTRING, LUA_TNUMBER })) {
            return 0;
        }

        const uint32_t type = SealedType(CheckString(L, 1));
        lua_pushinteger(L, static_cast<int32_t>(SealedToken(type)));
        lua_pushinteger(L, static_cast<int32_t>(SealedCost(type, static_cast<uint32_t>(ToInt(CheckNumber(L, 2))))));
        return 2;
    }

    // FUN_1031d610 (development realms only): as many of `type` as can be bought in one request.
    int PurchaseAllSealedCards(lua_State* L) {
        if (!CoA::RealmInfo::Get().Dev()) {
            return 0;
        }

        std::string s;

        if (!CoA::Script::ReadString(L, s)) {
            return 0;
        }

        const uint32_t type = SealedType(s);

        if (!CanPurchase(type, 1).empty()) {
            return 0;
        }

        uint32_t n = 1;

        while (CanPurchase(type, n + 1).empty()) {
            ++n;
        }

        CoA::Packet(0x66A).Str(s.c_str()).U32(n).Send();
        Mgr().pendingType = type;
        Mgr().pendingAmount = n;
        return 0;
    }

    // FUN_1031c1a0 (development realms only): every pending card, 150 per CMSG 0x656.
    int ClaimAllPendingSkillCards(lua_State*) {
        if (!CoA::RealmInfo::Get().Dev()) {
            return 0;
        }

        const std::vector<Pending>& all = Mgr().pending;

        for (size_t i = 0; i < all.size(); i += 0x96) {
            std::vector<const Pending*> chunk;

            for (size_t j = i; j < all.size() && j < i + 0x96; ++j) {
                chunk.push_back(&all[j]);
            }

            SendClaim(chunk);
        }

        return 0;
    }

    int GetSealedCardBoosterPackCost(lua_State* L) {
        uint32_t count;

        if (!CoA::Script::ReadNumber(L, count)) {
            return 0;
        }

        if (count == 0) {
            lua_pushinteger(L, 0);
        } else if (count > 0x10C6) {
            lua_pushnumber(L, 4294967295.0);
        } else {
            lua_pushnumber(L, static_cast<double>(count * 500000u));
        }

        return 1;
    }

    int GetMaxNumPurchasableSealedCardBoosterPacks(lua_State* L) {   // FUN_103196c0
        uint32_t n = 0x10C6;

        for (uint32_t k = 1; k <= 0x10C6; ++k) {
            if (!CanPurchaseBooster(k).empty()) {
                n = k - 1;
                break;
            }
        }

        lua_pushinteger(L, static_cast<int32_t>(n));
        return 1;
    }

    const CoA::Binding s_bindings[] = {
        COA_NATIVE("C_SkillCardCollection", "GetProgress", GetProgress),
        COA_NATIVE("C_SkillCardCollection", "IsCollected", IsCollected),
        COA_NATIVE("C_SkillCardCollection", "GetBonusSealedCardPacksProgress", GetBonusSealedCardPacksProgress),
        COA_NATIVE("C_SkillCardCollection", "GetNumSkillCards", GetNumSkillCards),
        COA_NATIVE("C_SkillCardCollection", "GetSkillCardAtIndex", GetSkillCardAtIndex),
        COA_NATIVE("C_SkillCardCollection", "SetSkillCardFilter", SetSkillCardFilter),
        COA_NATIVE("C_SkillCardCollection", "HasAnySkillCardsCollected", HasAnySkillCardsCollected),
        COA_NATIVE("C_SkillCardCollection", "GetMaxRank", GetMaxRank),
        COA_NATIVE("C_SkillCardCollection", "GetNumPendingSkillCards", GetNumPendingSkillCards),
        COA_NATIVE("C_SkillCardCollection", "GetPendingSkillCardAtIndex", GetPendingSkillCardAtIndex),
        COA_NATIVE("C_SkillCardCollection", "ClaimPendingSkillCard", ClaimPendingSkillCard),
        COA_NATIVE("C_SkillCardCollection", "CanClaimPendingSkillCardAtIndex", CanClaimPendingSkillCardAtIndex),
        COA_NATIVE("C_SkillCardCollection", "PurchaseSealedCard", PurchaseSealedCard),
        COA_NATIVE("C_SkillCardCollection", "CanPurchaseSealedCard", CanPurchaseSealedCard),
        COA_NATIVE("C_SkillCardCollection", "PurchaseSealedCardBoosterPack", PurchaseSealedCardBoosterPack),
        COA_NATIVE("C_SkillCardCollection", "CanPurchaseSealedCardBoosterPack", CanPurchaseSealedCardBoosterPack),
        COA_NATIVE("C_SkillCardCollection", "GetSealedCardCost", GetSealedCardCost),
        COA_NATIVE("C_SkillCardCollection", "PurchaseAllSealedCards", PurchaseAllSealedCards),
        COA_NATIVE("C_SkillCardCollection", "ClaimAllPendingSkillCards", ClaimAllPendingSkillCards),
        COA_NATIVE("C_SkillCardCollection", "GetSealedCardBoosterPackCost", GetSealedCardBoosterPackCost),
        COA_NATIVE("C_SkillCardCollection", "GetMaxNumPurchasableSealedCardBoosterPacks", GetMaxNumPurchasableSealedCardBoosterPacks),
    };

    // ---- the Deps other modules ask ---------------------------------------------------------------------------------
    bool StoreSkillCardByItem(uint32_t item, CoA::CustomStore::SkillCard& out) {   // FUN_102142a0
        const Card* c = CoA::SkillCard::ByItem(item);

        if (!c) {
            return false;
        }

        out.cardId = c->cardId;
        out.rank = c->rank;
        out.quality = c->quality;
        out.spell = c->spell;
        return true;
    }

    int32_t StoreQualityIndex(const std::string& quality) {   // FUN_101a33d0(q, 5)
        return CoA::SkillCard::QualityIndex(quality, 5);
    }

    // The 7a convention: a record is read only while its minimum size is left.
    const uint32_t kCollectedSize = 12;   // 0x648: u32 card, u32 progress, u32 rank
    const uint32_t kRevealSize = 9;       // 0x649: u32 card, u32 rank, u8 reveal
    const uint32_t kPendingSize = 9;      // 0x653 / 0x654: str (empty: 1), u32, u32
    const uint32_t kNameSize = 1;         // 0x655: str
}

// ---- the AscSkillCard queries --------------------------------------------------------------------------------------
CoA::SkillCard::Deps& CoA::SkillCard::Dependencies() {
    return s_deps;
}

const Card* CoA::SkillCard::ByItem(uint32_t item) {
    for (auto& e : Cards().byId) {
        if (e.second->item == item) {
            return e.second.get();
        }
    }

    return nullptr;
}

const Card* CoA::SkillCard::ByCardRank(uint32_t cardId, uint32_t rank) {
    auto& m = Cards().byCardRank;
    auto it = m.find({ cardId, rank });
    return it == m.end() ? nullptr : it->second;
}

int CoA::SkillCard::QualityIndex(const std::string& quality, int fallback) {
    const int i = IndexIn(quality, kQualities);

    if (i < 0) {
        CoA::Log::Printf("Unexpected Value: %s", quality.c_str());
        return fallback;
    }

    return i;
}

bool CoA::SkillCard::CollectedAtLeast(uint32_t cardId, uint32_t rank) {
    for (const Collected& e : Mgr().collected) {
        if (e.cardId == cardId && rank <= e.rank) {
            return true;
        }
    }

    return false;
}

uint32_t CoA::SkillCard::CollectedRank(uint32_t cardId) {
    const Collected* e = Find(cardId);
    return e ? e->rank : 0;
}

void CoA::SkillCard::PushCard(lua_State* L, const Card& card) {
    PushCardTable(L, card);
}

int CoA::SkillCard::TypeIndex(const std::string& type) {
    return IndexIn(type, kTypes);
}

const char* CoA::SkillCard::TypeName(uint32_t type) {
    return type < 9 ? kTypes[type] : nullptr;
}

// ---- SMSG ------------------------------------------------------------------------------------------------------------
// 0x647 (FUN_101e2f10): 12 u32 of the row, then type / expansion / class / quality.
int32_t CoA::SkillCard::OnPatchCard(void* param, uint32_t opcode, uint32_t time, CDataStore* msg) {
    uint32_t raw[12];

    for (uint32_t& v : raw) {
        v = CoA::Read::U32(msg);
    }

    auto c = std::make_unique<Card>();
    c->id = raw[0];
    c->cardId = raw[1];
    c->rank = raw[2];
    c->item = raw[3];
    c->spell = raw[4];
    c->type = CoA::Read::Str(msg);
    c->expansion = CoA::Read::Str(msg);
    c->cls = CoA::Read::Str(msg);
    c->quality = CoA::Read::Str(msg);
    c->wildcard = raw[9];
    c->draft = raw[10];
    c->starter = raw[11];
    Store& s = Cards();
    auto old = s.byId.find(c->id);

    if (old != s.byId.end()) {
        *old->second = *c;
        s.byCardRank[{ old->second->cardId, old->second->rank }] = old->second.get();
    } else {
        s.byCardRank[{ c->cardId, c->rank }] = c.get();
        s.byId[c->id] = std::move(c);
    }

    ResetContainers();
    return 1;
}

// 0x648: u32 bonus progress, 24 u32 purchase counts, u32 n x {u32 card, u32 progress, u32 rank}.
int32_t CoA::SkillCard::OnList(void* param, uint32_t opcode, uint32_t time, CDataStore* msg) {
    Manager& m = Mgr();
    m.collected.clear();
    m.bonusProgress = CoA::Read::U32(msg);

    for (uint32_t& c : m.purchased) {
        c = CoA::Read::U32(msg);
    }

    const uint32_t n = CoA::Read::U32(msg);

    for (uint32_t i = 0; i < n && CoA::Read::Remaining(msg) >= kCollectedSize; ++i) {
        Collected e;
        e.cardId = CoA::Read::U32(msg);
        e.progress = CoA::Read::U32(msg);
        e.rank = CoA::Read::U32(msg);
        m.collected.push_back(e);
    }

    return 1;
}

// 0x649: u32 n x {u32 card, u32 rank, u8 auto-revealed}.
int32_t CoA::SkillCard::OnCollected(void* param, uint32_t opcode, uint32_t time, CDataStore* msg) {
    const uint32_t n = CoA::Read::U32(msg);

    for (uint32_t i = 0; i < n && CoA::Read::Remaining(msg) >= kRevealSize; ++i) {
        const uint32_t cardId = CoA::Read::U32(msg);
        const uint32_t rank = CoA::Read::U32(msg);
        const uint8_t reveal = CoA::Read::U8(msg);
        const Card* c = CoA::SkillCard::ByCardRank(cardId, rank);

        if (!c) {
            continue;
        }

        if (reveal) {
            CoA::Events::Signal("SKILL_CARD_AUTO_REVEALED", "%u%u", c->cardId, c->rank);
        }

        Collect(*c);
        CoA::Events::Signal("SKILL_CARD_COLLECTED", "%u%u", c->cardId, c->rank);
    }

    return 1;
}

// 0x653: the pending list, u32 n x {str, u32, u32}.
int32_t CoA::SkillCard::OnPendingList(void* param, uint32_t opcode, uint32_t time, CDataStore* msg) {
    Mgr().pending.clear();
    const uint32_t n = CoA::Read::U32(msg);

    for (uint32_t i = 0; i < n && CoA::Read::Remaining(msg) >= kPendingSize; ++i) {
        Pending e;
        e.name = CoA::Read::Str(msg);
        e.a = CoA::Read::U32(msg);
        e.b = CoA::Read::U32(msg);
        Mgr().pending.push_back(e);
    }

    return 1;
}

// 0x654: added, each with PENDING_SKILL_CARD_ADDED (count, a, b).
int32_t CoA::SkillCard::OnPendingAdded(void* param, uint32_t opcode, uint32_t time, CDataStore* msg) {
    const uint32_t n = CoA::Read::U32(msg);

    for (uint32_t i = 0; i < n && CoA::Read::Remaining(msg) >= kPendingSize; ++i) {
        Pending e;
        e.name = CoA::Read::Str(msg);
        e.a = CoA::Read::U32(msg);
        e.b = CoA::Read::U32(msg);
        Mgr().pending.push_back(e);
        CoA::Events::Signal("PENDING_SKILL_CARD_ADDED", "%u%u%u", static_cast<uint32_t>(Mgr().pending.size()), e.a, e.b);
    }

    return 1;
}

// 0x655: u32 n x {str}; stops at the first name it does not hold. PENDING_SKILL_CARD_REMOVED (index, a, b).
int32_t CoA::SkillCard::OnPendingRemoved(void* param, uint32_t opcode, uint32_t time, CDataStore* msg) {
    const uint32_t n = CoA::Read::U32(msg);

    for (uint32_t i = 0; i < n && CoA::Read::Remaining(msg) >= kNameSize; ++i) {
        const std::string name = CoA::Read::Str(msg);
        std::vector<Pending>& v = Mgr().pending;
        auto it = std::find_if(v.begin(), v.end(), [&](const Pending& e) { return e.name == name; });

        if (it == v.end()) {
            return 1;
        }

        CoA::Events::Signal("PENDING_SKILL_CARD_REMOVED", "%u%u%u", static_cast<uint32_t>(it - v.begin() + 1), it->a, it->b);
        v.erase(it);
    }

    return 1;
}

int32_t CoA::SkillCard::OnClaimResult(void* param, uint32_t opcode, uint32_t time, CDataStore* msg) {   // 0x657
    const std::string r = CoA::Read::Str(msg);
    CoA::Events::Signal("CLAIM_SKILL_CARD_RESULT", "%s", r.c_str());
    return 1;
}

// 0x66B: a successful purchase adds the request in flight to its type's count.
int32_t CoA::SkillCard::OnPurchaseResult(void* param, uint32_t opcode, uint32_t time, CDataStore* msg) {
    const std::string r = CoA::Read::Str(msg);
    const int code = IndexIn(r, kPurchase);

    if (code < 0) {
        CoA::Log::Printf("Unexpected Value: %s", r.c_str());
    } else if (code == 0 && Mgr().pendingType != 0x18) {
        Mgr().purchased[Mgr().pendingType] += Mgr().pendingAmount;
    }

    CoA::Events::Signal("PURCHASE_SEALED_CARD_RESULT", "%s", r.c_str());
    return 1;
}

int32_t CoA::SkillCard::OnBoosterResult(void* param, uint32_t opcode, uint32_t time, CDataStore* msg) {   // 0x6DE
    const std::string r = CoA::Read::Str(msg);
    CoA::Events::Signal("PURCHASE_SEALED_CARD_BOOSTER_PACK_RESULT", "%s", r.c_str());
    return 1;
}

// 0x66C: a whole SealedCardCosts row (25 u32).
int32_t CoA::SkillCard::OnCostPatch(void* param, uint32_t opcode, uint32_t time, CDataStore* msg) {
    std::vector<uint32_t> row(25);

    for (uint32_t& v : row) {
        v = CoA::Read::U32(msg);
    }

    s_costPatches[row[0]] = row;
    return 1;
}

void CoA::SkillCard::OnGlueScreen() {   // 0x10317DF0
    Manager& m = Mgr();
    m.bonusProgress = 0;
    memset(m.purchased, 0, sizeof(m.purchased));
    m.pendingType = 0x18;
    m.pendingAmount = 0;
    m.collected.clear();
    m.pending.clear();
    ResetContainers();
}

void CoA::SkillCard::Init() {
    CoA::LuaReg::AddModule(s_bindings, sizeof(s_bindings) / sizeof(s_bindings[0]));
    COA_PACKET(0x647, CoA::SkillCard::OnPatchCard);
    COA_PACKET(0x648, CoA::SkillCard::OnList);
    COA_PACKET(0x649, CoA::SkillCard::OnCollected);
    COA_PACKET(0x653, CoA::SkillCard::OnPendingList);
    COA_PACKET(0x654, CoA::SkillCard::OnPendingAdded);
    COA_PACKET(0x655, CoA::SkillCard::OnPendingRemoved);
    COA_PACKET(0x657, CoA::SkillCard::OnClaimResult);
    COA_PACKET(0x66B, CoA::SkillCard::OnPurchaseResult);
    COA_PACKET(0x6DE, CoA::SkillCard::OnBoosterResult);
    COA_PACKET(0x66C, CoA::SkillCard::OnCostPatch);
    CoA::Lifecycle::OnGlueScreen(CoA::SkillCard::OnGlueScreen);
    CoA::GameMode::OnModeChanged(ResetContainers);
    COA_EVENT("BONUS_SEALED_CARD_PACK_PROGRESS_UPDATE");
    COA_EVENT("BONUS_SEALED_CARD_PACK_REWARDED");
    COA_EVENT("CLAIM_SKILL_CARD_RESULT");
    COA_EVENT("PENDING_SKILL_CARD_ADDED");
    COA_EVENT("PENDING_SKILL_CARD_REMOVED");
    COA_EVENT("PURCHASE_SEALED_CARD_BOOSTER_PACK_RESULT");
    COA_EVENT("PURCHASE_SEALED_CARD_RESULT");
    COA_EVENT("SKILL_CARD_ADDED");
    COA_EVENT("SKILL_CARD_AUTO_REVEALED");
    COA_EVENT("SKILL_CARD_COLLECTED");
    COA_EVENT("SKILL_CARD_PROGRESS_UPDATED");
    COA_EVENT("SKILL_CARD_UPGRADED");

    CoA::CustomStore::Deps& store = CoA::CustomStore::Dependencies();
    store.skillCardByItem = StoreSkillCardByItem;
    store.skillCardCollectedAtLeast = CoA::SkillCard::CollectedAtLeast;
    store.skillCardQualityIndex = StoreQualityIndex;
    CoA::Wildcard::Dependencies().skillCardUnblocked = CoA::SkillCard::AnyUnblocked;
}

void CoA::SkillCard::Reset() {
    s_mgr.reset();
    s_shared.Reset();
    s_store = Store();
    s_costPatches.clear();
    s_deps = Deps();
}
