// AscBuildCreator.cpp's client questions: the active player (present, class, name), the spell rank chains and the clock.
#include "CoA/Source/Modules/BuildCreator.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Modules/SpellRank.h"
#include "WoW/Source/Object/ObjectClient/Player_C.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"
#include <ctime>

namespace {
    bool HasPlayer() {
        return ClntObjMgrGetActivePlayerObj() != nullptr;
    }

    uint8_t PlayerClass() {   // descriptor +0x5C >> 8
        CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();
        return player ? player->m_unit->m_unitFieldBytes0[1] : 0;
    }

    const char* PlayerName() {   // the object's name (vtable +0xD8)
        CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();
        return player ? player->GetObjectName() : nullptr;
    }

    int64_t Now() {
        return static_cast<int64_t>(_time64(nullptr));
    }
}

void CoA::BuildCreatorClient::Init() {
    CoA::BuildCreator::Deps& deps = CoA::BuildCreator::Dependencies();
    deps.hasPlayer = HasPlayer;
    deps.playerClass = PlayerClass;
    deps.playerName = PlayerName;
    deps.firstRank = CoA::SpellRank::FirstRank;
    deps.rankNumber = CoA::SpellRank::RankNumber;
    deps.rankChain = CoA::SpellRank::Chain;
    deps.now = Now;
}
