#ifndef COA_SOURCE_MODULES_OBJECTADDON_H
#define COA_SOURCE_MODULES_OBJECTADDON_H

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

// AscObjectAddon.cpp: server-driven custom object fields, SMSG_UPDATE_OBJECT_ADDON 0x578 (handler_0x0578 /
// FUN_102ce0c0). Per GUID a vector of fields {value, callback, callback arg}: units and players (FUN_102cdcf0, map
// 0x10BE3648) have 0x58 fields, items (FUN_102cdbb0, map 0x10BE3608) one; the other GUID kinds get empty records.
// The client half (ObjectAddonClient.cpp) drops records as objects leave the client.
namespace CoA::ObjectAddon {
    struct Field;
    using TypeCallback = void (*)(uint64_t guid, uint32_t index, void* arg, Field* field);

    struct Field {
        uint32_t value = 0;
        TypeCallback callback = nullptr;   // the field's own callback
        void* arg = nullptr;
    };

    // The lookups create the record (sized) when it does not exist yet, as the originals do.
    std::vector<Field>& Unit(uint64_t guid);   // FUN_102cdcf0
    std::vector<Field>& Item(uint64_t guid);   // FUN_102cdbb0

    inline float AsFloat(const Field& f) {
        float v;
        memcpy(&v, &f.value, 4);
        return v;
    }

    // FUN_102ce900 -> FUN_102ce690 with a zero GUID: a callback for field `index` of every object of the given kind
    // (Kind's 0..7), run after the field's own callback.
    void RegisterTypeCallback(uint32_t kind, uint32_t index, TypeCallback fn, void* arg);

    // FUN_102ce0c0's classification of a GUID: 4 a unit / player (high < 0x10000000), 1 an item (0x4000), 3 a
    // creature / pet / vehicle (0xF130 / 0xF140 / 0xF150), 5 0xF110 / 0x1FC0, 6 0xF100, 7 0xF101, else 0.
    uint32_t Kind(uint64_t guid);

    // FUN_102ccfc0: an object leaving the client's list drops its unit record.
    void DropUnit(uint64_t guid);
    // FUN_102cd780 (every second SMSG_INITIAL_SPELLS): every unit record except `keep`'s is dropped.
    void DropAllUnitsExcept(uint64_t keep);

    void Init();
    // Tests only.
    void Reset();
    size_t UnitCount();
}

#endif
