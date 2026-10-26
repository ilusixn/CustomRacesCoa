// AscCallboardCache.cpp's client question: the Callboard cache level, the progress counter of criteria 0x13901
// (FUN_100b11b0 on Achievement_Criteria.dbc 0xAD3080, then 0x5B2350 over the criteria list; the node's +0x10).
#include "CoA/Source/Modules/Callboard.h"
#include "CoA/Source/Modules/Modules.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Source/DB/Rec/Achievement_CriteriaRec.h"
#include "WoW/Source/Ui/AchievementInfo.h"

namespace {
    const int32_t kCacheCriteria = 0x13901;

    uint32_t CacheLevel() {
        if (!g_achievement_CriteriaDB.GetRecord(kCacheCriteria)) {
            return 0;
        }

        for (CLIENT_ACHIEVEMENT_CRITERIA* node = s_criteria.Head(); node; node = s_criteria.Next(node)) {   // 0x5B2350
            if (node->m_id == kCacheCriteria) {
                return static_cast<uint32_t>(node->m_counter);
            }
        }

        return 0;
    }
}

void CoA::CallboardClient::Init() {
    CoA::CallboardCache::Dependencies().cacheLevel = CacheLevel;
}
