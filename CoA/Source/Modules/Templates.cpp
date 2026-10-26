// AscTemplates.cpp (see Templates.h): the quest / creature template and creature DBC readers.
#include "CoA/Source/Modules/Templates.h"
#include "CoA/Source/Modules/Json.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Script.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <cstring>
#include <string>

namespace {
    using json = nlohmann::json;

    CoA::Templates::Deps s_deps;

    uint32_t ArgId(lua_State* L) {   // luaL_checknumber(L, 1) through _ftol2
        return static_cast<uint32_t>(CoA::Script::ToInt(CoA::Script::CheckNumber(L, 1)));
    }

    std::string Str(const char* s) {
        return s ? s : "";
    }

    template <class T>
    json Array(const T* values, int n) {
        json a = json::array();

        for (int i = 0; i < n; ++i) {
            a.push_back(values[i]);
        }

        return a;
    }

    // The quest record read at the original's offsets; bytes past the client record's end read 0.
    struct QuestBytes {
        const uint8_t* rec;
        uint32_t size;

        uint8_t Byte(uint32_t off) const {
            return off < this->size ? this->rec[off] : 0;
        }

        int32_t I32(uint32_t off) const {
            uint32_t v = 0;

            for (uint32_t i = 0; i < 4; ++i) {
                v |= static_cast<uint32_t>(this->Byte(off + i)) << (i * 8);
            }

            return static_cast<int32_t>(v);
        }

        float F32(uint32_t off) const {
            const int32_t bits = this->I32(off);
            float v;
            memcpy(&v, &bits, 4);
            return v;
        }

        // std::string(first, first + n) pushed through c_str(): up to the first NUL inside n.
        std::string Fixed(uint32_t off, uint32_t n) const {
            std::string s;

            for (uint32_t i = 0; i < n; ++i) {
                const char c = static_cast<char>(this->Byte(off + i));

                if (c == '\0') {
                    break;
                }

                s.push_back(c);
            }

            return s;
        }

        json Ints(uint32_t off, int n) const {
            json a = json::array();

            for (int i = 0; i < n; ++i) {
                a.push_back(this->I32(off + static_cast<uint32_t>(i) * 4));
            }

            return a;
        }
    };

    // FUN_1021ea50 / FUN_1021ecb0.
    int GetQuestTemplate(lua_State* L) {
        uint32_t size = 0;
        const uint8_t* rec = s_deps.quest ? s_deps.quest(ArgId(L), size) : nullptr;

        if (!rec) {
            return 0;
        }

        const QuestBytes q { rec, size };
        json j = json::object();
        static const struct { const char* key; uint32_t off; } kInts[] = {
            { "QuestID", 0x00 }, { "QuestMethod", 0x04 }, { "QuestLevel", 0x08 }, { "QuestMinLevel", 0x0C },
            { "QuestSortID", 0x10 }, { "QuestType", 0x14 }, { "SuggestedGroupNum", 0x18 },
            { "FriendlyFactionID", 0x1C }, { "FriendlyFactionAmount", 0x20 }, { "HostileFactionID", 0x24 },
            { "HostileFactionAmount", 0x28 }, { "RewardNextQuest", 0x2C }, { "RewardXPDifficulty", 0x30 },
            { "RewardMoney", 0x34 }, { "RewardBonusMoney", 0x38 }, { "RewardDisplaySpell", 0x3C },
            { "RewardSpell", 0x40 }, { "RewardHonor", 0x44 }, { "StartItem", 0x4C }, { "Flags", 0x50 },
            { "POIContinent", 0xA4 }, { "POIPriority", 0xB0 }, { "RewardTitleId", 0x2494 },
            { "RequiredPlayerKills", 0x2498 }, { "RewardTalents", 0x249C }, { "RewardArenaPoints", 0x24A0 },
            { "Unknown", 0x2CE0 },
        };

        for (const auto& f : kInts) {
            j[f.key] = q.I32(f.off);
        }

        j["RewardKillHonor"] = static_cast<double>(q.F32(0x48));
        j["POIx"] = static_cast<double>(q.F32(0xA8));
        j["POIy"] = static_cast<double>(q.F32(0xAC));
        j["RewardItems"] = q.Ints(0x54, 4);
        j["RewardAmount"] = q.Ints(0x64, 4);
        j["ChoiceItems"] = q.Ints(0x74, 6);
        j["ChoiceItemAmount"] = q.Ints(0x8C, 6);
        j["Title"] = q.Fixed(0xB4, 0x200);
        j["Objectives"] = q.Fixed(0x2B4, 0xBB8);
        j["Details"] = q.Fixed(0xE6C, 0xBB8);
        j["AreaDescription"] = q.Fixed(0x1A24, 0x200);
        j["RequiredNpcOrGo"] = q.Ints(0x1C24, 4);
        j["RequiredNpcOrGoCount"] = q.Ints(0x1C34, 4);
        j["RequiredItemId"] = q.Ints(0x1C44, 6);
        j["RequiredItemCount"] = q.Ints(0x1C5C, 6);
        j["ItemDrop"] = q.Ints(0x1C74, 4);
        j["ItemDropCount"] = q.Ints(0x1C84, 4);
        json objectives = json::array();   // FUN_1021e970

        for (uint32_t i = 0; i < 4; ++i) {
            objectives.push_back(q.Fixed(0x1C94 + i * 0x200, 0x200));
        }

        j["ObjectiveText"] = objectives;
        j["CompletedText"] = q.Fixed(0x24A4, 0x800);
        j["RewardFactionValue"] = q.Ints(0x2CA4, 5);
        j["RewardFactionID"] = q.Ints(0x2CB8, 5);
        j["RewardFactionValueOverride"] = q.Ints(0x2CCC, 5);
        CoA::Json::Push(L, j);
        return 1;
    }

