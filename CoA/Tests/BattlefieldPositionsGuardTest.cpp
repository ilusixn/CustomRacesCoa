#include "CoA/Tests/TestMain.h"
#include "WoW/Source/Ui/BattlefieldPositionsGuard.h"

// MSG_BATTLEGROUND_PLAYER_POSITIONS (0x54B3F0): the server's two counts index m_playerPositionGuids / m_playerPositions
// [40] and m_flagCarriers / m_flagPositions [2]; the 32-bit client never checked them (the 3.3.5a RCE write primitive).
COA_TEST(BattlefieldPositions_PlayerCountGuard) {
    CHECK(BattlefieldPositionsGuard::PlayerCountValid(0));
    CHECK(BattlefieldPositionsGuard::PlayerCountValid(40));
    CHECK(!BattlefieldPositionsGuard::PlayerCountValid(41));
    CHECK(!BattlefieldPositionsGuard::PlayerCountValid(257488));
    CHECK(!BattlefieldPositionsGuard::PlayerCountValid(0xFFFFFFFFu));
}

COA_TEST(BattlefieldPositions_FlagCountGuard) {
    CHECK(BattlefieldPositionsGuard::FlagCountValid(0));
    CHECK(BattlefieldPositionsGuard::FlagCountValid(2));
    CHECK(!BattlefieldPositionsGuard::FlagCountValid(3));
    CHECK(!BattlefieldPositionsGuard::FlagCountValid(-1));   // negative counts take the "<= 0" reset path, never a write
    CHECK(!BattlefieldPositionsGuard::FlagCountValid(INT32_MIN));
}
