// AscChallenge.cpp's client questions: the active player (class, level, GUID), the party (size and leader), the
// completed-achievement list and Achievement.dbc flags, the current map and instance difficulty, spell attributes, the
// world-map position of a failure, the "player" unit token and the clock.
#include "CoA/Source/Modules/Challenge.h"
#include "CoA/Source/Modules/Modules.h"
#include "Engine/Source/Tempest/C3Vector.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Source/DB/Rec/AchievementRec.h"
#include "WoW/Source/DB/Rec/SpellRec.h"
#include "WoW/Source/Object/ObjectClient/Player_C.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"
#include "WoW/Source/Ui/AchievementInfo.h"
#include "WoW/Source/Ui/GameUI.h"
#include "WoW/Source/Ui/PartyFrame.h"
#include "WoW/Source/Ui/ScriptEvents.h"
#include "WoW/Source/Ui/WorldMap.h"
#include <ctime>

namespace {
    bool HasPlayer() {
        return ClntObjMgrGetActivePlayerObj() != nullptr;
    }

    uint8_t PlayerClass() {   // descriptor +0x5D
        CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();
        return player ? player->m_unit->m_unitFieldBytes0[1] : 0;
    }

    uint32_t PlayerLevel() {   // descriptor +0xD8
        CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();
        return player ? player->m_unit->m_unitFieldLevel : 0;
    }

    uint64_t ActivePlayerGuid() {   // 0x4D3790
        return ClntObjMgrGetActivePlayer().m_guid;
    }

    uint32_t PartyMembers() {   // 0x52BD10
        return CGPartyInfo::GetNumPartyMembers();
    }

    uint64_t PartyLeader() {   // 0xBD1968
        return CGPartyInfo::m_leader.m_guid;
    }

    bool AchievementDone(uint32_t id) {   // FUN_103087e0 over 0xACFDB0
        for (CLIENT_ACHIEVEMENTS* node = s_completedAchievements.Head(); node; node = s_completedAchievements.Next(node)) {
            if (static_cast<uint32_t>(node->m_id) == id) {
                return true;
            }
        }

        return false;
    }

    bool AchievementFlags(uint32_t id, uint32_t& flags) {   // FUN_100b1150 on 0xAD305C, +0x24
        const AchievementRec* rec = g_achievementDB.GetRecord(static_cast<int32_t>(id));

        if (!rec) {
            return false;
        }

        flags = static_cast<uint32_t>(rec->m_flags);
        return true;
    }

    int32_t CurrentMap() {   // 0xBD088C
        return CGGameUI::m_iCurrentMapID;
    }

    int32_t InstanceDifficulty() {   // 0xBD0894
        return static_cast<int32_t>(CGGameUI::m_instanceDifficulty);
    }

    bool SpellAttributes(uint32_t spell, uint32_t& attributes) {   // FetchSpell, +0x10
        SpellRec rec;

        if (!g_spellDB.GetRecord(static_cast<int32_t>(spell), &rec)) {
            return false;
        }

        attributes = static_cast<uint32_t>(rec.m_attributes);
        return true;
    }

    void WorldPosition(const float pos[3], uint32_t map, float& x, float& y) {   // 0x544140(pos, map, &x, &y, 0, 0, 0)
        const C3Vector p(pos[0], pos[1], pos[2]);
        CGWorldMap::GetWorldPosition(p, static_cast<int32_t>(map), &x, &y, nullptr, false, 0);
    }

    bool UnitTokenGuid(const char* token, uint64_t& guid) {
        WOWGUID g;
        const bool found = Script_GetGUIDFromToken(token, g, false);
        guid = found ? g.m_guid : 0;
        return found;
    }

    int64_t Now() {
        return static_cast<int64_t>(time(nullptr));
    }
}

void CoA::ChallengeClient::Init() {
    CoA::Challenge::Deps& deps = CoA::Challenge::Dependencies();
    deps.hasPlayer = HasPlayer;
    deps.playerClass = PlayerClass;
    deps.playerLevel = PlayerLevel;
    deps.activePlayerGuid = ActivePlayerGuid;
    deps.partyMembers = PartyMembers;
    deps.partyLeader = PartyLeader;
    deps.achievementDone = AchievementDone;
    deps.achievementFlags = AchievementFlags;
    deps.currentMap = CurrentMap;
    deps.instanceDifficulty = InstanceDifficulty;
    deps.spellAttributes = SpellAttributes;
    deps.worldPosition = WorldPosition;
    deps.unitTokenGuid = UnitTokenGuid;
    deps.now = Now;
}
