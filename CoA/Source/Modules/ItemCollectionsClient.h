#ifndef COA_SOURCE_MODULES_ITEMCOLLECTIONSCLIENT_H
#define COA_SOURCE_MODULES_ITEMCOLLECTIONSCLIENT_H

#include <cstdint>

class CGPlayer_C;
class CGUnit_C;

// AscItemCollections.cpp's client half: the hooks that read client objects. The decomp sites call these.
namespace CoA::ItemCollectionsClient {
    // FUN_102cab70 before CGPlayer_C::EquipVisibleItem (0x6E08C0): a player's character component re-attaches the slot's
    // collections for its current display id (m_itemDisplayID of the slot's ITEM_SLOT).
    void BeforeEquipVisibleItem(CGPlayer_C* player, int32_t invSlot);
    // sub_102cac90 / FUN_102cac10 after 0x6E1E10 / 0x6E09E0: a unit whose ranged inventory slot holds an item with a
    // sheathe type runs ShowRangedWeapon again (sheathed unless its sheath state is 2).
    void RefreshRanged(CGUnit_C* unit);
    // FUN_102cad20 after CGUnit_C::ShowRangedWeapon (0x72B7F0): for another player (sheathed false) whose visible item 16
    // (descriptor +0x4EC) is a bow (class 2 / subclass 2) a quiver is attached at ATTACH_SHEATH_MAINHAND with
    // ItemDisplayInfo 21328.
    void AfterShowRangedWeapon(CGUnit_C* unit, bool sheathed);
}

#endif
