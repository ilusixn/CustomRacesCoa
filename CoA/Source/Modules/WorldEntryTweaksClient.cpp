#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Modules/WorldEntryTweaks.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Source/DB/Rec/SpellShapeshiftFormRec.h"
#include "WoW/Source/Object/ObjectClient/Player_C.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"

namespace {
    bool HasPlayer() {
        return ClntObjMgrGetActivePlayerObj() != nullptr;
    }

    uint8_t PlayerClass() {   // descriptor +0x5C byte 1
        CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();
        return player ? player->m_unit->m_unitFieldBytes0[1] : 0;
    }

    void SetFormFlag(uint32_t form, bool on) {
        if (SpellShapeshiftFormRec* row = g_spellShapeshiftFormDB.GetRecord(static_cast<int32_t>(form))) {
            row->m_flags = on ? (row->m_flags | 2) : (row->m_flags & ~2);
        }
    }
}

void CoA::WorldEntryTweaksClient::Init() {
    CoA::WorldEntryTweaks::Deps& d = CoA::WorldEntryTweaks::Dependencies();
    d.hasPlayer = HasPlayer;
    d.playerClass = PlayerClass;
    d.setFormFlag = SetFormFlag;
}
