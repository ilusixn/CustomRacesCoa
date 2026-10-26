// AscSpellMods.cpp's client half: the records and unit questions SpellMods.cpp asks, the aura-flag and usable-action
// list entries (they take the client's records) and the decomp sites' record conversions.
#include "CoA/Source/Modules/SpellModsClient.h"
#include "CoA/Source/Modules/CrashContext.h"
#include "CoA/Source/Modules/AuraAmounts.h"
#include "CoA/Source/Modules/BridgeStore.h"
#include "CoA/Source/Modules/CA.h"
#include "CoA/Source/Modules/Manastorm.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Modules/PatchData.h"
#include "CoA/Source/Modules/SpellMods.h"
#include "CoA/Source/Modules/SpellRank.h"
#include "CoA/Source/Runtime/ClientHooks.h"
#include "Engine/Source/ConsoleBase/ConsoleVar.h"
#include "Engine/Source/Storm/H/storm.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Source/DB/Rec/SpellRangeRec.h"
#include "WoW/Source/DB/Rec/SpellRec.h"
#include "WoW/Source/Magic/MagicClient/Spell_C.h"
#include "WoW/Source/Object/ObjectClient/AuraData.h"
#include "WoW/Source/Object/ObjectClient/Player_C.h"
#include "WoW/Source/Object/ObjectClient/Unit_C.h"
#include "WoW/Source/Object/Player.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"
#include <cstring>

namespace {
    using CoA::SpellMods::Spell;

    void ToSpell(const SpellRec& r, Spell& out) {
        out.id = static_cast<uint32_t>(r.m_id);
        out.attributesExC = static_cast<uint32_t>(r.m_attributesExC);
        out.casterAuraSpell = static_cast<uint32_t>(r.m_casterAuraSpell);
        out.targetAuraSpell = static_cast<uint32_t>(r.m_targetAuraSpell);
        out.powerType = static_cast<uint32_t>(r.m_powerType);
        out.manaPerSecond = static_cast<uint32_t>(r.m_manaPerSecond);
        out.cumulativeAura = static_cast<uint32_t>(r.m_cumulativeAura);

        for (int32_t i = 0; i < 3; ++i) {
            out.effectAura[i] = static_cast<uint32_t>(r.m_effectAura[i]);
            out.effectMisc[i] = static_cast<uint32_t>(r.m_effectMiscValue[i]);
            out.maskA[i] = static_cast<uint32_t>(r.m_effectSpellClassMaskA[i]);
            out.maskB[i] = static_cast<uint32_t>(r.m_effectSpellClassMaskB[i]);
            out.maskC[i] = static_cast<uint32_t>(r.m_effectSpellClassMaskC[i]);
            out.classMask[i] = static_cast<uint32_t>(r.m_spellClassMask[i]);
        }

        out.classSet = static_cast<uint32_t>(r.m_spellClassSet);
        out.defenseType = static_cast<uint32_t>(r.m_defenseType);
        out.schoolMask = static_cast<uint32_t>(r.m_schoolMask);
    }

    Spell Convert(const SpellRec* r) {
        Spell out;

        if (r) {
            ToSpell(*r, out);
        }

        return out;
    }

    CGUnit_C* Unit(void* unit) {
        return static_cast<CGUnit_C*>(unit);
    }

    bool FetchSpell(uint32_t id, Spell& out) {   // AscScript::FetchSpell
        SpellRec rec;

        if (!g_spellDB.GetRecord(static_cast<int32_t>(id), &rec)) {
            return false;
        }

        ToSpell(rec, out);
        return true;
    }

    std::vector<uint32_t> RankChain(uint32_t first) {
        return CoA::SpellRank::Chain(first);
    }

    bool CustomAttr18(uint32_t spell, uint32_t& value) {
        const uint8_t* row = CoA::CA::SpellCustomAttrRow(spell);

        if (!row) {
            return false;
        }

        memcpy(&value, row + 0x18, 4);
        return true;
    }

    void SetCooldownLeft(uint32_t spell, uint32_t item, uint32_t category, uint32_t left, uint32_t categoryLeft, uint32_t onHold, uint32_t pet) {
        Spell_C_SetCooldownLeft(static_cast<int32_t>(spell), item, category, static_cast<int32_t>(left), static_cast<int32_t>(categoryLeft),
            onHold != 0, static_cast<int32_t>(pet));
    }

