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

COA_TEST(BothTwoPlayersBodiesHaveTheCarryPoint) {
    CHECK(CoA::CarriedMount::CarryPointModel("Creature\\TwoHeadedOgre\\TwoHeadedOgreMale.m2"));
    CHECK(CoA::CarriedMount::CarryPointModel("Creature\\ChoGall\\ChoGall.m2"));
    CHECK(!CoA::CarriedMount::CarryPointModel("Creature\\TwoHeadedOgre\\TwoHeadedOgreMaleHeld.m2"));
    CHECK(!CoA::CarriedMount::CarryPointModel("Creature\\ChoGall\\ChoGallHeld.m2"));
}

COA_TEST(VehicleMountsAreCarriedAsAHorse) {
    CHECK(CoA::CarriedMount::DefaultFit("CREATURE\\GOBLINHOTROD\\GOBLINHOTROD.M2").As == CoA::CarriedMount::kStandInHorse);
    CHECK(CoA::CarriedMount::DefaultFit("Creature\\MotorcycleVehicle\\MotorcycleVehicle.mdx").As == 2404);
    CHECK(CoA::CarriedMount::DefaultFit("Creature\\Gokart\\Gokart.m2").As == 2404);
    CoA::CarriedMount::CarryFit const drive = CoA::CarriedMount::DefaultFit("Creature\\drivemount\\drivemount.mdx");
    CHECK(drive.As == 0);
    CHECK(drive.Scale == 0.8f);
    CHECK(drive.Roll == 90.0f);
    CHECK(drive.Z == 1.3f);
    CHECK(drive.Y == CoA::CarriedMount::kOffsetY);
    CoA::CarriedMount::CarryFit const disc =
        CoA::CarriedMount::DefaultFit("Creature\\HotSMounts\\MountNexagonFAL16\\MountNexagonFAL16.m2");
    CHECK(disc.Scale == 1.0f);
    CHECK(disc.Yaw == 0.7f);
    CHECK(disc.Roll == 90.0f);
    CHECK(disc.Y == 0.3f);
    CHECK(disc.Z == 1.0f);
    CoA::CarriedMount::CarryFit const hotsHorse =
        CoA::CarriedMount::DefaultFit("Creature\\HotSMounts\\HorseMount\\HorseMount.m2");
    CHECK(hotsHorse.Scale == 1.0f);
    CHECK(hotsHorse.Yaw == 10.0f);
    CHECK(hotsHorse.X == -0.2f);
    CHECK(hotsHorse.Y == 0.0f);
    CHECK(hotsHorse.Z == -0.6f);
    CoA::CarriedMount::CarryFit const warhorse =
        CoA::CarriedMount::DefaultFit("Creature\\HotSMounts\\MountWarhorseChess\\MountWarhorseChess.m2");
    CHECK(warhorse.Yaw == 10.0f);
    CHECK(warhorse.Z == -0.6f);
    for (char const* sheep : {"Creature\\HotSMounts\\MountPlushSheepToys21\\MountPlushSheepToys21.m2",
                              "Creature\\HotSMounts\\MountSheepCraft20\\MountSheepCraft20.m2"}) {
        CoA::CarriedMount::CarryFit const fit = CoA::CarriedMount::DefaultFit(sheep);
        CHECK(fit.Scale == 1.0f);
        CHECK(fit.Yaw == 10.0f);
        CHECK(fit.X == -0.2f);
        CHECK(fit.Y == 0.0f);
        CHECK(fit.Z == -0.3f);
    }
    for (char const* beast : {"Creature\\HotSMounts\\BattleBeastNexusMount\\BattleBeastNexusMount.m2",
                              "Creature\\HotSMounts\\BattleBeastButcherMount\\BattleBeastButcherMount.m2",
                              "Creature\\HotSMounts\\MountBattleBeastRankEpic\\MountBattleBeastRankEpic.m2"}) {
        CoA::CarriedMount::CarryFit const fit = CoA::CarriedMount::DefaultFit(beast);
        CHECK(fit.Scale == 0.7f);
        CHECK(fit.Yaw == 10.0f);
        CHECK(fit.Roll == -90.0f);
        CHECK(fit.X == 0.0f);
        CHECK(fit.Y == -0.7f);
        CHECK(fit.Z == 0.5f);
    }
    CHECK(CoA::CarriedMount::DefaultFit("Creature\\RidingHorse\\RidingHorse.mdx").As == 0);
    CHECK(CoA::CarriedMount::DefaultFit("Creature\\RidingHorse\\RidingHorse.mdx").Y == CoA::CarriedMount::kOffsetY);
}

COA_TEST(EachMountKeepsItsOwnFit) {
    CoA::CarriedMount::CarryFit car;
    CoA::CarriedMount::SetFitValue(car, 3, 90.0f);
    CoA::CarriedMount::SetFitValue(car, 6, -0.25f);
    CoA::CarriedMount::CarryFit horse;
    CoA::CarriedMount::SetFitValue(horse, 7, 2404.0f);
    const std::string saved = CoA::CarriedMount::FormatCarryFits({ { 31988, car }, { 2410, horse } });
    const auto loaded = CoA::CarriedMount::ParseCarryFits(saved + "garbage line\n\n");
    CHECK(loaded.size() == 2);
    CHECK(loaded.at(31988).Roll == 90.0f);
    CHECK(loaded.at(31988).Z == -0.25f);
    CHECK(loaded.at(31988).As == 0);
    CHECK(loaded.at(2410).As == 2404);
    CHECK(loaded.at(2410).Scale == CoA::CarriedMount::kScale);
}

COA_TEST(AHandWrittenFitNeedsOnlyTheChangedValues) {
    const auto loaded = CoA::CarriedMount::ParseCarryFits("36022 PITCH=-90 as=0\r\n");
    CHECK(loaded.at(36022).Pitch == -90.0f);
    CHECK(loaded.at(36022).Y == CoA::CarriedMount::kOffsetY);
}

COA_TEST(EachMountKeepsItsOwnHeldPose) {
    CoA::CarriedMount::CarryFit fish;
    CHECK(fish.Pose == 0.0f && fish.Frame == 0.0f);
    CoA::CarriedMount::SetFitValue(fish, 8, 41.0f);
    CoA::CarriedMount::SetFitValue(fish, 9, 500.0f);
    const auto loaded = CoA::CarriedMount::ParseCarryFits(CoA::CarriedMount::FormatCarryFits({ { 99004, fish } }));
    CHECK(loaded.at(99004).Pose == 41.0f);
    CHECK(loaded.at(99004).Frame == 500.0f);
    CHECK(loaded.at(99004).As == 0);
    CHECK(CoA::CarriedMount::ParseCarryFits("99004 pose=-1\n").at(99004).Pose == -1.0f);
}

