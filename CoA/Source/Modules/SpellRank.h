#ifndef COA_SOURCE_MODULES_SPELLRANK_H
#define COA_SOURCE_MODULES_SPELLRANK_H

#include <cstdint>
#include <vector>

// AscSpellRank.cpp: spell rank chains from the DLL's SpellRank.dbc (+4 first rank, +8 spell, +0xC rank number) and the
// client's Talent.dbc rank columns (FUN_10216740's maps 0x10be0558 / ...). Built on first use. SpellRankClient.cpp.
namespace CoA::SpellRank {
    // The chain's first rank, or the spell itself.
    uint32_t FirstRank(uint32_t spell);
    // The 1-based rank number, or 1.
    uint32_t RankNumber(uint32_t spell);
    // Every rank under a first rank, in table order.
    const std::vector<uint32_t>& Chain(uint32_t firstRank);
}

#endif
