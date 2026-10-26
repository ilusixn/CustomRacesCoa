// AscGossipQuest.cpp (see GossipQuest.h): gossip icon names, the skill-line sort, three events.
#include "CoA/Source/Modules/GossipQuest.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/Events.h"
#include <climits>
#include <cstring>

namespace {
    CoA::GossipQuest::Deps s_deps;

    const char* ExtendedIcon(uint32_t type) {
        switch (type) {
            case 0x15: return "ActiveQuestIcon";
            case 0x16: return "AvailableQuestIcon";
            case 0x17: return "DailyActiveQuestIcon";
            case 0x18: return "DailyQuestIcon";
            case 0x19: return "activelegendaryquesticon";
            case 0x1A: return "availablelegendaryquesticon";
            case 0x1B: return "transmogrifygossipicon";
            case 0x1C: return "workordergossipicon";
            case 0x1D: return "campaignactivequesticon";
            case 0x1E: return "campaignavailablequesticon";
            case 0x1F: return "campaignincompletequesticon";
            case 0x20: return "campaignactivedailyquesticon";
            case 0x21: return "campaignavailabledailyquesticon";
            case 0x22: return "invisiblegossipicon";
            case 0x23: return "GreyTowergossipicon";
            case 0x24: return "SilverSkullgossipicon";
            case 0x25: return "SilverSwordsgossipicon";
            case 0x26: return "SilverChestgossipicon";
            case 0x27: return "AzzarFaireFriendsgossipicon";
            case 0x28: return "AzzarFairePlayergossipicon";
            default: return "";
        }
    }

    const char* SkillName(uint32_t id) {
        return s_deps.skillName ? s_deps.skillName(id) : nullptr;
    }
}

CoA::GossipQuest::Deps& CoA::GossipQuest::Dependencies() {
    return s_deps;
}

// FUN_100d9b40's per-option name.
const char* CoA::GossipQuest::IconName(uint32_t type, const char* stock) {
    if (type < 0x15) {
        return stock ? stock : "";
    }

    return ExtendedIcon(type);
}

// sub_100e0300: the rows come from the client's SkillLine table (0xAD45E0); the client compared with 0x76EC80.
int32_t CoA::GossipQuest::CompareSkills(uint32_t a, uint32_t b) {
    if (a == 0) {
        return -1;
    }

    if (b == 0) {
        return 1;
    }

    const char* nameA = SkillName(a);
    const char* nameB = SkillName(b);

    if (!nameA || !nameB) {
        return 0;
    }

    if (a == 0x78) {
        if (b != 0x78) {
            return INT32_MIN;
        }
    } else if (b == 0x78) {
        return INT32_MAX;
    }

    return _stricmp(nameA, nameB);
}

// FUN_100de060.
void CoA::GossipQuest::AfterSelectGossipOption(uint32_t gossipId, int32_t index) {
    CoA::Events::Signal("GOSSIP_OPTION_SELECTED", "%d%d", static_cast<int32_t>(gossipId), index + 1);
}

// FUN_100e0ab0: the dwords at *(msg + 4) (m_data) +0 / +4 / +8.
void CoA::GossipQuest::AfterQuestComplete(const uint8_t* data, uint32_t size) {
    uint32_t d[3] = {};

    if (data) {
        memcpy(d, data, size < sizeof(d) ? size : sizeof(d));
    }

    CoA::Events::Signal("QUEST_TURNED_IN", "%u%u%u", d[0], d[1], d[2]);
}

// FUN_100e0ba0.
void CoA::GossipQuest::AfterQuestRemoved(uint32_t questId) {
    CoA::Events::Signal("QUEST_REMOVED", "%u", questId);
}

void CoA::GossipQuest::Init() {
    COA_EVENT("GOSSIP_OPTION_SELECTED");
    COA_EVENT("QUEST_TURNED_IN");
    COA_EVENT("QUEST_REMOVED");
}
