#include "CoA/Source/Modules/CarriedMount.h"
#include "CoA/Tests/TestMain.h"

COA_TEST(OnlyTheMaleTwoHeadedOgrePlayerCarriesHisMount) {
    CHECK(CoA::CarriedMount::CarriesMounts(true, 85, 0));
    CHECK(CoA::CarriedMount::CarriesMounts(true, 85, 2));
    CHECK(!CoA::CarriedMount::CarriesMounts(true, 85, 1));
    CHECK(!CoA::CarriedMount::CarriesMounts(false, 85, 0));
    CHECK(!CoA::CarriedMount::CarriesMounts(true, 2, 0));
}

COA_TEST(WhileCarryingOnlyDeathLeavesTheCarryingPose) {
    for (uint32_t still : { 0u, 2u, 17u, 32u, 37u, 41u, 54u, 67u }) {
        CHECK(CoA::CarriedMount::CarryingAnim(still) == CoA::CarriedMount::kCarryStand);
    }
    for (uint32_t moving : { 4u, 5u, 11u, 12u, 13u, 42u, 92u, 93u, 119u, 143u, 223u }) {
        CHECK(CoA::CarriedMount::CarryingAnim(moving) == CoA::CarriedMount::kCarryMove);
    }
    for (uint32_t dying : { 1u, 6u, 7u }) {
        CHECK(CoA::CarriedMount::CarryingAnim(dying) == dying);
    }
}

COA_TEST(TheMountCastPlaysThePickUp) {
    for (uint32_t casting : { 2u, 31u, 32u, 52u, 54u, 125u }) {
        CHECK(CoA::CarriedMount::PickUpAnim(casting) == CoA::CarriedMount::kPickUp);
    }
    CHECK(CoA::CarriedMount::PickUpAnim(0) == 0);
    CHECK(CoA::CarriedMount::PickUpAnim(5) == 5);
}

COA_TEST(OnlyTheTwoPlayersOgreBodyHasTheCarryPoint) {
    CHECK(CoA::CarriedMount::CarryPointModel("Creature\\TwoHeadedOgre\\TwoHeadedOgreMale.m2"));
    CHECK(!CoA::CarriedMount::CarryPointModel("Creature\\TwoHeadedOgre\\TwoHeadedOgreMaleHeld.m2"));
    CHECK(!CoA::CarriedMount::CarryPointModel("Creature\\ChoGall\\ChoGall.m2"));
}
