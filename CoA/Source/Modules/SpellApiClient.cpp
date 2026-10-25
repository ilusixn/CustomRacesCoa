// AscSpellApi.cpp's client half: Spell.dbc records as SpellFields, SkillLineAbility / SkillLine rows, the active player,
// spell ranks, and the 0x934 patch hooks into the spell -> ability map.
#include "CoA/Source/Modules/ClientDbcPatch.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Modules/SpellApi.h"
#include "CoA/Source/Modules/SpellRank.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Source/DB/Rec/SkillLineAbilityRec.h"
#include "WoW/Source/DB/Rec/SkillLineRec.h"
#include "WoW/Source/DB/Rec/SpellRec.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"

namespace {
    using CoA::SpellApi::SpellFields;

    uint32_t U(int32_t v) {
        return static_cast<uint32_t>(v);
    }

    // FetchSpell: the dwords the spell API reads.
    bool Spell(uint32_t id, SpellFields& out) {
        SpellRec rec;

        if (!g_spellDB.GetRecord(static_cast<int32_t>(id), &rec)) {
            return false;
        }

        out.id = U(rec.m_id);
        out.mechanic = U(rec.m_mechanic);
        out.attributes = U(rec.m_attributes);

        for (uint32_t i = 0; i < 3; ++i) {
            out.effect[i] = U(rec.m_effect[i]);
            out.dieSides[i] = U(rec.m_effectDieSides[i]);
            out.basePoints[i] = U(rec.m_effectBasePoints[i]);
            out.targetA[i] = U(rec.m_implicitTargetA[i]);
            out.targetB[i] = U(rec.m_implicitTargetB[i]);
            out.aura[i] = U(rec.m_effectAura[i]);
            out.miscValue[i] = U(rec.m_effectMiscValue[i]);
            out.triggerSpell[i] = U(rec.m_effectTriggerSpell[i]);
        }

        out.iconId = U(rec.m_spellIconID);
        out.family = U(rec.m_spellClassSet);
        out.familyFlags = U(rec.m_spellClassMask[0]);
        out.spellLevel = U(rec.m_spellLevel);
        return true;
    }

    bool HasPlayer() {   // 0x4038F0
        return ClntObjMgrGetActivePlayerObj() != nullptr;
    }

    void SkillLineAbilityRange(uint32_t& minId, uint32_t& maxId) {   // 0xAD4598 +0x10 / +0x0C
        minId = U(g_skillLineAbilityDB.m_minID);
        maxId = U(g_skillLineAbilityDB.m_maxID);
    }

    bool SkillLineAbility(uint32_t id, uint32_t& spell, uint32_t& line) {
        const SkillLineAbilityRec* row = g_skillLineAbilityDB.GetRecord(static_cast<int32_t>(id));

        if (!row) {
            return false;
        }

        spell = U(row->m_spell);
        line = U(row->m_skillLine);
        return true;
    }

    bool SkillLineCategory(uint32_t line, uint32_t& category) {
        const SkillLineRec* row = g_skillLineDB.GetRecord(static_cast<int32_t>(line));

        if (!row) {
            return false;
        }

        category = U(row->m_categoryID);
        return true;
    }
}

void CoA::SpellApiClient::Init() {
    CoA::SpellApi::Deps& d = CoA::SpellApi::Dependencies();
    d.spell = Spell;
    d.firstRank = CoA::SpellRank::FirstRank;
    d.chain = CoA::SpellRank::Chain;
    d.hasPlayer = HasPlayer;
    d.skillLineAbilityRange = SkillLineAbilityRange;
    d.skillLineAbility = SkillLineAbility;
    d.skillLineCategory = SkillLineCategory;

    CoA::ClientDbcPatch::Deps& patch = CoA::ClientDbcPatch::Dependencies();
    patch.skillLineAbilityErase = CoA::SpellApi::SkillLineAbilityErase;
    patch.skillLineAbilityUpdate = CoA::SpellApi::SkillLineAbilityUpdate;
}
