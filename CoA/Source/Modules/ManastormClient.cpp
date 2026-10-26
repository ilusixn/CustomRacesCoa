// AscManastorm.cpp's client questions: the active player, the party / raid lists, spell ranks, Spell.dbc, SpellRange.dbc,
// unit tokens, the raid-icon quad and the objects the pre-animate hook sees.
#include "CoA/Source/Modules/Manastorm.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Modules/SpellRank.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Source/DB/Rec/SpellRec.h"
#include "WoW/Source/Game/GameClient/PlayerName.h"
#include "WoW/Source/Object/ObjectClient/Player_C.h"
#include "WoW/Source/Object/ObjectClient/Unit_C.h"
#include "WoW/Source/Object/Types.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"
#include "WoW/Source/Ui/GameUI.h"
#include "WoW/Source/Ui/PartyFrame.h"
#include "WoW/Source/Ui/RaidInfo.h"
#include "WoW/Source/Ui/ScriptEvents.h"

uint32_t GetRaidTargetIndexFromGuid(WOWGUID guid);

namespace {
    bool HasPlayer() {   // 0x4038F0
        return ClntObjMgrGetActivePlayerObj() != nullptr;
    }

    uint64_t PlayerGuid() {   // 0x4D3790
        return ClntObjMgrGetActivePlayer().m_guid;
    }

    uint32_t PlayerLevel() {   // descriptor +0xD8
        CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();
        return player ? player->m_unit->m_unitFieldLevel : 0;
    }

    bool PlayerHasAura(uint32_t spell) {   // 0x7282A0
        CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();
        return player && player->HasAuraBySpellId(static_cast<int32_t>(spell));
    }

    bool KnowsSpell(uint32_t spell) {   // 0x7260E0
        CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();
        return player && player->IsSpellKnown(static_cast<int32_t>(spell));
    }

    const std::vector<uint32_t>* RankChain(uint32_t first) {
        return &CoA::SpellRank::Chain(first);
    }

    uint64_t PartyGuid(uint32_t index) {   // 0xBD1948
        return CGPartyInfo::m_members[index].m_guid;
    }

    uint32_t RaidCount() {   // 0xBEB608
        return CGRaidInfo::m_numMembers;
    }

    // The original reads 8-byte entries from 0xBEB568, which is CGRaidInfo::m_members (40 x 4-byte pointers): entry i
    // is the pointer pair 2i / 2i + 1, never a real guid. Past entry 19 it runs off the array into m_numMembers and the
    // globals after it; those entries count as present here.
    uint64_t RaidGuid(uint32_t index) {
        if (index >= 20) {
            return 1;
        }

        CGRaidMember* a = CGRaidInfo::m_members[index * 2];
        CGRaidMember* b = CGRaidInfo::m_members[index * 2 + 1];
        return reinterpret_cast<uint64_t>(a ? a : b);
    }

    bool SpellBasePoints(uint32_t spell, int32_t& first, int32_t& second) {   // FetchSpell +0x140 / +0x144
        SpellRec rec;

        if (!g_spellDB.GetRecord(static_cast<int32_t>(spell), &rec)) {
            return false;
        }

        first = rec.m_effectBasePoints[0];
        second = rec.m_effectBasePoints[1];
        return true;
    }

    uint32_t InstanceDifficulty() {   // 0xBD0894
        return CGGameUI::m_instanceDifficulty;
    }

    void UnitTokens(uint64_t guid, std::vector<std::string>& out) {   // 0x60BB70
        WOWGUID g;
        g.m_guid = guid;
        int32_t count = 0;
        char** tokens = Script_GetTokenIDsFromGUID(g, &count);

        for (int32_t i = 0; tokens && i < count; ++i) {
            out.push_back(tokens[i]);
        }
    }

    bool UnitTokenGuid(const char* token, uint64_t& guid) {
        WOWGUID g;
        const bool found = Script_GetGUIDFromToken(token, g, false);
        guid = found ? g.m_guid : 0;
        return found;
    }

    void SetSpellRangeFlags(uint32_t flags) {   // FUN_100b2590 rows 0x72 / 0x1BD, +0x14
        for (int32_t id : { 0x72, 0x1BD }) {
            if (SpellRangeRec* row = g_spellRangeDB.GetRecord(id)) {
                row->m_flags = static_cast<int32_t>(flags);
            }
        }
    }

    void SetObjectiveIconScale(float scale) {   // FUN_102a6c10
        PlayerNameSetIconScale(scale);
    }

    int32_t RaidTargetIndex(void* object) {   // 0x5728C0 on the object's guid
        return static_cast<int32_t>(GetRaidTargetIndexFromGuid(static_cast<CGObject_C*>(object)->m_obj->m_guid));
    }

    uint32_t ObjectType(void* object) {   // +0x14
        return static_cast<uint32_t>(static_cast<CGObject_C*>(object)->m_typeId);
    }

    bool Positions(void* object, float player[3], float other[3]) {   // vtable +0x2C on both
        CGPlayer_C* p = ClntObjMgrGetActivePlayerObj();

        if (!p) {
            return false;
        }

        const C3Vector a = p->GetPosition();
        const C3Vector b = static_cast<CGObject_C*>(object)->GetPosition();
        player[0] = a.x;
        player[1] = a.y;
        player[2] = a.z;
        other[0] = b.x;
        other[1] = b.y;
        other[2] = b.z;
        return true;
    }
}

void CoA::ManastormClient::Init() {
    CoA::Manastorm::Deps& d = CoA::Manastorm::Dependencies();
    d.hasPlayer = HasPlayer;
    d.playerGuid = PlayerGuid;
    d.playerLevel = PlayerLevel;
    d.playerHasAura = PlayerHasAura;
    d.knowsSpell = KnowsSpell;
    d.firstRank = CoA::SpellRank::FirstRank;
    d.rankChain = RankChain;
    d.partyGuid = PartyGuid;
    d.raidCount = RaidCount;
    d.raidGuid = RaidGuid;
    d.spellBasePoints = SpellBasePoints;
    d.instanceDifficulty = InstanceDifficulty;
    d.unitTokens = UnitTokens;
    d.unitTokenGuid = UnitTokenGuid;
    d.setSpellRangeFlags = SetSpellRangeFlags;
    d.setObjectiveIconScale = SetObjectiveIconScale;
    d.raidTargetIndex = RaidTargetIndex;
    d.objectType = ObjectType;
    d.positions = Positions;
}
