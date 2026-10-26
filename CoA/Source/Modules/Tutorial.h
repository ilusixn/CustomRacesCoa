#ifndef COA_SOURCE_MODULES_TUTORIAL_H
#define COA_SOURCE_MODULES_TUTORIAL_H

#include <cstdint>
#include <string>

// AscTutorial.cpp: C_Tutorial -- tutorials, tutorial keywords and mentors. The original's manager at 0x10d3c5a8
// (accessor FUN_10a18d70, ctor FUN_10a15d00, init 0x10a18c20, Lua table FUN_10a1dd80).
//
// Manager layout: +0x000 TutorialEntryContainer, +0x07C TutorialKeywordContainer, +0x0F8 MentorWhoisDataContainer;
// +0x174 rewarded tutorial ids; +0x180 active mentor specializations; +0x18C pending specialization; +0x190 mentor whois
// records; +0x19C mentor status; +0x19D last seen level; +0x1A0 tutorials not yet applicable; +0x1C0 applicable
// tutorials not yet complete.
//
// Tables (the DLL's own DBCs): Tutorial, TutorialCategories, TutorialKeywords, TutorialObjectives,
// TutorialObjectiveTypes, TutorialRewards, MentorSpecializations.
// Tutorial row: +0 id, +4 category, +8 completion id, +0xC start quest, +0x10..+0x34 previous tutorials (any one
// complete), +0x3C race mask, +0x40 class mask (64-bit), +0x48 min level, +0x4C max level, +0x50 int, +0x54 sort order,
// +0x58..+0x78 required spells (any one known), +0x7C required game modes, +0x80 excluded game modes, +0x84 expansion
// (3 = any), +0x88 required for mentor status, +0x8C / +0x90 / +0x9C strings, +0x94 name, +0x98 description,
// +0xA0..+0xB0 realm flags.
// Keyword row: +0 id, +4 bracketed, +8 colour, +0xC key, +0x10 / +0x14 strings, +0x18 name, +0x1C info text, +0x20
// index text, +0x24..+0x34 realm flags, +0x38 race mask, +0x3C class mask (64-bit).
namespace CoA::Tutorial {
    // The client questions; TutorialClient.cpp installs them.
    struct Deps {
        bool (*hasPlayer)() = nullptr;                        // 0x4038F0 found the active player
        uint8_t (*playerRace)() = nullptr;                    // descriptor +0x5C
        uint8_t (*playerClass)() = nullptr;                   // descriptor +0x5D
        uint32_t (*playerLevel)() = nullptr;                  // descriptor +0xD8
        bool (*knowsSpell)(uint32_t spell) = nullptr;         // 0x7260E0 on the player
        // FUN_103087e0: the id is in the client's completed-ACHIEVEMENT list (0xACFDB0). The original checks a
        // tutorial's completion id there.
        bool (*achievementDone)(uint32_t id) = nullptr;
        bool (*showTutorials)() = nullptr;                    // CVar showTutorials: absent or non-zero
    };

    Deps& Dependencies();

    // FUN_10219110: every applicable keyword name in `text` linked (or, plain, replaced by "name info index").
    std::string FormatKeywords(std::string text, bool plain, uint32_t exclude);

    void Init();
    // Tests only.
    void Reset();
}

#endif
