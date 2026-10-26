// AscAlternatePower.cpp's client questions: an object's guid, the forced death (0x4D4DB0 / 0x729220) and the jump cast's
// player, aura, spell record, falling flag and cast (0x7282A0 / 0x802F80 / 0x80DA40).
#include "CoA/Source/Modules/AlternatePower.h"
#include "CoA/Source/Modules/Modules.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Source/DB/Rec/SpellRec.h"
#include "WoW/Source/Magic/MagicClient/Spell_C.h"
#include "WoW/Source/Object/ObjectClient/Player_C.h"
#include "WoW/Source/Object/ObjectClient/Unit_C.h"
#include "WoW/Source/Object/Types.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"

namespace {
    uint64_t ObjectGuid(void* object) {   // object +8 -> the guid
        return static_cast<CGObject_C*>(object)->m_obj->m_guid.m_guid;
    }

    // 0x10A4BAF0's body: ClntObjMgrObjectPtr(guid, TYPE_UNIT); a health descriptor above 0 becomes 0, then OnDeath.
    void ForceDeath(uint64_t guid) {
        WOWGUID g;
        g.m_guid = guid;
        CGUnit_C* unit = static_cast<CGUnit_C*>(ClntObjMgrObjectPtr(g, TYPE_UNIT, __FILE__, __LINE__));

        if (!unit || unit->m_unit->m_unitFieldHealth == 0) {
            return;
        }

        unit->m_unit->m_unitFieldHealth = 0;
        unit->OnDeath();
    }

    bool HasPlayer() {   // 0x4038F0
        return ClntObjMgrGetActivePlayerObj() != nullptr;
    }

    bool HasAura(uint32_t spell) {   // 0x7282A0
        CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();
        return player && player->HasAuraBySpellId(static_cast<int32_t>(spell));
    }

    bool SpellExists(uint32_t spell) {   // FetchSpell
        SpellRec rec;
        return g_spellDB.GetRecord(static_cast<int32_t>(spell), &rec) != 0;
    }

    void CancelAura(uint32_t spell) {   // 0x802F80(rec, player, 0)
        CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();
        SpellRec rec;

        if (player && g_spellDB.GetRecord(static_cast<int32_t>(spell), &rec)) {
            Spell_C_CancelAura(&rec, player, 0);
        }
    }

    bool Falling() {   // player +0xD8 (movement) +0x44 & 0x1000
        CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();
        return player && player->m_movement && (player->m_movement->m_flags & MOVEMENTFLAG_FALLING) != 0;
    }

    void CastOnSelf(uint32_t spell) {   // 0x80DA40(spell, 0, player guid, 0)
        CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();

        if (player) {
            Spell_C::CastSpell(static_cast<int32_t>(spell), nullptr, player->m_obj->m_guid, false);
        }
    }
}

void CoA::AlternatePowerClient::Init() {
    CoA::AlternatePower::Deps& d = CoA::AlternatePower::Dependencies();
    d.objectGuid = ObjectGuid;
    d.forceDeath = ForceDeath;
    d.hasPlayer = HasPlayer;
    d.hasAura = HasAura;
    d.spellExists = SpellExists;
    d.cancelAura = CancelAura;
    d.falling = Falling;
    d.castOnSelf = CastOnSelf;
}
