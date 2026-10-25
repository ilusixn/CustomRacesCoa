// AscObjectAddon.cpp's lifecycle (init at 0x102CDF82): an object leaving the client's list (0x6FC0F0) drops its unit
// record (FUN_102ccfc0); on every second SMSG_INITIAL_SPELLS (0x6DF050) every unit record except the active player's is
// dropped (FUN_102cd070 -> FUN_102cd780). Also answers the attach fixes' unit field reads.
#include "CoA/Source/Modules/AttachFixes.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Modules/ObjectAddon.h"
#include "CoA/Source/Runtime/ClientHooks.h"
#include "WoW/Source/Object/ObjectClient/Object_C.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"

namespace {
    // FUN_102ccfc0: the object's GUID is the first qword of its descriptor block.
    void OnObjectFree(void* object) {
        CoA::ObjectAddon::DropUnit(static_cast<CGObject_C*>(object)->m_obj->m_guid.m_guid);
    }

    void KeepActivePlayer() {
        CoA::ObjectAddon::DropAllUnitsExcept(ClntObjMgrGetActivePlayer().m_guid);
    }

    // AscObjectAddon::Unit(guid)[index].value (creating the record, as the original's lookup does).
    bool UnitAddonField(uint64_t guid, uint32_t index, uint32_t& value) {
        std::vector<CoA::ObjectAddon::Field>& fields = CoA::ObjectAddon::Unit(guid);

        if (index >= fields.size()) {
            return false;
        }

        value = fields[index].value;
        return true;
    }
}

void CoA::ObjectAddonClient::Init() {
    CoA::ClientHooks::objectFree.Add(OnObjectFree);
    CoA::ClientHooks::initialSpellsAlternate.Add(KeepActivePlayer);
    CoA::AttachFixes::Dependencies().unitAddonField = UnitAddonField;
}
