// AscAnimationCount.cpp's client half: FUN_10a51180 after ClientDBInitialize -- AnimationData's loaded row count
// (container 0xAD30C8 +8) for every site, the Engine's included.
#include "CoA/Source/Modules/AnimationCount.h"
#include "CoA/Source/Modules/BridgeStoreClient.h"
#include "Engine/Source/Model2/M2Model.h"
#include "WoW/Source/DB/DBClient/DBClient.h"

void CoA::BridgeStoreClient::ApplyAnimationCount() {
    CoA::AnimationCount::Set(static_cast<uint32_t>(g_animationDataDB.m_numRecords));
    g_animationCount = CoA::AnimationCount::Rows();
}
