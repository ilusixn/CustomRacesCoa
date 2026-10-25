#include "CoA/Source/Modules/GlobalsJ.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Modules/SpellText.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Script.h"
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "Engine/Source/Os/Gui.h"
#include "WoW/Common/ComponentCore/CharacterComponent.h"
#include "WoW/Source/DB/DBClient/DBCacheInstances.h"
#include "WoW/Source/Game/GameTime.h"
#include "WoW/Source/Magic/MagicClient/Spell_C.h"
#include "WoW/Source/Object/ObjectClient/Item_C.h"
#include "WoW/Source/Object/ObjectClient/Player_C.h"
#include "WoW/Source/Object/ObjectClient/UnitCombatLog_C.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"
#include "WoW/Source/UIUtil/InputControl.h"
#include "WoW/Source/UIUtil/PortraitButton.h"
#include "WoW/Source/UIUtil/Tooltip.h"
#include "WoW/Source/Ui/CurrencyTypes.h"
#include "WoW/Source/Ui/EquipmentManager.h"
#include "WoW/Source/Ui/GameUI.h"
#include "WoW/Source/Ui/ScriptEvents.h"
#include "WoW/Source/Ui/SpellBookFrame.h"
#include "WoW/Source/Ui/WorldFrame.h"
#include "CoA/Source/Modules/BridgeStore.h"
#include "CoA/Source/Modules/MythicPlus.h"
#include "CoA/Source/Runtime/Dbc.h"
#include <Windows.h>
#include <cstdio>
#include <cstring>
#include <unordered_set>
#include <string>

namespace {
    // GetSpellData(spell [, a, b]) (FUN_10a4da30): accepts {number, boolean, boolean}, then {number, boolean} (the
    // boolean is read and ignored), then {number}. Pushes the Spell.dbc name (+0x220), the description
    // (FUN_101ac910(spell, a, 0, b) = SpellText::Description) and the SpellIcon path (+0x214 -> SpellIcon.dbc +4,
    // default the question mark). Nothing for an unknown spell.
    int GetSpellData(lua_State* L) {
        uint32_t spell = 0;
        bool a = false;
        bool b = false;

        if (CoA::Script::ValidateInput(L, { LUA_TNUMBER, LUA_TBOOLEAN, LUA_TBOOLEAN })) {
            spell = static_cast<uint32_t>(CoA::Script::ToInt(CoA::Script::CheckNumber(L, 1)));
            a = lua_toboolean(L, 2) != 0;
            b = lua_toboolean(L, 3) != 0;
        } else if (CoA::Script::ValidateInput(L, { LUA_TNUMBER, LUA_TBOOLEAN })) {
            spell = static_cast<uint32_t>(CoA::Script::ToInt(CoA::Script::CheckNumber(L, 1)));
            lua_toboolean(L, 2);
        } else if (!CoA::Script::ReadNumber(L, spell)) {
            return 0;
        }

        SpellRec rec;

        if (!g_spellDB.GetRecord(static_cast<int32_t>(spell), &rec)) {
            return 0;
        }

        std::string icon = "Interface\\Icons\\INV_Misc_QuestionMark";

        if (const SpellIconRec* row = g_spellIconDB.GetRecord(rec.m_spellIconID)) {
            icon = row->m_textureFilename ? row->m_textureFilename : "";
        }

        CoA::Script::PushStr(L, rec.m_name);
        CoA::Script::PushStr(L, CoA::SpellText::Description(spell, a, false, b ? 1 : 0).c_str());
        CoA::Script::PushStr(L, icon.c_str());
        return 3;
    }

    using CoA::Script::CheckNumber;
    using CoA::Script::CheckString;
    using CoA::Script::PushInt;
    using CoA::Script::PushNum;
    using CoA::Script::PushStr;
    using CoA::Script::ToInt;

    CGItem_C* ItemObject(WOWGUID guid) {
        return static_cast<CGItem_C*>(ClntObjMgrObjectPtr(guid, TYPE_ITEM, __FILE__, __LINE__));
    }

    // GetWeaponTempEnchantInfo(slot): the item in inventory slot `slot` (the original's descriptor +0x508 + slot * 8,
    // PLAYER_FIELD_INV_SLOT_HEAD[slot - 1]); its temporary enchantment (ITEM_FIELD_ENCHANTMENT slot 1) -> name, id.
    int GetWeaponTempEnchantInfo(lua_State* L) {
        const int32_t slot = ToInt(CheckNumber(L, 1));

        if (slot == 0) {
            lua_pushnil(L);
            return 1;
        }

        if (CGPlayer_C* player = ClntObjMgrGetActivePlayerObj()) {
            if (CGItem_C* item = ItemObject(player->m_player->m_playerFieldInvSlotHead[slot - 1])) {
                const uint32_t enchant = item->m_item->m_itemFieldEnchantment[1].m_id;

                if (const SpellItemEnchantmentRec* row = g_spellItemEnchantmentDB.GetRecord(static_cast<int32_t>(enchant))) {
                    PushStr(L, row->m_name);
                    PushInt(L, static_cast<int32_t>(enchant));
                    return 2;
                }
            }
        }

        return CoA::Script::PushNils(L, 2);
    }

