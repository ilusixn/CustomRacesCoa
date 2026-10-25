// AscAuraAmounts.cpp's client half: the spell / unit questions the map asks, the object-free hook (FUN_10a10630) and
// C_Aura.GetArmorPenetrationModifier, which walks the player's auras.
#include "CoA/Source/Modules/AuraAmounts.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/ClientHooks.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Script.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Source/Object/ObjectClient/AuraData.h"
#include "WoW/Source/Object/ObjectClient/Player_C.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"
#include "WoW/Source/Ui/ScriptEvents.h"
#include "WoW/Source/Ui/SpellBookFrame.h"

namespace {
    bool SpellEffects(uint32_t spell, CoA::AuraAmounts::SpellEffects& out) {
        SpellRec rec;

        if (!g_spellDB.GetRecord(static_cast<int32_t>(spell), &rec)) {
            return false;
        }

        for (int32_t j = 0; j < 3; ++j) {
            out.aura[j] = rec.m_effectAura[j];
            out.misc[j] = rec.m_effectMiscValue[j];
            out.basePoints[j] = rec.m_effectBasePoints[j];
        }

        out.stackAmount = rec.m_cumulativeAura;
        return true;
    }

    // 0x53D580(rec, &guid, 1): the client shows the spell's aura for that unit.
    bool AcceptsSpell(uint32_t spell, uint64_t unit) {
        SpellRec rec;

        if (!g_spellDB.GetRecord(static_cast<int32_t>(spell), &rec)) {
            return false;
        }

        WOWGUID guid;
        guid.m_guid = unit;
        return (CGSpellBook::CanShowAura(&rec, &guid, true) & 0xFF) != 0;
    }

    std::vector<std::string> Tokens(uint64_t unit) {   // 0x60BB70
        WOWGUID guid;
        guid.m_guid = unit;
        int32_t count = 0;
        char** tokens = Script_GetTokenIDsFromGUID(guid, &count);
        std::vector<std::string> out;

        for (int32_t i = 0; i < count; ++i) {
            out.push_back(tokens[i]);
        }

        return out;
    }

    bool TokenGuid(const char* token, uint64_t& guid) {
        WOWGUID g;

        if (!Script_GetGUIDFromToken(token, g, false)) {
            return false;
        }

        guid = g.m_guid;
        return true;
    }

    bool UnitExists(uint64_t guid) {
        WOWGUID g;
        g.m_guid = guid;
        return ClntObjMgrObjectPtr(g, TYPE_UNIT, __FILE__, __LINE__) != nullptr;
    }

    uint64_t ActivePlayer() {
        return ClntObjMgrGetActivePlayer().m_guid;
    }

    void OnObjectFree(void* object) {   // FUN_10a10630
        CoA::AuraAmounts::Forget(static_cast<CGObject_C*>(object)->m_obj->m_guid.m_guid);
    }

    // handler_GetArmorPenetrationModifier: over the player's auras, each SPELL_AURA_MOD_ARMOR_PENETRATION_PCT (280)
    // effect contributes the server's amount when one is known for (caster, spell), else BasePoints + 1 times the stack
    // count for stacking spells.
    int GetArmorPenetrationModifier(lua_State* L) {
        CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();

        if (!player) {
            CoA::Script::PushInt(L, 0);
            return 1;
        }

        const uint64_t guid = player->m_obj->m_guid.m_guid;
        int32_t sum = 0;

        for (uint32_t i = 0; i < player->GetAuraCount(); ++i) {
            const AuraData* aura = player->GetAura(i);
            SpellRec rec;

            if (!aura || !g_spellDB.GetRecord(aura->m_spellId, &rec)) {
                continue;
            }

            const int32_t* known = CoA::AuraAmounts::Find(guid, aura->m_creator.m_guid, static_cast<uint32_t>(aura->m_spellId));

            for (int32_t j = 0; j < 3; ++j) {
                if (rec.m_effectAura[j] != 0x118) {
                    continue;
                }

                if (known) {
                    sum += known[j];
                    continue;
                }

                int32_t amount = rec.m_effectBasePoints[j] + 1;

                if (rec.m_cumulativeAura) {
                    // +0x0E as a word: the stack count and the byte after it.
                    const uint32_t stacks = aura->m_stackCount | (aura->unused_000F << 8);
                    amount *= stacks ? static_cast<int32_t>(stacks) : 1;
                }

                sum += amount;
            }
        }

        CoA::Script::PushInt(L, sum);
        return 1;
    }

    const CoA::Binding s_bindings[] = {
        COA_NATIVE("C_Aura", "GetArmorPenetrationModifier", GetArmorPenetrationModifier),
    };
}

void CoA::AuraAmountsClient::Init() {
    CoA::AuraAmounts::Deps& deps = CoA::AuraAmounts::Dependencies();
    deps.spellEffects = SpellEffects;
    deps.acceptsSpell = AcceptsSpell;
    deps.tokens = Tokens;
    deps.tokenGuid = TokenGuid;
    deps.unitExists = UnitExists;
    deps.activePlayer = ActivePlayer;
    CoA::ClientHooks::objectFree.Add(OnObjectFree);
    CoA::LuaReg::AddModule(s_bindings, sizeof(s_bindings) / sizeof(s_bindings[0]));
}
