#ifndef COA_SOURCE_MODULES_SPELLMODSCLIENT_H
#define COA_SOURCE_MODULES_SPELLMODSCLIENT_H

#include <cstdint>

class CGUnit_C;
class SpellRec;

// AscSpellMods.cpp's client half: what the decomp sites call with the client's records (SpellMods.h has the logic).
namespace CoA::SpellModsClient {
    // 0x103288A0, replacing Spell_C_GetSpellModifiers (0x7FD970).
    bool GetSpellModifiers(const SpellRec* spell, int32_t modifier, int32_t& flat, int32_t& pct);
    // 0x10328DB0 over Spell_C_GetPowerCostPerSecond (0x7FF100): the replacement, then the scaling of the result.
    bool PowerPerSecondReplaced(const SpellRec* spell, const CGUnit_C* unit, int32_t& out);
    int32_t AfterPowerPerSecond(const SpellRec* spell, const CGUnit_C* unit, int32_t result);
    // 0x10328CB0 after Spell_C_GetPowerCost (0x8012F0).
    int32_t AfterPowerCost(const SpellRec* spell, const CGUnit_C* unit, int32_t result);
    // 0x103284E0 after SpellRec::GetCastTime (0x7FF180).
    int32_t AfterCastTime(const SpellRec* spell, int32_t result);
}

#endif
