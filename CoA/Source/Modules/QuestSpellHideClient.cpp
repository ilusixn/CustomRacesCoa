#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Modules/QuestSpellHide.h"
#include "WoW/Source/Object/ObjectClient/Effect_C.h"
#include "WoW/Source/Ui/QuestLog.h"

namespace {
    // FUN_101940a0: every entry, headers included, by +0 quest id. CoA64: m_quests covers all 64 slots (128 entries).
    bool QuestInLog(uint32_t quest) {
        for (int32_t i = 0; i < CGQuestLog::m_numQuest; ++i) {
            if (static_cast<uint32_t>(CGQuestLog::m_quests[i].m_questId) == quest) {
                return true;
            }
        }

        return false;
    }

    uint32_t EffectSpell(void* effect) {
        return static_cast<CEffect*>(effect)->m_spellId;
    }
}

void CoA::QuestSpellHideClient::Init() {
    CoA::QuestSpellHide::Deps& d = CoA::QuestSpellHide::Dependencies();
    d.questInLog = QuestInLog;
    d.effectSpell = EffectSpell;
}
