#ifndef COA_SOURCE_MODULES_ATTACHLATE_H
#define COA_SOURCE_MODULES_ATTACHLATE_H

#include <cstdint>

// AscAttachLate.cpp: FUN_10a60d20's 32 client byte patches (marked source edits) and the attach init's hook groups at
// 0x10A6271A / FUN_10a60d20. What the hooks compute without client objects lives here.
namespace CoA::AttachLate {
    // 0x5E4B70 (GetQuestLogRewardHonor): the client's value times the realm's first rate, truncated.
    int32_t ScaleByRealmRate(float value);

    // 0x54A370 (GetRandomBGHonorCurrencyBonuses): win honor x rate1, win arena x rate2, loss honor x rate1, each as the
    // unsigned low half of the float product (FUN_10ae6220).
    void RandomBgHonor(int32_t winHonor, int32_t winArena, int32_t lossHonor, double out[3]);

    // 0x726160 (CGUnit_C::IsSpellSuperceded): whether a later rank along SkillLineAbility's supercededBy chain is known.
    // lookup gives a spell's supercededBy (false: no ability row). A spell that supersedes itself is fatal.
    bool KnowsHigherRank(uint32_t spell, bool (*lookup)(uint32_t spell, uint32_t& supercededBy), bool (*known)(uint32_t spell));

    // Where AscCrashContext::Assert goes (the client's fatal error; tests record it). nullptr: the default (nothing).
    void SetFatalSink(void (*sink)(const char* text));
}

#endif
