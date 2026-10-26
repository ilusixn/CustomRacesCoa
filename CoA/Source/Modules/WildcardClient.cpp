// C_Wildcard's client questions (AscWildcardRolls.cpp / AscWildcardRapid.cpp): the active player (present, level,
// money, its Marks of Ascension), the clock, the spell rank chains, and the spellbook add 0x542030 the mode manager's
// detour (AscRuntime.cpp 0x10275ED0) wraps.
#include "CoA/Source/Modules/CA.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Modules/SpellRank.h"
#include "CoA/Source/Modules/Wildcard.h"
#include "CoA/Source/Runtime/ClientHooks.h"
#include "WoW/Source/Object/ObjectClient/Player_C.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"
#include "WoW/Source/Ui/SpellBookFrame.h"
#include <windows.h>

namespace {
    bool HasPlayer() {
        return ClntObjMgrGetActivePlayerObj() != nullptr;
    }

    uint32_t PlayerLevel() {   // descriptor +0xD8 = UNIT_FIELD_LEVEL
        CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();
        return player ? player->m_unit->m_unitFieldLevel : 0;
    }

    uint32_t PlayerMoney() {   // PLAYER_FIELD_COINAGE (+0x1248)
        CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();
        return player ? static_cast<const CGPlayerLocalData*>(player->m_player)->m_playerFieldCoinage : 0;
    }

    uint32_t ItemCount(uint32_t entry) {   // FUN_10308570, as CA counts it
        return CoA::CA::UnitItemCount(ClntObjMgrGetActivePlayer().m_guid, entry);
    }

    uint32_t TickCount() {
        return GetTickCount();
    }

    void AddKnownSpell(uint32_t spell, uint32_t notify, uint32_t replace) {   // the detour's trampoline into 0x542030
        CGSpellBook::AddKnownSpell(static_cast<int32_t>(spell), static_cast<int32_t>(notify), static_cast<int32_t>(replace));
    }

    bool AddKnownSpellHook(int32_t spell, int32_t notify, int32_t replace) {
        return CoA::WildcardRolls::AddKnownSpellHook(static_cast<uint32_t>(spell), static_cast<uint32_t>(notify),
            static_cast<uint32_t>(replace));
    }
}

void CoA::WildcardClient::Init() {
    CoA::Wildcard::Deps& deps = CoA::Wildcard::Dependencies();
    deps.hasPlayer = HasPlayer;
    deps.playerLevel = PlayerLevel;
    deps.playerMoney = PlayerMoney;
    deps.itemCount = ItemCount;
    deps.tickCount = TickCount;
    deps.firstRank = CoA::SpellRank::FirstRank;
    deps.addKnownSpell = AddKnownSpell;
    CoA::ClientHooks::addKnownSpell = AddKnownSpellHook;
}
