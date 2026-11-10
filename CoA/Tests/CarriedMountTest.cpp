#include "CoA/Source/Modules/CarriedMount.h"
#include "CoA/Tests/TestMain.h"

COA_TEST(OnlyTheMaleTwoHeadedOgrePlayerCarriesHisMount) {
    CHECK(CoA::CarriedMount::CarriesMounts(true, 85, 0));
    CHECK(CoA::CarriedMount::CarriesMounts(true, 85, 2));
    CHECK(!CoA::CarriedMount::CarriesMounts(true, 85, 1));
    CHECK(!CoA::CarriedMount::CarriesMounts(false, 85, 0));
    CHECK(!CoA::CarriedMount::CarriesMounts(true, 2, 0));
}