    // FUN_1021b200 / FUN_1021b2d0.
    int GetCreatureTemplate(lua_State* L) {
        CoA::Templates::Creature c;

        if (!s_deps.creature || !s_deps.creature(ArgId(L), c)) {
            return 0;
        }

        json j = json::object();
        j["ID"] = c.id;
        j["SubName"] = Str(c.subName);
        j["IconName"] = Str(c.iconName);
        j["Flags"] = c.flags;
        j["Type"] = c.type;
        j["Family"] = c.family;
        j["Rank"] = c.rank;
        j["ProxyCreatureID"] = Array(c.proxy, 2);
        j["DisplayID"] = Array(c.display, 4);
        j["HealthModifier"] = static_cast<double>(c.health);
        j["PowerModifier"] = static_cast<double>(c.power);
        j["RacialLeader"] = c.racialLeader;
        j["QuestItem"] = Array(c.questItem, 6);
        j["MovementID"] = c.movement;
        j["Name"] = Str(c.name);
        CoA::Json::Push(L, j);
        return 1;
    }

    // FUN_100b0ef0 / FUN_100b39f0. GeoBoxMax* read the GeoBoxMin* columns in the original.
    int GetCreatureModelData(lua_State* L) {
        CoA::Templates::ModelData m;

        if (!s_deps.modelData || !s_deps.modelData(ArgId(L), m)) {
            return 0;
        }

        json j = json::object();
        j["ID"] = m.id;
        j["Flags"] = m.flags;
        j["ModelName"] = Str(m.modelName);
        j["SizeClass"] = m.sizeClass;
        j["ModelScale"] = static_cast<double>(m.modelScale);
        j["BloodID"] = m.bloodId;
        j["FootprintTextureID"] = m.footprintTexture;
        j["FootprintTextureLength"] = static_cast<double>(m.footprintLength);
        j["FootprintTextureWidth"] = static_cast<double>(m.footprintWidth);
        j["FootprintParticleScale"] = static_cast<double>(m.footprintScale);
        j["FoleyMaterialID"] = m.foleyMaterial;
        j["FootstepShakeSize"] = m.footstepShake;
        j["DeathThudShakeSize"] = m.deathThudShake;
        j["SoundID"] = m.soundId;
        j["CollisionWidth"] = static_cast<double>(m.collisionWidth);
        j["CollisionHeight"] = static_cast<double>(m.collisionHeight);
        j["MountHeight"] = static_cast<double>(m.mountHeight);
        j["GeoBoxMinX"] = static_cast<double>(m.geoBoxMin[0]);
        j["GeoBoxMinY"] = static_cast<double>(m.geoBoxMin[1]);
        j["GeoBoxMinZ"] = static_cast<double>(m.geoBoxMin[2]);
        j["GeoBoxMaxX"] = static_cast<double>(m.geoBoxMin[0]);
        j["GeoBoxMaxY"] = static_cast<double>(m.geoBoxMin[1]);
        j["GeoBoxMaxZ"] = static_cast<double>(m.geoBoxMin[2]);
        j["WorldEffectScale"] = static_cast<double>(m.worldEffectScale);
        j["AttachedEffectScale"] = static_cast<double>(m.attachedEffectScale);
        j["MissileCollisionRadius"] = static_cast<double>(m.missileRadius);
        j["MissileCollisionPush"] = static_cast<double>(m.missilePush);
        j["MissileCollisionRaise"] = static_cast<double>(m.missileRaise);
        CoA::Json::Push(L, j);
        return 1;
    }

