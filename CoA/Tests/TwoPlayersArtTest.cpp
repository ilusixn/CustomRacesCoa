#include "CoA/Source/Modules/TwoPlayersArt.h"
#include "CoA/Tests/TestMain.h"

COA_TEST(TwoPlayersArtDressesAMaleModelInMaleArt) {
    CHECK(CoA::TwoPlayersArt::ItemSex(1, 985010, 985010) == 0);
    CHECK(CoA::TwoPlayersArt::ItemSex(1, 49, 50) == 1);
    CHECK(CoA::TwoPlayersArt::ItemSex(0, 985010, 985010) == 0);
    CHECK(CoA::TwoPlayersArt::ItemSex(1, 0, 0) == 1);
    CHECK(CoA::TwoPlayersArt::ItemSex(2, 985010, 985010) == 2);
}
