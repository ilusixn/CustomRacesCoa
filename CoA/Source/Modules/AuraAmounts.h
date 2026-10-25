#ifndef COA_SOURCE_MODULES_AURAAMOUNTS_H
#define COA_SOURCE_MODULES_AURAAMOUNTS_H

#include <cstdint>
#include <string>
#include <vector>

// AscAuraAmounts.cpp: server-sent aura effect amounts (absorb shields, armor penetration, ...) -- the original's map
// at 0x10d3c280 and everything that reads it.
//
// SMSG_AURA_UPDATE_ADDON (0x673) carries one record for a unit, SMSG_AURA_UPDATE_ALL_ADDON (0x674) a run of them to the
// end of the packet (FUN_10a121e0 / handler_0x0674 -> FUN_10a12470): u64 unit, then per record u32 slot, u64 caster,
// u32 spell, and unless slot == 0xFFFFFFFF (removal) u32 amount[3] -- one per spell effect. Entries are keyed per unit
// by (caster, spell). Another unit's new entry is only kept when the client's 0x53D580 accepts the spell for that
// unit. Every change fires UNIT_ABSORB_AMOUNT_CHANGED or UNIT_HEAL_ABSORB_AMOUNT_CHANGED ("%s", token) for each unit
// token naming the unit (FUN_10a13440). C_Aura.GetArmorPenetrationModifier is in AuraAmountsClient.cpp.
namespace CoA::AuraAmounts {
    // The Spell.dbc fields read here: EffectApplyAuraName[3], EffectMiscValue[3], EffectBasePoints[3], StackAmount.
    struct SpellEffects {
        int32_t aura[3] = {};
        int32_t misc[3] = {};
        int32_t basePoints[3] = {};
        int32_t stackAmount = 0;
    };

    // The client questions; AuraAmountsClient.cpp installs them.
    struct Deps {
        bool (*spellEffects)(uint32_t spell, SpellEffects& out) = nullptr;     // FetchSpell
        bool (*acceptsSpell)(uint32_t spell, uint64_t unit) = nullptr;         // 0x53D580(rec, &guid, 1)
        std::vector<std::string> (*tokens)(uint64_t unit) = nullptr;           // 0x60BB70
        bool (*tokenGuid)(const char* token, uint64_t& guid) = nullptr;        // a valid unit token's GUID
        bool (*unitExists)(uint64_t guid) = nullptr;                           // ObjectPtr(guid, TYPE_UNIT)
        uint64_t (*activePlayer)() = nullptr;
    };

    Deps& Dependencies();

    // FUN_103284e0's lookup: `unit`'s three effect amounts for (caster, spell), or nullptr.
    const int32_t* Find(uint64_t unit, uint64_t caster, uint32_t spell);
    // The spell-description tokens 0xA2..0xA7 (FUN_10a10c30): every entry of `unit` for `spell`, whatever the caster;
    // the last one wins. False when there is none.
    bool LastForSpell(uint64_t unit, uint32_t spell, int32_t out[3]);

    // FUN_10a10630 (object free): the freed object's entries go.
    void Forget(uint64_t unit);

    void Init();
}

#endif