    // FUN_100b0d70 / FUN_100b2d20.
    int GetCreatureDisplayInfo(lua_State* L) {
        CoA::Templates::DisplayInfo d;

        if (!s_deps.displayInfo || !s_deps.displayInfo(ArgId(L), d)) {
            return 0;
        }

        json j = json::object();
        j["ID"] = d.id;
        j["ModelID"] = d.modelId;
        j["SoundID"] = d.soundId;
        j["ExtendedDisplayInfoID"] = d.extraId;
        j["CreatureModelScale"] = static_cast<double>(d.scale);
        j["CreatureModelAlpha"] = d.alpha;
        j["TextureVariation"] = json::array({ Str(d.textures[0]), Str(d.textures[1]), Str(d.textures[2]) });
        j["PortraitTextureName"] = Str(d.portrait);
        j["BloodLevel"] = d.bloodLevel;
        j["BloodID"] = d.bloodId;
        j["NpcSoundID"] = d.npcSoundId;
        j["ParticleColorID"] = d.particleColorId;
        j["CreatureGeosetData"] = d.geosetData;
        j["ObjectEffectPackageID"] = d.effectPackageId;
        CoA::Json::Push(L, j);
        return 1;
    }

    // FUN_100b0e30 / FUN_100b3350.
    int GetCreatureDisplayInfoExtra(lua_State* L) {
        CoA::Templates::DisplayInfoExtra e;

        if (!s_deps.displayInfoExtra || !s_deps.displayInfoExtra(ArgId(L), e)) {
            return 0;
        }

        static const char* const kInts[] = { "ID", "DisplayRaceID", "DisplaySexID", "SkinID", "FaceID", "HairStyleID",
            "HairColorID", "FacialHairID" };
        json j = json::object();

        for (uint32_t i = 0; i < 8; ++i) {
            j[kInts[i]] = e.fields[i];
        }

        j["NPCItemDisplay"] = Array(e.items, 11);
        j["Flags"] = e.flags;
        j["BakeName"] = Str(e.bakeName);
        CoA::Json::Push(L, j);
        return 1;
    }

    const CoA::Binding s_bindings[] = {
        COA_NATIVE("", "GetCreatureDisplayInfo", GetCreatureDisplayInfo),
        COA_NATIVE("", "GetCreatureDisplayInfoExtra", GetCreatureDisplayInfoExtra),
        COA_NATIVE("", "GetCreatureModelData", GetCreatureModelData),
        COA_NATIVE("", "GetCreatureTemplate", GetCreatureTemplate),
        COA_NATIVE("", "GetQuestTemplate", GetQuestTemplate),
    };
}

CoA::Templates::Deps& CoA::Templates::Dependencies() {
    return s_deps;
}

void CoA::Templates::Init() {
    CoA::LuaReg::AddModule(s_bindings, sizeof(s_bindings) / sizeof(s_bindings[0]));
}

void CoA::Templates::Reset() {
    s_deps = Deps();
}
