#ifndef COA_SOURCE_MODULES_SPELLVISUALSCLIENT_H
#define COA_SOURCE_MODULES_SPELLVISUALSCLIENT_H

class CGDynamicObject_C;
class CGUnit_C;
class CMissile;
class SpellRec;
class SpellVisualRec;

// AscSpellVisuals.cpp's client half: the detour bodies at the marked client sites (see SpellVisuals.h).
namespace CoA::SpellVisualsClient {
    // The call at 0x80ED4E (PendingSpellCast::OnSpellGo): the next CGUnit_C::GetAppropriateSpellVisual comes from there.
    struct SpellGoScope {
        SpellGoScope();
        ~SpellGoScope();
    };

    // 0x10322B50 around 0x720F80: before (true returns nullptr without the original; consumes the OnSpellGo mark) and after
    // (the visual record).
    bool SkipAppropriateVisual(const CGUnit_C* unit, const SpellRec* spell);
    void AfterAppropriateVisual(const CGUnit_C* unit, const SpellRec* spell, SpellVisualRec& visual);
    // 0x10322AD0 before 0x702DB0: the visual record HitTarget uses.
    const SpellVisualRec* MissileHitVisual(const CMissile* missile, const SpellRec* spell, const SpellVisualRec* visual);
    // 0x10322760 before 0x705100: true returns 0.
    bool SkipDynamicObject(const CGDynamicObject_C* object);
    // 0x10322810 after 0x7022D0.
    void AfterFireMissiles(const CMissile* missile);
}

#endif