    // The ranged slot (descriptor +0x598, slot 17): has-enchant (1), then the temporary enchantment's duration and
    // charges (+0x68 / +0x6C, the charges read as the whole dword), nil when 0.
    int GetRangedWeaponEnchantInfo(lua_State* L) {
        if (CGPlayer_C* player = ClntObjMgrGetActivePlayerObj()) {
            if (CGItem_C* item = ItemObject(player->m_player->m_playerFieldInvSlotHead[17])) {
                const ItemEnchantment& e = item->m_item->m_itemFieldEnchantment[1];

                if (g_spellItemEnchantmentDB.GetRecord(static_cast<int32_t>(e.m_id))) {
                    const int32_t duration = static_cast<int32_t>(e.m_duration);
                    const int32_t charges = static_cast<int32_t>(static_cast<uint16_t>(e.m_charges) | (static_cast<uint32_t>(e.unk_000A) << 16));
                    PushInt(L, e.m_id != 0 ? 1 : 0);

                    if (duration == 0) {
                        lua_pushnil(L);
                    } else {
                        PushInt(L, duration);
                    }

                    if (charges == 0) {
                        lua_pushnil(L);
                    } else {
                        PushInt(L, charges);
                    }

                    return 3;
                }
            }
        }

        return CoA::Script::PushNils(L, 3);
    }

    // GetEquipmentSetItemGUIDs(name): the set's 19 item GUIDs as GUIDToHexString formats them, keyed 1..19. Nothing
    // for an unknown set or an empty name.
    int GetEquipmentSetItemGUIDs(lua_State* L) {
        const std::string name = CheckString(L, 1);

        if (name.empty()) {
            return 0;
        }

        const EQUIPMENT_SET* set = CGEquipmentManager::GetSetByName(name.c_str());

        if (!set) {
            return 0;
        }

        lua_createtable(L, 0, 0);

        for (int32_t i = 0; i < 19; ++i) {
            lua_checkstack(L, 2);
            PushNum(L, static_cast<double>(i + 1));
            char buffer[24];
            GUIDToHexString(set->m_items[i], buffer);
            PushStr(L, buffer);
            lua_settable(L, -3);
        }

        return 1;
    }

    // ---- terrain click (Spell_C_HandleTerrainClick {guid 0, point, button 1}) ----
    void ClickTerrain(const C3Vector& point) {
        CTerrainClickEvent click;
        click.m_guid = WOWGUID();
        click.m_point = point;
        click.m_button = static_cast<MOUSEBUTTON>(1);
        Spell_C_HandleTerrainClick(click);
    }

    // FUN_10307950 / FUN_10307d20: CameraOrSelectOrMoveStart / Stop with their CanPerformAction id patched to 200.
    void CallWithAction200(int32_t (*fn)(lua_State*), lua_State* L) {
        g_cameraOrSelectAction = 200;
        fn(L);
        g_cameraOrSelectAction = 0;
    }

    // At the world frame's cursor terrain point (the default-action hit point).
    int Custom_HandleTerrainClick(lua_State* L) {
        CallWithAction200(CameraOrSelectOrMoveStart, L);
        ClickTerrain(CGWorldFrame::s_currentWorldFrame->m_defaultActionHit.m_point);
        CallWithAction200(CameraOrSelectOrMoveStop, L);
        return 0;
    }

    int Custom_HandleTerrainClick_PlayerPos(lua_State*) {
        if (CGPlayer_C* player = ClntObjMgrGetActivePlayerObj()) {
            ClickTerrain(player->GetPosition());
        }

        return 0;
    }

    // At the locked target (CGGameUI::m_lockedTarget, any object type).
    int Custom_HandleTerrainClick_TargetPos(lua_State*) {
        if (!ClntObjMgrGetActivePlayerObj()) {
            return 0;
        }

        if (CGObject_C* target = ClntObjMgrObjectPtr(CGGameUI::m_lockedTarget, static_cast<OBJECT_TYPE>(0xFFFF), __FILE__, __LINE__)) {
            ClickTerrain(target->GetPosition());
        }

        return 0;
    }

    // SetAutoRangedCombatSpell(spell): the client's auto-shot spell id (0xBE5D84).
    int SetAutoRangedCombatSpell(lua_State* L) {
        CGSpellBook::m_autoRangedSpell = ToInt(CheckNumber(L, 1));
        return 0;
    }

    // ---- GetItemTemplate ----
    // The item cache record as FUN_1021c4b0's JSON object: integer fields pushed as integers, floats as numbers,
    // strings as strings, arrays as tables keyed 1..n. Nothing when the item is not cached.
    void PushTemplateInt(lua_State* L, const char* key, int32_t value) {
        PushStr(L, key);
        PushInt(L, value);
        lua_settable(L, -3);
    }

