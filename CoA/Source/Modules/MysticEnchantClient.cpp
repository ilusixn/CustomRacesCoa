// AscMysticEnchant.cpp's client reads (CoA::MysticEnchant::Deps), the SpellText mystic questions, and the hooks: world
// entry (the scroll prefetch, 0x102EA2A0), a used game object (the altar, 0x10A42DD0) and a used item (0x10A42F30).
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Modules/MysticEnchant.h"
#include "CoA/Source/Modules/SpellText.h"
#include "CoA/Source/Runtime/ClientHooks.h"
#include "Engine/Source/Tempest/C3Vector.h"
#include "WoW/Source/DB/DBClient/DBCacheInstances.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Source/DB/Rec/SpellRec.h"
#include "WoW/Source/Object/ObjectClient/Bag_C.h"
#include "WoW/Source/Object/ObjectClient/Container_C.h"
#include "WoW/Source/Object/ObjectClient/Item_C.h"
#include "WoW/Source/Object/ObjectClient/Player_C.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"
#include "WoW/Source/Ui/ContainerFrame.h"
#include "WoW/Source/Ui/ScriptEvents.h"
#include <cmath>
#include <vector>

namespace {
    using Item = CoA::MysticEnchant::Item;

    CGPlayer_C* Player() {
        return ClntObjMgrGetActivePlayerObj();
    }

    const CGPlayerLocalData* PlayerData(CGPlayer_C* player) {
        return static_cast<const CGPlayerLocalData*>(player->m_player);
    }

    bool HasPlayer() {
        return Player() != nullptr;
    }

    // FUN_100C5F40 (AscCA::UnitClassOf): a unit or player's UNIT_FIELD_BYTES_0 byte 1.
    uint8_t ClassOf(const CGUnit_C* unit) {
        return unit->m_unit->m_unitFieldBytes0[1];
    }

    uint8_t PlayerClass() {
        CGPlayer_C* p = Player();
        return p ? ClassOf(p) : 0;
    }

    uint32_t PlayerLevel() {
        CGPlayer_C* p = Player();
        return p ? p->m_unit->m_unitFieldLevel : 0;
    }

    uint32_t Coinage() {
        CGPlayer_C* p = Player();
        return p ? PlayerData(p)->m_playerFieldCoinage : 0;
    }

    bool Casting() {   // player +0xA60
        CGPlayer_C* p = Player();
        return p && p->m_castSpellId != 0;
    }

    const CGItem_C* ItemPtr(WOWGUID guid) {
        return guid.m_guid ? static_cast<const CGItem_C*>(ClntObjMgrObjectPtr(guid, TYPE_ITEM, __FILE__, __LINE__)) : nullptr;
    }

    CGContainer_C* Bag(int32_t bag) {
        const WOWGUID guid = CGContainerInfo::m_containers[bag];
        return guid.m_guid ? static_cast<CGContainer_C*>(ClntObjMgrObjectPtr(guid, TYPE_CONTAINER, __FILE__, __LINE__)) : nullptr;
    }

    Item MakeItem(const CGItem_C* item, uint8_t bag, uint8_t slot) {
        Item out;
        out.low = static_cast<uint32_t>(item->m_obj->m_guid.m_guid);
        out.entry = item->m_obj->m_entryID;
        out.count = static_cast<uint32_t>(item->m_item->m_itemFieldStackCount);
        out.bag = bag;
        out.slot = slot;
        return out;
    }

    // Every item the player holds, with its bag and 1-based slot (FUN_10111D30 / FUN_10111E20): equipment (no slot),
    // the backpack (PLAYER_FIELD_PACK_SLOT_1, bag 0) and each equipped bag's contents (bags 1..4).
    template <class F>
    void ForEachItem(F f) {
        CGPlayer_C* player = Player();

        if (!player) {
            return;
        }

        const CGPlayerLocalData* data = PlayerData(player);

        for (const WOWGUID& guid : data->m_playerFieldInvSlotHead) {
            if (const CGItem_C* item = ItemPtr(guid)) {
                f(MakeItem(item, 0, 0));
            }
        }

        for (uint32_t s = 0; s < 16; ++s) {
            if (const CGItem_C* item = ItemPtr(data->m_playerFieldPackSlot1[s])) {
                f(MakeItem(item, 0, static_cast<uint8_t>(s + 1)));
            }
        }

        for (int32_t b = 0; b < 4; ++b) {
            CGContainer_C* container = Bag(b);
            const CGBag_C* inv = container ? container->GetBag() : nullptr;

            if (!inv) {
                continue;
            }

            for (uint32_t i = 0; i < container->m_container->m_containerFieldNumSlots; ++i) {
                if (const CGItem_C* item = inv->GetItemPointer(static_cast<int32_t>(i))) {
                    f(MakeItem(item, static_cast<uint8_t>(b + 1), static_cast<uint8_t>(i + 1)));
                }
            }
        }
    }

