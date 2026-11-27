#include "CoA/Source/Modules/QuestSpellHide.h"
#include "CoA/Source/Runtime/ClientHooks.h"

namespace {
    CoA::QuestSpellHide::Deps s_deps;
}

CoA::QuestSpellHide::Deps& CoA::QuestSpellHide::Dependencies() {
    return s_deps;
}

bool CoA::QuestSpellHide::HiddenSpell(uint32_t spell) {
    return spell == kSpell && s_deps.questInLog && s_deps.questInLog(kQuest);
}

bool CoA::QuestSpellHide::RemoveEffect(void*, void* effect) {
    return effect && s_deps.effectSpell && HiddenSpell(s_deps.effectSpell(effect));
}

bool CoA::QuestSpellHide::HideVisual(uint32_t, uint32_t, uint32_t, uint32_t spell) {
    return HiddenSpell(spell);
}

void CoA::QuestSpellHide::Reset() {
    s_deps = Deps();
}

void CoA::QuestSpellHide::Init() {
    CoA::ClientHooks::removeEffect.Add(RemoveEffect, 0x10112625);
    CoA::ClientHooks::visualHide.Add(HideVisual, 0x1011262F);
}
