// AscLootLockout.cpp's client questions: unit tokens, the DungeonEncounter / SpellIcon / Map tables and a unit's entry.
#include "CoA/Source/Modules/LootLockout.h"
#include "CoA/Source/Modules/Modules.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Source/Object/ObjectClient/Unit_C.h"
#include "WoW/Source/Object/Types.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"
#include "WoW/Source/Ui/ScriptEvents.h"

namespace {
    uint64_t TokenGuid(const char* token) {   // FUN_103086c0 -> 0x60C1C0
        return GetGuidByKeyword(token).m_guid;
    }

    bool Encounter(uint32_t id, CoA::LootLockout::Encounter& out) {   // 0x100B1600 on 0xAD36B0
        const DungeonEncounterRec* row = g_dungeonEncounterDB.GetRecord(static_cast<int32_t>(id));

        if (!row) {
            return false;
        }

        out.id = static_cast<uint32_t>(row->m_id);
        out.map = row->m_mapID;
        out.difficulty = row->m_difficulty;
        out.order = row->m_orderIndex;
        out.name = row->m_name;
        out.icon = static_cast<uint32_t>(row->m_spellIconID);
        return true;
    }

    uint32_t EncounterMinId() {   // container +0x10
        return static_cast<uint32_t>(g_dungeonEncounterDB.m_minID);
    }

    uint32_t EncounterMaxId() {   // container +0x0C
        return static_cast<uint32_t>(g_dungeonEncounterDB.m_maxID);
    }

    const char* SpellIcon(uint32_t id) {   // SpellIcon.dbc +4
        const SpellIconRec* row = g_spellIconDB.GetRecord(static_cast<int32_t>(id));
        return row ? row->m_textureFilename : nullptr;
    }

    bool MapInstanceType(uint32_t map, int32_t& type) {   // Map.dbc +8
        const MapRec* row = g_mapDB.GetRecord(static_cast<int32_t>(map));

        if (!row) {
            return false;
        }

        type = row->m_instanceType;
        return true;
    }

    bool UnitEntry(const char* token, uint32_t& entry) {   // FUN_103086d0: the token's unit, descriptor +0xC
        WOWGUID guid = GetGuidByKeyword(token);
        CGUnit_C* unit = static_cast<CGUnit_C*>(ClntObjMgrObjectPtr(guid, TYPE_UNIT, __FILE__, __LINE__));

        if (!unit) {
            return false;
        }

        entry = static_cast<uint32_t>(unit->m_obj->m_entryID);
        return true;
    }
}

void CoA::LootLockoutClient::Init() {
    CoA::LootLockout::Deps& d = CoA::LootLockout::Dependencies();
    d.tokenGuid = TokenGuid;
    d.encounter = Encounter;
    d.encounterMinId = EncounterMinId;
    d.encounterMaxId = EncounterMaxId;
    d.spellIcon = SpellIcon;
    d.mapInstanceType = MapInstanceType;
    d.unitEntry = UnitEntry;
}