    void PushTemplateNum(lua_State* L, const char* key, float value) {
        PushStr(L, key);
        PushNum(L, static_cast<double>(value));
        lua_settable(L, -3);
    }

    void PushTemplateStr(lua_State* L, const char* key, const char* value) {
        PushStr(L, key);
        PushStr(L, value ? value : "");
        lua_settable(L, -3);
    }

    void PushTemplateElement(lua_State* L, int32_t v) {
        PushInt(L, v);
    }

    void PushTemplateElement(lua_State* L, uint32_t v) {
        PushInt(L, static_cast<int32_t>(v));
    }

    void PushTemplateElement(lua_State* L, float v) {
        PushNum(L, static_cast<double>(v));
    }

    template <typename T>
    void PushTemplateArray(lua_State* L, const char* key, const T* values, uint32_t count) {
        PushStr(L, key);
        lua_createtable(L, 0, 0);
        lua_checkstack(L, 2);

        for (uint32_t i = 0; i < count; ++i) {
            PushInt(L, static_cast<int32_t>(i + 1));
            PushTemplateElement(L, values[i]);
            lua_settable(L, -3);
        }

        lua_settable(L, -3);
    }

    int GetItemTemplate(lua_State* L) {
        const uint32_t id = static_cast<uint32_t>(ToInt(CheckNumber(L, 1)));
        const ItemStats_C* rec = g_itemDBCache.GetRecord(static_cast<int32_t>(id), WOWGUID(), nullptr, nullptr, false);

        if (!rec) {
            return 0;
        }

        lua_createtable(L, 0, 0);
        lua_checkstack(L, 2);
        PushTemplateInt(L, "ID", static_cast<int32_t>(id));
        PushTemplateStr(L, "Name", rec->m_name[0]);
        PushTemplateInt(L, "Class", rec->m_class);
        PushTemplateInt(L, "SubClass", rec->m_subClass);
        PushTemplateInt(L, "SoundOverrideSubclass", rec->m_soundOverrideSubclass);
        PushTemplateInt(L, "DisplayInfoID", rec->m_displayId);
        PushTemplateInt(L, "Quality", rec->m_quality);
        PushTemplateInt(L, "Flags1", rec->m_flags[0]);
        PushTemplateInt(L, "Flags2", rec->m_flags[1]);
        PushTemplateInt(L, "BuyPrice", rec->m_buyPrice);
        PushTemplateInt(L, "SellPrice", rec->m_sellPrice);
        PushTemplateInt(L, "InvType", rec->m_inventoryType);
        PushTemplateInt(L, "AllowableClass", rec->m_allowableClass);
        PushTemplateInt(L, "AllowableRace", rec->m_allowableRace);
        PushTemplateInt(L, "ItemLevel", rec->m_itemLevel);
        PushTemplateInt(L, "RequiredLevel", rec->m_requiredLevel);
        PushTemplateInt(L, "RequiredSkill", rec->m_requiredSkill);
        PushTemplateInt(L, "RequiredSkillRank", rec->m_requiredSkillRank);
        PushTemplateInt(L, "RequiredSpell", rec->m_requiredSpell);
        PushTemplateInt(L, "RequiredHonorRank", rec->m_requiredHonorRank);
        PushTemplateInt(L, "RequiredCityRank", rec->m_requiredCityRank);
        PushTemplateInt(L, "RequiredReputationFaction", rec->m_requiredReputationFaction);
        PushTemplateInt(L, "RequiredReputationRank", rec->m_requiredReputationRank);
        PushTemplateInt(L, "MaxCount", rec->m_maxCount);
        PushTemplateInt(L, "Stackable", rec->m_stackable);
        PushTemplateInt(L, "ContainerSlots", rec->m_containerSlots);
        PushTemplateInt(L, "StatsCount", rec->m_statsCount);
        PushTemplateArray(L, "ItemStatType", &rec->m_statType[0], 10);
        PushTemplateArray(L, "ItemStatValue", &rec->m_statValue[0], 10);
        PushTemplateInt(L, "ScalingStatDistribution", rec->m_scalingStatDistribution);
        PushTemplateInt(L, "ScalingStatValue", rec->m_scalingStatValue);
        PushTemplateArray(L, "DamageMin", &rec->m_damageMin[0], 2);
        PushTemplateArray(L, "DamageMax", &rec->m_damageMax[0], 2);
        PushTemplateArray(L, "DamageType", &rec->m_damageType[0], 2);
        PushTemplateInt(L, "Armor", rec->m_resistances[0]);
        PushTemplateInt(L, "HolyRes", rec->m_resistances[1]);
        PushTemplateInt(L, "FireRes", rec->m_resistances[2]);
        PushTemplateInt(L, "NatureRes", rec->m_resistances[3]);
        PushTemplateInt(L, "FrostRes", rec->m_resistances[4]);
        PushTemplateInt(L, "ShadowRes", rec->m_resistances[5]);
        PushTemplateInt(L, "ArcaneRes", rec->m_resistances[6]);
        PushTemplateInt(L, "Delay", rec->m_delay);
        PushTemplateInt(L, "AmmoType", rec->m_ammoType);
        PushTemplateNum(L, "RangedModRange", rec->m_rangedModRange);
        PushTemplateArray(L, "SpellId", &rec->m_spellId[0], 5);
        PushTemplateArray(L, "SpellTrigger", &rec->m_spellTrigger[0], 5);
        PushTemplateArray(L, "SpellCharges", &rec->m_spellCharges[0], 5);
        PushTemplateArray(L, "SpellCooldown", &rec->m_spellCooldown[0], 5);
        PushTemplateArray(L, "SpellCategory", &rec->m_spellCategory[0], 5);
        PushTemplateArray(L, "SpellCategoryCooldown", &rec->m_spellCategoryCooldown[0], 5);
        PushTemplateInt(L, "Bonding", rec->m_bonding);
        PushTemplateStr(L, "Description", rec->m_description);
        PushTemplateInt(L, "PageTextID", rec->m_pageText);
        PushTemplateInt(L, "LanguageID", rec->m_languageId);
        PushTemplateInt(L, "PageMaterial", rec->m_pageMaterial);
        PushTemplateInt(L, "StartQuest", rec->m_startQuest);
        PushTemplateInt(L, "LockID", rec->m_lockId);
        PushTemplateInt(L, "Material", rec->m_material);
        PushTemplateInt(L, "Sheath", rec->m_sheatheType);
        PushTemplateInt(L, "RandomProperty", rec->m_randomProperty);
        PushTemplateInt(L, "RandomSuffix", rec->m_randomSuffix);
        PushTemplateInt(L, "Block", rec->m_block);
        PushTemplateInt(L, "ItemSetID", rec->m_itemSet);
        PushTemplateInt(L, "MaxDurability", rec->m_maxDurability);
        PushTemplateInt(L, "AreaID", rec->m_area);
        PushTemplateInt(L, "MapID", rec->m_map);
        PushTemplateInt(L, "BagFamily", rec->m_bagFamily);
        PushTemplateInt(L, "TotemCategory", rec->m_totemCategory);
        PushTemplateArray(L, "SocketColor", &rec->m_socketColor[0], 3);
        PushTemplateArray(L, "SocketContent", &rec->m_socketContent[0], 3);
        PushTemplateInt(L, "SocketBonus", rec->m_socketBonus);
        PushTemplateInt(L, "GemProperties", rec->m_gemProperties);
        PushTemplateInt(L, "RequiredDisenchantSkill", rec->m_requiredDisenchantSkill);
        PushTemplateNum(L, "ArmorDamageModifier", rec->m_armorDamageModifier);
        PushTemplateInt(L, "Duration", rec->m_duration);
        PushTemplateInt(L, "ItemLimitCategory", rec->m_itemLimitCategory);
        PushTemplateInt(L, "HolidayID", rec->m_holidayId);
        return 1;
    }

