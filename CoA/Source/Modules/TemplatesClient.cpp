// AscTemplates.cpp's client questions: the quest / creature caches and the CreatureModelData / CreatureDisplayInfo /
// CreatureDisplayInfoExtra tables.
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Modules/Templates.h"
#include "WoW/Source/DB/DBClient/DBCacheInstances.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Source/DB/DBClient/CreatureStats_C.h"
#include "WoW/Source/Object/QuestCache.h"
#include <cstddef>

// The quest record has no pointers, so its 64-bit layout is the 32-bit client's; FUN_1021ecb0's offsets apply as-is.
static_assert(sizeof(QuestCache) == 0x28E4, "QuestCache");
static_assert(offsetof(QuestCache, m_objectiveText) == 0x1C94, "QuestCache objective text");
static_assert(offsetof(QuestCache, m_rewardReputationMask) == 0x28E0, "QuestCache reputation mask");

namespace {
    const uint8_t* Quest(uint32_t id, uint32_t& size) {   // 0x67DE90(id, 0, 0, 0, 0)
        const QuestCache* rec = g_questDBCache.GetRecord(static_cast<int32_t>(id), WOWGUID(), nullptr, nullptr, false);
        size = sizeof(QuestCache);
        return reinterpret_cast<const uint8_t*>(rec);
    }

    bool Creature(uint32_t id, CoA::Templates::Creature& out) {   // 0x67B6A0(id, 0, 0, 0, 0)
        const CreatureStats_C* rec = g_creatureDBCache.GetRecord(static_cast<int32_t>(id), WOWGUID(), nullptr, nullptr, false);

        if (!rec) {
            return false;
        }

        out.id = id;   // the node's key (+0x6C, right after the record)
        out.subName = rec->m_subName;
        out.iconName = rec->m_iconName;
        out.flags = rec->m_typeFlags;
        out.type = rec->m_type;
        out.family = rec->m_family;
        out.rank = rec->m_rank;

        for (int i = 0; i < 2; ++i) {
            out.proxy[i] = rec->m_killCredit[i];
        }

        for (int i = 0; i < 4; ++i) {
            out.display[i] = rec->m_displayId[i];
        }

        out.health = rec->m_hpModifier;
        out.power = rec->m_mpModifier;
        out.racialLeader = rec->m_racialLeader ? 1 : 0;

        for (int i = 0; i < 6; ++i) {
            out.questItem[i] = rec->m_questItem[i];
        }

        out.movement = rec->m_movementId;
        out.name = rec->m_name[0];
        return true;
    }

    bool ModelData(uint32_t id, CoA::Templates::ModelData& out) {   // 0xAD3500
        const CreatureModelDataRec* r = g_creatureModelDataDB.GetRecord(static_cast<int32_t>(id));

        if (!r) {
            return false;
        }

        out.id = r->m_id;
        out.flags = r->m_flags;
        out.modelName = r->m_modelName;
        out.sizeClass = r->m_sizeClass;
        out.modelScale = r->m_modelScale;
        out.bloodId = r->m_bloodID;
        out.footprintTexture = r->m_footprintTextureID;
        out.footprintLength = r->m_footprintTextureLength;
        out.footprintWidth = r->m_footprintTextureWidth;
        out.footprintScale = r->m_footprintParticleScale;
        out.foleyMaterial = r->m_foleyMaterialID;
        out.footstepShake = r->m_footstepShakeSize;
        out.deathThudShake = r->m_deathThudShakeSize;
        out.soundId = r->m_soundID;
        out.collisionWidth = r->m_collisionWidth;
        out.collisionHeight = r->m_collisionHeight;
        out.mountHeight = r->m_mountHeight;
        out.geoBoxMin[0] = r->m_geoBoxMinX;
        out.geoBoxMin[1] = r->m_geoBoxMinY;
        out.geoBoxMin[2] = r->m_geoBoxMinZ;
        out.geoBoxMax[0] = r->m_geoBoxMaxX;
        out.geoBoxMax[1] = r->m_geoBoxMaxY;
        out.geoBoxMax[2] = r->m_geoBoxMaxZ;
        out.worldEffectScale = r->m_worldEffectScale;
        out.attachedEffectScale = r->m_attachedEffectScale;
        out.missileRadius = r->m_missileCollisionRadius;
        out.missilePush = r->m_missileCollisionPush;
        out.missileRaise = r->m_missileCollisionRaise;
        return true;
    }

    bool DisplayInfo(uint32_t id, CoA::Templates::DisplayInfo& out) {   // 0xAD34B8
        const CreatureDisplayInfoRec* r = g_creatureDisplayInfoDB.GetRecord(static_cast<int32_t>(id));

        if (!r) {
            return false;
        }

        out.id = r->m_id;
        out.modelId = r->m_modelID;
        out.soundId = r->m_soundID;
        out.extraId = r->m_extendedDisplayInfoID;
        out.scale = r->m_creatureModelScale;
        out.alpha = r->m_creatureModelAlpha;

        for (int i = 0; i < 3; ++i) {
            out.textures[i] = r->m_textureVariation[i];
        }

        out.portrait = r->m_portraitTextureName;
        out.bloodLevel = r->m_sizeClass;   // +0x28
        out.bloodId = r->m_bloodID;
        out.npcSoundId = r->m_npcsoundID;
        out.particleColorId = r->m_particleColorID;
        out.geosetData = r->m_creatureGeosetData;
        out.effectPackageId = r->m_objectEffectPackageID;
        return true;
    }

    bool DisplayInfoExtra(uint32_t id, CoA::Templates::DisplayInfoExtra& out) {   // 0xAD3494
        const CreatureDisplayInfoExtraRec* r = g_creatureDisplayInfoExtraDB.GetRecord(static_cast<int32_t>(id));

        if (!r) {
            return false;
        }

        out.fields[0] = r->m_id;
        out.fields[1] = r->m_displayRaceID;
        out.fields[2] = r->m_displaySexID;
        out.fields[3] = r->m_skinID;
        out.fields[4] = r->m_faceID;
        out.fields[5] = r->m_hairStyleID;
        out.fields[6] = r->m_hairColorID;
        out.fields[7] = r->m_facialHairID;

        for (int i = 0; i < 11; ++i) {
            out.items[i] = r->m_npcitemDisplay[i];
        }

        out.flags = r->m_flags;
        out.bakeName = r->m_bakeName;
        return true;
    }
}

void CoA::TemplatesClient::Init() {
    CoA::Templates::Deps& d = CoA::Templates::Dependencies();
    d.quest = Quest;
    d.creature = Creature;
    d.modelData = ModelData;
    d.displayInfo = DisplayInfo;
    d.displayInfoExtra = DisplayInfoExtra;
}
