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
                              "Creature\\HotSMounts\\MountBattleBeastRankEpic\\MountBattleBeastRankEpic.m2",
                              "Creature\\HotSMounts\\VoidSpeeder\\VoidSpeeder.m2",
                              "Creature\\HotSMounts\\MountSurfMecha19\\MountSurfMecha19.m2",
                              "Creature\\HotSMounts\\MountBearRank21S1\\MountBearRank21S1.m2",
                              "Creature\\HotSMounts\\MountWolfDark18\\MountWolfDark18.m2",
                              "Creature\\HotSMounts\\MountHeart\\MountHeart.m2",
                              "Creature\\HotSMounts\\MountBearWar\\MountBearWar.m2",
                              "Creature\\HotSMounts\\MountBearCelestial\\MountBearCelestial.m2",
                              "Creature\\HotSMounts\\Carpet\\Carpet.m2",
                              "Creature\\HotSMounts\\BlackheartsDoubloon\\BlackheartsDoubloon.m2",
                              "Creature\\HotSMounts\\MountSnowflake\\MountSnowflake.m2"}) {
        CoA::CarriedMount::CarryFit const fit = CoA::CarriedMount::DefaultFit(beast);
        CHECK(fit.Scale == 0.7f);
        CHECK(fit.Yaw == 10.0f);
        CHECK(fit.Roll == -90.0f);
        CHECK(fit.X == 0.0f);
        CHECK(fit.Y == -0.7f);
        CHECK(fit.Z == 0.5f);
    }
    for (char const* billie : {"Creature\\HotSMounts\\BillieGoatMount\\BillieGoatMount.m2",
                               "Creature\\HotSMounts\\GoatMount\\GoatMount.m2",
                               "Creature\\HotSMounts\\MountHellBillie\\MountHellBillie.m2",
                               "Creature\\HotSMounts\\MountSpiderEnforcer\\MountSpiderEnforcer.m2",
                               "Creature\\HotSMounts\\MoneyPig\\MoneyPig.m2",
                               "Creature\\HotSMounts\\MoneyPigVar3\\MoneyPigVar3.m2",
                               "Creature\\HotSMounts\\MountCatFire\\MountCatFire.m2",
                               "Creature\\HotSMounts\\MountCatHaunted\\MountCatHaunted.m2",
                               "Creature\\HotSMounts\\MountSurfNexusShark\\MountSurfNexusShark.m2",
                               "Creature\\HotSMounts\\MountSurfShark18\\MountSurfShark18.m2",
                               "Creature\\HotSMounts\\MountDogFooVar4\\MountDogFooVar4.m2",
                               "Creature\\HotSMounts\\MountMouseSummer20\\MountMouseSummer20.m2",
                               "Creature\\HotSMounts\\MountMonoWheelMadAxe19\\MountMonoWheelMadAxe19.m2"}) {
        CoA::CarriedMount::CarryFit const fit = CoA::CarriedMount::DefaultFit(billie);
        CHECK(fit.Scale == 1.0f);
        CHECK(fit.Yaw == 15.0f);
        CHECK(fit.Roll == 0.0f);
        CHECK(fit.X == 0.0f);
        CHECK(fit.Y == -0.1f);
        CHECK(fit.Z == 0.0f);
    }
    CHECK(CoA::CarriedMount::DefaultFit("Creature\\HotSMounts\\MountGoatAdultBillie\\MountGoatAdultBillie.m2").Z
        == -0.5f);
    CoA::CarriedMount::CarryFit const seeker =
        CoA::CarriedMount::DefaultFit("Creature\\HotSMounts\\MountOwlSeekerAltered20\\MountOwlSeekerAltered20.m2");
    CHECK(seeker.Scale == 0.8f);
    CHECK(seeker.Yaw == 15.0f);
    CHECK(seeker.Roll == 0.0f);
    CHECK(seeker.X == 0.0f);
    CHECK(seeker.Y == 0.0f);
    CHECK(seeker.Z == -0.2f);
    CoA::CarriedMount::CarryFit const soapBar = CoA::CarriedMount::DefaultFit(
        "Creature\\HotSMounts\\MountHearthstoneCardSoapBar\\MountHearthstoneCardSoapBar.m2");
    CHECK(soapBar.Scale == 0.8f);
    CHECK(soapBar.Yaw == 15.0f);
    CHECK(soapBar.Roll == 90.0f);
    CHECK(soapBar.X == -0.2f);
    CHECK(soapBar.Y == 0.4f);
    CHECK(soapBar.Z == 0.7f);
    CoA::CarriedMount::CarryFit const hare =
        CoA::CarriedMount::DefaultFit("Creature\\HotSMounts\\MountRedHare\\MountRedHare.m2");
    CHECK(hare.Scale == 0.8f);
    CHECK(hare.Yaw == 15.0f);
    CHECK(hare.Roll == 0.0f);
    CHECK(hare.X == 0.0f);
    CHECK(hare.Y == 0.0f);
    CHECK(hare.Z == -0.5f);
    CoA::CarriedMount::CarryFit const invincilisk =
        CoA::CarriedMount::DefaultFit("Creature\\HotSMounts\\MountHorseCraft20\\MountHorseCraft20.m2");
    CHECK(invincilisk.Scale == 0.8f);
    CHECK(invincilisk.X == 0.0f);
    CHECK(invincilisk.Z == -0.5f);
    CHECK(CoA::CarriedMount::DefaultFit("Creature\\HotSMounts\\MountNazeebra\\MountNazeebra.m2").Z == -0.5f);
    CoA::CarriedMount::CarryFit const stallion = CoA::CarriedMount::DefaultFit(
        "Creature\\HotSMounts\\MountHorseGhostInvisible\\MountHorseGhostInvisible.m2");
    CHECK(stallion.Scale == 0.8f);
    CHECK(stallion.X == 0.0f);
    CHECK(stallion.Z == -0.5f);
    CoA::CarriedMount::CarryFit const unicorn =
        CoA::CarriedMount::DefaultFit("Creature\\HotSMounts\\HorseUnicornRainbow\\HorseUnicornRainbow.m2");
    CHECK(unicorn.Scale == 1.0f);
    CHECK(unicorn.Yaw == 15.0f);
    CHECK(unicorn.Roll == 0.0f);
    CHECK(unicorn.X == 0.0f);
    CHECK(unicorn.Y == 0.0f);
    CHECK(unicorn.Z == -0.5f);
    CoA::CarriedMount::CarryFit const phoenix =
        CoA::CarriedMount::DefaultFit("Creature\\HotSMounts\\MountPhoenix\\MountPhoenix.m2");
    CHECK(phoenix.Scale == 0.8f);
    CHECK(phoenix.Yaw == 15.0f);
    CHECK(phoenix.Roll == 0.0f);
    CHECK(phoenix.X == -0.2f);
    CHECK(phoenix.Y == -0.1f);
    CHECK(phoenix.Z == -0.9f);
    for (char const* charger : {"Creature\\HotSMounts\\NexusChargerGold\\NexusChargerGold.m2",
                                "Creature\\HotSMounts\\TyraelsCharger\\TyraelsCharger.m2",
                                "Creature\\HotSMounts\\MountDeer_Rank16_01\\MountDeer_Rank16_01.m2",
                                "Creature\\HotSMounts\\ReignDeerIce17\\ReignDeerIce17.m2",
                                "Creature\\HotSMounts\\MountAntelopeRank18_01\\MountAntelopeRank18_01.m2",
                                "Creature\\HotSMounts\\MountBalloonDuckSummer20\\MountBalloonDuckSummer20.m2"}) {
        CoA::CarriedMount::CarryFit const fit = CoA::CarriedMount::DefaultFit(charger);
        CHECK(fit.Scale == 0.8f);
        CHECK(fit.Yaw == 15.0f);
        CHECK(fit.Roll == 0.0f);
        CHECK(fit.X == -0.2f);
        CHECK(fit.Y == -0.1f);
        CHECK(fit.Z == -0.4f);
    }
    CoA::CarriedMount::CarryFit const surfboard =
        CoA::CarriedMount::DefaultFit("Creature\\HotSMounts\\MountSurfboardBite\\MountSurfboardBite.m2");
    CHECK(surfboard.Scale == 1.0f);
    CHECK(surfboard.Yaw == 10.0f);
    CHECK(surfboard.Roll == -90.0f);
    CHECK(surfboard.X == 0.0f);
    CHECK(surfboard.Y == -0.6f);
    CHECK(surfboard.Z == 0.5f);
    CoA::CarriedMount::CarryFit const turtle = CoA::CarriedMount::DefaultFit(
        "Creature\\HotSMounts\\MountTurtleRankSpr19Epic\\MountTurtleRankSpr19Epic.m2");
    CHECK(turtle.Scale == 0.5f);
    CHECK(turtle.Yaw == 10.0f);
    CHECK(turtle.Roll == -90.0f);
    CHECK(turtle.X == 0.0f);
    CHECK(turtle.Y == -0.5f);
    CHECK(turtle.Z == 0.5f);
    CHECK(CoA::CarriedMount::DefaultFit("Creature\\HotSMounts\\MountEyePadDark18\\MountEyePadDark18.m2").Scale == 0.5f);
    CHECK(CoA::CarriedMount::DefaultFit("Creature\\HotSMounts\\MountEye\\MountEye.m2").Scale == 0.5f);
    CoA::CarriedMount::CarryFit const reaver =
        CoA::CarriedMount::DefaultFit("Creature\\HotSMounts\\MountReaver\\MountReaver.m2");
    CHECK(reaver.Scale == 0.7f);
    CHECK(reaver.Yaw == 15.0f);
    CHECK(reaver.Roll == 0.0f);
    CHECK(reaver.X == -0.4f);
    CHECK(reaver.Y == -0.2f);
    CHECK(reaver.Z == -0.2f);
    CHECK(CoA::CarriedMount::DefaultFit("Creature\\HotSMounts\\LunarDragon\\LunarDragon.m2").X == -0.4f);
    CHECK(CoA::CarriedMount::DefaultFit("Creature\\HotSMounts\\MountHearthstoneCard\\MountHearthstoneCard.m2").Roll
        == 90.0f);
    CHECK(CoA::CarriedMount::DefaultFit(
        "Creature\\HotSMounts\\MountHearthstoneCardMedivh\\MountHearthstoneCardMedivh.m2").Z == 0.7f);
    CHECK(CoA::CarriedMount::DefaultFit("Creature\\HotSMounts\\MountDollarBill\\MountDollarBill.m2").Roll == 90.0f);
    CHECK(CoA::CarriedMount::DefaultFit("Creature\\HotSMounts\\MountCrabRank18Epic\\MountCrabRank18Epic.m2").Z == 0.7f);
    CHECK(CoA::CarriedMount::DefaultFit(
        "Creature\\HotSMounts\\MountSurfRedPocketLunar20\\MountSurfRedPocketLunar20.m2").Roll == 90.0f);
    CHECK(CoA::CarriedMount::DefaultFit("Creature\\HotSMounts\\MountBannerDorm18\\MountBannerDorm18.m2").Y == 0.4f);
    CHECK(CoA::CarriedMount::DefaultFit("Creature\\HotSMounts\\MountMeatWagon\\MountMeatWagon.m2").Roll == 90.0f);
    for (char const* raptor : {"Creature\\HotSMounts\\MountRaptorCelestial\\MountRaptorCelestial.m2",
                               "Creature\\HotSMounts\\MountRaptor_Rank17_01\\MountRaptor_Rank17_01.m2",
                               "Creature\\HotSMounts\\VultureMount\\VultureMount.m2",
                               "Creature\\HotSMounts\\MountRaccoonMobster19\\MountRaccoonMobster19.m2",
                               "Creature\\HotSMounts\\SpeederBikeHellfire\\SpeederBikeHellfire.m2",
                               "Creature\\HotSMounts\\MountSpeederBikeOni\\MountSpeederBikeOni.m2",
                               "Creature\\HotSMounts\\MountDogSummer18\\MountDogSummer18.m2",
                               "Creature\\HotSMounts\\MountCerberusMyth20\\MountCerberusMyth20.m2",
                               "Creature\\HotSMounts\\MountPigPinata\\MountPigPinata.m2",
                               "Creature\\HotSMounts\\MountPig_Rank18_01\\MountPig_Rank18_01.m2",
                               "Creature\\HotSMounts\\MountTigerAltered20\\MountTigerAltered20.m2",
                               "Creature\\HotSMounts\\MountDogBandage19\\MountDogBandage19.m2",
                               "Creature\\HotSMounts\\LionGreymane\\LionGreymane.m2",
                               "Creature\\HotSMounts\\MountPigFel\\MountPigFel.m2",
                               "Creature\\HotSMounts\\MountPigSpring19\\MountPigSpring19.m2",
                               "Creature\\HotSMounts\\MountPigNexus\\MountPigNexus.m2",
                               "Creature\\HotSMounts\\DirewolfMount\\DirewolfMount.m2",
                               "Creature\\HotSMounts\\CyberWolfGold\\CyberWolfGold.m2",
                               "Creature\\HotSMounts\\LunarFestivalRocket\\LunarFestivalRocket.m2",
                               "Creature\\HotSMounts\\MountRamCyber\\MountRamCyber.m2",
                               "Creature\\HotSMounts\\MountRamWCAV_01\\MountRamWCAV_01.m2",
                               "Creature\\HotSMounts\\TigerLunarMount\\TigerLunarMount.m2",
                               "Creature\\HotSMounts\\MountLion_Rank16_01\\MountLion_Rank16_01.m2",
                               "Creature\\HotSMounts\\Felstalker\\Felstalker.m2",
                               "Creature\\HotSMounts\\FelstalkerDemon\\FelstalkerDemon.m2",
                               "Creature\\HotSMounts\\MountRoosterLunar19\\MountRoosterLunar19.m2",
                               "Creature\\HotSMounts\\MountRaven\\MountRaven.m2",
                               "Creature\\HotSMounts\\MountZergling\\MountZergling.m2"}) {
        CoA::CarriedMount::CarryFit const fit = CoA::CarriedMount::DefaultFit(raptor);
        CHECK(fit.Scale == 0.8f);
        CHECK(fit.Yaw == 15.0f);
        CHECK(fit.Z == -0.2f);
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

COA_TEST(OnlyFlyingMountsHoldStillByDefault) {
    using CoA::CarriedMount::FlyingModel;
    using CoA::CarriedMount::HoldsStill;
    CHECK(FlyingModel(true, false, true, true));
    CHECK(FlyingModel(false, true, true, true));
    CHECK(FlyingModel(false, false, false, false));
    CHECK(!FlyingModel(false, false, true, true));
    CoA::CarriedMount::CarryFit fit;
    CHECK(HoldsStill(fit, true));
    CHECK(!HoldsStill(fit, false));
    fit.Pose = -1.0f;
    CHECK(!HoldsStill(fit, true));
    fit.Pose = 0.0f;
    fit.Frame = 1.0f;
    CHECK(HoldsStill(fit, false));
    fit.Frame = 0.0f;
    fit.Pose = 41.0f;
    CHECK(HoldsStill(fit, false));
}
