#include "CoA/Source/Modules/QuestSpellHide.h"
#include "CoA/Tests/TestMain.h"

namespace {
    bool s_logged = false;

    void Setup(bool logged) {
        CoA::QuestSpellHide::Reset();
        s_logged = logged;
        CoA::QuestSpellHide::Deps& d = CoA::QuestSpellHide::Dependencies();
        d.questInLog = [](uint32_t quest) { return s_logged && quest == CoA::QuestSpellHide::kQuest; };
        d.effectSpell = [](void* effect) { return *static_cast<uint32_t*>(effect); };
    }
}

// Spell 0xE4E2D is hidden (effect and visual) only while quest 0x1D0BB4 is logged; other spells never are.
COA_TEST(QuestSpellHide_OnlyWhileQuestLogged) {
    uint32_t hidden = CoA::QuestSpellHide::kSpell;
    uint32_t other = 133;

    Setup(true);
    CHECK(CoA::QuestSpellHide::RemoveEffect(nullptr, &hidden));
    CHECK(CoA::QuestSpellHide::HideVisual(0, 1, 2, hidden));
    CHECK(!CoA::QuestSpellHide::RemoveEffect(nullptr, &other));
    CHECK(!CoA::QuestSpellHide::HideVisual(0, 1, 2, other));

    Setup(false);
    CHECK(!CoA::QuestSpellHide::RemoveEffect(nullptr, &hidden));
    CHECK(!CoA::QuestSpellHide::HideVisual(0, 1, 2, hidden));

    CoA::QuestSpellHide::Reset();
    CHECK(!CoA::QuestSpellHide::HiddenSpell(hidden));
}
