// AscGossipQuest.cpp's client half: the SkillLine rows the sort compares.
#include "CoA/Source/Modules/GossipQuest.h"
#include "CoA/Source/Modules/Modules.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Source/DB/Rec/SkillLineRec.h"

namespace {
    // FUN_100b2020 on the client table 0xAD45E0: the row's name (+0xC), nullptr without a row.
    const char* SkillName(uint32_t id) {
        const SkillLineRec* rec = g_skillLineDB.GetRecord(static_cast<int32_t>(id));
        return rec ? rec->m_displayName : nullptr;
    }
}

void CoA::GossipQuestClient::Init() {
    CoA::GossipQuest::Dependencies().skillName = SkillName;
}
