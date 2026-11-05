#ifndef COA_SOURCE_MODULES_TEMPLATES_H
#define COA_SOURCE_MODULES_TEMPLATES_H

#include <cstdint>

// AscTemplates.cpp: the quest / creature template and creature DBC readers.
// Quest record offsets and the original's over-read: docs/2026-10-29-coa-module-notes.md.
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
