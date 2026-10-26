// AscSkillCardCollection.cpp / AscSkillCardSlots.cpp's client questions: the active player (present, class, level,
// money, held items) and a spell's name (FetchSpell +0x220) and description (FUN_101ac910).
#include "CoA/Source/Modules/CA.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Modules/MysticEnchant.h"
#include "CoA/Source/Modules/SkillCard.h"
#include "CoA/Source/Modules/SpellText.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Source/DB/Rec/SpellRec.h"
#include "WoW/Source/Object/ObjectClient/Player_C.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"

namespace {
    bool HasPlayer() {   // 0x4038F0
        return ClntObjMgrGetActivePlayerObj() != nullptr;
    }

    uint8_t PlayerClass() {   // FUN_100a0390: the unit's class byte (+0x5D)
        CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();
        return player ? player->m_unit->m_unitFieldBytes0[1] : 0;
    }

    uint32_t PlayerLevel() {   // descriptor +0xD8
        CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();
        return player ? player->m_unit->m_unitFieldLevel : 0;
    }

    uint32_t PlayerMoney() {   // PLAYER_FIELD_COINAGE (+0x1248)
        CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();
        return player ? static_cast<const CGPlayerLocalData*>(player->m_player)->m_playerFieldCoinage : 0;
    }

    uint32_t HeldCount(uint32_t item) {   // FUN_10308470 (bags) + FUN_10308570 (currency-token slots)
        return CoA::MysticEnchant::BagItemCount(item) + CoA::CA::UnitItemCount(ClntObjMgrGetActivePlayer().m_guid, item);
    }

    bool SpellName(uint32_t spell, std::string& name) {   // FetchSpell, Spell.dbc +0x220
        SpellRec rec;

        if (!g_spellDB.GetRecord(static_cast<int32_t>(spell), &rec)) {
            return false;
        }

        name = rec.m_name ? rec.m_name : "";
        return true;
    }

    std::string SpellDescription(uint32_t spell) {   // FUN_101ac910(spell, 1, 1, 0)
        return CoA::SpellText::Description(spell, true, true, 0);
    }
}

void CoA::SkillCardClient::Init() {
    CoA::SkillCard::Deps& d = CoA::SkillCard::Dependencies();
    d.hasPlayer = HasPlayer;
    d.playerClass = PlayerClass;
    d.playerLevel = PlayerLevel;
    d.playerMoney = PlayerMoney;
    d.heldCount = HeldCount;
    d.spellName = SpellName;
    d.spellDescription = SpellDescription;
}