    bool AffectedByAura(void* unit, uint32_t type, uint32_t spell) {   // 0x7283A0 with the fetched record
        SpellRec rec;
        return unit && g_spellDB.GetRecord(static_cast<int32_t>(spell), &rec) && Unit(unit)->AffectedByAura(static_cast<int32_t>(type), &rec);
    }

    bool HasAura(void* unit, uint32_t spell) {
        return unit && Unit(unit)->HasAuraBySpellId(static_cast<int32_t>(spell));
    }

    bool IsActivePlayer(void* unit) {
        return unit && Unit(unit)->IsActivePlayer();
    }

    // FUN_1008d8a0: the low dword of PLAYER_FIELD_INV_SLOT_HEAD + slot (only asked after IsActivePlayer).
    uint32_t InvSlotLow(void* unit, uint32_t slot) {
        if (!unit || !(Unit(unit)->m_obj->m_type & TYPE_PLAYER) || slot >= 23) {
            return 0;
        }

        return static_cast<uint32_t>(static_cast<CGPlayer_C*>(Unit(unit))->m_player->m_playerFieldInvSlotHead[slot].m_guid);
    }

    uint32_t AutoRepeatingSpell() {
        return static_cast<uint32_t>(Spell_C_GetAutoRepeatingSpell());
    }

    uint32_t BasePower(void* unit, uint32_t powerType) {
        return Unit(unit)->GetBasePowerForType(static_cast<POWER_TYPE>(powerType));
    }

    uint64_t UnitGuid(void* unit) {
        return unit ? Unit(unit)->m_obj->m_guid.m_guid : 0;
    }

    void* ActivePlayer() {
        return ClntObjMgrGetActivePlayerObj();
    }

    uint32_t AuraCount(void* unit) {
        return Unit(unit)->GetAuraCount();
    }

    bool AuraAt(void* unit, uint32_t index, CoA::SpellMods::Aura& out) {
        const AuraData* aura = Unit(unit)->GetAura(index);

        if (!aura) {
            return false;
        }

        out.caster = aura->m_creator.m_guid;
        out.spell = static_cast<uint32_t>(aura->m_spellId);
        out.stack = static_cast<uint16_t>(aura->m_stackCount | (aura->unused_000F << 8));
        return true;
    }

    uint32_t AuraIndex(void* unit, uint32_t spell) {
        return static_cast<uint32_t>(Unit(unit)->GetAuraIndex(static_cast<int32_t>(spell)));
    }

    int32_t CastTime(const void* rec) {
        return SpellRec::GetCastTime(static_cast<const SpellRec*>(rec), 0, 0, 0);
    }

    bool ErrorSpeech() {
        CVar* cvar = CVar::Lookup("Sound_EnableErrorSpeech");
        return !cvar || cvar->m_intValue != 0;
    }

    uint32_t CastingSpellId(void* unit) {
        return Unit(unit)->m_castingSpellId;
    }

    int32_t UiCastingSpell(void* player) {
        return Unit(player)->m_castSpellId;
    }

    void SetSpellRangeFlags(uint32_t range, uint32_t flags) {   // FUN_100b2590
        if (SpellRangeRec* rec = g_spellRangeDB.GetRecord(static_cast<int32_t>(range))) {
            rec->m_flags = static_cast<int32_t>(flags);
        }
    }

    void SetModalSkip(bool on) {
        // The same byte AscManastorm writes (0x80DB37); the last writer wins, as in the original.
        CoA::ClientHooks::manastormSkipsModalSpell = on;
    }

    void Fatal(const char* message) {   // AscCrashContext::Assert
        CoA::CrashContext::Assert(message, nullptr, 0);
    }

    bool AuraFlagOn(void*, uint32_t, const void* rec) {   // 0x10325AA0
        return !rec || CoA::SpellMods::AuraFlagOn(Convert(static_cast<const SpellRec*>(rec)));
    }

    bool AuraFlagOff(void*, uint32_t, const void* rec) {   // 0x10325B50
        return !rec || CoA::SpellMods::AuraFlagOff(Convert(static_cast<const SpellRec*>(rec)));
    }

    bool StackRequirement(void* unit, const void* rec, const void*, const void*, bool) {   // 0x103267E0
        return !rec || CoA::SpellMods::StackRequirement(unit, Convert(static_cast<const SpellRec*>(rec)));
    }

