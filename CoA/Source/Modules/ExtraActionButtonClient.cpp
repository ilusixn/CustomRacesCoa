// AscExtraActionButton.cpp's client questions and its world-entry start (0x10236b00).
#include "CoA/Source/Modules/ExtraActionButton.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/ClientHooks.h"
#include "Engine/Source/Os/TimeManager.h"
#include "WoW/Source/Magic/MagicClient/Spell_C.h"
#include "WoW/Source/Object/ObjectClient/Bag_C.h"
#include "WoW/Source/Object/ObjectClient/Container_C.h"
#include "WoW/Source/Object/ObjectClient/Item_C.h"
#include "WoW/Source/Object/ObjectClient/Player_C.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"
#include "WoW/Source/Ui/ContainerFrame.h"

namespace {
    bool HasPlayer() {
        return ClntObjMgrGetActivePlayerObj() != nullptr;
    }

    bool HasAura(uint32_t spell) {
        CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();
        return player && player->HasAuraBySpellId(static_cast<int32_t>(spell));
    }

    // 0x809000(spell, 0, &a, &b, &enabled): the two times kept in the order the original passes them; only their
    // sum is used.
    CoA::ExtraActionButton::Cooldown SpellCooldown(uint32_t spell) {
        CoA::ExtraActionButton::Cooldown c;
        uint32_t enabled = 0;
        Spell_C_GetSpellCooldown(static_cast<int32_t>(spell), 0, &c.start, &c.duration, &enabled);
        c.enabled = static_cast<uint8_t>(enabled);
        return c;
    }

    // 0x809030(item, &a, &b, &enabled)
    CoA::ExtraActionButton::Cooldown ItemCooldown(uint32_t item) {
        CoA::ExtraActionButton::Cooldown c;
        uint32_t enabled = 0;
        Spell_C_GetItemCooldown(static_cast<int32_t>(item), &c.start, &c.duration, &enabled);
        c.enabled = static_cast<uint8_t>(enabled);
        return c;
    }

    bool IsEntry(WOWGUID guid, uint32_t entry) {
        const CGObject_C* item = guid.m_guid ? ClntObjMgrObjectPtr(guid, TYPE_ITEM, __FILE__, __LINE__) : nullptr;
        return item && item->m_obj->m_entryID == entry;
    }

    // FUN_10308270: every item the player holds -- the 0x27 slots from PLAYER_FIELD_INV_SLOT_HEAD (equipment, then the
    // backpack), then each of the four equipped bags' contents (CGContainerInfo::m_containers, 0xC23540).
    bool HasItem(uint32_t entry) {
        CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();

        if (!player) {
            return false;
        }

        const CGPlayerLocalData* data = static_cast<const CGPlayerLocalData*>(player->m_player);

        for (const WOWGUID& guid : data->m_playerFieldInvSlotHead) {
            if (IsEntry(guid, entry)) {
                return true;
            }
        }

        for (const WOWGUID& guid : data->m_playerFieldPackSlot1) {
            if (IsEntry(guid, entry)) {
                return true;
            }
        }

        for (int32_t bag = 0; bag < 4; ++bag) {
            const WOWGUID guid = CGContainerInfo::m_containers[bag];
            CGObject_C* container = guid.m_guid ? ClntObjMgrObjectPtr(guid, TYPE_CONTAINER, __FILE__, __LINE__) : nullptr;
            const CGBag_C* inv = container ? container->GetBag() : nullptr;

            if (!inv) {
                continue;
            }

            const uint32_t slots = static_cast<CGContainer_C*>(container)->m_container->m_containerFieldNumSlots;

            for (uint32_t i = 0; i < slots; ++i) {
                const CGItem_C* item = inv->GetItemPointer(static_cast<int32_t>(i));

                if (item && item->m_obj->m_entryID == entry) {
                    return true;
                }
            }
        }

        return false;
    }
}

void CoA::ExtraActionButtonClient::Init() {
    CoA::ExtraActionButton::Deps& deps = CoA::ExtraActionButton::Dependencies();
    deps.hasPlayer = HasPlayer;
    deps.hasAura = HasAura;
    deps.spellCooldown = SpellCooldown;
    deps.itemCooldown = ItemCooldown;
    deps.now = OsGetAsyncTimeMs;
    deps.hasItem = HasItem;
    CoA::ClientHooks::enterWorld.Add(CoA::ExtraActionButton::Start);
}
