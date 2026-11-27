// QuestLogExtended.h's client half: the HELLO on world entry and, after each 0x8F7, the refresh a quest update-field
// change runs (OnUpdateQuest 0x6DF370).
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Modules/QuestLogExtended.h"
#include "CoA/Source/Runtime/ClientHooks.h"
#include "CoA/Source/Runtime/Events.h"
#include "WoW/Source/Object/ObjectClient/Player_C.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"
#include "WoW/Source/Ui/QuestLog.h"

namespace {
    // OnUpdateQuest once for the whole packet (quest-giver status, CGQuestLog::Update), then its per-slot half for
    // each changed slot. One QUEST_LOG_UPDATE: Update(1)'s, or ours when it returns early for pending quest records.
    void Refresh() {
        CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();

        if (!player) {
            return;
        }

        CGPlayer_C::QueryQuestGiverStatusMultiple();
        const bool pending = CGQuestLog::PendingQuestRecords() != 0;
        CGQuestLog::Update(1);

        if (pending) {
            CoA::Events::Signal("QUEST_LOG_UPDATE", nullptr);
        }

        // World entry's LoadTrackedQuests ran before slots 25..63 arrived, so their saved watches were refused. Load
        // them again (AddQuestWatch skips duplicates), or let Update(1) do it once the pending quest records arrive.
        if (CoA::QuestLogExtended::FirstSinceClear()) {
            if (CGQuestLog::PendingQuestRecords()) {
                CGQuestLog::m_trackedQuestsLoaded = 0;
            } else {
                CGQuestLog::LoadTrackedQuests();
            }
        }

        for (const CoA::QuestLogExtended::Change& c : CoA::QuestLogExtended::Changes()) {
            PlayerQuestLogEntry old = PlayerQuestLogEntry();
            old.m_questId = c.old.questId;
            old.m_state = c.old.state;

            for (uint32_t i = 0; i < 4; ++i) {
                old.m_counts[i] = c.old.counters[i];
            }

            old.m_time = c.old.time;
            QuestLogSlotUpdated(player, c.slot, &old);
        }
    }
}

void CoA::QuestLogExtendedClient::Init() {
    CoA::ClientHooks::afterEnterWorld.Add(CoA::QuestLogExtended::SendHello);
    CoA::QuestLogExtended::OnChanged(Refresh);
}