    // ---- GetEncounterInfo ----
    // (encounter [, unit]) -> name, map, difficulty + 1, SpellIcon path (default the question mark), order index, the
    // unit's loot-lockout seconds and whether the current Mythic+ run killed it. nil for an unknown encounter.
    int GetEncounterInfo(lua_State* L) {
        const bool hasUnit = lua_type(L, 2) > 0;

        if (hasUnit ? !CoA::Script::ValidateInput(L, { LUA_TNUMBER, LUA_TSTRING }) : !CoA::Script::ValidateInput(L, { LUA_TNUMBER })) {
            return 0;
        }

        const uint32_t encounter = static_cast<uint32_t>(static_cast<int64_t>(CheckNumber(L, 1)));

        if (hasUnit) {
            static_cast<void>(CheckString(L, 2));
        }

        const DungeonEncounterRec* row = g_dungeonEncounterDB.GetRecord(static_cast<int32_t>(encounter));

        if (!row) {
            lua_pushnil(L);
            return 1;
        }

        const SpellIconRec* icon = g_spellIconDB.GetRecord(row->m_spellIconID);
        const std::string iconPath = icon ? icon->m_textureFilename : "Interface\\Icons\\inv_misc_questionmark";
        PushStr(L, row->m_name);
        PushInt(L, row->m_mapID);
        PushInt(L, row->m_difficulty + 1);
        PushStr(L, iconPath.c_str());
        PushInt(L, row->m_orderIndex);
        // AscLootLockout::SecondsRemaining waits for the loot-lockout module.
        PushInt(L, 0);
        CoA::Script::PushBool(L, CoA::MythicPlus::EncounterKilled(encounter));
        return 7;
    }

