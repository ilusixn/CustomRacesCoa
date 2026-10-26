#ifndef COA_SOURCE_MODULES_SPELLAPICLIENT_H
#define COA_SOURCE_MODULES_SPELLAPICLIENT_H

#include "CoA/Source/Modules/SpellApi.h"

class SpellRec;

// AscSpellApi.cpp's client half (SpellApiClient.cpp).
namespace CoA::SpellApiClient {
    // The dwords FUN_10325130 reads, from a client Spell.dbc record.
    void Fields(const SpellRec& rec, CoA::SpellApi::SpellFields& out);
}

#endif
