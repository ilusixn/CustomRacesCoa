#ifndef COA_SOURCE_MODULES_UNITTOKENS_H
#define COA_SOURCE_MODULES_UNITTOKENS_H

#include <cstdint>

// AscUnitTokens.cpp, the two hooks of installer FUN_100a05d0:
//
//   0x60BB70  Script_GetTokenIDsFromGUID: after it, "nameplate<N>" is appended when the world is up
//             (CGGameUI::s_inWorld, 0xBD0792), the GUID names a unit and that unit has a nameplate (FUN_100a09e0).
//   0x84E350  lua_pushstring: a null string would become "" when the return address is one of five function entry
//             points -- which a return address never is, so it never does (FUN_100a0880; marker only).
namespace CoA::UnitTokens {
    // FUN_100a09e0. `lines` is the client's token array (nullptr when it returned none), `capacity` its length and
    // `bufferSize` each buffer's size; `unit` the GUID's unit object or nullptr.
    void AfterTokens(char** lines, int32_t* count, uint32_t capacity, uint32_t bufferSize, const void* unit);
}

#endif