    // ---- GetReducedChanceToBeCrit ----
    // The attacker-crit reductions the player carries: BasePoints + 1 of each SPELL_AURA_MOD_ATTACKER_{SPELL (179),
    // MELEE (187), RANGED (188)}_CRIT_CHANCE effect, 197 counting towards all three. Over the player's auras (every
    // aura, repeats included), then the spell book, each of those only once and not when an aura counted it.
    int GetReducedChanceToBeCrit(lua_State* L) {
        CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();

        if (!player) {
            PushNum(L, 0);
            PushNum(L, 0);
            PushNum(L, 0);
            return 3;
        }

        int32_t spell = 0;
        int32_t melee = 0;
        int32_t ranged = 0;
        std::unordered_set<int32_t> seen;
        SpellRec rec;

        auto add = [&](const SpellRec& r) {
            for (int32_t j = 0; j < 3; ++j) {
                const int32_t amount = r.m_effectBasePoints[j] + 1;

                switch (r.m_effectAura[j]) {
                    case 0xB3: spell += amount; break;
                    case 0xBB: melee += amount; break;
                    case 0xBC: ranged += amount; break;
                    case 0xC5: spell += amount; melee += amount; ranged += amount; break;
                }
            }
        };

        const uint32_t auras = player->GetAuraCount();

        for (uint32_t i = 0; i < auras; ++i) {
            const AuraData* aura = player->GetAura(i);

            if (aura && g_spellDB.GetRecord(aura->m_spellId, &rec)) {
                seen.insert(rec.m_id);
                add(rec);
            }
        }

        // The original's AscCA::KnownSpells pass waits for the character-advancement module.
        for (uint32_t i = 0; i < s_numSpells; ++i) {
            if (!seen.count(s_spellList[i]) && g_spellDB.GetRecord(s_spellList[i], &rec)) {
                seen.insert(rec.m_id);
                add(rec);
            }
        }

        PushNum(L, spell);
        PushNum(L, melee);
        PushNum(L, ranged);
        return 3;
    }

    // ---- IsSpellHoldToCast ----
    // Nothing unless the game window has focus (OsGuiGetWindow(0) == (2)) and the spell has a record. Shaman spells
    // (family 11) hold with family flag 0x2 when mystic enchant 81091 is slotted, or with flag[2] 0x20000000 when CA
    // spell 955831 is known. Otherwise a spell holds when it has no SpellCharges row, lacks AttributesEx2 0x20, has a
    // family, applies no shapeshift (aura 36) and targets neither 23 nor 26 with ImplicitTargetA / B.
    int IsSpellHoldToCast(lua_State* L) {
        const uint32_t spell = static_cast<uint32_t>(ToInt(CheckNumber(L, 1)));

        if (!spell || OsGuiGetWindow(0) != OsGuiGetWindow(2)) {
            return 0;
        }

        SpellRec rec;

        if (!g_spellDB.GetRecord(static_cast<int32_t>(spell), &rec)) {
            return 0;
        }

        const CoA::SpellText::Deps& deps = CoA::SpellText::Dependencies();
        const bool mystic = deps.mysticSlotted && deps.mysticSlotted(0x13CC3);
        const bool known = deps.spellKnown && deps.spellKnown(0xE91B7, false);
        bool hold = false;

        if (rec.m_spellClassSet == 0xB && (((rec.m_spellClassMask[0] & 2) && mystic) || ((rec.m_spellClassMask[2] & 0x20000000) && known))) {
            hold = true;
        } else if (!CoA::Dbc::Get("DBFilesClient\\SpellCharges.dbc").Row(static_cast<uint32_t>(rec.m_id)) && !(rec.m_attributesExB & 0x20) &&
                   rec.m_spellClassSet != 0) {
            bool shapeshift = false;
            bool targets = false;

            for (int32_t j = 0; j < 3; ++j) {
                shapeshift = shapeshift || rec.m_effectAura[j] == 0x24;
                targets = targets || rec.m_implicitTargetA[j] == 0x17 || rec.m_implicitTargetB[j] == 0x17 ||
                    rec.m_implicitTargetA[j] == 0x1A || rec.m_implicitTargetB[j] == 0x1A;
            }

            hold = !shapeshift && !targets;
        }

        CoA::Script::PushBool(L, hold);
        return 1;
    }

