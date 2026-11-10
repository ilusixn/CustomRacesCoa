#include "CoA/Source/Modules/CarriedMount.h"
#include "CoA/Tests/TestMain.h"

COA_TEST(OnlyTheMaleTwoHeadedOgrePlayerCarriesHisMount) {
    CHECK(CoA::CarriedMount::CarriesMounts(true, 85, 0));
    CHECK(CoA::CarriedMount::CarriesMounts(true, 85, 2));
    CHECK(!CoA::CarriedMount::CarriesMounts(true, 85, 1));
    CHECK(!CoA::CarriedMount::CarriesMounts(false, 85, 0));
    CHECK(!CoA::CarriedMount::CarriesMounts(true, 2, 0));
}

COA_TEST(TheCarriedMountSwapsStandAndMovementForCarryingAnims) {
    CHECK(CoA::CarriedMount::CarryingAnim(0) == CoA::CarriedMount::kCarryStand);
    for (uint32_t moving : { 4u, 5u, 11u, 12u, 13u, 92u, 93u, 119u, 143u }) {
        CHECK(CoA::CarriedMount::CarryingAnim(moving) == CoA::CarriedMount::kCarryMove);
    }
    CHECK(CoA::CarriedMount::CarryingAnim(17) == 17);
    CHECK(CoA::CarriedMount::CarryingAnim(37) == 37);
}

COA_TEST(OnlyTheTwoPlayersOgreBodyHasTheCarryPoint) {
    CHECK(CoA::CarriedMount::CarryPointModel("Creature\\TwoHeadedOgre\\TwoHeadedOgreMale.m2"));
    CHECK(!CoA::CarriedMount::CarryPointModel("Creature\\TwoHeadedOgre\\TwoHeadedOgreMaleHeld.m2"));
    CHECK(!CoA::CarriedMount::CarryPointModel("Creature\\ChoGall\\ChoGall.m2"));
}
