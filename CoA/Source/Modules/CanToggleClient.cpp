// AscCanToggle.cpp's client questions: the active player, its level and class, and its auras.
#include "CoA/Source/Modules/CanToggle.h"
#include "CoA/Source/Modules/Modules.h"
#include "WoW/Source/Object/ObjectClient/Player_C.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"

namespace {
    bool HasPlayer() {
        return ClntObjMgrGetActivePlayerObj() != nullptr;
    }

    uint32_t PlayerLevel() {   // descriptor +0xD8 = UNIT_FIELD_LEVEL
        CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();
        return player ? player->m_unit->m_unitFieldLevel : 0;
    }

    uint8_t PlayerClass() {   // descriptor +0x5C byte 1
        CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();
        return player ? player->m_unit->m_unitFieldBytes0[1] : 0;
    }

    bool HasAura(uint32_t spell) {   // 0x7282A0
        CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();
        return player && player->HasAuraBySpellId(static_cast<int32_t>(spell));
    }
}

void CoA::CanToggleClient::Init() {
    CoA::CanToggle::Deps& deps = CoA::CanToggle::Dependencies();
    deps.hasPlayer = HasPlayer;
    deps.playerLevel = PlayerLevel;
    deps.playerClass = PlayerClass;
    deps.hasAura = HasAura;
}