    std::vector<Item> PlayerItems() {   // FUN_10308270
        std::vector<Item> v;
        ForEachItem([&v](const Item& i) { v.push_back(i); });
        return v;
    }

    // ObjectPtr(item guid (high 0x4000 << 16), 2): any item object, with its place when the player holds it.
    bool ItemByLow(uint32_t low, Item& out) {
        const WOWGUID guid = { (static_cast<uint64_t>(0x4000u << 16) << 32) | low };
        const CGItem_C* item = ItemPtr(guid);

        if (!item) {
            return false;
        }

        out = MakeItem(item, 0, 0);
        ForEachItem([&out](const Item& i) {
            if (i.low == out.low) {
                out = i;
            }
        });
        return true;
    }

    // FUN_10308700: the currency-token slots (0x76..0x95).
    uint32_t TokenCount(uint32_t entry) {
        CGPlayer_C* player = Player();

        if (!player) {
            return 0;
        }

        uint32_t n = 0;

        for (const WOWGUID& guid : PlayerData(player)->m_playerFieldCurrencytokenSlot1) {
            const CGItem_C* item = ItemPtr(guid);

            if (item && item->m_obj->m_entryID == entry) {
                n += static_cast<uint32_t>(item->m_item->m_itemFieldStackCount);
            }
        }

        return n;
    }

    // FUN_103085F0: free backpack slots plus free slots in equipped bags.
    uint32_t FreeBagSlots() {
        CGPlayer_C* player = Player();

        if (!player) {
            return 0;
        }

        uint32_t n = 0;

        for (const WOWGUID& guid : PlayerData(player)->m_playerFieldPackSlot1) {
            n += guid.m_guid == 0;
        }

        for (int32_t b = 0; b < 4; ++b) {
            CGContainer_C* container = Bag(b);
            const CGBag_C* inv = container ? container->GetBag() : nullptr;

            if (!inv) {
                continue;
            }

            for (uint32_t i = 0; i < container->m_container->m_containerFieldNumSlots; ++i) {
                n += inv->GetItemPointer(static_cast<int32_t>(i)) == nullptr;
            }
        }

        return n;
    }

    // CGItem_C::GetFullItemName (0x707C20) into a 0x100 buffer.
    std::string ItemName(uint32_t low) {
        const WOWGUID guid = { (static_cast<uint64_t>(0x4000u << 16) << 32) | low };
        const CGItem_C* item = ItemPtr(guid);

        if (!item) {
            return std::string();
        }

        char buffer[0x100] = {};
        item->GetFullItemName(buffer, sizeof(buffer));
        return buffer;
    }

    // 0x67CA30 on the item cache: the record's buy price (+0x20).
    bool ItemBuyPrice(uint32_t entry, uint32_t& price) {
        const ItemStats_C* rec = g_itemDBCache.GetRecord(static_cast<int32_t>(entry), WOWGUID(), nullptr, nullptr, false);

        if (!rec) {
            return false;
        }

        price = static_cast<uint32_t>(rec->m_buyPrice);
        return true;
    }

    bool SpellStacks(uint32_t spell, uint32_t& stacks) {   // Spell.dbc +0xC4
        SpellRec rec;

        if (!g_spellDB.GetRecord(static_cast<int32_t>(spell), &rec)) {
            return false;
        }

        stacks = static_cast<uint32_t>(rec.m_cumulativeAura);
        return true;
    }

    bool SpellName(uint32_t spell, std::string& name) {   // Spell.dbc +0x220
        SpellRec rec;

        if (!g_spellDB.GetRecord(static_cast<int32_t>(spell), &rec)) {
            return false;
        }

        name = rec.m_name ? rec.m_name : "";
        return true;
    }

    // FUN_100CF140: the player has the aura; the spell record's +0x144 (EffectBasePoints[1]).
    bool AuraPoints(uint32_t aura, int32_t& points) {
        CGPlayer_C* player = Player();
        SpellRec rec;

        if (!player || !player->HasAuraBySpellId(static_cast<int32_t>(aura)) || !g_spellDB.GetRecord(static_cast<int32_t>(aura), &rec)) {
            return false;
        }

        points = rec.m_effectBasePoints[1];
        return true;
    }

