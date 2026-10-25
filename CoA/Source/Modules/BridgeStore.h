#ifndef COA_SOURCE_MODULES_BRIDGESTORE_H
#define COA_SOURCE_MODULES_BRIDGESTORE_H

#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

class CDataStore;

// AscBridgeStore.cpp: the DLL's DBC manager (FUN_101d3d20) moves six large DBCs into MemoryBridge tables --
// Creature, Quest, ItemAddon, VanityCollection, MythicKeystones, SpellAffect -- and answers lookups from the
// tables through a size-bounded LRU cache (0x400 records / 2 MB, as the original). The tables keep the original's
// 32-bit records, string fields as string-block offsets; the records handed out here are the x64 shapes, strings
// as pointers owned by the cache. Returned pointers stay valid until the cache evicts them.
namespace CoA::BridgeStore {
    // FUN_101fd240's Creature.dbc record (table row 0x1C bytes).
    struct CreatureRecord {
        uint32_t id;          // file column 0
        uint32_t entry;       // column 1
        const char* name;     // the locale slot of columns 2..18
        uint32_t displayId;   // column 19
        uint32_t f10, f14, f18;   // columns 20..22
    };

    // FUN_100b8c00 -> FUN_100b8cc0: the record whose entry is `entry`, or nullptr.
    const CreatureRecord* CreatureByEntry(uint32_t entry);

    // Quest.dbc rows verbatim (FUN_102137b0: 29 u32, 0x74 bytes): +0 id, +8..+0x10 groups, +0x14 min level, +0x18
    // reward items x4, +0x28 reward amounts x4, +0x38 required items x6, +0x50 required counts x6, +0x68..+0x70 sort
    // ids.
    const uint8_t* QuestById(uint32_t id);                       // FUN_101dc700 -> FUN_101d0bd0
    std::vector<const uint8_t*> QuestsInGroup(uint32_t group);   // FUN_10212ad0 -> FUN_102123e0
    bool PatchQuest(const uint8_t* row);                         // FUN_10209860

    // VanityCollection.dbc as FUN_1021a230 packs it (45 dwords): dword i is the item browser's record slot i -- 0 ID,
    // 1 item, 2 category, 3 artwork (string), 4..6 costs, 7..11 realm kinds, 0xC flags, 0xD..0xE class mask, 0xF race
    // mask, 0x10/0x11 previews, 0x12..0x29 contents, 0x2A description (string), 0x2B additional text (string), 0x2C
    // learned spell. The string slots' dwords are not meaningful here; Str gives the strings (3 / 0x2A / 0x2B).
    struct VanityRecord {
        uint32_t f[0x2D];
        const char* strings[3];   // slots 3, 0x2A, 0x2B

        const char* Str(uint32_t i) const {
            return i == 3 ? this->strings[0] : i == 0x2A ? this->strings[1] : i == 0x2B ? this->strings[2] : nullptr;
        }
    };
    struct VanityEntry {
        uint32_t index;
        bool found;
        VanityRecord record;
    };

    uint32_t VanityCount();
    // FUN_1032ec60's store part (SMSG 0x573): strings are C strings on input.
    bool PatchVanity(const VanityRecord& r);
    bool VanityRecords(const uint32_t* indexes, uint32_t n, std::vector<VanityEntry>& out);   // FUN_1032dc10
    const VanityRecord* VanityAt(uint32_t index, uint32_t itemId);                          // FUN_1032e090

    // MythicKeystones.dbc record {+0 key (the keystone item id), +4 dungeon id, +8 level} by key (FUN_101d0500).
    bool KeystoneById(uint32_t id, uint32_t out[3]);
    void PatchKeystone(const uint32_t rec[3]);   // SMSG 0x56D

    // SpellAffect.dbc records {id, modifier spell, affected spell or -(first rank of a chain)} whose modifier spell
    // (+4) is `spell` (FUN_103234d0).
    std::vector<std::array<uint32_t, 3>> SpellAffectsFor(uint32_t spell);

    // The DBC manager's bridged part, run after ClientDBInitialize: the six tables (MythicKeystones and SpellAffect
    // first, so they get the first handles), then -- after the HD tables -- the extra server indexes.
    void LoadTables();
    void BuildIndexes();
    // Every store emptied, its table destroyed.
    void Reset();

    // SMSG 0x56F / 0x569 / 0x567 / 0x574.
    int32_t OnPatchQuest(void* param, uint32_t opcode, uint32_t time, CDataStore* msg);
    int32_t OnPatchItemAddon(void* param, uint32_t opcode, uint32_t time, CDataStore* msg);
    int32_t OnPatchCreature(void* param, uint32_t opcode, uint32_t time, CDataStore* msg);
    int32_t OnPatchSpellAffect(void* param, uint32_t opcode, uint32_t time, CDataStore* msg);
}

// AscItemAddon.hpp: the ItemAddon store (FUN_101a2690), keyed by item entry.
namespace CoA::ItemAddon {
    // FUN_1020e270's row (table row 0x40 bytes).
    struct Record {
        uint32_t rowId;              // file column 0
        uint32_t id;                 // column 1, the item entry (the key)
        const char* name;            // the locale slot of columns 2..18
        const char* description;     // the locale slot of columns 19..35
        uint32_t fields[12];         // columns 36..47, table row +0x10..+0x3C

        uint32_t Quality() const { return this->fields[0]; }
    };

    // FUN_101a2690: nullptr when the store has no such item.
    const Record* Find(uint32_t itemId);

    // The item text + quality the UI shows (the 0x48-byte result FUN_1020e9c0 returns).
    struct Info {
        std::string name, description;
        uint32_t quality = 0, flags = 0;   // row +0x10 / +0x14
    };
    // FUN_1020ef10: a batch read past the record cache -- one 0x6D over the unique items, one 0x6E over the unique
    // name / description offsets. `out` holds only the items found; false (and `out` empty) when a query fails.
    bool FetchInfo(const uint32_t* items, uint32_t n, std::unordered_map<uint32_t, Info>& out);
    // FUN_1020e9c0: FetchInfo for one item, else the cached record, else empty strings and zeros.
    Info GetInfo(uint32_t itemId);
}

#endif
