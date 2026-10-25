// AscHighRisk.cpp's client questions: the active player's GUID and level.
#include "CoA/Source/Modules/HighRisk.h"
#include "CoA/Source/Modules/Modules.h"
#include "WoW/Source/Object/ObjectClient/Player_C.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"

namespace {
    uint64_t PlayerGuid() {   // 0x4038F0's object, then its GUID
        CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();
        return player ? ClntObjMgrGetActivePlayer().m_guid : 0;
    }

    uint32_t PlayerLevel() {   // descriptor +0xD8 = UNIT_FIELD_LEVEL
        CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();
        return player ? player->m_unit->m_unitFieldLevel : 0;
    }
}

void CoA::HighRiskClient::Init() {
    CoA::HighRisk::Deps& deps = CoA::HighRisk::Dependencies();
    deps.playerGuid = PlayerGuid;
    deps.playerLevel = PlayerLevel;
}
