#ifndef COA_SOURCE_MODULES_SPAWNVISIBILITY_H
#define COA_SOURCE_MODULES_SPAWNVISIBILITY_H

#include <cstdint>

// AscSpawnVisibility.cpp (FUN_101a2020): spell 0x16E59 is applied (list 0x10BE2B6C before 0x724820, callback
// 0x101A1FA0 at site 0x101A2025) only to an object ObjectSpawnVisibilitySettings.dbc does NOT show (FUN_10211e10 false).
//
// ObjectSpawnVisibilitySettings.dbc: +0 id, +4 group, +8 TYPEID_* name, +0xC entry, +0x10 GUID counter, +0x14 a quest
// that must be completed (s_completedQuests, 0xACFDEC), +0x18 an achievement that must be earned (s_completedAchievements,
// 0xACFDB0; FUN_103087e0). FUN_10211e10(type, entry, counter): the rows of the object's type are grouped by +4 twice --
// those naming its entry, those naming its counter. Each set passes when it is empty or one of its groups has every row
// passing FUN_10211db0; the object is shown when both pass. Every call reads the whole table again.
//
// The module's glue-screen callback FUN_101a2040 zeroes 0x10BDEDA0, whose only accessor (0x101A2070) has no caller in
// the plain code -- not reproduced.
namespace CoA::SpawnVisibility {
    struct ObjectInfo {
        uint32_t type = 0;   // +0x14 (TYPEID_*)
        uint64_t guid = 0;   // descriptor +0
        uint32_t entry = 0;  // descriptor +0xC
    };

    struct Deps {
        bool (*objectInfo)(void* object, ObjectInfo& out) = nullptr;
        bool (*questCompleted)(uint32_t quest) = nullptr;            // s_completedQuests (0xACFDEC)
        bool (*achievementCompleted)(uint32_t achievement) = nullptr;   // s_completedAchievements (0xACFDB0)
    };

    Deps& Dependencies();

    // FUN_10211aa0: a TYPEID_* name -> its index (0x10B3C7A8); anything else logs "Unexpected Value: %s" and gives fallback.
    uint32_t TypeIdByName(const char* name, uint32_t fallback);
    // FUN_10211e10.
    bool Visible(uint32_t type, uint32_t entry, uint32_t counter);
    // 0x101A1FA0: false refuses the aura effect.
    bool BeforeAuraApply(void* object, uint32_t index, const void* spell);

    void Init();
    // Tests only.
    void Reset();
}

#endif
