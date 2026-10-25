#ifndef COA_SOURCE_MODULES_ANIMATIONCOUNT_H
#define COA_SOURCE_MODULES_ANIMATIONCOUNT_H

#include <cstdint>

// The client's animation count (port of AscAnimationCount.cpp, FUN_10a51180 in the 0x634E00 set): the client hard-codes
// stock AnimationData.dbc's 506 rows (0x1FA) in 44 places; after the DBCs load each takes the loaded row count, so
// Ascension's extra animations are accepted. The WoW sites read Rows(); the Engine sites read g_animationCount
// (AnimationCountClient.cpp keeps it equal).
namespace CoA::AnimationCount {
    // 506 until Set.
    uint32_t Rows();
    void Set(uint32_t rows);
    // Tests only.
    void Reset();
}

#endif
