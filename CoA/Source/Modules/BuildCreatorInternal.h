#ifndef COA_SOURCE_MODULES_BUILDCREATORINTERNAL_H
#define COA_SOURCE_MODULES_BUILDCREATORINTERNAL_H

// What BuildCreator.cpp (the container, the link codec, the validators, the packets) and BuildCreatorNatives.cpp (the
// Lua natives) share: the reconstruction's AscBuildCreator.cpp anonymous namespace. Not for other modules.
#include "CoA/Source/Modules/BuildCreator.h"
#include "CoA/Source/Runtime/Dbc.h"
#include "CoA/Source/Runtime/FilterContainer.h"
#include "CoA/Source/Runtime/Log.h"
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace CoA {
    class Packet;
}

namespace CoA::BuildCreator::detail {
    // The 32-bit build's std::hash<uint32_t> (FNV-1a over the four bytes, 32-bit basis and prime): with MSVC's bucket
    // policy it gives the original's unordered_map iteration order (the link's chain tokens, the enchant stacks,
    // UpdateFilter's sort selections).
    struct Fnv32 {
        size_t operator()(uint32_t v) const {
            uint32_t h = 2166136261u;

            for (int i = 0; i < 4; ++i) {
                h ^= (v >> (i * 8)) & 0xFF;
                h *= 16777619u;
            }

            return h;
        }
    };

    template <class V>
    using U32Map = std::unordered_map<uint32_t, V, Fnv32>;

    const uint32_t kRoleValues[6] = { 0, 1, 2, 4, 8, 15 };   // 0x10B21FDC

    // The enum lookups' "not found" return is the stack word after their by-value string argument (FUN_100efb90 /
    // FUN_100effa0 return in_stack_0000001c): caller garbage that matches no value.
    const uint32_t kNoValue = 0xFFFFFFFF;

    // ---- the entry ---------------------------------------------------------------------------------------------
    struct Spell {   // 0x28
        uint32_t spell = 0;         // +0x00
        uint32_t level = 0;         // +0x04
        uint8_t core = 0;           // +0x08
        uint8_t optimal = 0;        // +0x09
        uint8_t empowering = 0;     // +0x0A
        uint8_t synergistic = 0;    // +0x0B
        uint32_t flags = 0;         // +0x0C
        std::string comment;        // +0x10
    };

    struct Enchant {   // 0x28
        uint32_t id = 0;            // +0x00 (a MysticEnchant spell)
        uint32_t stacks = 0;        // +0x04
        uint32_t level = 0;         // +0x08
        uint32_t flags = 0;         // +0x0C
        std::string comment;        // +0x10
    };

    struct Typed {   // 0x1C: a weapon or armour subclass
        uint32_t type = 0;          // +0x00
        std::string comment;        // +0x04
    };

    struct Entry {   // 0x138, FUN_100f7bc0
        std::string id;             // +0x00
        std::string levelingId;     // +0x18
        std::string pveId;          // +0x30
        std::string pvpId;          // +0x48
        uint32_t cls = 0;           // +0x60
        std::string author;         // +0x64
        std::string name;           // +0x7C
        std::string subtext;        // +0x94
        std::string description;    // +0xAC
        std::string icon;           // +0xC4
        uint32_t upvotes = 0;       // +0xDC
        uint64_t created = 0;       // +0xE0
        uint64_t updated = 0;       // +0xE8
        uint32_t category = 0;      // +0xF0
        uint32_t roles = 8;         // +0xF4
        uint32_t primaryStat = 0;   // +0xF8
        std::vector<Spell> spells;          // +0xFC
        std::vector<Enchant> enchants;      // +0x108
        std::vector<Typed> weapons;         // +0x114
        std::vector<Typed> armor;           // +0x120
        uint8_t hidden = 0;         // +0x12C
        uint32_t flags = 0;         // +0x130
        uint32_t difficulty = 0;    // +0x134
    };

    struct SpecRecord {   // 0x1C (FUN_100fc950)
        std::string id;
        uint8_t a = 0;   // +0x18
        uint8_t b = 0;   // +0x19
    };

    struct Mgr {
        uint32_t category = 0;                  // +0x000
        std::vector<Entry> queried;             // +0x004
        Entry pending;                          // +0x010
        std::vector<Entry> page;                // +0x148
        std::vector<SpecRecord> specs;          // +0x1D0
        std::vector<std::string> owned;         // +0x1DC
        std::vector<std::string> upvoted;       // +0x1E8
        std::string ratedId;                    // +0x1F4
        bool rateUp = false;                    // +0x20C
        std::string deletedId;                  // +0x210
    };

