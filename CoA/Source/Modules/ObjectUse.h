#ifndef COA_SOURCE_MODULES_OBJECTUSE_H
#define COA_SOURCE_MODULES_OBJECTUSE_H

#include <cstdint>

// AscItemUse / AscGameObjectUse: the original's detours over an item's use (0x10A42F30 over 0x708C20) and a game
// object's use (0x10A42DD0 over 0x70F680). After the client function returns they route the used entry to the module
// that owns it, then fire ITEM_USED / GAMEOBJECT_USED("%u", entry) for every use. Neither event is in the original's
// name list: the interface registers both at run time (RegisterInterfaceEvent, or a frame registering it while the
// world loads), and until then they are dropped like any unknown event. ObjectUseClient.cpp reads the entries.
namespace CoA::ItemUse {
    void Used(uint32_t entry);
}

namespace CoA::GameObjectUse {
    void Used(uint32_t entry);
}

#endif
