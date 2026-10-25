// AscCAMgr.cpp's client half: the reads behind CoA::CA::Deps, the spell-packet detours (ClientHooks::spellPackets), the
// RemoveMacro hook (0x5AAB90), the cast requirement after 0x80B5D0, the global IsItemAction, the SpellText CA questions
// and the world-entry index rebuild.
#include "CoA/Source/Modules/CA.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Modules/SpellRank.h"
#include "CoA/Source/Modules/SpellText.h"
#include "CoA/Source/Runtime/ClientHooks.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Script.h"
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Source/DB/Rec/MapRec.h"
#include "WoW/Source/DB/Rec/SpellRec.h"
#include "WoW/Source/Object/ObjectClient/AuraData.h"
#include "WoW/Source/Object/ObjectClient/Item_C.h"
#include "WoW/Source/Object/ObjectClient/Player_C.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"
#include "WoW/Source/Ui/ActionBarFrame.h"
#include "WoW/Source/Ui/CGWorldStateInfo.h"
#include "WoW/Source/Ui/GameUI.h"

namespace {
    CGUnit_C* UnitOf(uint64_t guid) {
        CGObject_C* o = guid ? ClntObjMgrObjectPtr(WOWGUID { guid }, TYPE_UNIT, __FILE__, __LINE__) : nullptr;
        return o && (o->m_typeId == ID_UNIT || o->m_typeId == ID_PLAYER) ? static_cast<CGUnit_C*>(o) : nullptr;
    }

    CGPlayer_C* PlayerOf(uint64_t guid) {   // ObjectPtr(guid, 0x10)
        return guid ? static_cast<CGPlayer_C*>(ClntObjMgrObjectPtr(WOWGUID { guid }, TYPE_PLAYER, __FILE__, __LINE__)) : nullptr;
    }

    uint64_t ActivePlayerGuid() {   // 0x4D3790
        return ClntObjMgrGetActivePlayer().m_guid;
    }

    bool UnitExists(uint64_t guid) {
        return PlayerOf(guid) != nullptr;
    }

    uint8_t UnitClass(uint64_t guid) {
        const CGUnit_C* unit = UnitOf(guid);
        return unit ? unit->m_unit->m_unitFieldBytes0[1] : 0;
    }

    uint32_t UnitLevel(uint64_t guid) {
        const CGUnit_C* unit = UnitOf(guid);
        return unit ? unit->m_unit->m_unitFieldLevel : 0;
    }

    bool UnitInCombat(uint64_t guid) {   // UNIT_FIELD_FLAGS (+0xEC) bit 19
        const CGUnit_C* unit = UnitOf(guid);
        return unit && (unit->m_unit->m_unitFieldFlags >> 19) & 1;
    }

    bool UnitDead(uint64_t guid) {   // 0x6DAC10
        const CGPlayer_C* player = PlayerOf(guid);
        return player && player->IsDeadOrGhost();
    }

    // FUN_10160f50: an aura whose spell applies SPELL_AURA_SCHOOL_IMMUNITY (0x27) in any effect.
    bool UnitSchoolImmune(uint64_t guid) {
        CGUnit_C* unit = UnitOf(guid);

        if (!unit) {
            return false;
        }

        const uint32_t count = unit->GetAuraCount();

        for (uint32_t i = 0; i < count; ++i) {
            const AuraData* aura = unit->GetAura(i);
            SpellRec rec;

            if (aura && g_spellDB.GetRecord(aura->m_spellId, &rec)) {
                for (uint32_t k = 0; k < 3; ++k) {
                    if (rec.m_effectAura[k] == 0x27) {
                        return true;
                    }
                }
            }
        }

        return false;
    }

    // FUN_10308570: the active player's currency-token slots (0x76..0x95).
    uint32_t UnitItemCount(uint64_t guid, uint32_t item) {
        CGPlayer_C* player = PlayerOf(guid);

        if (!player) {
            return 0;
        }

        uint32_t total = 0;

        for (const WOWGUID& slot : static_cast<const CGPlayerLocalData*>(player->m_player)->m_playerFieldCurrencytokenSlot1) {
            const CGItem_C* it = slot.m_guid ? static_cast<const CGItem_C*>(ClntObjMgrObjectPtr(slot, TYPE_ITEM, __FILE__, __LINE__)) : nullptr;

            if (it && it->m_obj->m_entryID == item) {
                total += static_cast<uint32_t>(it->m_item->m_itemFieldStackCount);
            }
        }

        return total;
    }

    uint32_t UnitMoney(uint64_t guid) {   // PLAYER_FIELD_COINAGE (+0x1248)
        CGPlayer_C* player = PlayerOf(guid);
        return player ? static_cast<const CGPlayerLocalData*>(player->m_player)->m_playerFieldCoinage : 0;
    }

    int32_t MapType() {   // Map.dbc (0xAD4160) +8 of the current map (0xBD088C)
        const MapRec* map = g_mapDB.GetRecord(CGGameUI::m_iCurrentMapID);
        return map ? map->m_instanceType : -1;
    }

    int32_t BattlegroundState() {   // 0x548D10(0x1097)
        return GetWorldState(0x1097);
    }

    bool ClientKnowsSpell(uint32_t spell) {   // 0x7260E0 on the active player
        CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();
        return player && player->IsSpellKnown(static_cast<int32_t>(spell));
    }

    bool SpellExists(uint32_t spell) {
        SpellRec rec;
        return g_spellDB.GetRecord(static_cast<int32_t>(spell), &rec) != 0;
    }

    bool SpellRuneCost(uint32_t spell) {   // Spell +0x288
        SpellRec rec;
        return g_spellDB.GetRecord(static_cast<int32_t>(spell), &rec) && rec.m_runeCostID != 0;
    }