    // ---- PlayerAuraHidden ----
    // UnitAura's eleven values for the player's first aura of the spell, hidden or not: name, rank, icon, count,
    // dispel type name (when its immunity flag is set), duration and expiration in seconds (0, 0 when permanent),
    // caster token, stealable and AttributesEx7 0x10000000 as 1 or nil, and the spell id.
    int PlayerAuraHidden(lua_State* L) {
        uint32_t spell = 0;
        CGPlayer_C* player = nullptr;

        if (!CoA::Script::ReadNumber(L, spell) || !(player = ClntObjMgrGetActivePlayerObj())) {
            return 0;
        }

        SpellRec rec;

        for (uint32_t i = 0; i < player->GetAuraCount(); ++i) {
            const AuraData* aura = player->GetAura(i);

            if (!aura || static_cast<uint32_t>(aura->m_spellId) != spell || !g_spellDB.GetRecord(static_cast<int32_t>(spell), &rec)) {
                continue;
            }

            PushStr(L, rec.m_name);
            PushStr(L, rec.m_nameSubtext);
            const SpellIconRec* icon = g_spellIconDB.GetRecord(rec.m_spellIconID);
            PushStr(L, icon ? icon->m_textureFilename : nullptr);
            // +0x0E as a word: the stack count and the byte after it.
            PushNum(L, static_cast<double>(aura->m_stackCount | (aura->unused_000F << 8)));
            const char* dispel = nullptr;

            if (rec.m_dispelType) {
                if (const SpellDispelTypeRec* d = g_spellDispelTypeDB.GetRecord(rec.m_dispelType)) {
                    if (d->m_immunityPossible) {
                        dispel = d->m_internalName;
                    }
                }
            }

            PushStr(L, dispel);

            if (aura->m_duration < 1) {
                PushNum(L, 0);
                PushNum(L, 0);
            } else {
                PushNum(L, aura->m_duration * 0.001);
                PushNum(L, static_cast<uint32_t>(aura->m_endTime) * 0.001);
            }

            PushStr(L, Script_GetAuraTokenFromGUID(&aura->m_creator));

            if (CGSpellBook::CanStealAura(player, aura, &rec)) {
                PushNum(L, 1.0);
            } else {
                lua_pushnil(L);
            }

            if (rec.m_attributesExG & 0x10000000) {
                PushNum(L, 1.0);
            } else {
                lua_pushnil(L);
            }

            PushNum(L, static_cast<double>(static_cast<uint32_t>(rec.m_id)));
            return 11;
        }

        return 0;
    }

    // ---- SetItemQualityColor(quality 0..7, r, g, b) ----
    // Nothing when a component is above 1. Each is scaled by 255 and written as two lower-case hex digits ("" above
    // 0xFF skips the update); "|cffRRGGBB" goes over the quality's colour string and its colour becomes 0xFFRRGGBB.
    int SetItemQualityColor(lua_State* L) {
        const uint32_t q = static_cast<uint32_t>(ToInt(CheckNumber(L, 1)));

        if (q > 7) {
            return 0;
        }

        const float r = static_cast<float>(CheckNumber(L, 2));
        const float g = static_cast<float>(CheckNumber(L, 3));
        const float b = static_cast<float>(CheckNumber(L, 4));

        if (r > 1.0f || g > 1.0f || b > 1.0f) {
            return 0;
        }

        auto hex = [](float f) {
            const uint32_t v = static_cast<uint32_t>(static_cast<int64_t>(f));
            char buffer[8] = {};

            if (v <= 0xFF) {
                snprintf(buffer, sizeof(buffer), "%02x", v);
            }

            return std::string(buffer);
        };

        const float r255 = r * 255.0f;
        const float g255 = g * 255.0f;
        const float b255 = b * 255.0f;
        const std::string rr = hex(r255);
        const std::string gg = hex(g255);
        const std::string bb = hex(b255);

        if (rr.empty() || gg.empty() || bb.empty()) {
            return 0;
        }

        const std::string color = "|cff" + rr + gg + bb;
        const uint32_t argb = 0xFF000000u | (static_cast<uint32_t>(static_cast<uint8_t>(static_cast<int32_t>(r255))) << 16) |
            (static_cast<uint32_t>(static_cast<uint8_t>(static_cast<int32_t>(g255))) << 8) |
            static_cast<uint32_t>(static_cast<uint8_t>(static_cast<int32_t>(b255)));
        CGTooltip::SetItemQualityColor(q, color.c_str(), argb);
        return 0;
    }

    // ---- SetWorldTimeSpeed(speed) ----
    // The game-time rate becomes `speed`, or the stock 1/60 (0x3C888889) for a negative one.
    void SetRateBits(uint32_t bits) {
        memcpy(&g_clientGameTime.m_minutesPerSecond, &bits, 4);
    }

    int SetWorldTimeSpeed(lua_State* L) {
        const float speed = static_cast<float>(CheckNumber(L, 1));
        uint32_t bits = 0x3C888889u;

        if (speed >= 0.0f) {
            memcpy(&bits, &speed, 4);
        }

        SetRateBits(bits);
        return 0;
    }

    // ---- SetWorldTime(hour 0..23, minute 0..59) ----
    // The clock's low bytes are set, the rate is overwritten with the double 100.0 (8 bytes: rate 0, the next field
    // 0x40590000 -- the original's width), the thread sleeps 50 ms, the clock bytes are written again and the rate
    // goes back to the stock 1/60.
    void SetClockBytes(uint8_t hour, uint8_t minute) {
        reinterpret_cast<uint8_t*>(&g_clientGameTime.m_hour)[0] = hour;
        reinterpret_cast<uint8_t*>(&g_clientGameTime.m_minute)[0] = minute;
    }

