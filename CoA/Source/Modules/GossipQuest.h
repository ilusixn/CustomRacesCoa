#ifndef COA_SOURCE_MODULES_GOSSIPQUEST_H
#define COA_SOURCE_MODULES_GOSSIPQUEST_H

#include <cstdint>

// AscGossipQuest.cpp: the gossip and quest-log hooks of the misc packet installer FUN_100d4b90 and of FUN_100e0cc0.
//
//   0x58A9E0  Script_GetGossipOptions REPLACED (FUN_100d9b40): icon types below 0x15 name the client's table entry
//             ("" for its null ones), 0x15..0x28 Ascension's icons, anything else "".
//   0x58AF10  CGGossipInfo::SelectGossipOption, then GOSSIP_OPTION_SELECTED("%d%d", gossip id, index + 1)
//             (FUN_100de060) -- after every return.
//   0x53BA40  SortSkillInfoByName REPLACED (sub_100e0300): a zero skill line first, a missing row equal, line 120
//             before every other, else _stricmp of the names.
//   0x6D1110  CGPlayer_C::OnQuestGiverQuestComplete, then QUEST_TURNED_IN("%u%u%u") with the first three dwords of
//             the message buffer -- which begins with the 16-bit opcode, so the values straddle the fields, as shipped
//             (FUN_100e0ab0).
//   0x6D4F80  CGPlayer_C::QuestLogRemoveQuest(slot), then QUEST_REMOVED("%u") with the quest id the player's log slot
//             holds after the removal (FUN_100e0ba0).
namespace CoA::GossipQuest {
    struct Deps {
        const char* (*skillName)(uint32_t skillLine) = nullptr;   // SkillLine.dbc row +0xC, nullptr without a row
    };

    Deps& Dependencies();

    // The icon name GetGossipOptions pushes for `type`; `stock` is the client's table entry (types below 0x15).
    const char* IconName(uint32_t type, const char* stock);

    // sub_100e0300 on two skill lines.
    int32_t CompareSkills(uint32_t a, uint32_t b);

    void AfterSelectGossipOption(uint32_t gossipId, int32_t index);
    // The message buffer as the handler saw it (m_data, m_size).
    void AfterQuestComplete(const uint8_t* data, uint32_t size);
    void AfterQuestRemoved(uint32_t questId);

    void Init();
}

#endif