    bool SpellHidden(uint32_t spell) {   // Spell +0x10 bit 7
        SpellRec rec;
        return g_spellDB.GetRecord(static_cast<int32_t>(spell), &rec) && (rec.m_attributes >> 7) & 1;
    }

    uint32_t SpellMaxId() {   // 0xAD49DC
        return static_cast<uint32_t>(g_spellDB.m_maxID);
    }

    // ---- the spell-packet detours -------------------------------------------------------------------------------
    void InitialSpells(const uint32_t* spells, uint32_t count) {
        CoA::CA::OnInitialSpells(std::vector<uint32_t>(spells, spells + count));
    }

    void RemovedSpells(const uint32_t* spells, uint32_t count) {
        CoA::CA::OnRemovedSpells(std::vector<uint32_t>(spells, spells + count));
    }

    // FUN_101728e0: a trailing 0 byte marks the spell at 0x10BDEBC4 while the client handles it; a spell removed outside
    // CharacterAdvancement leaves the known set first.
    void RemovedBefore(uint32_t spell, bool trailingZero) {
        if (trailingZero) {
            CoA::CA::SetRemovingSpell(spell);
        }

        if (CoA::CA::RemovedSpellLeavesKnownSet(spell)) {
            std::vector<uint32_t> one = { spell };
            CoA::CA::OnRemovedSpells(one);
        }
    }

    void RemovedAfter() {
        CoA::CA::SetRemovingSpell(0);
    }

    // FUN_10193a60 on 0x5AAB90: nothing for the spell 0x6E7840 is removing.
    bool RemoveMacro(uint32_t spell) {
        return CoA::CA::RemovingSpell() != spell;
    }

    // 0x1016FDE0 after 0x80B5D0: the active player's casts only.
    bool CastRequirements(void* unit, const void* rec, const void*, const void*, bool) {
        if (!static_cast<CGUnit_C*>(unit)->IsActivePlayer()) {
            return true;
        }

        return CoA::CA::CastRequirementsMet(static_cast<uint32_t>(static_cast<const SpellRec*>(rec)->m_id));
    }

    // LAB_10193f70, IsItemAction(slot): true when action slot 1..144 holds an item action (top nibble 8). A non-number or
    // an out-of-range slot is false.
    int IsItemAction(lua_State* L) {
        bool item = false;

        if (lua_isnumber(L, 1)) {
            // __dtoul3 (0x10AE6370): double -> unsigned 64-bit, low dword kept
            const uint32_t slot = static_cast<uint32_t>(static_cast<unsigned long long>(CoA::Script::CheckNumber(L, 1))) - 1;

            if (slot < 0x90) {
                const uint32_t action = CGActionBar::m_slotActions[slot];
                item = action != 0 && (action & 0xF0000000) == 0x80000000;
            }
        }

        lua_pushboolean(L, item ? 1 : 0);
        return 1;
    }

    // FUN_10193e40: IsItemAction goes into the live Lua state once and never again, so later states lack it. The DLL's
    // init runs before any state exists, so the one registration lands in the first glue state.
    bool s_isItemActionDone = false;

    void RegisterIsItemAction() {
        if (s_isItemActionDone) {
            return;
        }

        lua_State* L = FrameScript_GetContext();

        if (!L) {
            return;
        }

        s_isItemActionDone = true;
        lua_pushcclosure(L, IsItemAction, 0);
        lua_setfield(L, LUA_GLOBALSINDEX, "IsItemAction");
    }

    bool SpellKnownForText(uint32_t spell, bool includeRanks) {
        return CoA::CA::SpellKnown(spell, includeRanks);
    }
}

void CoA::CAClient::Init() {
    CoA::CA::Deps& d = CoA::CA::Dependencies();
    d.activePlayerGuid = ActivePlayerGuid;
    d.unitExists = UnitExists;
    d.unitClass = UnitClass;
    d.unitLevel = UnitLevel;
    d.unitInCombat = UnitInCombat;
    d.unitDead = UnitDead;
    d.unitSchoolImmune = UnitSchoolImmune;
    d.unitItemCount = UnitItemCount;
    d.unitMoney = UnitMoney;
    d.mapType = MapType;
    d.battlegroundState = BattlegroundState;
    d.clientKnowsSpell = ClientKnowsSpell;
    d.spellExists = SpellExists;
    d.spellRuneCost = SpellRuneCost;
    d.spellHidden = SpellHidden;
    d.spellMaxId = SpellMaxId;
    d.firstRank = CoA::SpellRank::FirstRank;
    d.rankChain = CoA::SpellRank::Chain;

    CoA::ClientHooks::SpellPacketHooks& h = CoA::ClientHooks::spellPackets;
    h.initialSpells = InitialSpells;
    h.learnedIsQuiet = CoA::CA::LearnedSpellIsQuiet;
    h.learned = CoA::CA::OnLearnedSpell;
    h.removedSpells = RemovedSpells;
    h.removedBefore = RemovedBefore;
    h.removedAfter = RemovedAfter;
    h.supersedeIsQuiet = CoA::CA::SupersedeIsQuiet;
    h.superseded = CoA::CA::OnSupersededSpell;
    h.removeMacro = RemoveMacro;

    CoA::ClientHooks::enterWorld.Add(CoA::CA::EnterWorld);
    CoA::ClientHooks::usableAction.Add(CastRequirements, 0x10171F65);
    CoA::LuaReg::OnGlueRegistered(RegisterIsItemAction);

    CoA::SpellText::Deps& text = CoA::SpellText::Dependencies();
    text.spellKnown = SpellKnownForText;
    text.isCASpell = CoA::CA::IsCASpell;
}
