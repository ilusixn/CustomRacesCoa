#include "CoA/Source/Modules/HeldGrip.h"
#include "CoA/Tests/TestMain.h"

COA_TEST(HeldGripTakesPouchesBooksAndOrbsButNotWeaponsOrShields) {
    CHECK(CoA::HeldGrip::HeldLike(2, 14, 22));
    CHECK(CoA::HeldGrip::HeldLike(4, 0, 23));
    CHECK(!CoA::HeldGrip::HeldLike(2, 15, 22));
    CHECK(!CoA::HeldGrip::HeldLike(2, 4, 13));
    CHECK(!CoA::HeldGrip::HeldLike(4, 6, 14));
}
