#include "CoA/Source/Modules/GlobalsJ.h"
#include "CoA/Source/Modules/InitHooks.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Modules/ObjectAddon.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include "CoA/Tests/Support.h"
#include "CoA/Tests/TestMain.h"
#include <cstring>

namespace {
    bool s_havePlayer = true;
    uint32_t s_questLog[25] = {};
    const uint64_t kPlayer = 0x77;

    bool QuestLogQuest(uint32_t slot, uint32_t& questId) {
        questId = slot < 25 ? s_questLog[slot] : 0;
        return s_havePlayer;
    }

    uint64_t PlayerGuid() { return kPlayer; }

    void Start() {
        CoA::InitHooks::Reset();
        CoA::ObjectAddon::Reset();
        CoA::GlobalsJ::Reset();
        s_havePlayer = true;
        memset(s_questLog, 0, sizeof(s_questLog));
        CoA::InitHooks::Deps& d = CoA::InitHooks::Dependencies();
        d.questLogQuest = QuestLogQuest;
        d.activePlayerGuid = PlayerGuid;
    }

    void Addon(uint32_t index, uint32_t value) {
        CoA::ObjectAddon::Unit(kPlayer)[index].value = value;
    }
}

// sub_10a44ae0 over 0x5E0420 CalculateExperience: a quest in the player's log takes its XP from the player's addon field
// 0x24 + slot (the first slot that holds it) and the original is not run.
COA_TEST(InitHooks_QuestExperience) {
    Start();
    s_questLog[2] = 500;
    s_questLog[7] = 500;
    Addon(0x24 + 2, 1234);
    Addon(0x24 + 7, 99);
    int32_t xp = -1;
    CHECK(CoA::InitHooks::QuestExperience(500, xp));
    CHECK(xp == 1234);
    CHECK(!CoA::InitHooks::QuestExperience(501, xp));
    // Quest id 0 never matches an empty slot.
    CHECK(!CoA::InitHooks::QuestExperience(0, xp));
    s_havePlayer = false;
    CHECK(!CoA::InitHooks::QuestExperience(500, xp));
}

// sub_10a790a0 over 0x5E0D20 GetQuestLevel: field 0x3D + the first matching slot.
COA_TEST(InitHooks_QuestLevel) {
    Start();
    s_questLog[24] = 9;
    Addon(0x3D + 24, 61);
    int32_t level = 0;
    CHECK(CoA::InitHooks::QuestLevel(9, level));
    CHECK(level == 61);
    CHECK(!CoA::InitHooks::QuestLevel(10, level));
}

// sub_10a4d770 over 0x61E5C0 CGTooltip::GetQuestLink: every matching slot overrides the level (the last one wins), then
// the original runs with it.
COA_TEST(InitHooks_QuestLinkLevel) {
    Start();
    s_questLog[1] = 42;
    s_questLog[3] = 42;
    Addon(0x3D + 1, 10);
    Addon(0x3D + 3, 20);
    CHECK(CoA::InitHooks::QuestLinkLevel(42, 80) == 20);
    CHECK(CoA::InitHooks::QuestLinkLevel(43, 80) == 80);
    s_havePlayer = false;
    CHECK(CoA::InitHooks::QuestLinkLevel(42, 80) == 80);
}

// sub_10a44860 over 0x6E4950 CGPlayer_C::CombatModeEnter: autoRangedCombat "1" -> its int is whether the ranged slot
// holds an item; any other string leaves it.
COA_TEST(InitHooks_AutoRangedCombat) {
    int32_t value = 7;
    CHECK(CoA::InitHooks::AutoRangedCombatValue("1", true, value));
    CHECK(value == 1);
    CHECK(CoA::InitHooks::AutoRangedCombatValue("1", false, value));
    CHECK(value == 0);
    value = 7;
    CHECK(!CoA::InitHooks::AutoRangedCombatValue("0", true, value));
    CHECK(!CoA::InitHooks::AutoRangedCombatValue("1 ", true, value));
    CHECK(!CoA::InitHooks::AutoRangedCombatValue(nullptr, true, value));
    CHECK(value == 7);
}

// sub_10a46600 over 0x744030 CGObject_C::DoFade: a time over 100000 ms becomes 1000.
COA_TEST(InitHooks_FadeTimeCap) {
    CHECK(CoA::InitHooks::FadeTime(0) == 0);
    CHECK(CoA::InitHooks::FadeTime(100000) == 100000);
    CHECK(CoA::InitHooks::FadeTime(100001) == 1000);
    CHECK(CoA::InitHooks::FadeTime(0xFFFFFFFF) == 1000);
}

// sub_10a79110 over 0x710390 CGGameObject_C::IsUsableMounted: entry 900010 always.
COA_TEST(InitHooks_UsableMounted) {
    CHECK(CoA::InitHooks::AlwaysUsableMounted(900010));
    CHECK(!CoA::InitHooks::AlwaysUsableMounted(900011));
    CHECK(!CoA::InitHooks::AlwaysUsableMounted(0));
}

// sub_10a79080 replacing 0x54D010 CGBattlefieldInfo::GetFlagToken.
COA_TEST(InitHooks_FlagToken) {
    CHECK(strcmp(CoA::InitHooks::FlagToken(0), "HordeFlag") == 0);
    CHECK(strcmp(CoA::InitHooks::FlagToken(1), "AllianceFlag") == 0);
    CHECK(strcmp(CoA::InitHooks::FlagToken(5), "AllianceFlag") == 0);
}

// FUN_10a46990 / FUN_10a45d80: a spell whose visual has a missile, an impact kit and follow-ground flags (and a class
// set) drops the 0x100 bit of the three trajectory intersections until the next targeting update.
COA_TEST(InitHooks_TrajectoryCollision) {
    Start();
    CHECK(CoA::InitHooks::TrajectoryIntersectFlags() == 0x100111);
    CoA::InitHooks::BeforeMissileFirePos(1, false, 1, 1, 1);
    CHECK(CoA::InitHooks::TrajectoryIntersectFlags() == 0x100111);
    CoA::InitHooks::BeforeMissileFirePos(0, true, 1, 1, 1);
    CoA::InitHooks::BeforeMissileFirePos(1, true, 0, 1, 1);
    CoA::InitHooks::BeforeMissileFirePos(1, true, 1, 0, 1);
    CoA::InitHooks::BeforeMissileFirePos(1, true, 1, 1, 0);
    CHECK(CoA::InitHooks::TrajectoryIntersectFlags() == 0x100111);
    CoA::InitHooks::BeforeMissileFirePos(4, true, 2, 3, 5);
    CHECK(CoA::InitHooks::TrajectoryIntersectFlags() == 0x100011);
    // A later spell that does not qualify leaves the bit off.
    CoA::InitHooks::BeforeMissileFirePos(0, false, 0, 0, 0);
    CHECK(CoA::InitHooks::TrajectoryIntersectFlags() == 0x100011);
    CoA::InitHooks::BeforeTrajectoryTargeting();
    CHECK(CoA::InitHooks::TrajectoryIntersectFlags() == 0x100111);
}

// sub_10a761c0 over 0x7512B0 UnitCombatLog: skipped while CombatLogDisableAll is on.
COA_TEST(InitHooks_UnitCombatLogSkipped) {
    Start();
    CoA::LuaReg::Reset();
    CoA::GlobalsJ::Init();
    CHECK(!CoA::InitHooks::UnitCombatLogSkipped());
    lua_State* L = CoATest::NewState(false);
    CoATest::Call(L, "", "CombatLogDisableAll");
    CHECK(CoA::InitHooks::UnitCombatLogSkipped());
    lua_close(L);
}
