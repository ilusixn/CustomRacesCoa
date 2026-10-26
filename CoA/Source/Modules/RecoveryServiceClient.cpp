// AscRecoveryService.cpp's client questions: the Item.dbc row of an item (FUN_100b1870) and the ChrRaces / ChrClasses
// names (FUN_100b13f0 +0x38, FUN_100b13c0 +0x10) the searches compare against.
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Modules/RecoveryService.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Source/DB/Rec/ChrClassesRec.h"
#include "WoW/Source/DB/Rec/ChrRacesRec.h"
#include "WoW/Source/DB/Rec/ItemRec.h"

namespace {
    bool ItemRow(uint32_t entry, CoA::RecoveryService::ItemRow& out) {
        const ItemRec* row = g_itemDB.GetRecord(static_cast<int32_t>(entry));

        if (!row) {
            return false;
        }

        out.classId = static_cast<uint32_t>(row->m_classID);
        out.subclassId = static_cast<uint32_t>(row->m_subclassID);
        out.inventoryType = static_cast<uint32_t>(row->m_inventoryType);
        return true;
    }

    const char* RaceName(uint32_t id) {
        const ChrRacesRec* row = g_chrRacesDB.GetRecord(static_cast<int32_t>(id));
        return row ? row->m_name : nullptr;
    }

    const char* ClassName(uint32_t id) {
        const ChrClassesRec* row = g_chrClassesDB.GetRecord(static_cast<int32_t>(id));
        return row ? row->m_name : nullptr;
    }
}

void CoA::RecoveryServiceClient::Init() {
    CoA::RecoveryService::Deps& deps = CoA::RecoveryService::Dependencies();
    deps.itemRow = ItemRow;
    deps.raceName = RaceName;
    deps.className = ClassName;
}
