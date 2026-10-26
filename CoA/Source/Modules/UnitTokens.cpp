// AscUnitTokens.cpp (see UnitTokens.h): the nameplate unit token.
#include "CoA/Source/Modules/UnitTokens.h"
#include "CoA/Source/Modules/NamePlates.h"
#include "CoA/Source/Runtime/ClientState.h"
#include <cstdio>

// FUN_100a09e0. The original formats with an unbounded sprintf (FUN_100a4f80) into lines[*count] whatever the count
// and whether the client returned an array; here only into a buffer that exists (Ruling).
void CoA::UnitTokens::AfterTokens(char** lines, int32_t* count, uint32_t capacity, uint32_t bufferSize, const void* unit) {
    if (!CoA::ClientState::InWorld() || !unit || !count) {
        return;
    }

    const uint32_t id = CoA::NamePlates::IdForUnit(unit);

    if (!id || !lines || *count < 0 || static_cast<uint32_t>(*count) >= capacity) {
        return;
    }

    snprintf(lines[*count], bufferSize, "nameplate%d", static_cast<int32_t>(id));
    ++*count;
}
