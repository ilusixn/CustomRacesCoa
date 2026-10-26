#ifndef COA_SOURCE_MODULES_INITHOOKS_H
#define COA_SOURCE_MODULES_INITHOOKS_H

#include <cstdint>

// AscInitHooks.cpp: the 23 detours of the original's main init FUN_10a60d20. Most are a line at their client site (the
// markers there); the decisions with state or data live here.
//
//   0x717910  CGUnit_C::SetMountModel: after it, a null model sets m_mountAnimBehaviorId (+0xB7C) to 0x5B.
//   0x740450  CGUnit_C::OnMountDisplayChanged: after it, UpdateSelectionRadius.
//   0x720BF0  CGUnit_C::OverrideMissileFirePos / 0x6FDA20 UpdateMissileTrajectoryTargeting: the 0x100 bit of the three
//             ComputeMissileTrajectory intersections (the immediates 0x6FD335 / 0x6FD4C7 / 0x6FD59E) -- see
//             BeforeMissileFirePos.
//   0x756240  CGUnit_C::HandleCombatAnimEvent: its je at 0x756458 is a jmp while it runs (the only code that reaches
//             it): the target's PlayVictimRound at the swing's hit event is skipped ...
//   0x755130  CGUnit_C::PlayAttackerRound: ... because the round's target (a unit) plays it before the attacker's round.
//   0x7512B0  UnitCombatLog: skipped while CombatLogDisableAll is on.
//   0x6DE330  CGPlayer_C::GetVisibleItem: from 0x62BAC8 (the tooltip's equipped item-set count) on the active player,
//             the entry of the item in the inventory slot instead of the visible (displayed) one.
//   0x401A80  DesktopGammaCallback: its value is always "0" (the callback ignores it here; marker only).
//   0x5E0420 / 0x5E0D20 / 0x61E5C0  CalculateExperience / GetQuestLevel / CGTooltip::GetQuestLink: a quest in the
//             player's quest log (25 slots) takes its XP (addon field 0x24 + slot) or level (0x3D + slot) from the
//             player's object addon fields.
//   0x6E4950  CGPlayer_C::CombatModeEnter: autoRangedCombat "1" -> its int is whether the ranged slot holds an item.
//   0x7441D0  CGObject_C::HandleScaleUpdate: after it, a game object's UpdateWorldObject(1).
//   0x52E4D0  CGBarberShop::AppearanceApplied: after it, the active player stands up (ChangeStandState(0)).
//   0x6E9600  CMovement_C::SetCollisionHeight: after it, CGGameUI::ResetCamera(0).
//   0x7234D0  UnitPowerUpdateHandler: after it, for the active player, CGSpellBook::UpdateUsable.
//   0x59D0B0  CGAuctionHouse::IsSellItemValid: false for one of the four equipped bags.
//   0x7FF070  Spell_C_GetSpellLevel: 0 when called from SpellParserParseText (0x57ABE4).
//   0x8D2400  FMOD_strcmp: 1 when either string is null.
//   0x744030  CGObject_C::DoFade: see FadeTime.
//   0x710390  CGGameObject_C::IsUsableMounted: see AlwaysUsableMounted.
//   0x54D010  CGBattlefieldInfo::GetFlagToken: replaced by FlagToken.
namespace CoA::InitHooks {
    struct Deps {
        // The quest id in the active player's quest log slot; false without an active player.
        bool (*questLogQuest)(uint32_t slot, uint32_t& questId) = nullptr;
        uint64_t (*activePlayerGuid)() = nullptr;
    };

    Deps& Dependencies();

    // sub_10a45c60: the value a null mount model leaves in m_mountAnimBehaviorId.
    constexpr int32_t kNoMountAnimBehavior = 0x5B;

    // sub_10a44ae0: true (and the XP) when the quest is in the player's log; the original is then not run.
    bool QuestExperience(uint32_t questId, int32_t& xp);
    // sub_10a790a0: true (and the level) when the quest is in the player's log.
    bool QuestLevel(uint32_t questId, int32_t& level);
    // sub_10a4d770: the level GetQuestLink goes on with.
    int32_t QuestLinkLevel(uint32_t questId, int32_t level);

    // sub_10a44860: true when the CVar's int is to be rewritten (its string is exactly "1").
    bool AutoRangedCombatValue(const char* value, bool rangedEquipped, int32_t& out);

    // sub_10a46600: over 100000 ms becomes 1000.
    uint32_t FadeTime(uint32_t time);
    // sub_10a79110: the game object with entry 900010 is usable while mounted.
    bool AlwaysUsableMounted(uint32_t entry);
    // sub_10a79080.
    const char* FlagToken(uint32_t index);

    // FUN_10a46990, before OverrideMissileFirePos: a spell with a class set (+0x240) whose visual row exists and has a
    // missile (+0x1C), an impact kit (+0x0C) and follow-ground flags (+0x50) turns the 0x100 bit off.
    void BeforeMissileFirePos(int32_t spellClassSet, bool haveVisual, int32_t hasMissile, int32_t impactKit, int32_t followGroundFlags);
    // FUN_10a45d80, before UpdateMissileTrajectoryTargeting: the bit back on.
    void BeforeTrajectoryTargeting();
    // The flags ComputeMissileTrajectory's three World::Intersect calls pass.
    uint32_t TrajectoryIntersectFlags();

    // sub_10a761c0.
    bool UnitCombatLogSkipped();

    // Tests only.
    void Reset();
}

#endif