    int SetWorldTime(lua_State* L) {
        const double hour = CheckNumber(L, 1);
        const double minute = CheckNumber(L, 2);

        if (static_cast<uint32_t>(static_cast<int32_t>(hour)) > 0x17 || static_cast<uint32_t>(static_cast<int32_t>(minute)) > 0x3B) {
            return 0;
        }

        const uint8_t h = static_cast<uint8_t>(static_cast<int32_t>(hour));
        const uint8_t m = static_cast<uint8_t>(static_cast<int32_t>(minute));
        SetClockBytes(h, m);
        SetRateBits(0);
        const uint32_t high = 0x40590000u;
        memcpy(&g_clientGameTime.m_fracMinutes, &high, 4);
        Sleep(50);
        SetClockBytes(h, m);
        SetRateBits(0x3C888889u);
        return 0;
    }

    // ---- GetItemInfoInstant(item) ----
    // The client Item.dbc row and the item addon record, else nil: {itemID, icon, classID, subclassID, name,
    // description, quality, pvePower, pvpPower, miscValue {3}, inventoryType, itemLevel}.
    int GetItemInfoInstant(lua_State* L) {
        if (lua_gettop(L) == 1 && lua_isnumber(L, 1)) {
            const uint32_t id = static_cast<uint32_t>(ToInt(CheckNumber(L, 1)));
            const ItemRec* item = g_itemDB.GetRecord(static_cast<int32_t>(id));
            const CoA::ItemAddon::Record* addon = item ? CoA::ItemAddon::Find(static_cast<uint32_t>(item->m_id)) : nullptr;

            if (item && addon) {
                lua_createtable(L, 0, 0);
                CoA::Script::SetField(L, "itemID", item->m_id);
                ItemDisplayInfoRec display;
                std::string icon;

                if (g_itemDisplayInfoDB.GetRecord(item->m_displayInfoID, &display)) {
                    icon = std::string("Interface\\Icons\\") + (display.m_inventoryIcon[0] ? display.m_inventoryIcon[0] : "");
                } else {
                    icon = "Interface\\Icons\\INV_Misc_QuestionMark";
                }

                lua_pushstring(L, "icon");
                PushStr(L, icon.c_str());
                lua_settable(L, -3);
                CoA::Script::SetField(L, "classID", item->m_classID);
                CoA::Script::SetField(L, "subclassID", item->m_subclassID);
                lua_pushstring(L, "name");
                PushStr(L, addon->name);
                lua_settable(L, -3);
                lua_pushstring(L, "description");
                PushStr(L, addon->description);
                lua_settable(L, -3);
                CoA::Script::SetField(L, "quality", static_cast<int32_t>(addon->fields[0]));
                CoA::Script::SetField(L, "pvePower", static_cast<int32_t>(addon->fields[2]));
                CoA::Script::SetField(L, "pvpPower", static_cast<int32_t>(addon->fields[3]));
                lua_pushstring(L, "miscValue");
                lua_createtable(L, 0, 3);

                for (uint32_t i = 0; i < 3; ++i) {
                    PushNum(L, i + 1);
                    PushInt(L, static_cast<int32_t>(addon->fields[4 + i]));
                    lua_settable(L, -3);
                }

                lua_settable(L, -3);
                CoA::Script::SetField(L, "inventoryType", item->m_inventoryType);
                CoA::Script::SetField(L, "itemLevel", static_cast<int32_t>(addon->fields[11]));
                return 1;
            }
        }

        lua_pushnil(L);
        return 1;
    }

    // ---- ApplyComponents(slot, texture, model) ----
    // "Item\\ObjectComponents\\" + each name, with ItemDisplayInfo row 17, linked onto the player's character model.
    // (The original copies the paths into the component code's own path buffers first and reads the player unchecked.)
    int ApplyComponents(lua_State* L) {
        const int32_t slot = ToInt(CheckNumber(L, 1));
        const std::string first = std::string("Item\\ObjectComponents\\") + CheckString(L, 2);
        const std::string second = std::string("Item\\ObjectComponents\\") + CheckString(L, 3);
        CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();
        ItemDisplayInfoRec display;

        if (player && g_itemDisplayInfoDB.GetRecord(0x11, &display)) {
            CCharacterComponent::AddLink(player->m_characterComponent->m_data.m_model, static_cast<GEOCOMPONENTLINKS>(slot), first.c_str(),
                second.c_str(), 0, &display);
        }

        return 0;
    }

    // ---- the original's code patches, as the flags the client sites read ----
    // SetNamePlateCastBarMode(mode) (FUN_102c1000): non-zero shows every unit's name-plate cast bar.
    int SetNamePlateCastBarMode(lua_State* L) {
        g_namePlateCastBarAll = ToInt(CheckNumber(L, 1)) != 0;
        return 0;
    }

    // SetAttackTexture(on) / SetRangedAttackTexture(on): off makes the attack (ranged) spells show their own icon.
    int SetAttackTexture(lua_State* L) {
        g_attackTextureEnabled = lua_toboolean(L, 1) != 0;
        return 0;
    }

