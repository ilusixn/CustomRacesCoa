// AscTemporalContracts.cpp's client questions: the quest cache (0x67DE90 on 0xC5DA48) and the client's AreaTable /
// QuestSort tables (FUN_100b1240 / FUN_100b1ea0 on 0xAD3140.. / 0xAD443C..).
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Modules/TemporalContracts.h"
#include "WoW/Source/DB/DBClient/DBCacheInstances.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Source/DB/Rec/AreaTableRec.h"
#include "WoW/Source/DB/Rec/QuestSortRec.h"
#include "WoW/Source/Object/QuestCache.h"
#include <cstring>

namespace {
    // The DLL reads the record at fixed offsets; +0x24A0 is past this client's arena points (+0x20A0), inside its
    // completion text, and is read as it is.
    bool QuestRecord(uint32_t quest, CoA::TemporalContracts::QuestRecord& out) {
        QuestCache* rec = g_questDBCache.GetRecord(static_cast<int32_t>(quest), WOWGUID(), nullptr, nullptr, false);

        if (!rec) {
            return false;
        }

        const uint8_t* p = reinterpret_cast<const uint8_t*>(rec);
        auto u32 = [p](uint32_t off) {
            uint32_t v;
            memcpy(&v, p + off, 4);
            return v;
        };
        out.zoneOrSort = u32(0x10);
        out.money = u32(0x34);
        out.honor = u32(0x44);

        for (uint32_t i = 0; i < 4; ++i) {
            out.items[i] = u32(0x54 + i * 4);
            out.amounts[i] = u32(0x64 + i * 4);
        }

        out.arena = u32(0x24A0);
        return true;
    }

    const char* AreaName(uint32_t id) {
        AreaTableRec* row = g_areaTableDB.GetRecord(static_cast<int32_t>(id));
        return row ? row->m_areaName : nullptr;
    }

    const char* QuestSortName(uint32_t id) {
        QuestSortRec* row = g_questSortDB.GetRecord(static_cast<int32_t>(id));
        return row ? row->m_sortName : nullptr;
    }
}

void CoA::TemporalContractsClient::Init() {
    CoA::TemporalContracts::Deps& deps = CoA::TemporalContracts::Dependencies();
    deps.questRecord = QuestRecord;
    deps.areaName = AreaName;
    deps.questSortName = QuestSortName;
}
