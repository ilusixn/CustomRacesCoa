// AscGroupFinder.cpp's client questions: the player's level and name, the party member counts and the class token.
#include "CoA/Source/Modules/GroupFinder.h"
#include "CoA/Source/Modules/Modules.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Source/Object/ObjectClient/Player_C.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"
#include "WoW/Source/Ui/PartyFrame.h"

namespace {
    uint32_t PlayerLevel() {   // descriptor +0xD8 = UNIT_FIELD_LEVEL
        CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();
        return player ? player->m_unit->m_unitFieldLevel : 0;
    }

    const char* PlayerName() {   // the object's name (vtable +0xD8)
        CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();
        return player ? player->GetObjectName() : "Unknown";
    }

    // 0xBD1998 / 0xBD199C, read as the original reads them.
    void PartyCounts(uint32_t& a, uint32_t& b) {
        a = CGPartyInfo::m_realNumMembers;
        b = CGPartyInfo::m_lastAssignmentTime;
    }

    const char* ClassToken(uint32_t id) {   // ChrClasses +0x1C (FUN_100b13c0)
        const ChrClassesRec* row = g_chrClassesDB.GetRecord(static_cast<int32_t>(id));
        return row ? row->m_filename : nullptr;
    }
}

void CoA::GroupFinderClient::Init() {
    CoA::GroupFinder::Deps& deps = CoA::GroupFinder::Dependencies();
    deps.playerLevel = PlayerLevel;
    deps.playerName = PlayerName;
    deps.partyCounts = PartyCounts;
    deps.classToken = ClassToken;
}