    bool TokenGuid(const char* token, uint64_t& guid) {
        WOWGUID g;
        const bool found = Script_GetGUIDFromToken(token, g, false);
        guid = found ? g.m_guid : 0;
        return found;
    }

    uint64_t ActivePlayerGuid() {
        return ClntObjMgrGetActivePlayer().m_guid;
    }

    bool PlayerClassByGuid(uint64_t guid, uint8_t& cls) {   // ObjectPtr(guid, 0x10)
        const CGUnit_C* unit = static_cast<const CGUnit_C*>(ClntObjMgrObjectPtr(WOWGUID { guid }, TYPE_PLAYER, __FILE__, __LINE__));

        if (!unit) {
            return false;
        }

        cls = ClassOf(unit);
        return true;
    }

    // FUN_102E96E0: a mystic altar (a game object, type id 5, whose entry FUN_102EA0C0 lists) within 10 yards
    // (LAB_102DD3B0 collects every visible game object).
    int32_t CollectGameObject(WOWGUID guid, void* param) {
        if (CGObject_C* o = ClntObjMgrObjectPtr(guid, TYPE_GAMEOBJECT, __FILE__, __LINE__)) {
            static_cast<std::vector<CGObject_C*>*>(param)->push_back(o);
        }

        return 1;
    }

    bool AltarNearby() {
        CGPlayer_C* player = Player();

        if (!player) {
            return false;
        }

        const C3Vector p = player->GetPosition();
        std::vector<CGObject_C*> nearby;
        ClntObjMgrEnumVisibleObjects(CollectGameObject, &nearby);

        for (CGObject_C* o : nearby) {
            const C3Vector q = o->GetPosition();
            const float dz = static_cast<float>(std::pow(static_cast<double>(q.z - p.z), 2.0));
            const float dx = static_cast<float>(std::pow(static_cast<double>(q.x - p.x), 2.0));
            const float dy = static_cast<float>(std::pow(static_cast<double>(q.y - p.y), 2.0));
            const float d = static_cast<float>(std::sqrt(static_cast<double>(dy + dx + dz)));

            if (d <= 10.0f && o->m_typeId == ID_GAMEOBJECT && CoA::MysticEnchant::IsAltar(o->m_obj->m_entryID)) {
                return true;
            }
        }

        return false;
    }

    std::string SpellTextName(uint32_t spell) {
        return CoA::SpellText::Name(spell);
    }

    std::string SpellTextDescription(uint32_t spell) {
        return CoA::SpellText::Description(spell, true, true, 0);
    }

    void OnGameObjectUsed(void* object) {
        CoA::MysticEnchant::OnGameObjectUse(static_cast<CGObject_C*>(object)->m_obj->m_entryID);
    }

    void OnItemUsed(void* object) {
        CoA::MysticEnchant::OnItemUse(static_cast<CGObject_C*>(object)->m_obj->m_entryID);
    }
}

void CoA::MysticEnchantClient::Init() {
    CoA::MysticEnchant::Deps& d = CoA::MysticEnchant::Dependencies();
    d.hasPlayer = HasPlayer;
    d.playerClass = PlayerClass;
    d.playerLevel = PlayerLevel;
    d.coinage = Coinage;
    d.casting = Casting;
    d.item = ItemByLow;
    d.playerItems = PlayerItems;
    d.tokenCount = TokenCount;
    d.freeBagSlots = FreeBagSlots;
    d.itemName = ItemName;
    d.itemBuyPrice = ItemBuyPrice;
    d.spellStacks = SpellStacks;
    d.spellName = SpellName;
    d.auraPoints = AuraPoints;
    d.tokenGuid = TokenGuid;
    d.activePlayerGuid = ActivePlayerGuid;
    d.playerClassByGuid = PlayerClassByGuid;
    d.altarNearby = AltarNearby;
    d.spellTextName = SpellTextName;
    d.spellTextDescription = SpellTextDescription;

    CoA::SpellText::Deps& text = CoA::SpellText::Dependencies();
    text.mysticSlotted = CoA::MysticEnchant::Slotted;
    text.mysticWorldforged = CoA::MysticEnchant::Worldforged;
    text.mysticCollected = CoA::MysticEnchant::Collected;

    CoA::ClientHooks::enterWorld.Add(CoA::MysticEnchant::EnterWorld);
    CoA::ClientHooks::gameObjectUsed.Add(OnGameObjectUsed);
    CoA::ClientHooks::itemUsed.Add(OnItemUsed);
}
