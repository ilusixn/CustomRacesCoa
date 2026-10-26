// AscSmallHooks.cpp's client half: the player's cast end, the clock and the banker GUID.
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Modules/SmallHooks.h"
#include "Engine/Source/Os/TimeManager.h"
#include "WoW/Source/Object/ObjectClient/Player_C.h"
#include "WoW/Source/Object/ObjectClient/Unit_C.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"
#include "WoW/Source/Ui/CGBankInfo.h"

namespace {
    // AscScript::ActivePlayer (0x4038F0) +0xA7C.
    bool CastEndTime(uint32_t& out) {
        CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();

        if (!player) {
            return false;
        }

        out = player->m_castEndTime;
        return true;
    }

    uint64_t BankerGuid() {
        return CGBankInfo::m_banker.m_guid;
    }

    void SetBankerGuid(uint64_t guid) {
        CGBankInfo::m_banker.m_guid = guid;
    }

    uint64_t ActivePlayerGuid() {
        return ClntObjMgrGetActivePlayer().m_guid;
    }
}

void CoA::SmallHooksClient::Init() {
    CoA::SmallHooks::Deps& d = CoA::SmallHooks::Dependencies();
    d.castEndTime = CastEndTime;
    d.now = OsGetAsyncTimeMs;
    d.bankerGuid = BankerGuid;
    d.setBankerGuid = SetBankerGuid;
    d.activePlayerGuid = ActivePlayerGuid;
}
