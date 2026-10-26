// AscInitHooks.cpp's client half: the quest log and the player GUID for the quest detours. The other detours are their
// client sites (markers there).
#include "CoA/Source/Modules/InitHooks.h"
#include "CoA/Source/Modules/Modules.h"
#include "WoW/Source/Object/ObjectClient/Player_C.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"

namespace {
    // The active player's quest log (descriptor +0x278, stride 0x14); false without one (0x4038F0).
    bool QuestLogQuest(uint32_t slot, uint32_t& questId) {
        CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();

        if (!player) {
            return false;
        }

        PlayerQuestLogEntry* entry = player->GetQuestLogEntry(slot);
        questId = entry ? entry->m_questId : 0;
        return true;
    }

    uint64_t ActivePlayerGuid() {   // AscScript::ActivePlayerGuid
        return ClntObjMgrGetActivePlayer().m_guid;
    }
}

void CoA::InitHooksClient::Init() {
    CoA::InitHooks::Deps& d = CoA::InitHooks::Dependencies();
    d.questLogQuest = QuestLogQuest;
    d.activePlayerGuid = ActivePlayerGuid;
}