    Mgr& M();

    // BuildCreatorEntryContainer's filter (vtable 0x10B2E6A4 over the base 0x10B2E674): the category page, text in name
    // / description / author, argument groups and sort selections from UpdateFilter. The original keeps pointers into
    // the page (FUN_10107ef0); an entry here is the page index, which reads what the original's pointer reads as long
    // as the page is not reallocated (and stays readable when it is, where the original's pointer dangles).
    class BuildFilter : public CoA::Filter::Container<size_t> {
    protected:
        std::vector<size_t> DoPopulate() override;                              // [0] FUN_10107ef0
        bool TextMatch(const std::string& lower, const size_t& e) override;     // [3] FUN_101081f0
        uint32_t ArgGroup(uint32_t a) override;                                 // [4] FUN_101087a0
        bool ArgMatch(uint32_t a, const size_t& e) override;                    // [2] FUN_101083b0
        std::string SortKey(uint32_t s, const size_t& e) override;              // [9] FUN_10108a00
    };

    BuildFilter& Builds();   // +0x154
    // The filtered entry at a 0-based index, or nullptr.
    const Entry* FilteredAt(size_t i);

    // ---- enums -------------------------------------------------------------------------------------------------
    inline std::string Unexpected(uint32_t v) {
        return "UNEXPECTED_ENUM_VALUE_" + std::to_string(v);
    }

    template <size_t N>
    std::string EnumName(const char* const (&table)[N], uint32_t v) {
        return v < N ? std::string(table[v]) : Unexpected(v);
    }

    template <size_t N>
    bool EnumIndex(const char* const (&table)[N], const std::string& s, uint32_t& out) {
        for (uint32_t i = 0; i < N; ++i) {
            if (s == table[i]) {
                out = i;
                return true;
            }
        }

        return false;
    }

    // FUN_100efb90 and its siblings: name -> value, logging "Unexpected Value: {}" otherwise.
    template <size_t N>
    uint32_t EnumOrLog(const char* const (&table)[N], const std::string& s, uint32_t fallback) {
        uint32_t v = 0;

        if (EnumIndex(table, s, v)) {
            return v;
        }

        CoA::Log::Printf("Unexpected Value: %s", s.c_str());
        return fallback;
    }

    // ---- the client, through Deps ------------------------------------------------------------------------------
    bool HasPlayer();
    uint32_t PlayerClass();   // 0 without a player
    const char* PlayerName();
    uint32_t FirstRank(uint32_t spell);
    uint32_t RankNumber(uint32_t spell);
    const std::vector<uint32_t>& Chain(uint32_t firstRank);
    int64_t Now();
    uint32_t MaxLevel();   // FUN_102fc4d0

    // MysticEnchant.dbc by spell (map 0x10BE04C8) and its RE_QUALITY_* (FUN_102e65d0).
    const uint8_t* EnchantRow(uint32_t spell);
    uint32_t EnchantQuality(const uint8_t* row);

    // ---- the entry helpers -------------------------------------------------------------------------------------
    void SortEntry(Entry& e);            // FUN_10107e00
    void ResetPending();                 // FUN_100f7bc0 + the player's class
    Entry* FindBuild(const std::string& id);   // FUN_100fd750: the category page, then the queried list
    bool Contains(const std::vector<std::string>& v, const std::string& s);
    void Erase(std::vector<std::string>& v, const std::string& s);
    void ResetFilter();                  // FUN_101016d0 on +0x154
    CoA::CA::BuildSeed SeedOf(const Entry& e);
    Spell* PendingSpell(uint32_t spell);
    void WriteEntry(CoA::Packet& pk, const Entry& e);   // FUN_100f9c30

    // ---- the link ----------------------------------------------------------------------------------------------
    std::string ExportLink(const Entry& e);                       // FUN_100fd8f0
    void ImportLink(Entry& e, const std::string& url);            // FUN_101001e0
    Entry EntryFromBuild(const CoA::CA::Build& b, bool enchants); // FUN_100fc240

    // ---- the validators ----------------------------------------------------------------------------------------
    std::vector<uint32_t> CanAddSpell(const Spell& s);                         // FUN_100fac40
    std::vector<uint32_t> CanRemoveSpell(uint32_t spell);                      // FUN_100fb9b0
    std::vector<uint32_t> CanSetSpellLevel(uint32_t spell, uint32_t level);    // FUN_100fbd20
    uint32_t ValidatePublish(bool isNew);                                      // FUN_100fb090

    void InitNatives();   // BuildCreatorNatives.cpp
}

#endif
