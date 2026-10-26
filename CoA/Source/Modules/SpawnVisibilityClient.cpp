// AscSpawnVisibility.cpp's client half: the object's type / GUID / entry and the completed quest / achievement lists
// (FUN_103087e0 over 0xACFDEC / 0xACFDB0).
#include "CoA/Source/Modules/SpawnVisibility.h"
#include "CoA/Source/Modules/Modules.h"
#include "WoW/Source/Object/ObjectClient/Object_C.h"
#include "WoW/Source/Ui/AchievementInfo.h"

namespace {
    bool ObjectInfo(void* object, CoA::SpawnVisibility::ObjectInfo& out) {   // +0x14, descriptor +0 / +0xC
        const CGObject_C* o = static_cast<const CGObject_C*>(object);
        out.type = static_cast<uint32_t>(o->m_typeId);
        out.guid = o->m_obj->m_guid.m_guid;
        out.entry = o->m_obj->m_entryID;
        return true;
    }

    bool QuestCompleted(uint32_t quest) {   // s_completedQuests (0xACFDEC), node +8
        for (COMPLETED_QUESTS* node = s_completedQuests.Head(); node; node = s_completedQuests.Next(node)) {
            if (static_cast<uint32_t>(node->m_questId) == quest) {
                return true;
            }
        }

        return false;
    }

    bool AchievementCompleted(uint32_t id) {   // s_completedAchievements (0xACFDB0), node +8
        for (CLIENT_ACHIEVEMENTS* node = s_completedAchievements.Head(); node; node = s_completedAchievements.Next(node)) {
            if (static_cast<uint32_t>(node->m_id) == id) {
                return true;
            }
        }

        return false;
    }
}

void CoA::SpawnVisibilityClient::Init() {
    CoA::SpawnVisibility::Deps& deps = CoA::SpawnVisibility::Dependencies();
    deps.objectInfo = ObjectInfo;
    deps.questCompleted = QuestCompleted;
    deps.achievementCompleted = AchievementCompleted;
}
