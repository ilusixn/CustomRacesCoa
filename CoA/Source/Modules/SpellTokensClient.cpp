// AscSpellTokens.cpp's client half: the player's descriptor, equipped items, item sets and global strings.
#include "CoA/Source/Modules/SpellTokensClient.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Modules/SpellTokens.h"
#include "Engine/Source/FrameScript/FrameScript.h"
#include "WoW/Source/DB/DBClient/DBCacheInstances.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Source/DB/Rec/ItemSetRec.h"
#include "WoW/Source/DB/Rec/SpellRec.h"
#include "WoW/Source/Magic/MagicClient/Spell_C.h"
#include "WoW/Source/Object/ItemStats.h"
#include "WoW/Source/Object/ObjectClient/Player_C.h"
#include "WoW/Source/Object/Player.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"
#include <cstring>

namespace {
    using CoA::SpellTokens::Equipped;

    bool HasPlayer() {
        return ClntObjMgrGetActivePlayerObj() != nullptr;
    }

    // The active player's descriptor (object +8 in the original: object, unit and player fields in one block).
    uint32_t PlayerU32(uint32_t offset) {
        CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();
        uint32_t v = 0;

        if (player && player->m_obj) {
            memcpy(&v, reinterpret_cast<const uint8_t*>(player->m_obj) + offset, sizeof(v));
        }

        return v;
    }

    float PlayerF32(uint32_t offset) {
        const uint32_t bits = PlayerU32(offset);
        float v;
        memcpy(&v, &bits, sizeof(v));
        return v;
    }

    int32_t ModifyValue(const void* rec, int32_t value, int32_t op) {
        Spell_C_ModifySpellValue(static_cast<const SpellRec*>(rec), value, op);
        return value;
    }

    // FUN_10a12260's walk: the 19 equipment slots (descriptor +0x510), each item object (0x4D4DB0 mask 2) and its
    // cached record (0x67CA30 on 0xC5D828).
    std::vector<Equipped> EquippedItems() {
        std::vector<Equipped> out;
        CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();

        if (!player || !player->m_player) {
            return out;
        }

        for (uint32_t slot = 0; slot < 0x13; ++slot) {
            CGObject_C* item = ClntObjMgrObjectPtr(player->m_player->m_playerFieldInvSlotHead[slot], TYPE_ITEM, __FILE__, __LINE__);

            if (!item) {
                continue;
            }

            Equipped e;
            e.entry = item->m_obj->m_entryID;
            const ItemStats_C* stats = g_itemDBCache.GetRecord(static_cast<int32_t>(e.entry), WOWGUID{ 0 }, nullptr, nullptr, false);

            if (stats) {
                e.hasProto = true;
                e.itemSet = stats->m_itemSet;

                for (uint32_t j = 0; j < 5; ++j) {
                    e.spells[j] = stats->m_spellId[j];
                    e.triggers[j] = stats->m_spellTrigger[j];
                }
            }

            out.push_back(e);
        }

        return out;
    }

    // FUN_100b19c0 on ItemSet.dbc (0xAD3EFC).
    bool ItemSet(uint32_t id, CoA::SpellTokens::ItemSet& out) {
        const ItemSetRec* rec = g_itemSetDB.GetRecord(static_cast<int32_t>(id));

        if (!rec) {
            return false;
        }

        for (uint32_t i = 0; i < 17; ++i) {
            out.items[i] = static_cast<uint32_t>(rec->m_itemID[i]);
        }

        for (uint32_t i = 0; i < 8; ++i) {
            out.spells[i] = static_cast<uint32_t>(rec->m_setSpellID[i]);
            out.thresholds[i] = static_cast<uint32_t>(rec->m_setThreshold[i]);
        }

        return true;
    }

    const char* GlobalText(const char* key) {
        return FrameScript_GetText(key, -1, GENDER_NOT_APPLICABLE);
    }
}

bool CoA::SpellTokensClient::Value(uint32_t op, const SpellRec* spell, float& out) {
    if (!spell) {
        return false;
    }

    CoA::SpellTokens::Spell s;
    s.id = static_cast<uint32_t>(spell->m_id);
    s.maxLevel = static_cast<uint32_t>(spell->m_maxLevel);
    s.baseLevel = static_cast<uint32_t>(spell->m_baseLevel);

    for (uint32_t i = 0; i < 3; ++i) {
        s.realPointsPerLevel[i] = spell->m_effectRealPointsPerLevel[i];
        s.miscValueB[i] = spell->m_effectMiscValueB[i];
    }

    s.maxTargets = spell->m_maxTargets;
    s.schoolMask = static_cast<uint32_t>(spell->m_schoolMask);
    return CoA::SpellTokens::Value(op, s, spell, out);
}

void CoA::SpellTokensClient::Init() {
    CoA::SpellTokens::Deps& d = CoA::SpellTokens::Dependencies();
    d.hasPlayer = HasPlayer;
    d.playerU32 = PlayerU32;
    d.playerF32 = PlayerF32;
    d.modifyValue = ModifyValue;
    d.equipped = EquippedItems;
    d.itemSet = ItemSet;
    d.globalText = GlobalText;
}
