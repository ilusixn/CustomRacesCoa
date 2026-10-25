// AscSpellActivationOverlay.cpp's client questions and its world-entry start (0x1031f940).
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Modules/SpellActivationOverlay.h"
#include "CoA/Source/Modules/SpellRank.h"
#include "CoA/Source/Runtime/ClientHooks.h"
#include "Engine/Source/Os/TimeManager.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Source/Magic/MagicClient/Spell_C.h"
#include "WoW/Source/Object/ObjectClient/AuraData.h"
#include "WoW/Source/Object/ObjectClient/Player_C.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"

namespace {
    bool HasPlayer() {
        return ClntObjMgrGetActivePlayerObj() != nullptr;
    }

    bool HasAura(uint32_t spell) {
        CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();
        return player && player->HasAuraBySpellId(static_cast<int32_t>(spell));
    }

    // 0x7289C0 then 0x556E10: the player's aura of the spell, its +0x0E word.
    bool AuraStacks(uint32_t spell, uint32_t& stacks) {
        CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();
        const AuraData* aura = player ? player->GetAura(static_cast<uint32_t>(player->GetAuraIndex(static_cast<int32_t>(spell)))) : nullptr;

        if (!aura) {
            return false;
        }

        stacks = aura->m_stackCount | (aura->unused_000F << 8);
        return true;
    }

    // 0x809000(spell, 0, &a, &b, &enabled): the two times kept in the order the original passes them.
    CoA::SpellActivationOverlay::Cooldown Cooldown(uint32_t spell) {
        CoA::SpellActivationOverlay::Cooldown c;
        uint32_t enabled = 0;
        Spell_C_GetSpellCooldown(static_cast<int32_t>(spell), 0, &c.start, &c.duration, &enabled);
        c.enabled = static_cast<uint8_t>(enabled);
        return c;
    }

    bool StartRecovery(uint32_t spell, uint32_t& ms) {
        SpellRec rec;

        if (!g_spellDB.GetRecord(static_cast<int32_t>(spell), &rec)) {
            return false;
        }

        ms = static_cast<uint32_t>(rec.m_startRecoveryTime);
        return true;
    }
}

void CoA::SpellActivationOverlayClient::Init() {
    CoA::SpellActivationOverlay::Deps& deps = CoA::SpellActivationOverlay::Dependencies();
    deps.hasPlayer = HasPlayer;
    deps.hasAura = HasAura;
    deps.auraStacks = AuraStacks;
    deps.cooldown = Cooldown;
    deps.now = OsGetAsyncTimeMs;
    deps.startRecovery = StartRecovery;
    deps.firstRank = CoA::SpellRank::FirstRank;
    CoA::ClientHooks::enterWorld.Add(CoA::SpellActivationOverlay::Start);
}
