#ifndef COA_SOURCE_MODULES_SPELLTOKENSCLIENT_H
#define COA_SOURCE_MODULES_SPELLTOKENSCLIENT_H

#include <cstdint>

class SpellRec;

// AscSpellTokens.cpp's client half: what the decomp sites call with the client's records (SpellTokens.h has the logic).
namespace CoA::SpellTokensClient {
    // FUN_10a10c30 at the top of MathState::GetVariableValue (0x5782D0): true when the DLL evaluated `op`.
    bool Value(uint32_t op, const SpellRec* spell, float& out);
}

#endif