    int SetRangedAttackTexture(lua_State* L) {
        g_rangedAttackTextureEnabled = lua_toboolean(L, 1) != 0;
        return 0;
    }

    // UnitAuraAllowHidden(bool) (FUN_100dd950): true lets UnitAura return the auras the client hides. Returns its
    // argument, as the original does by returning 1 with nothing pushed.
    int UnitAuraAllowHidden(lua_State* L) {
        bool allow = false;

        if (!CoA::Script::ReadBool(L, allow)) {
            return 0;
        }

        g_unitAuraAllowHidden = allow;
        return 1;
    }

    // SetCurrencyShow(n) (FUN_10a71430): non-zero lists every known currency in the unused category.
    int SetCurrencyShow(lua_State* L) {
        g_currencyShowAll = ToInt(CheckNumber(L, 1)) != 0;
        return 0;
    }

    // SetMaskTexture(size, path) (FUN_10a71fc0) re-points the portrait alpha mask (the path kept in 0x10BCCAF4);
    // ClearMaskTexture (FUN_10a6d670) restores the stock masks. Both return 0.
    std::string s_maskTexture;

    int SetMaskTexture(lua_State* L) {
        const int32_t size = ToInt(CheckNumber(L, 1));
        s_maskTexture = CheckString(L, 2);
        g_portraitMaskSize = static_cast<uint32_t>(size);
        g_portraitMaskPath = s_maskTexture.c_str();
        return 0;
    }

    int ClearMaskTexture(lua_State*) {
        g_portraitMaskSize = 0;
        g_portraitMaskPath = nullptr;
        return 0;
    }

    const CoA::Binding s_bindings[] = {
        COA_NATIVE("", "GetSpellData", GetSpellData),
        COA_NATIVE("", "GetWeaponTempEnchantInfo", GetWeaponTempEnchantInfo),
        COA_NATIVE("", "GetRangedWeaponEnchantInfo", GetRangedWeaponEnchantInfo),
        COA_NATIVE("", "GetEquipmentSetItemGUIDs", GetEquipmentSetItemGUIDs),
        COA_NATIVE("", "Custom_HandleTerrainClick", Custom_HandleTerrainClick),
        COA_NATIVE("", "Custom_HandleTerrainClick_PlayerPos", Custom_HandleTerrainClick_PlayerPos),
        COA_NATIVE("", "Custom_HandleTerrainClick_TargetPos", Custom_HandleTerrainClick_TargetPos),
        COA_NATIVE("", "SetAutoRangedCombatSpell", SetAutoRangedCombatSpell),
        COA_NATIVE("", "GetItemTemplate", GetItemTemplate),
        COA_NATIVE("", "GetEncounterInfo", GetEncounterInfo),
        COA_NATIVE("", "GetReducedChanceToBeCrit", GetReducedChanceToBeCrit),
        COA_NATIVE("", "IsSpellHoldToCast", IsSpellHoldToCast),
        COA_NATIVE("", "PlayerAuraHidden", PlayerAuraHidden),
        COA_NATIVE("", "SetItemQualityColor", SetItemQualityColor),
        COA_NATIVE("", "SetWorldTime", SetWorldTime),
        COA_NATIVE("", "SetWorldTimeSpeed", SetWorldTimeSpeed),
        COA_NATIVE("", "GetItemInfoInstant", GetItemInfoInstant),
        COA_NATIVE("", "ApplyComponents", ApplyComponents),
        COA_NATIVE("", "SetNamePlateCastBarMode", SetNamePlateCastBarMode),
        COA_NATIVE("", "SetAttackTexture", SetAttackTexture),
        COA_NATIVE("", "SetRangedAttackTexture", SetRangedAttackTexture),
        COA_NATIVE("", "UnitAuraAllowHidden", UnitAuraAllowHidden),
        COA_NATIVE("", "SetCurrencyShow", SetCurrencyShow),
        COA_NATIVE("", "SetMaskTexture", SetMaskTexture),
        COA_NATIVE("", "ClearMaskTexture", ClearMaskTexture),
    };
}

void CoA::GlobalsJ::InstallClient() {
    // AscRuntime::ReplaceFunction(0x819730, ScriptErrorHandler).
    g_frameScriptErrorHook = CoA::GlobalsJ::OnErrorHandler;
    // AscRuntime::Detour(0x74F910, 0x7385C0, 0x4F5F40): the combat log filter, the player's attack animation and the
    // attack-cursor range.
    g_combatLogAllowedHook = CoA::GlobalsJ::CombatLogAllowed;
    g_attackAnimationHook = CoA::GlobalsJ::AttackAnimation;
    g_attackDistanceHook = CoA::GlobalsJ::AttackDistance;
}

void CoA::GlobalsJClient::Init() {
    CoA::LuaReg::AddModule(s_bindings, sizeof(s_bindings) / sizeof(s_bindings[0]));
}
