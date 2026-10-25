#include "CoA/Source/Modules/AnimationCount.h"
#include "CoA/Tests/TestMain.h"

// The client's stock AnimationData.dbc row count until the tables load.
COA_TEST(AnimationCount_DefaultIsStock506) {
    CoA::AnimationCount::Reset();
    CHECK(CoA::AnimationCount::Rows() == 506);
}

// FUN_10a51180: every site takes the loaded row count.
COA_TEST(AnimationCount_FollowsLoadedRows) {
    CoA::AnimationCount::Set(700);
    CHECK(CoA::AnimationCount::Rows() == 700);
    CoA::AnimationCount::Reset();
}
