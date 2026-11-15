#include "CoA/Source/Modules/Esteria/CreatureAppearance.h"
#include "CoA/Tests/TestMain.h"

COA_TEST(EveryOneNineNinePackRaceUsesTheCreatureAppearanceForBothGenders) {
    for (unsigned race = 80; race <= 91; ++race) {
        CHECK(CreatureAppearance::NewPackRace(race));
        CHECK(CreatureAppearance::Uses(race));
        CHECK(CreatureAppearance::GenderAllowed(race, 0));
        CHECK(CreatureAppearance::GenderAllowed(race, 1));
    }
    CHECK(!CreatureAppearance::NewPackRace(79));
    CHECK(!CreatureAppearance::NewPackRace(92));
}

COA_TEST(APackRaceOffersTheSkinsAndDetailsItsOwnFileHas) {
    CHECK(CreatureAppearance::Options(80, 0)[0].count == 10);
    CHECK(CreatureAppearance::Options(81, 1)[0].count == 64);
    CHECK(CreatureAppearance::PackControlCount(83, 0) == 1);
    CHECK(CreatureAppearance::PackControlCount(83, 1) == 2);
    CHECK(CreatureAppearance::PackControlCount(84, 0) == 5);
    CHECK(CreatureAppearance::PackControlCount(87, 0) == 2);
    CHECK(CreatureAppearance::PackControlCount(87, 1) == 1);
}

COA_TEST(APackRaceKeepsTheUnusedStockFieldsOutOfItsValidation) {
    CHECK(CreatureAppearance::Validate(80, 0, {9, 4, 7, 2, 1}));
    CHECK(!CreatureAppearance::Validate(80, 0, {10, 0, 0, 0, 0}));
    CHECK(CreatureAppearance::Validate(91, 1, {8, 4, 3, 0, 0}));
    CHECK(!CreatureAppearance::Validate(91, 1, {8, 5, 0, 0, 0}));
}
