// AscDraftMode.cpp / AscScrollOfFortune.cpp's client questions: the active player and its level, the Marks of Ascension
// in its currency-token slots, and the clock.
#include "CoA/Source/Modules/CA.h"
#include "CoA/Source/Modules/DraftMode.h"
#include "CoA/Source/Modules/Modules.h"
#include "WoW/Source/Object/ObjectClient/Player_C.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"
#include <ctime>

namespace {
    bool HasPlayer() {
        return ClntObjMgrGetActivePlayerObj() != nullptr;
    }

    uint32_t PlayerLevel() {   // descriptor +0xD8 = UNIT_FIELD_LEVEL
        CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();
        return player ? player->m_unit->m_unitFieldLevel : 0;
    }

    uint32_t ItemCount(uint32_t entry) {   // FUN_10308570, as CA counts it
        return CoA::CA::UnitItemCount(ClntObjMgrGetActivePlayer().m_guid, entry);
    }

    int64_t Now() {
        return _time64(nullptr);
    }
}

void CoA::DraftModeClient::Init() {
    CoA::DraftMode::Deps& deps = CoA::DraftMode::Dependencies();
    deps.hasPlayer = HasPlayer;
    deps.playerLevel = PlayerLevel;
    deps.itemCount = ItemCount;
    deps.now = Now;
}
