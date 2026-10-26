#ifndef COA_SOURCE_MODULES_TEMPLATES_H
#define COA_SOURCE_MODULES_TEMPLATES_H

#include <cstdint>

// AscTemplates.cpp: GetQuestTemplate (0x1021EA50) / GetCreatureTemplate (0x1021B200) / GetCreatureModelData (0x100B0EF0) /
// GetCreatureDisplayInfo (0x100B0D70) / GetCreatureDisplayInfoExtra (0x100B0E30). Each reads luaL_checknumber(L, 1),
// looks the record up (the client quest / creature caches with no callback, or the client DBC container), builds a
// nlohmann::json object from it (FUN_1021ecb0, FUN_1021b2d0, FUN_100b39f0, FUN_100b2d20, FUN_100b3350) and pushes it
// through FUN_100a4580 (CoA::Json::Push); a missing record returns nothing. Checked against the genuine DLL.
//
// FUN_1021ecb0 reads the quest record at the offsets of a larger record than this client's QuestCache (0x28E4 bytes):
// ObjectiveText is 4 x 0x200 from +0x1C94 (the client's is 4 x 0x100), and RewardTitleId .. Unknown sit 0x400 further
// on (+0x2494 .. +0x2CE0). Read here against the client's record as the original does; bytes past its end read 0.
namespace CoA::Templates {
    struct Creature {   // CreatureStats_C (x86 offsets)
        uint32_t id = 0;                 // +0x6C (the cache node's key after the record)
        const char* subName = nullptr;   // +0x04
        const char* iconName = nullptr;  // +0x08
        uint32_t flags = 0;              // +0x0C
        uint32_t type = 0;               // +0x10
        uint32_t family = 0;             // +0x14
        uint32_t rank = 0;               // +0x18
        uint32_t proxy[2] = {};          // +0x1C
        uint32_t display[4] = {};        // +0x24
        float health = 0.0f;             // +0x34
        float power = 0.0f;              // +0x38
        uint32_t racialLeader = 0;       // +0x3C (a bool and its padding, read as a u32)
        uint32_t questItem[6] = {};      // +0x40
        uint32_t movement = 0;           // +0x58
        const char* name = nullptr;      // +0x5C
    };

    struct ModelData {   // CreatureModelData.dbc (client container 0xAD3500)
        int32_t id = 0, flags = 0;
        const char* modelName = nullptr;
        int32_t sizeClass = 0;
        float modelScale = 0.0f;
        int32_t bloodId = 0, footprintTexture = 0;
        float footprintLength = 0.0f, footprintWidth = 0.0f, footprintScale = 0.0f;
        int32_t foleyMaterial = 0, footstepShake = 0, deathThudShake = 0, soundId = 0;
        float collisionWidth = 0.0f, collisionHeight = 0.0f, mountHeight = 0.0f;
        float geoBoxMin[3] = {};
        float geoBoxMax[3] = {};
        float worldEffectScale = 0.0f, attachedEffectScale = 0.0f;
        float missileRadius = 0.0f, missilePush = 0.0f, missileRaise = 0.0f;
    };

    struct DisplayInfo {   // CreatureDisplayInfo.dbc (0xAD34B8)
        int32_t id = 0, modelId = 0, soundId = 0, extraId = 0;
        float scale = 0.0f;
        int32_t alpha = 0;
        const char* textures[3] = {};
        const char* portrait = nullptr;
        int32_t bloodLevel = 0, bloodId = 0, npcSoundId = 0, particleColorId = 0, geosetData = 0, effectPackageId = 0;
    };

    struct DisplayInfoExtra {   // CreatureDisplayInfoExtra.dbc (0xAD3494)
        int32_t fields[8] = {};   // ID, DisplayRaceID, DisplaySexID, SkinID, FaceID, HairStyleID, HairColorID, FacialHairID
        int32_t items[11] = {};
        int32_t flags = 0;
        const char* bakeName = nullptr;
    };

    // The client questions; TemplatesClient.cpp installs them.
    struct Deps {
        // The quest cache's record (0x67DE90 on 0xC5DA48) as its bytes, and their count.
        const uint8_t* (*quest)(uint32_t id, uint32_t& size) = nullptr;
        bool (*creature)(uint32_t id, Creature& out) = nullptr;            // 0x67B6A0 on 0xC5D690
        bool (*modelData)(uint32_t id, ModelData& out) = nullptr;
        bool (*displayInfo)(uint32_t id, DisplayInfo& out) = nullptr;
        bool (*displayInfoExtra)(uint32_t id, DisplayInfoExtra& out) = nullptr;
    };

    Deps& Dependencies();

    void Init();
    // Tests only: Deps forgotten.
    void Reset();
}

#endif