    bool AuraRequirement(void* unit, const void* rec, const void*, const void*, bool) {   // 0x10326780
        return !rec || CoA::SpellMods::AuraRequirement(unit, Convert(static_cast<const SpellRec*>(rec)));
    }
}

bool CoA::SpellModsClient::GetSpellModifiers(const SpellRec* spell, int32_t modifier, int32_t& flat, int32_t& pct) {
    if (!spell) {
        flat = 0;
        pct = 100;
        return false;
    }

    const uint32_t mask[3] = { static_cast<uint32_t>(spell->m_spellClassMask[0]), static_cast<uint32_t>(spell->m_spellClassMask[1]),
        static_cast<uint32_t>(spell->m_spellClassMask[2]) };
    return CoA::SpellMods::ModifierSum(static_cast<uint32_t>(spell->m_id), static_cast<uint32_t>(spell->m_spellClassSet), mask,
        static_cast<uint32_t>(spell->m_attributesExC), static_cast<uint32_t>(modifier), flat, pct);
}

bool CoA::SpellModsClient::PowerPerSecondReplaced(const SpellRec* spell, const CGUnit_C* unit, int32_t& out) {
    return spell && CoA::SpellMods::PowerPerSecondReplaced(Convert(spell), const_cast<CGUnit_C*>(unit), out);
}

int32_t CoA::SpellModsClient::AfterPowerPerSecond(const SpellRec* spell, const CGUnit_C* unit, int32_t result) {
    return spell ? CoA::SpellMods::AfterPowerPerSecond(Convert(spell), const_cast<CGUnit_C*>(unit), result) : result;
}

int32_t CoA::SpellModsClient::AfterPowerCost(const SpellRec* spell, const CGUnit_C* unit, int32_t result) {
    return spell ? CoA::SpellMods::AfterPowerCost(Convert(spell), spell, const_cast<CGUnit_C*>(unit), result) : result;
}

int32_t CoA::SpellModsClient::AfterCastTime(const SpellRec* spell, int32_t result) {
    if (!spell) {
        return CoA::SpellMods::AfterCastTime(nullptr, result);
    }

    const Spell s = Convert(spell);
    return CoA::SpellMods::AfterCastTime(&s, result);
}

void CoA::SpellModsClient::Init() {
    CoA::SpellMods::Deps& d = CoA::SpellMods::Dependencies();
    d.fetchSpell = FetchSpell;
    d.spellAffects = CoA::BridgeStore::SpellAffectsFor;
    d.rankChain = RankChain;
    d.customAttr18 = CustomAttr18;
    d.setCooldownLeft = SetCooldownLeft;
    d.affectedByAura = AffectedByAura;
    d.hasAura = HasAura;
    d.isActivePlayer = IsActivePlayer;
    d.invSlotLow = InvSlotLow;
    d.inManastorm = CoA::Manastorm::InManastorm;
    d.autoRepeatingSpell = AutoRepeatingSpell;
    d.basePower = BasePower;
    d.unitGuid = UnitGuid;
    d.activePlayer = ActivePlayer;
    d.auraCount = AuraCount;
    d.auraAt = AuraAt;
    d.auraIndex = AuraIndex;
    d.auraAmounts = CoA::AuraAmounts::Find;
    d.spellAddon = CoA::PatchData::SpellAddonForSpell;
    d.castTime = CastTime;
    d.errorSpeech = ErrorSpeech;
    d.castingSpellId = CastingSpellId;
    d.uiCastingSpell = UiCastingSpell;
    d.setSpellRangeFlags = SetSpellRangeFlags;
    d.setModalSkip = SetModalSkip;
    d.fatal = Fatal;

    // FUN_10278430 / FUN_10278620 (sites 0x10324D9A / 0x10324DA4) and FUN_102783D0 (0x10324DAE / 0x10324DB8).
    CoA::ClientHooks::addAuraEffect.Add(AuraFlagOn, 0x10324D9A);
    CoA::ClientHooks::removeAuraEffect.Add(AuraFlagOff, 0x10324DA4);
    CoA::ClientHooks::usableAction.Add(StackRequirement, 0x10324DAE);
    CoA::ClientHooks::usableAction.Add(AuraRequirement, 0x10324DB8);
}
