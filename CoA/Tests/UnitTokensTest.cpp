#include "CoA/Source/Modules/NamePlates.h"
#include "CoA/Source/Modules/UnitTokens.h"
#include "CoA/Source/Runtime/ClientState.h"
#include "CoA/Tests/TestMain.h"
#include <cstring>

namespace {
    int s_unitA = 0;
    int s_unitB = 0;
    char s_buffers[8][32];
    char* s_lines[8];

    void Start(bool inWorld) {
        CoA::NamePlates::Reset();
        CoA::ClientState::SetInWorldProbe(inWorld ? +[] { return true; } : +[] { return false; });

        for (int i = 0; i < 8; ++i) {
            s_lines[i] = s_buffers[i];
            s_buffers[i][0] = '\0';
        }
    }
}

COA_TEST(UnitTokens_AppendsTheNameplateToken) {
    Start(true);
    CoA::NamePlates::AddPlate(&s_unitA);
    CoA::NamePlates::AddPlate(&s_unitB);
    const uint32_t id = CoA::NamePlates::IdForUnit(&s_unitB);
    CHECK(id != 0);
    strcpy(s_buffers[0], "target");
    int32_t count = 1;
    CoA::UnitTokens::AfterTokens(s_lines, &count, 8, 32, &s_unitB);
    CHECK(count == 2);
    char want[32];
    snprintf(want, sizeof(want), "nameplate%u", id);
    CHECK(strcmp(s_buffers[1], want) == 0);
    CHECK(strcmp(s_buffers[0], "target") == 0);

    // No unit, or a unit without a plate: nothing.
    count = 0;
    CoA::UnitTokens::AfterTokens(s_lines, &count, 8, 32, nullptr);
    CHECK(count == 0);
    int other = 0;
    CoA::UnitTokens::AfterTokens(s_lines, &count, 8, 32, &other);
    CHECK(count == 0);
    CoA::NamePlates::Reset();
}

COA_TEST(UnitTokens_NothingOutOfTheWorldOrPastTheArray) {
    Start(false);
    CoA::NamePlates::AddPlate(&s_unitA);
    int32_t count = 0;
    CoA::UnitTokens::AfterTokens(s_lines, &count, 8, 32, &s_unitA);
    CHECK(count == 0);

    Start(true);
    CoA::NamePlates::AddPlate(&s_unitA);
    // A full array: the original writes past it; here nothing is added.
    count = 8;
    CoA::UnitTokens::AfterTokens(s_lines, &count, 8, 32, &s_unitA);
    CHECK(count == 8);
    // The client returned no array (no active player): the original writes through null; here nothing.
    count = 0;
    CoA::UnitTokens::AfterTokens(nullptr, &count, 8, 32, &s_unitA);
    CHECK(count == 0);
    CoA::NamePlates::Reset();
    CoA::ClientState::SetInWorldProbe(nullptr);
}
