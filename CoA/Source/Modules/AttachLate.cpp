// AscAttachLate.cpp -- see AttachLate.h. The byte patches and hook call sites are "(Extensions.dll patch / orig hook
// 0xADDR)" edits in the decomp.
#include "CoA/Source/Modules/AttachLate.h"
#include "CoA/Source/Modules/RealmInfo.h"
#include <cstdio>

namespace {
    void (*s_fatal)(const char* text) = nullptr;

    // FUN_10ae6220 (float -> 64-bit, low half) read back as unsigned.
    double AsUnsigned(float f) {
        return static_cast<double>(static_cast<uint32_t>(static_cast<int64_t>(f)));
    }
}

int32_t CoA::AttachLate::ScaleByRealmRate(float value) {
    return static_cast<int32_t>(value * CoA::RealmInfo::Get().rate1);
}

void CoA::AttachLate::RandomBgHonor(int32_t winHonor, int32_t winArena, int32_t lossHonor, double out[3]) {
    const CoA::RealmInfo::Info& realm = CoA::RealmInfo::Get();
    out[0] = AsUnsigned(static_cast<float>(winHonor) * realm.rate1);
    out[1] = AsUnsigned(static_cast<float>(winArena) * realm.rate2);
    out[2] = AsUnsigned(static_cast<float>(lossHonor) * realm.rate1);
}

// 0x10A45AA0
bool CoA::AttachLate::KnowsHigherRank(uint32_t spell, bool (*lookup)(uint32_t, uint32_t&), bool (*known)(uint32_t)) {
    uint32_t next;

    if (!lookup(spell, next)) {
        return false;
    }

    if (next == spell) {
        char text[160];
        snprintf(text, sizeof(text), "Spell: %u has recursive supercededBySpell in SkillLineAbility.dbc (dbc_skill_line_ability)", spell);

        if (s_fatal) {
            s_fatal(text);
        }

        return false;
    }

    while (next) {
        if (known(next)) {
            return true;
        }

        if (!lookup(next, next)) {
            return false;
        }
    }

    return false;
}

void CoA::AttachLate::SetFatalSink(void (*sink)(const char* text)) {
    s_fatal = sink;
}
