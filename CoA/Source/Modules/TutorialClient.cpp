// AscTutorial.cpp's client questions: the active player's race, class, level and spellbook, the completed-achievement
// list and the showTutorials CVar.
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Modules/Tutorial.h"
#include "Engine/Source/ConsoleBase/ConsoleVar.h"
#include "WoW/Source/Object/ObjectClient/Player_C.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"
#include "WoW/Source/Ui/AchievementInfo.h"

namespace {
    bool HasPlayer() {
        return ClntObjMgrGetActivePlayerObj() != nullptr;
    }

    uint8_t PlayerRace() {   // descriptor +0x5C
        CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();
        return player ? player->m_unit->m_unitFieldBytes0[0] : 0;
    }

    uint8_t PlayerClass() {   // descriptor +0x5D
        CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();
        return player ? player->m_unit->m_unitFieldBytes0[1] : 0;
    }

    uint32_t PlayerLevel() {   // descriptor +0xD8
        CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();
        return player ? player->m_unit->m_unitFieldLevel : 0;
    }

    bool KnowsSpell(uint32_t spell) {   // 0x7260E0
        CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();
        return player && player->IsSpellKnown(static_cast<int32_t>(spell));
    }

    bool AchievementDone(uint32_t id) {   // FUN_103087e0 over 0xACFDB0
        for (CLIENT_ACHIEVEMENTS* node = s_completedAchievements.Head(); node; node = s_completedAchievements.Next(node)) {
            if (static_cast<uint32_t>(node->m_id) == id) {
                return true;
            }
        }

        return false;
    }

    bool ShowTutorials() {   // 0x767440("showTutorials"): absent, or a non-zero value (+0x30)
        CVar* cvar = CVar::Lookup("showTutorials");
        return !cvar || cvar->m_intValue != 0;
    }
}

void CoA::TutorialClient::Init() {
    CoA::Tutorial::Deps& deps = CoA::Tutorial::Dependencies();
    deps.hasPlayer = HasPlayer;
    deps.playerRace = PlayerRace;
    deps.playerClass = PlayerClass;
    deps.playerLevel = PlayerLevel;
    deps.knowsSpell = KnowsSpell;
    deps.achievementDone = AchievementDone;
    deps.showTutorials = ShowTutorials;
}
