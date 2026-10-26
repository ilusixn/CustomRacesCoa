#ifndef COA_SOURCE_MODULES_SPELLVISUALS_H
#define COA_SOURCE_MODULES_SPELLVISUALS_H

#include "CoA/Source/Modules/SpellApi.h"
#include <cstdint>

class CDataStore;
struct CVar;

// AscSpellVisuals.cpp (installer FUN_10322fb0): server-assigned spell visuals per object, the
// hideOtherPlayerHarmfulSpellVisuals option and the report of a missile launch to the server.
//
//   Map        FUN_10323200 (0x10BE447C): guid kind (FUN_102cdc10) -> guid counter (FUN_100a03b0) -> spell -> SpellVisual
//              id. SMSG 0x662 (FUN_10322ee0): u64 guid, u32 spell, u32 visual. Never cleared.
//   Resolve    FUN_10322c70(kind, guid, spell): 0xFFFFFFFF when list 0x10BE2D98 hides it (FUN_10277870); for kind 4 whose
//              counter is the active player's GUID, the applied appearance's visual (AscAppearance); else the map; else 0.
//   0x720F80 CGUnit_C::GetAppropriateSpellVisual -> 0x10322B50: called from 0x80ED4E (PendingSpellCast::OnSpellGo) on
//              another player (type 4) with the option on and a harmful spell (FUN_10325940 false): returns 0 without the
//              original. Otherwise, after the original, the visual record (a2, 0x80 bytes) is zeroed for 0xFFFFFFFF or
//              replaced by the resolved SpellVisual row (kind = the object's type +0x14); the result is the original's.
//   0x702DB0 CMissile::HitTarget -> 0x10322AD0: the visual record argument is replaced before the original (nullptr for
//              0xFFFFFFFF; a resolved id without a row keeps it); kind from the caster GUID (+0).
//   0x705100 CGDynamicObject_C::GetModelFileName -> 0x10322760: with the option on, a dynamic object another player
//              (descriptor +0x18, an existing player object) cast with a harmful spell (+0x24) is not created (returns 0).
//   0x7022D0 CMissile::FireMissiles -> 0x10322810: after the original, CMSG 0x9C7 {u64 caster, u64 target, u32 spell,
//              u8 cast id, launch xyz, impact xyz} for our own missiles and those of units no other player owns
//              (charmed +0x30 / summoned +0x38 / created +0x40).
//   Lists      0x10323300 (list 0x10BE2B6C before 0x724820, site 0x10323151): the spell record's first visual (+0x20C)
//              follows the resolved visual (0 for 0xFFFFFFFF, unchanged for 0 or a missing row), and spell 0x31029 is
//              refused for the active player and units it summoned. 0x10323480 (list 0x10BE2D78, site 0x1032313D) removes,
//              and 0x10323410 (list 0x10BE2D98, site 0x10323147) hides, the effects of spells flagged SpellCustomAttr
//              +0x14 & 8 on dead players / units (+0x60 health 0) or corpses, and +0x18 & 0x40 effects on objects that are
//              not ours (FUN_103233a0).
//   CVar       hideOtherPlayerHarmfulSpellVisuals (FUN_10114540 world queue; flags 1, "0", category 1; slot 0x10BE4478).
namespace CoA::SpellVisuals {
    struct ObjectInfo {
        uint32_t type = 0;          // +0x14
        uint64_t guid = 0;          // descriptor +0
        uint64_t charmedBy = 0;     // descriptor +0x30 (units)
        uint64_t summonedBy = 0;    // +0x38
        uint64_t createdBy = 0;     // +0x40
        uint32_t health = 0;        // +0x60
    };

    struct Missile {
        uint64_t caster = 0;        // +0
        uint64_t target = 0;        // +8
        uint32_t spell = 0;         // +0x1C
        uint8_t castId = 0;         // +0x20
        float launch[3] = {};       // +0x38
        float impact[3] = {};       // +0x44
    };

    struct Deps {
        bool (*objectInfo)(void* object, ObjectInfo& out) = nullptr;
        void* (*objectByGuid)(uint64_t guid, uint32_t typeMask) = nullptr;   // 0x4D4DB0
        void* (*activePlayer)() = nullptr;                                  // 0x4038F0
        uint64_t (*activeGuid)() = nullptr;
        bool (*visualRowExists)(uint32_t id) = nullptr;                     // SpellVisual.dbc (0xAD4AA8)
        uint32_t (*effectSpell)(void* effect) = nullptr;                    // CEffect +0x18
        void (*setRecordVisual)(const void* spellRec, uint32_t visual) = nullptr;   // SpellRec +0x20C
        bool (*spell)(uint32_t id, CoA::SpellApi::SpellFields& out) = nullptr;     // FetchSpell
        // AscAppearance_PlayerSpellVisual (Init installs CoA::Appearance::PlayerSpellVisual).
        bool (*playerSpellVisual)(uint32_t spell, uint32_t& visual) = nullptr;
    };

    Deps& Dependencies();

    uint32_t GuidKind(uint64_t guid);       // FUN_102cdc10
    uint32_t GuidCounter(uint64_t guid);    // FUN_100a03b0
    uint32_t Resolve(uint32_t kind, uint64_t guid, uint32_t spell);   // FUN_10322c70
    bool OptionOn();                        // DAT_10BE4478 +0x30
    // FUN_10325940: every effect positive.
    bool IsHelpful(const CoA::SpellApi::SpellFields& spell);

    int32_t OnSpellVisualOverride(void* param, uint32_t opcode, uint32_t time, CDataStore* msg);   // SMSG 0x662

    // 0x10322B50. Before: true returns 0 without the original. After: 0 keeps the record, 0xFFFFFFFF zeroes it, any other
    // value is the SpellVisual id whose row replaces it (when the row exists).
    bool SkipAppropriateVisual(void* object, bool fromSpellGo, const CoA::SpellApi::SpellFields& spell);
    uint32_t AppropriateVisual(void* object, uint32_t spell);
    // 0x10322AD0: the visual id to use instead of the argument -- 0 keeps it, 0xFFFFFFFF clears it.
    uint32_t MissileHitVisual(uint64_t caster, uint32_t spell);
    // 0x10322760: true skips the dynamic object's model.
    bool SkipDynamicObject(uint64_t caster, uint32_t spell);
    // 0x10322810.
    void AfterFireMissiles(const Missile& missile);

    bool HideVisual(uint32_t kind, uint32_t guidLo, uint32_t guidHi, uint32_t spell);   // 0x10323410
    bool RemoveEffect(void* unit, void* effect);                                       // 0x10323480
    bool BeforeAuraApply(void* unit, uint32_t index, const void* spellRec);            // 0x10323300

    void Init();
    // Tests only.
    void Reset();
}

#endif
