#include "CoA/Source/Modules/GossipQuest.h"
#include "CoA/Tests/Support.h"
#include "CoA/Tests/TestMain.h"
#include <climits>
#include <cstring>
#include <map>
#include <string>

namespace {
    std::map<uint32_t, std::string> s_skills;

    const char* SkillName(uint32_t id) {
        auto it = s_skills.find(id);
        return it == s_skills.end() ? nullptr : it->second.c_str();
    }

    void Start() {
        s_skills.clear();
        CoA::GossipQuest::Dependencies().skillName = SkillName;
        CoA::GossipQuest::Init();
        CoATest::RecordSignals();
        CoATest::Signals().clear();
    }
}

COA_TEST(GossipQuest_IconNames) {
    using CoA::GossipQuest::IconName;
    CHECK(strcmp(IconName(0, "gossip"), "gossip") == 0);
    CHECK(strcmp(IconName(6, "banker"), "banker") == 0);
    CHECK(strcmp(IconName(0x0E, nullptr), "") == 0);   // the client's null entries become ""
    CHECK(strcmp(IconName(0x14, "gossip"), "gossip") == 0);
    CHECK(strcmp(IconName(0x15, nullptr), "ActiveQuestIcon") == 0);
    CHECK(strcmp(IconName(0x19, nullptr), "activelegendaryquesticon") == 0);
    CHECK(strcmp(IconName(0x22, nullptr), "invisiblegossipicon") == 0);
    CHECK(strcmp(IconName(0x23, nullptr), "GreyTowergossipicon") == 0);
    CHECK(strcmp(IconName(0x28, nullptr), "AzzarFairePlayergossipicon") == 0);
    CHECK(strcmp(IconName(0x29, nullptr), "") == 0);
    CHECK(strcmp(IconName(0xFFFFFFFF, nullptr), "") == 0);   // a negative type is unsigned here
}

COA_TEST(GossipQuest_SkillComparator) {
    Start();
    using CoA::GossipQuest::CompareSkills;
    s_skills[10] = "beta";
    s_skills[11] = "Alpha";
    s_skills[120] = "zzz";
    s_skills[121] = "ALPHA";
    CHECK(CompareSkills(0, 10) == -1);
    CHECK(CompareSkills(0, 0) == -1);
    CHECK(CompareSkills(10, 0) == 1);
    CHECK(CompareSkills(10, 99) == 0);    // no row
    CHECK(CompareSkills(99, 10) == 0);
    CHECK(CompareSkills(120, 10) == INT32_MIN);
    CHECK(CompareSkills(10, 120) == INT32_MAX);
    CHECK(CompareSkills(120, 120) == 0);
    CHECK(CompareSkills(10, 11) > 0);
    CHECK(CompareSkills(11, 10) < 0);
    CHECK(CompareSkills(11, 121) == 0);   // case-insensitive
}

COA_TEST(GossipQuest_Events) {
    Start();
    CoA::GossipQuest::AfterSelectGossipOption(77, 2);
    CHECK(CoATest::Signals().size() == 1 && CoATest::Signals()[0] == "GOSSIP_OPTION_SELECTED:77,3");

    // The buffer starts with the u16 opcode: the three dwords straddle the quest id, xp and money, as shipped.
    CoATest::Bytes b;
    b.U16(0x191).U32(0x00020001).U32(0x00040003).U32(0x00060005).U32(9);
    CoATest::Signals().clear();
    CoA::GossipQuest::AfterQuestComplete(b.data.data(), static_cast<uint32_t>(b.data.size()));
    char want[96];
    snprintf(want, sizeof(want), "QUEST_TURNED_IN:%u,%u,%u", 0x00010191u, 0x00030002u, 0x00050004u);
    CHECK(CoATest::Signals().size() == 1 && CoATest::Signals()[0] == want);

    // A buffer shorter than the twelve bytes: the missing ones read as zero.
    CoATest::Signals().clear();
    CoATest::Bytes s;
    s.U16(0x191).U16(5);
    CoA::GossipQuest::AfterQuestComplete(s.data.data(), static_cast<uint32_t>(s.data.size()));
    snprintf(want, sizeof(want), "QUEST_TURNED_IN:%u,0,0", 0x00050191u);
    CHECK(CoATest::Signals().size() == 1 && CoATest::Signals()[0] == want);

    CoATest::Signals().clear();
    CoA::GossipQuest::AfterQuestRemoved(1234);
    CHECK(CoATest::Signals().size() == 1 && CoATest::Signals()[0] == "QUEST_REMOVED:1234");
    CoATest::StopRecordingSignals();
}
