#ifndef COA_SOURCE_MODULES_SPELLTEXT_H
#define COA_SOURCE_MODULES_SPELLTEXT_H

#include <cstdint>
#include <string>
#include <vector>

// Spell text the original DLL builds from Spell.dbc (port of AscSpellText.cpp; all of it in SpellTextClient.cpp).
namespace CoA::SpellText {
    // FUN_101ac910 / FUN_101ac3d0: the spell's description as the client formats it (SpellParserParseText), then
    // through the DLL's @-markup pass (FUN_101a8e30). "" when the spell or its description is missing.
    std::string Description(uint32_t spellId, bool a, bool cached, int c);
    // FUN_101a8e30: rewrite the @-markup in text in place. a = show everything (as if SHIFT were held), stripQuotes
    // drops one pair of enclosing quotes, spell = the spell being described (@wflocation, @flyingmountspeed), collect
    // moves @s / @re / @wflocation results into collected instead of inlining them, noRequirements drops @req /
    // @unlockby. True when anything changed.
    bool Markup(std::string& text, bool a, bool cached, int stripQuotes, uint32_t spell, std::vector<std::string>* collected,
        bool collect, bool noRequirements);
    // The per-spell caches: the name (Spell +0x220) and the shapeshift forms the spell requires (FUN_101c1b60).
    const std::string& Name(uint32_t spellId);
    const std::string& FormText(uint32_t spellId);

    // What the markup asks other DLL modules: character advancement (AscCA::SpellKnown / IsCASpell), the game mode
    // (AscGameMode::Mode) and mystic enchants (Slotted / Worldforged / Collected). Each module installs its entry
    // when it is ported; until then the answer is false / 0.
    struct Deps {
        bool (*spellKnown)(uint32_t spell, bool includeRanks) = nullptr;
        bool (*isCASpell)(uint32_t spell) = nullptr;
        uint32_t (*gameMode)() = nullptr;
        bool (*mysticSlotted)(uint32_t spell) = nullptr;
        bool (*mysticWorldforged)(uint32_t spell) = nullptr;
        bool (*mysticCollected)(uint32_t spell) = nullptr;
    };

    Deps& Dependencies();
}

#endif
