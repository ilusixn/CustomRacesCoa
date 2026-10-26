#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Modules/ObjectUse.h"
#include "CoA/Source/Runtime/ClientHooks.h"
#include "WoW/Source/Object/ObjectClient/Object_C.h"

// The client half of ObjectUse.h: the detours sit after CGItem_C::UseItem (0x708C20) and CGGameObject_C::UseObject
// (0x70F680); both read the used object's entry (object +8 -> descriptors +0xC).
namespace {
    void OnItemUsed(void* object) {
        CoA::ItemUse::Used(static_cast<CGObject_C*>(object)->m_obj->m_entryID);
    }

    void OnGameObjectUsed(void* object) {
        CoA::GameObjectUse::Used(static_cast<CGObject_C*>(object)->m_obj->m_entryID);
    }
}

void CoA::ObjectUseClient::Init() {
    CoA::ClientHooks::itemUsed.Add(OnItemUsed);
    CoA::ClientHooks::gameObjectUsed.Add(OnGameObjectUsed);
}
