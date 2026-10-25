#ifndef COA_SOURCE_MODULES_SMALLAPIS_H
#define COA_SOURCE_MODULES_SMALLAPIS_H

#include <cstdint>

// AscSmallApis.cpp's small namespaces. SmallApisClient.cpp has the natives over client objects (C_Sun, C_Item,
// C_QuestLog, C_Taxi's node ids).
namespace CoA::SmallApis {
    // C_LoyaltyPass's "now": local time plus the client's server-time offset (CGQuestLog::m_timeOffset, 0xC23AE8),
    // which SmallApisClient.cpp installs; 0 without it.
    void SetTimeOffsetSource(int32_t (*offset)());
}

#endif
