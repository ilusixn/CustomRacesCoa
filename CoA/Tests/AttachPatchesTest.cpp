#include "CoA/Source/Modules/AttachPatches.h"
#include "CoA/Tests/TestMain.h"
#include <cstring>

COA_TEST(AttachPatches_LevelTablesAreEightEntries) {
    const int32_t levels[8] = { 18, 16, 14, 12, 10, 9, 7, 4 };
    const float distances[8] = { 210.0f, 180.0f, 150.0f, 120.0f, 90.0f, 60.0f, 40.0f, 25.0f };
    CHECK(sizeof(CoA::AttachPatches::kLevels) / sizeof(CoA::AttachPatches::kLevels[0]) == 8);
    CHECK(memcmp(CoA::AttachPatches::kLevels, levels, sizeof(levels)) == 0);
    CHECK(memcmp(CoA::AttachPatches::kDistances, distances, sizeof(distances)) == 0);
    CHECK(CoA::AttachPatches::kMaxMinimapZoom == 7);
}

COA_TEST(AttachPatches_UiScaleDefault) {
    CHECK(strcmp(CoA::AttachPatches::kUiScaleDefault, "0.85") == 0);
}

// 0x52B27A: jne -> jmp, so the idle logout never runs until SetAFKLogoutTimer turns it back on.
COA_TEST(AttachPatches_AfkLogoutDefaultAndOverride) {
    CHECK(!CoA::AttachPatches::AfkLogoutEnabled());
    CoA::AttachPatches::SetAfkLogoutEnabled(true);
    CHECK(CoA::AttachPatches::AfkLogoutEnabled());
    CoA::AttachPatches::SetAfkLogoutEnabled(false);
    CHECK(!CoA::AttachPatches::AfkLogoutEnabled());
}

// 0x728AA3: cmp ebx, 6 -> cmp ebx, 0 -- the rune refresh in DoPowerRegen follows power type 0 until AscCAMgr sets it.
COA_TEST(AttachPatches_RuneUiClassDefaultZero) {
    CHECK(CoA::AttachPatches::RuneUiClass() == 0);
    CoA::AttachPatches::SetRuneUiClass(6);
    CHECK(CoA::AttachPatches::RuneUiClass() == 6);
    CoA::AttachPatches::SetRuneUiClass(0);
}

// FUN_10a5f0e0: the client's 12 class colours with Hero (10) recoloured, then the 21 CoA classes; 0xAARRGGBB.
COA_TEST(AttachPatches_ClassColoursCoverCoAClasses) {
    CHECK(sizeof(CoA::AttachPatches::kClassColours) / sizeof(CoA::AttachPatches::kClassColours[0]) == 33);
    CHECK(CoA::AttachPatches::kClassColours[1] == 0xFFC69B6D);
    CHECK(CoA::AttachPatches::kClassColours[10] == 0xFFFFD624);
    CHECK(CoA::AttachPatches::kClassColours[12] == 0xFF8A3303);
    CHECK(CoA::AttachPatches::kClassColours[32] == 0xFF40C7EB);
}

// FUN_10a5e020: 22 race file names from the client's table, then "" up to 32.
COA_TEST(AttachPatches_RaceFileNamesThirtyTwo) {
    CHECK(sizeof(CoA::AttachPatches::kRaceFileNames) / sizeof(CoA::AttachPatches::kRaceFileNames[0]) == 32);
    CHECK(CoA::AttachPatches::kRaceFileNames[0] == nullptr);
    CHECK(strcmp(CoA::AttachPatches::kRaceFileNames[21], "IceTroll") == 0);
    CHECK(strcmp(CoA::AttachPatches::kRaceFileNames[22], "") == 0);
    CHECK(strcmp(CoA::AttachPatches::kRaceFileNames[31], "") == 0);
}
