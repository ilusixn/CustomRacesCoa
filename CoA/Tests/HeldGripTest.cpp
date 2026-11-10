#include "CoA/Source/Modules/HeldGrip.h"
#include "CoA/Tests/TestMain.h"

COA_TEST(HeldGripTakesPouchesBooksAndOrbsButNotWeaponsOrShields) {
    CHECK(CoA::HeldGrip::HeldLike(2, 14, 22));
    CHECK(CoA::HeldGrip::HeldLike(4, 0, 23));
    CHECK(!CoA::HeldGrip::HeldLike(2, 15, 22));
    CHECK(!CoA::HeldGrip::HeldLike(2, 4, 13));
    CHECK(!CoA::HeldGrip::HeldLike(4, 6, 14));
}

COA_TEST(HeldGripModelsAreTheTwoPlayersOgreAndItsChoGallLook) {
    CHECK(CoA::HeldGrip::HeldGripModel("Creature\\TwoHeadedOgre\\TwoHeadedOgreMale.m2"));
    CHECK(CoA::HeldGrip::HeldGripModel("creature\\chogall\\chogall.m2"));
    CHECK(!CoA::HeldGrip::HeldGripModel("Character\\Human\\Male\\HumanMale.m2"));
}
