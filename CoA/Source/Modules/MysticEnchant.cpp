// AscMysticEnchant.cpp (see MysticEnchant.h).
#include "CoA/Source/Modules/MysticEnchant.h"
#include "CoA/Source/Modules/BridgeStore.h"
#include "CoA/Source/Modules/Config.h"
#include "CoA/Source/Modules/GameMode.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Modules/RealmInfo.h"
#include "CoA/Source/Runtime/ClientState.h"
#include "CoA/Source/Runtime/Dbc.h"
#include "CoA/Source/Runtime/Events.h"
#include "CoA/Source/Runtime/FilterContainer.h"
#include "CoA/Source/Runtime/Lifecycle.h"
#include "CoA/Source/Runtime/Log.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Packet.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Source/Runtime/Read.h"
#include "CoA/Source/Runtime/Script.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <ctime>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {
#include "CoA/Source/Modules/CAEnums.generated.inc"

    using CoA::Script::CheckNumber;
    using CoA::Script::CheckString;
    using CoA::Script::PushBool;
    using CoA::Script::PushInt;
    using CoA::Script::PushNils;
    using CoA::Script::PushNum;
    using CoA::Script::PushStr;
    using CoA::Script::ReadInt2;
    using CoA::Script::ReadNumber;
    using CoA::Script::ToInt;
    using CoA::Script::ValidateInput;
    using Item = CoA::MysticEnchant::Item;

    CoA::MysticEnchant::Deps s_deps;

    const uint32_t kMysticScroll = 0xF25D0;        // "Untarnished Mystic Scroll"
    const uint32_t kMarkOfAscension = 0x5B9D2;
    const uint32_t kMysticExtract = 0x1809F;
    const uint32_t kPresetUnlockToken = 0x1B9271;

    // ---- string tables at the addresses given -----------------------------------------------------------------
    const char* const kFilters[55] = {   // 0x10B55DA0
        "RE_FILTER_NONE", "RE_FILTER_ALL", "RE_FILTER_RELEVANT_POPULAR", "RE_FILTER_RELEVANT_ALL", "RE_FILTER_KNOWN",
        "RE_FILTER_UNKNOWN", "RE_FILTER_UNCOMMON", "RE_FILTER_RARE", "RE_FILTER_EPIC", "RE_FILTER_LEGENDARY",
        "RE_FILTER_ARTIFACT", "RE_FILTER_WORLDFORGED", "RE_FILTER_NOT_WORLDFORGED", "RE_FILTER_CLASS_WARRIOR",
        "RE_FILTER_CLASS_PALADIN", "RE_FILTER_CLASS_HUNTER", "RE_FILTER_CLASS_ROGUE", "RE_FILTER_CLASS_PRIEST",
        "RE_FILTER_CLASS_DEATH_KNIGHT", "RE_FILTER_CLASS_SHAMAN", "RE_FILTER_CLASS_MAGE", "RE_FILTER_CLASS_WARLOCK",
        "RE_FILTER_CLASS_HERO", "RE_FILTER_CLASS_DRUID", "RE_FILTER_CLASS_WARRIOR_SPEC_PROTECTION",
        "RE_FILTER_CLASS_WARRIOR_SPEC_ARMS", "RE_FILTER_CLASS_WARRIOR_SPEC_FURY", "RE_FILTER_CLASS_PALADIN_SPEC_PROTECTION",
        "RE_FILTER_CLASS_PALADIN_SPEC_HOLY", "RE_FILTER_CLASS_PALADIN_SPEC_RETRIBUTION", "RE_FILTER_CLASS_HUNTER_SPEC_SURVIVAL",
        "RE_FILTER_CLASS_HUNTER_SPEC_BEASTMASTERY", "RE_FILTER_CLASS_HUNTER_SPEC_MARKSMANSHIP",
        "RE_FILTER_CLASS_ROGUE_SPEC_ASSASSINATION", "RE_FILTER_CLASS_ROGUE_SPEC_COMBAT", "RE_FILTER_CLASS_ROGUE_SPEC_SUBTLETY",
        "RE_FILTER_CLASS_PRIEST_SPEC_DISCIPLINE", "RE_FILTER_CLASS_PRIEST_SPEC_HOLY", "RE_FILTER_CLASS_PRIEST_SPEC_SHADOW",
        "RE_FILTER_CLASS_DEATH_KNIGHT_SPEC_BLOOD", "RE_FILTER_CLASS_DEATH_KNIGHT_SPEC_FROST",
        "RE_FILTER_CLASS_DEATH_KNIGHT_SPEC_UNHOLY", "RE_FILTER_CLASS_SHAMAN_SPEC_ELEMENTAL",
        "RE_FILTER_CLASS_SHAMAN_SPEC_ENHANCEMENT", "RE_FILTER_CLASS_SHAMAN_SPEC_RESTORATION", "RE_FILTER_CLASS_MAGE_SPEC_ARCANE",
        "RE_FILTER_CLASS_MAGE_SPEC_FROST", "RE_FILTER_CLASS_MAGE_SPEC_FIRE", "RE_FILTER_CLASS_WARLOCK_SPEC_DEMONOLOGY",
        "RE_FILTER_CLASS_WARLOCK_SPEC_DESTRUCTION", "RE_FILTER_CLASS_WARLOCK_SPEC_AFFLICTION", "RE_FILTER_CLASS_HERO_SPEC_HERO",
        "RE_FILTER_CLASS_DRUID_SPEC_RESTORATION", "RE_FILTER_CLASS_DRUID_SPEC_BALANCE", "RE_FILTER_CLASS_DRUID_SPEC_FERAL",
    };
    const char* const kPurchase[8] = {   // 0x10B20748
        "RE_PURCHASE_OK", "RE_PURCHASE_UNKNOWN", "RE_PURCHASE_ITEM_NOT_FOUND", "RE_PURCHASE_NOT_ENOUGH_MONEY",
        "RE_PURCHASE_NOT_ENOUGH_SPACE", "RE_PURCHASE_NO_MYSTIC_ALTAR", "RE_PURCHASE_NOT_WHILE_CASTING", "RE_PURCHASE_BAD_CLASS",
    };
    const char* const kInspect[5] = {   // 0x10B1C600
        "RE_INSPECT_OK", "RE_INSPECT_UNKNOWN", "RE_INSPECT_PLAYER_NOT_FOUND", "RE_INSPECT_PLAYER_NOT_IN_MAP", "RE_INSPECT_BAD_CLASS",
    };
    const char* const kDestroy[7] = {   // 0x10B21D74
        "RE_DESTROY_OK", "RE_DESTROY_UNKNOWN", "RE_DESTROY_BAD_SLOT", "RE_DESTROY_NO_ENCHANT_APPLIED",
        "RE_DESTROY_NOT_WHILE_CASTING", "RE_DESTROY_BAD_CLASS", "RE_DESTROY_BUILD_DRAFT",
    };
    const char* const kApply[26] = {   // 0x10B21DE0
        "RE_APPLY_OK", "RE_APPLY_UNKNOWN", "RE_APPLY_BAD_ITEM", "RE_APPLY_NOT_MYSTIC_SCROLL", "RE_APPLY_BAD_SLOT",
        "RE_APPLY_DUPLICATE_BAG_SLOT_PAIR", "RE_APPLY_STACK_LIMIT", "RE_APPLY_UNCOMMON_LIMIT", "RE_APPLY_RARE_LIMIT",
        "RE_APPLY_EPIC_LIMIT", "RE_APPLY_LEGENDARY_LIMIT", "RE_APPLY_ARTIFACT_LIMIT", "RE_APPLY_UNHANDLED_LIMIT",
        "RE_APPLY_BAD_ENCHANTMENT", "RE_APPLY_BUILD_DRAFT", "RE_APPLY_NOT_WHILE_CASTING", "RE_APPLY_NO_MONEY",
        "RE_APPLY_BAD_CLASS", "RE_APPLY_BAD_REALM", "RE_APPLY_DISABLED_IN_WILDCARD", "RE_APPLY_RARE_WORLDFORGED_LIMIT",
        "RE_APPLY_REQUIRED_AE_INVESTMENT", "RE_APPLY_REQUIRED_TE_INVESTMENT", "RE_APPLY_TOO_LOW_LEVEL",
        "RE_APPLY_DUPLICATE_SLOT", "RE_APPLY_NO_MYSTIC_ALTAR",
    };
    const char* const kCollReforge[30] = {   // 0x10B1CAA0
        "RE_COLLECTION_REFORGE_OK", "RE_COLLECTION_REFORGE_UNKNOWN", "RE_COLLECTION_REFORGE_BAD_ITEM",
        "RE_COLLECTION_REFORGE_NOT_MYSTIC_SCROLL", "RE_COLLECTION_REFORGE_WORLDFORGED_SCROLL",
        "RE_COLLECTION_REFORGE_WORLDFORGED_ENCHANT", "RE_COLLECTION_REFORGE_BAD_SLOT", "RE_COLLECTION_REFORGE_BUILD_DRAFT",
        "RE_COLLECTION_REFORGE_BAD_CLASS", "RE_COLLECTION_REFORGE_ALREADY_APPLIED", "RE_COLLECTION_REFORGE_BAD_ENCHANTMENT",
        "RE_COLLECTION_REFORGE_NOT_KNOWN_ENCHANTMENT", "RE_COLLECTION_REFORGE_NO_MONEY",
        "RE_COLLECTION_REFORGE_NOT_IN_BATTLEGROUNDS", "RE_COLLECTION_REFORGE_DISABLED", "RE_COLLECTION_REFORGE_STACK_LIMIT",
        "RE_COLLECTION_REFORGE_UNCOMMON_LIMIT", "RE_COLLECTION_REFORGE_RARE_LIMIT", "RE_COLLECTION_REFORGE_EPIC_LIMIT",
        "RE_COLLECTION_REFORGE_LEGENDARY_LIMIT", "RE_COLLECTION_REFORGE_ARTIFACT_LIMIT", "RE_COLLECTION_REFORGE_UNHANDLED_LIMIT",
        "RE_COLLECTION_REFORGE_NO_MYSTIC_ALTAR", "RE_COLLECTION_REFORGE_NOT_WHILE_CASTING", "RE_COLLECTION_REFORGE_BAD_REALM",
        "RE_COLLECTION_REFORGE_DISABLED_IN_WILDCARD", "RE_COLLECTION_REFORGE_RARE_WORLDFORGED_LIMIT",
        "RE_COLLECTION_REFORGE_REQUIRED_AE_INVESTMENT", "RE_COLLECTION_REFORGE_REQUIRED_TE_INVESTMENT",
        "RE_COLLECTION_REFORGE_TOO_LOW_LEVEL",
    };
    const char* const kSpecTags[91] = {   // 0x10B1DAC8
        "NONE", "BRUTALITY", "TACTICS", "ANCESTRY", "VOODOO", "BREWING", "SHADOWHUNTING", "SLAYING", "FELBLOOD", "DEMONOLOGY",
        "BOLTSLINGER", "DARKNESS", "INQUISITION", "LIGHTNING", "WIND", "GIFTS", "WAR", "HELLFIRE", "DEFIANCE", "PROTECTION",
        "INSPIRATION", "GLADIATOR", "FIGHTING", "DISCIPLINE", "RUNES", "BLOOD", "FEROCITY", "PACKLEADER", "ARCHERY", "DUELING",
        "SURVIVAL", "DUALITY", "TIME", "DISPLACEMENT", "DEATH", "RIME", "ANIMATION", "INCINERATION", "DESTRUCTION", "DRACONIC",
        "GODBLADE", "CORRUPTION", "INFLUENCE", "ASTRALWARFARE", "TIDES", "MOONBOW", "PIETY", "BLESSINGS", "SERAPHIM",
        "FIREARMS", "INVENTION", "MECHANICS", "VENOM", "STALKING", "FORTITUDE", "REAPING", "SOUL", "DOMINATION", "PRIMAL",
        "GEOMANCY", "LIFE", "ARCANE", "RUNIC", "RIFTBLADE", "ARMS", "FURY", "HOLY", "RETRIBUTION", "BEASTMASTERY",
        "MARKSMANSHIP", "ASSASSINATION", "COMBAT", "SUBTLETY", "SHADOW", "FROST", "UNHOLY", "ELEMENTAL", "ENHANCEMENT",
        "RESTORATION", "FIRE", "AFFLICTION", "BALANCE", "FERAL", "HERO", "BULWARK", "HYDROMANCY", "VALKYR", "MOUNTAINKING",
        "VIZIER", "FLESHWEAVER", "WITCHKNIGHT",
    };
    const char* const kReforge[13] = {   // 0x10B1E5D8
        "RE_REFORGE_OK", "RE_REFORGE_UNKNOWN", "RE_REFORGE_BAD_ITEM", "RE_REFORGE_NOT_MYSTIC_SCROLL",
        "RE_REFORGE_WORLDFORGED_SCROLL", "RE_REFORGE_BAD_SLOT", "RE_REFORGE_BUILD_DRAFT", "RE_REFORGE_NO_MONEY",
        "RE_REFORGE_NOT_IN_BATTLEGROUNDS", "RE_REFORGE_NO_MYSTIC_ALTAR", "RE_REFORGE_NOT_WHILE_CASTING", "RE_REFORGE_BAD_CLASS",
        "RE_REFORGE_DISABLED_IN_WILDCARD",
    };
    const char* const kDisenchant[16] = {   // 0x10B1BCB8
        "RE_DISENCHANT_OK", "RE_DISENCHANT_UNKNOWN", "RE_DISENCHANT_BAD_ITEM", "RE_DISENCHANT_NOT_MYSTIC_SCROLL",
        "RE_DISENCHANT_BAD_SLOT", "RE_DISENCHANT_BUILD_DRAFT", "RE_DISENCHANT_NO_ENCHANTMENT", "RE_DISENCHANT_BAD_ENCHANTMENT",
        "RE_DISENCHANT_ALREADY_KNOWN_ENCHANTMENT", "RE_DISENCHANT_NO_MONEY", "RE_DISENCHANT_NOT_IN_BATTLEGROUNDS",
        "RE_DISENCHANT_DISABLED", "RE_DISENCHANT_NO_MYSTIC_ALTAR", "RE_DISENCHANT_NOT_WHILE_CASTING", "RE_DISENCHANT_BAD_REALM",
        "RE_DISENCHANT_DISABLED_IN_WILDCARD",
    };
    const char* const kEquip[19] = {   // 0x10B1E188
        "RE_EQUIP_OK", "RE_EQUIP_UNKNOWN", "RE_EQUIP_BAD_ENCHANTMENT", "RE_EQUIP_STACK_LIMIT", "RE_EQUIP_UNCOMMON_LIMIT",
        "RE_EQUIP_RARE_LIMIT", "RE_EQUIP_EPIC_LIMIT", "RE_EQUIP_LEGENDARY_LIMIT", "RE_EQUIP_ARTIFACT_LIMIT",
        "RE_EQUIP_UNHANDLED_LIMIT", "RE_EQUIP_BAD_CLASS", "RE_EQUIP_DISABLED", "RE_EQUIP_BAD_REALM",
        "RE_EQUIP_DISABLED_IN_WILDCARD", "RE_EQUIP_RARE_WORLDFORGED_LIMIT", "RE_EQUIP_SPELL_GROUP_STACK_RULES",
        "RE_EQUIP_REQUIRED_AE_INVESTMENT", "RE_EQUIP_REQUIRED_TE_INVESTMENT", "RE_EQUIP_TOO_LOW_LEVEL",
    };
    const char* const kExtract[4] = {   // 0x10B55D7C
        "RE_PURCHASE_MYSTIC_EXTRACT_OK", "RE_PURCHASE_MYSTIC_EXTRACT_UNKNOWN", "RE_PURCHASE_MYSTIC_EXTRACT_NO_TOKENS",
        "RE_PURCHASE_MYSTIC_EXTRACT_ALREADY_OBTAINED",
    };

    // The enum-to-string helpers (FUN_102DFEC0 & co): the name, or "UNEXPECTED_ENUM_VALUE_<n>".
    std::string EnumName(const char* const* table, uint32_t count, uint32_t v) {
        if (v < count) {
            return table[v];
        }

        return "UNEXPECTED_ENUM_VALUE_" + std::to_string(v);
    }

    void PushEnum(lua_State* L, const char* const* table, uint32_t count, uint32_t v) {
        PushStr(L, EnumName(table, count, v).c_str());
    }

    // FUN_10087260 over a table: the index whose name equals s, or -1.
    int EnumIndex(const char* const* table, uint32_t count, const std::string& s) {
        for (uint32_t i = 0; i < count; ++i) {
            if (s == table[i]) {
                return static_cast<int>(i);
            }
        }

        return -1;
    }

    // FUN_10099DE0 / FUN_10099D10 pushed as FUN_1009ADC0 / FUN_1009AC40 build it: {[i] = name}.
    void PushEnumList(lua_State* L, const char* const* table, uint32_t count, const std::vector<uint32_t>& v) {
        lua_createtable(L, 0, static_cast<int>(v.size()));
        lua_checkstack(L, 2);

        for (size_t i = 0; i < v.size(); ++i) {
            PushNum(L, static_cast<double>(i + 1));
            PushEnum(L, table, count, v[i]);
            lua_settable(L, -3);
        }
    }

    // ---- the enchant table and its two indexes ------------------------------------------------------------------
    CoA::Dbc::Table& Dbc() {
        return CoA::Dbc::Get("DBFilesClient\\MysticEnchant.dbc");
    }

    using Row = const uint8_t*;

    struct Index {
        bool built = false;
        std::unordered_map<uint32_t, Row> bySpell;   // 0x10BE04C8
        std::unordered_map<uint32_t, Row> byItem;    // 0x10BE0510
    } s_index;

    uint32_t U32(Row r, uint32_t off) {
        return CoA::Dbc::Table::U32(r, off);
    }

    const char* Str(Row r, uint32_t off) {
        return Dbc().Str(r, off);
    }

    // FUN_101E72E0 at DBC load: every row in file order, a later row with the same key wins.
    Index& Idx() {
        if (!s_index.built) {
            s_index.built = true;
            CoA::Dbc::Table& t = Dbc();

            for (uint32_t i = 0; i < t.Count(); ++i) {
                if (Row r = t.RowAt(i)) {
                    s_index.bySpell[U32(r, 4)] = r;
                    s_index.byItem[U32(r, 0x18)] = r;
                }
            }
        }

        return s_index;
    }

    // FUN_100A0420 on 0x10BE04C8 / 0x10BE0510 (no fallback provider is ever installed).
    Row BySpell(uint32_t spell) {
        auto& m = Idx().bySpell;
        auto it = m.find(spell);
        return it == m.end() ? nullptr : it->second;
    }

    Row ByItem(uint32_t item) {
        auto& m = Idx().byItem;
        auto it = m.find(item);
        return it == m.end() ? nullptr : it->second;
    }

    // ---- the player ----------------------------------------------------------------------------------------------
    bool HasPlayer() {
        return s_deps.hasPlayer && s_deps.hasPlayer();
    }

    uint8_t PlayerClass() {
        return HasPlayer() && s_deps.playerClass ? s_deps.playerClass() : 0;
    }

    uint32_t PlayerLevel() {
        return HasPlayer() && s_deps.playerLevel ? s_deps.playerLevel() : 0;
    }

    uint32_t Coinage() {
        return HasPlayer() && s_deps.coinage ? s_deps.coinage() : 0;
    }

    bool Casting() {
        return HasPlayer() && s_deps.casting && s_deps.casting();
    }

    // FUN_100C6380: the stock ("reborn") classes; FUN_100C5F50: the Conquest of Azeroth classes.
    bool IsStockClass(uint8_t c) {
        return (c >= 1 && c <= 9) || c == 11;
    }

    bool IsCoAClass(uint8_t c) {
        return c >= 12 && c <= 32;
    }

    // ObjectPtr(item guid, 2): the item by its low guid.
    bool ItemObject(uint32_t low, Item& out) {
        return low && s_deps.item && s_deps.item(low, out);
    }

    std::vector<Item> PlayerItems() {
        return HasPlayer() && s_deps.playerItems ? s_deps.playerItems() : std::vector<Item>();
    }

    // FUN_100A03B0: the counter part of a guid.
    uint32_t GuidLow(uint64_t guid) {
        const uint32_t lo = static_cast<uint32_t>(guid);
        const uint32_t hi = static_cast<uint32_t>(guid >> 32);

        if (hi > 0x0FFFFFFF) {
            const uint32_t h = hi >> 16;

            if (h > 0xF101) {
                return lo & 0xFFFFFF;
            }

            if (h != 0xF101 && h != 0x1FC0 && h != 0 && h != 0x1F40 && h != 0x1F50 && h != 0x4000 && h != 0xF100) {
                return lo & 0xFFFFFF;
            }
        }

        return lo;
    }

    // The wire form every item-addressed CMSG uses: bag 0 -> 0xFF / slot + 0x16, bag n -> n + 0x12 / slot - 1.
    void WireBagSlot(const Item& item, uint8_t& bagByte, uint8_t& slotByte) {
        bagByte = item.bag ? static_cast<uint8_t>(item.bag + 0x12) : 0xFF;
        slotByte = item.bag ? static_cast<uint8_t>(item.slot - 1) : static_cast<uint8_t>(item.slot + 0x16);
    }

    // FUN_10308700(player, item, count): at least count of item in the currency-token slots.
    bool HasTokenCount(uint32_t item, uint32_t count) {
        if (!HasPlayer()) {
            return count == 0;
        }

        return count <= (s_deps.tokenCount ? s_deps.tokenCount(item) : 0);
    }

    // FUN_102EA1F0: {item, count} affordable -- the token slots first, else everything the player holds.
    bool HasItems(uint32_t item, uint32_t count) {
        if (HasTokenCount(item, count)) {
            return true;
        }

        uint32_t n = 0;

        for (const Item& it : PlayerItems()) {
            if (it.entry == item) {
                n += it.count;
            }
        }

        return count <= n;
    }

    uint32_t FreeBagSlots() {
        return HasPlayer() && s_deps.freeBagSlots ? s_deps.freeBagSlots() : 0;
    }

    std::string ClientItemName(uint32_t low) {
        return s_deps.itemName ? s_deps.itemName(low) : std::string();
    }

    // Spell.dbc +0xC4 (StackAmount), at least 1; false without a spell record.
    bool MaxStacks(uint32_t spell, uint32_t& stacks) {
        uint32_t raw = 0;

        if (!s_deps.spellStacks || !s_deps.spellStacks(spell, raw)) {
            return false;
        }

        stacks = std::max<uint32_t>(1, raw);
        return true;
    }

    bool HasSpell(uint32_t spell) {
        uint32_t n;
        return MaxStacks(spell, n);
    }

    // ---- row predicates ------------------------------------------------------------------------------------------
    // FUN_102DF780: an RE_QUALITY_* name to its index; an unknown name is logged.
    uint32_t QualityIndex(const char* name) {
        const int i = EnumIndex(kReQualities, 9, name ? name : "");

        if (i >= 0) {
            return static_cast<uint32_t>(i);
        }

        CoA::Log::Printf("Unexpected Value: %s", name ? name : "");
        return 0;
    }

    // FUN_102E65D0: the row's quality -- the "reborn" (+0x10) column for a stock-class player.
    uint32_t QualityOf(Row r) {
        if (HasPlayer() && IsStockClass(PlayerClass())) {
            return QualityIndex(Str(r, 0x10));
        }

        return QualityIndex(Str(r, 0xC));
    }

    // The inlined class test: bit (class - 1) of the 64-bit mask at +0x34 / +0x38.
    bool ClassAllowed(Row r, uint8_t cls) {
        const uint32_t u = static_cast<uint32_t>(cls) - 1;
        uint32_t lo = 1u << (u & 31);
        uint32_t hi = 0;

        if (u > 31) {
            hi = lo;
        }

        lo ^= hi;

        if (u > 63) {
            hi = lo;
        }

        return (lo & U32(r, 0x34)) != 0 || (hi & U32(r, 0x38)) != 0;
    }

    // The realm gates (FUN_102FC660 / 6B0 / 650 / 670 / 640 on RealmInfo +0x40..+0x44).
    bool RealmAllows(Row r) {
        const uint8_t* g = CoA::RealmInfo::Get().gates;
        return (U32(r, 0x20) && g[0]) || (U32(r, 0x24) && g[1]) || (U32(r, 0x28) && g[2]) || (U32(r, 0x2C) && g[3]) ||
            (U32(r, 0x30) && g[4]);
    }

    // FUN_1031EF10 +0x00 bit 6 (the wildcard mode disables mystic enchanting).
    bool WildcardMode() {
        return (CoA::GameMode::Mode() >> 6) & 1;
    }

    // FUN_1031EFD0(FUN_1016F580()): the active spec is a build draft.
    bool DraftSpec() {
        return CoA::GameMode::SpecBuildDraft(CoA::GameMode::ActiveSpecIndex());
    }

    // FUN_102115F0: a rare worldforged enchant.
    bool RareWorldforged(Row r) {
        return QualityOf(r) == 3 && U32(r, 0x1C) != 0;
    }

    // FUN_10211570(cls, strict) / FUN_10211500(cls, tab, strict) over the three class / tab slots.
    bool AllNone(Row r) {
        for (uint32_t i = 0; i < 3; ++i) {
            if (U32(r, 0x4C + i * 4) != 1) {
                return false;
            }
        }

        return true;
    }

    bool HasClassReq(Row r, uint32_t cls, bool strict) {
        if (!strict && AllNone(r)) {
            return true;
        }

        for (uint32_t i = 0; i < 3; ++i) {
            if (U32(r, 0x4C + i * 4) == cls) {
                return true;
            }
        }

        return false;
    }

    bool HasTabReq(Row r, uint32_t cls, uint32_t tab, bool strict) {
        if (!strict && AllNone(r)) {
            return true;
        }

        bool cl = false;

        for (uint32_t i = 0; i < 3; ++i) {
            cl = cl || U32(r, 0x4C + i * 4) == cls;
        }

        if (!cl) {
            return false;
        }

        for (uint32_t i = 0; i < 3; ++i) {
            if (U32(r, 0x58 + i * 4) == tab) {
                return true;
            }
        }

        return false;
    }

    // FUN_102113B0 / FUN_10211440: the AE (+0x64) / TE (+0x70) investment one of the three requirements asks for is met
    // by the active build (FUN_1016F390); no requirement at all passes. The build's answer comes through Deps.
    bool InvestmentMet(Row r, uint32_t base, bool te) {
        bool any = false;

        for (uint32_t i = 0; i < 3; ++i) {
            any = any || U32(r, base + i * 4) != 0;
        }

        if (!any) {
            return true;
        }

        for (uint32_t i = 0; i < 3; ++i) {
            const uint32_t need = U32(r, base + i * 4);

            if (!need) {
                continue;
            }

            const uint32_t have = s_deps.investment ? s_deps.investment(U32(r, 0x4C + i * 4), U32(r, 0x58 + i * 4), te) : 0;

            if (need <= have) {
                return true;
            }
        }

        return false;
    }

    bool AEMet(Row r) {
        return InvestmentMet(r, 0x64, false);
    }

    bool TEMet(Row r) {
        return InvestmentMet(r, 0x70, true);
    }

    // ---- config --------------------------------------------------------------------------------------------------
    int32_t ConfigInt(const std::string& name, int32_t def) {   // FUN_10196AB0
        const int32_t* v = CoA::Config::Int(name.c_str());
        return v ? *v : def;
    }

    bool ConfigBool(const std::string& name, bool def) {   // FUN_10196990
        const bool* v = CoA::Config::Bool(name.c_str());
        return v ? *v : def;
    }

    // FUN_102E9CC0: a class-fusion character (only slots 1..15 exist).
    bool Fusion() {
        if (!HasPlayer()) {
            return false;
        }

        if (!ConfigBool("CONFIG_CLASS_FUSION_ENABLED", false)) {
            return false;
        }

        if (PlayerClass() != 10) {
            return false;
        }

        return (CoA::GameMode::Mode() & 0x1DEF) == 0;
    }

    bool SlotValid(uint32_t slot0) {
        return slot0 < 0x11 && (!Fusion() || slot0 - 1 < 0xF);
    }

    // ---- the manager ---------------------------------------------------------------------------------------------
    struct Pair {
        uint32_t slot;   // 1-based
        uint32_t value;
    };

    struct InspectData {
        uint64_t progress = 0;
        uint32_t level = 0;
        std::vector<uint32_t> applied;
        std::vector<uint32_t> known;
    };

    struct ScrollEntry {   // 0x30
        uint32_t guid = 0;
        uint32_t entry = 0;
        std::string name;
        uint8_t bag = 0;
        uint8_t slot = 0;
        uint32_t quality = 0;
        uint32_t spell = 0;
        bool known = false;
    };

    // MysticEnchantContainer: FilterableContainerBase<MysticEnchantEntry, REFilterType, NullSortType> (vtable 0x10B56504).
    class EnchantContainer : public CoA::Filter::Container<Row> {
    public:
        bool MatchArg(uint32_t f, Row r) {
            return this->ArgMatch(f, r);
        }

        std::string lastRawText;   // what TextMatch compares (the original passes the un-lowered text)

    protected:
        std::vector<Row> DoPopulate() override;
        bool ArgMatch(uint32_t f, const Row& r) override;
        uint32_t ArgGroup(uint32_t f) override;
        bool TextMatch(const std::string& lowerText, const Row& r) override;
        std::string FilterKey(const Row& r) override;
    };

    struct Mgr {
        std::vector<uint32_t> known;                        // +0x00
        std::vector<uint32_t> slots;                        // +0x0C
        std::vector<uint32_t> slotsAux;                     // +0x18
        uint64_t progress = 0;                              // +0x28
        uint32_t level = 0;                                 // +0x30
        std::vector<Pair> pendingReforge;                   // +0x34
        std::vector<Pair> pendingApply;                     // +0x40
        uint32_t inspectKey = 0;                            // +0x4C
        std::unordered_map<uint32_t, InspectData> inspect;  // +0x50
        bool altarOpen = false;                             // +0x70
        EnchantContainer container;                         // +0x74
        std::vector<ScrollEntry> scrolls;                   // +0xF0
        __time64_t altarCheck = 0;                          // DAT_10BE3920
    };

    Mgr& M() {
        static Mgr m;
        return m;
    }

    bool Known(uint32_t spell) {   // FUN_102E96B0
        const auto& k = M().known;
        return std::find(k.begin(), k.end(), spell) != k.end();
    }

    uint32_t SlotAt(uint32_t slot0) {   // FUN_102E3FC0
        if (SlotValid(slot0) && slot0 < M().slots.size()) {
            return M().slots[slot0];
        }

        return 0;
    }

    // FUN_102EA0C0's list: the mystic altar game objects.
    bool IsAltarEntry(uint32_t entry) {
        static const uint32_t kAltars[] = {
            0x30, 0x13914, 0x2B18A, 0x2B18B, 0x2B18D, 0x3BD0E, 0x57390, 0xC354F, 0xF428F, 0x1D0B98, 0x1D0B99, 0x3174B8,
            0x3182BE, 0x3183CC, 0x3183CE, 0x3183ED, 0x31869E, 0x3188F0, 0x31956C, 0x31A0B9, 7100000, 0x7A1233, 0x7A1234, 0x843F77,
        };

        for (uint32_t a : kAltars) {
            if (a == entry) {
                return true;
            }
        }

        return false;
    }

    // FUN_102E96E0: a mystic altar within 10 yards (MysticEnchantClient.cpp walks the visible objects).
    bool AltarNearby() {
        return HasPlayer() && s_deps.altarNearby && s_deps.altarNearby();
    }

    // ---- progress ------------------------------------------------------------------------------------------------
    // FUN_102E51A0: the progress a level needs.
    uint64_t LevelProgress(uint32_t level) {
        if (level == 0) {
            return 1;
        }

        if (level > 0xF9) {
            return static_cast<uint32_t>(level * 0x1001 - 0x72038);
        }

        const double l = static_cast<double>(level);
        return static_cast<uint64_t>(std::floor(l * 7.5 * l + static_cast<double>(static_cast<int32_t>(level * 0x162))));
    }

    // GetProgress / SMSG 0x619: (progress - need(level - 1)) / (need(level) - need(level - 1)) * 100. The differences
    // convert as UNSIGNED 64-bit (FUN_10AE6420).
    double ProgressPercent(uint64_t progress, uint32_t level) {
        const uint32_t prev = level ? level - 1 : 0;
        const double span = static_cast<double>(LevelProgress(level) - LevelProgress(prev));
        const double into = static_cast<double>(progress - LevelProgress(prev));
        return into / span * 100.0;
    }

    // FUN_102E6530: the same ratio (a float, not scaled) for an inspect result.
    float InspectProgress(const InspectData& d) {
        if (d.level == 0) {
            return 0.0f;
        }

        const uint64_t a = LevelProgress((d.level > 1 ? d.level : 1) - 1);
        const uint64_t b = LevelProgress(d.level);

        if (b == a) {
            return 0.0f;
        }

        return static_cast<float>(d.progress - a) / static_cast<float>(b - a);   // FUN_10AE64C0: unsigned
    }

    // ---- the projected slot maps FUN_102E6620 / FUN_102E6DF0 / FUN_102E7390 --------------------------------------
    // Slot -> enchant for the first count (at most 17) slots, then (with overlay) the pending applies and collection
    // reforges, then slot0 <- spell when a spell is given.
    std::unordered_map<uint32_t, uint32_t> Projected(bool overlay, uint32_t slot0, uint32_t spell, uint32_t count) {
        std::unordered_map<uint32_t, uint32_t> m;

        if (count > 0x11) {
            count = 0x11;
        }

        for (uint32_t i = 0; i < count; ++i) {
            m[i] = SlotAt(i);
        }

        if (overlay) {
            for (const Pair& p : M().pendingApply) {
                Item item;

                if (ItemObject(p.value, item)) {
                    if (Row r = ByItem(item.entry)) {
                        m[p.slot - 1] = U32(r, 4);
                    }
                }
            }

            for (const Pair& p : M().pendingReforge) {
                m[p.slot - 1] = p.value;
            }
        }

        if (spell != 0) {
            m[slot0] = spell;
        }

        return m;
    }

    uint32_t CountSpell(uint32_t which, bool overlay, uint32_t slot0, uint32_t spell, uint32_t count) {   // FUN_102E6DF0
        uint32_t n = 0;

        for (const auto& kv : Projected(overlay, slot0, spell, count)) {
            n += kv.second == which;
        }

        return n;
    }

    uint32_t CountQuality(uint32_t q, bool overlay, uint32_t slot0, uint32_t spell, uint32_t count) {   // FUN_102E7390
        uint32_t n = 0;

        for (const auto& kv : Projected(overlay, slot0, spell, count)) {
            if (Row r = BySpell(kv.second)) {
                n += QualityOf(r) == q;
            }
        }

        return n;
    }

    uint32_t CountRareWorldforged(bool overlay, uint32_t slot0, uint32_t spell, uint32_t count) {   // FUN_102E6620
        uint32_t n = 0;

        for (const auto& kv : Projected(overlay, slot0, spell, count)) {
            if (Row r = BySpell(kv.second)) {
                n += RareWorldforged(r);
            }
        }

        return n;
    }

    // FUN_102E0790: how many of the first n "<fmt>{i}" unlock levels the player has reached, capped.
    uint32_t UnlockedCount(uint32_t cap, uint32_t n, const char* fmt) {
        uint32_t c = 0;
        const uint32_t level = PlayerLevel();

        for (uint32_t i = 1; i <= n; ++i) {
            if (static_cast<uint32_t>(ConfigInt(fmt + std::to_string(i), 0)) <= level) {
                ++c;
            }
        }

        return cap < c ? cap : c;
    }

    // The per-quality unlock loop of FUN_102E5250 (the "REBORN" keys for a stock-class player).
    uint32_t UnlockLoop(uint32_t max, const char* normal, const char* reborn) {
        for (uint32_t i = 0;;) {
            const uint32_t level = PlayerLevel();
            const char* fmt = IsStockClass(PlayerClass()) ? reborn : normal;
            const uint32_t need = static_cast<uint32_t>(ConfigInt(fmt + std::to_string(i + 1), 0));

            if (level < need) {
                return i;
            }

            ++i;

            if (max <= i) {
                return max;
            }
        }
    }

    // FUN_102E5250: how many enchants of quality q may be equipped.
    uint32_t QualityCap(uint32_t q) {
        if (!Fusion()) {
            const bool stock = IsStockClass(PlayerClass());
            uint32_t max;

            switch (q) {
                case 2:
                    return 0x11;
                case 3:
                    max = static_cast<uint32_t>(ConfigInt(stock ? "CONFIG_MAX_RARE_RANDOM_ENCHANTS_REBORN" : "CONFIG_MAX_RARE_RANDOM_ENCHANTS", 0x11));
                    return max ? UnlockLoop(max, "CONFIG_RARE_RANDOM_ENCHANT_UNLOCK_LEVEL_", "CONFIG_RARE_RANDOM_ENCHANT_REBORN_UNLOCK_LEVEL_") : 0;
                case 4:
                    max = static_cast<uint32_t>(stock ? ConfigInt("CONFIG_MAX_EPIC_RANDOM_ENCHANTS_REBORN", 5) : ConfigInt("CONFIG_MAX_EPIC_RANDOM_ENCHANTS", 3));
                    return max ? UnlockLoop(max, "CONFIG_EPIC_RANDOM_ENCHANT_UNLOCK_LEVEL_", "CONFIG_EPIC_RANDOM_ENCHANT_REBORN_UNLOCK_LEVEL_") : 0;
                case 5:
                    max = static_cast<uint32_t>(ConfigInt(stock ? "CONFIG_MAX_LEGENDARY_RANDOM_ENCHANTS_REBORN" : "CONFIG_MAX_LEGENDARY_RANDOM_ENCHANTS", 1));
                    return max ? UnlockLoop(max, "CONFIG_LEGENDARY_RANDOM_ENCHANT_UNLOCK_LEVEL_", "CONFIG_LEGENDARY_RANDOM_ENCHANT_REBORN_UNLOCK_LEVEL_") : 0;
                case 6:
                    max = static_cast<uint32_t>(ConfigInt(stock ? "CONFIG_MAX_ARTIFACT_RANDOM_ENCHANTS_REBORN" : "CONFIG_MAX_ARTIFACT_RANDOM_ENCHANTS", 1));
                    return max ? UnlockLoop(max, "CONFIG_ARTIFACT_RANDOM_ENCHANT_UNLOCK_LEVEL_", "CONFIG_ARTIFACT_RANDOM_ENCHANT_REBORN_UNLOCK_LEVEL_") : 0;
                default:
                    return 0;
            }
        }

        switch (q) {
            case 2:
                return UnlockedCount(static_cast<uint32_t>(ConfigInt("CONFIG_MAX_UNCOMMON_RANDOM_ENCHANTS", 6)), 6, "CONFIG_UNCOMMON_RANDOM_ENCHANT_UNLOCK_LEVEL_");
            case 3:
                return UnlockedCount(static_cast<uint32_t>(ConfigInt("CONFIG_MAX_RARE_RANDOM_ENCHANTS", 5)), 5, "CONFIG_RARE_RANDOM_ENCHANT_UNLOCK_LEVEL_");
            case 4:
                return UnlockedCount(static_cast<uint32_t>(ConfigInt("CONFIG_MAX_EPIC_RANDOM_ENCHANTS", 4)), 4, "CONFIG_EPIC_RANDOM_ENCHANT_UNLOCK_LEVEL_");
            default:
                return 0;
        }
    }

    // ---- costs ---------------------------------------------------------------------------------------------------
    // FUN_102E4000(row, money, slotVariant): the collection-reforge cost; -1 for "no token price". An unexpected
    // quality returns the quality itself (the original returns the switch value).
    uint32_t CollectionReforgeCost(Row r, bool money, bool slot) {
        static const struct {
            const char* item;
            const char* slotMoney;
            const char* slotToken;
            int32_t moneyDef;
            int32_t tokenDef;
        } kCosts[] = {
            { "CONFIG_MYSTIC_ENCHANT_COLLECTION_REFORGE_ITEM_UNCOMMON_MONEY_COST", "CONFIG_MYSTIC_ENCHANT_COLLECTION_REFORGE_UNCOMMON_MONEY_COST",
                "CONFIG_MYSTIC_ENCHANT_COLLECTION_REFORGE_UNCOMMON_TOKEN_COST", 300000, 0x4B0 },
            { "CONFIG_MYSTIC_ENCHANT_COLLECTION_REFORGE_ITEM_RARE_MONEY_COST", "CONFIG_MYSTIC_ENCHANT_COLLECTION_REFORGE_RARE_MONEY_COST",
                "CONFIG_MYSTIC_ENCHANT_COLLECTION_REFORGE_RARE_TOKEN_COST", 600000, 0x960 },
            { "CONFIG_MYSTIC_ENCHANT_COLLECTION_REFORGE_ITEM_EPIC_MONEY_COST", "CONFIG_MYSTIC_ENCHANT_COLLECTION_REFORGE_EPIC_MONEY_COST",
                "CONFIG_MYSTIC_ENCHANT_COLLECTION_REFORGE_EPIC_TOKEN_COST", 1000000, 4000 },
            { "CONFIG_MYSTIC_ENCHANT_COLLECTION_REFORGE_ITEM_LEGENDARY_MONEY_COST", "CONFIG_MYSTIC_ENCHANT_COLLECTION_REFORGE_LEGENDARY_MONEY_COST",
                "CONFIG_MYSTIC_ENCHANT_COLLECTION_REFORGE_LEGENDARY_TOKEN_COST", 2500000, 10000 },
            { "CONFIG_MYSTIC_ENCHANT_COLLECTION_REFORGE_ITEM_ARTIFACT_MONEY_COST", "CONFIG_MYSTIC_ENCHANT_COLLECTION_REFORGE_ARTIFACT_MONEY_COST",
                "CONFIG_MYSTIC_ENCHANT_COLLECTION_REFORGE_ARTIFACT_TOKEN_COST", 2500000, 10000 },
        };
        const uint32_t q = QualityOf(r);

        if (q < 2 || q > 6) {
            return q;
        }

        const auto& c = kCosts[q - 2];

        if (!slot) {
            if (!money) {
                return 0xFFFFFFFF;
            }

            return static_cast<uint32_t>(ConfigInt(c.item, c.moneyDef));
        }

        return money ? static_cast<uint32_t>(ConfigInt(c.slotMoney, c.moneyDef)) : static_cast<uint32_t>(ConfigInt(c.slotToken, c.tokenDef));
    }

    // FUN_102E4AA0: the extract (disenchant) token cost.
    uint32_t ExtractCost(Row r) {
        switch (const uint32_t q = QualityOf(r)) {
            case 2: return static_cast<uint32_t>(ConfigInt("CONFIG_MYSTIC_ENCHANT_EXTRACT_UNCOMMON_TOKEN_COST", 0x9C4));
            case 3: return static_cast<uint32_t>(ConfigInt("CONFIG_MYSTIC_ENCHANT_EXTRACT_RARE_TOKEN_COST", 5000));
            case 4: return static_cast<uint32_t>(ConfigInt("CONFIG_MYSTIC_ENCHANT_EXTRACT_EPIC_TOKEN_COST", 10000));
            case 5: return static_cast<uint32_t>(ConfigInt("CONFIG_MYSTIC_ENCHANT_EXTRACT_LEGENDARY_TOKEN_COST", 20000));
            case 6: return static_cast<uint32_t>(ConfigInt("CONFIG_MYSTIC_ENCHANT_EXTRACT_ARTIFACT_TOKEN_COST", 20000));
            default: return q;
        }
    }

    // FUN_102E6C50
    uint32_t ReforgeCost(bool money) {
        return money ? static_cast<uint32_t>(ConfigInt("CONFIG_MYSTIC_ENCHANT_REFORGE_MONEY_COST", 25000))
                     : static_cast<uint32_t>(ConfigInt("CONFIG_MYSTIC_ENCHANT_REFORGE_TOKEN_COST", 0xFA));
    }

    // The extract purchase (FUN_102E2CC0 and the bindings): level * 200 + 1000, at most 5000.
    uint32_t ExtractPurchaseCost() {
        const uint32_t v = M().level * 200 + 1000;
        return v > 5000 ? 5000 : v;
    }

    uint32_t SatAdd(uint32_t a, uint32_t b) {
        return b > ~a ? 0xFFFFFFFF : a + b;
    }

    // The cost object FUN_102E0B20 accumulates into: money plus {item, count} pairs.
    struct Cost {
        uint32_t money = 0;
        std::vector<std::pair<uint32_t, uint32_t>> items;

        uint32_t Count(uint32_t item) const {
            for (const auto& p : this->items) {
                if (p.first == item) {
                    return p.second;
                }
            }

            return 0;
        }

        void Add(uint32_t item, uint32_t n) {
            for (auto& p : this->items) {
                if (p.first == item) {
                    p.second = SatAdd(p.second, n);
                    return;
                }
            }

            this->items.emplace_back(item, n);
        }
    };

    // The shared walk of FUN_102E0BA0 (affordable?) and FUN_102E48C0 (totals): each row pays in Marks of Ascension when
    // the player holds enough (cumulatively), else in money if that still fits.
    bool Tally(const std::vector<Row>& rows, bool slot, Cost& c, bool strict) {
        for (Row r : rows) {
            if (!r) {   // CanApplyItem with no scroll row: the original dereferences null here
                continue;
            }

            const uint32_t tok = CollectionReforgeCost(r, false, slot);
            const uint32_t money = CollectionReforgeCost(r, true, slot);
            const bool tokOk = tok != 0 && tok != 0xFFFFFFFF;
            const bool moneyOk = money != 0 && money != 0xFFFFFFFF;

            if (tokOk && HasItems(kMarkOfAscension, SatAdd(c.Count(kMarkOfAscension), tok))) {
                c.Add(kMarkOfAscension, tok);
                continue;
            }

            if (!moneyOk || Coinage() < SatAdd(money, c.money)) {
                if (strict) {
                    return false;
                }

                continue;
            }

            c.money = SatAdd(money, c.money);
        }

        return !strict || c.money <= Coinage();
    }

    bool Affordable(const std::vector<Row>& rows, bool slot) {   // FUN_102E0BA0
        Cost c;
        return HasPlayer() && Tally(rows, slot, c, true);
    }

    void Totals(const std::vector<Row>& rows, bool slot, uint32_t& money, uint32_t& tokens) {   // FUN_102E48C0
        Cost c;

        if (HasPlayer()) {
            Tally(rows, slot, c, false);
        }

        money = c.money;
        tokens = c.Count(kMarkOfAscension);
    }

    // FUN_102E0D60: the plain reforge is affordable.
    bool ReforgeAffordable() {
        const uint32_t tok = ReforgeCost(false);
        const uint32_t money = ReforgeCost(true);

        if (tok != 0 && HasItems(kMarkOfAscension, tok)) {
            return true;
        }

        if (money == 0) {
            return false;
        }

        return money <= Coinage();
    }

    std::vector<Row> PendingReforgeRows() {
        std::vector<Row> rows;

        for (const Pair& p : M().pendingReforge) {
            if (Row r = BySpell(p.value)) {
                rows.push_back(r);
            }
        }

        return rows;
    }

    // ---- validators (each returns the original's error vector; the bindings use the first) ------------------------
    // FUN_102E0F00: the apply gates every apply check shares.
    void ApplyGates(std::vector<uint32_t>& e) {
        if (DraftSpec()) {
            e.push_back(0xE);
        }

        if (Casting()) {
            e.push_back(0xF);
        }

        if (WildcardMode()) {
            e.push_back(0x13);
        }
    }

    // The quality-limit error of each validator: base + 0 (uncommon) .. base + 4 (artifact), base + 5 any other.
    uint32_t QualityLimitError(uint32_t q, uint32_t base) {
        return q >= 2 && q <= 6 ? base + (q - 2) : base + 5;
    }

    // FUN_102E1500: can spell go into slot0 (stack, quality, level, class, realm).
    void ApplyEnchantChecks(uint32_t slot0, uint32_t spell, std::vector<uint32_t>& e) {
        Row r = BySpell(spell);

        if (!r) {
            e.push_back(0xD);
            return;
        }

        uint32_t maxStacks = 0;
        MaxStacks(spell, maxStacks);

        if (maxStacks < CountSpell(spell, true, slot0, spell, 0xFFFFFFFF)) {
            e.push_back(6);
        }

        const uint32_t q = QualityOf(r);

        if (QualityCap(q) < CountQuality(q, true, slot0, spell, 0xFFFFFFFF)) {
            e.push_back(QualityLimitError(q, 7));
        }

        if (RareWorldforged(r) && CountRareWorldforged(true, slot0, spell, 0xFFFFFFFF) > 3) {
            e.push_back(0x14);
        }

        if (!AEMet(r)) {
            e.push_back(0x15);
        }

        if (!TEMet(r)) {
            e.push_back(0x16);
        }

        if (HasPlayer() && PlayerLevel() < U32(r, 0x14)) {
            e.push_back(0x17);
        }

        if (HasPlayer() && !ClassAllowed(r, PlayerClass())) {
            e.push_back(0x11);
        }

        if (!RealmAllows(r)) {
            e.push_back(0x12);
        } else if (WildcardMode()) {
            e.push_back(0x13);
        }
    }

    // FUN_102E1360: CanApplySlot(slot0, item).
    std::vector<uint32_t> CheckApplySlot(uint32_t slot0, uint32_t itemLow) {
        std::vector<uint32_t> e;

        if (!SlotValid(slot0)) {
            e.push_back(4);
        }

        Item item;

        if (!ItemObject(itemLow, item)) {
            e.push_back(2);
        } else if (Row r = ByItem(item.entry)) {
            ApplyEnchantChecks(slot0, U32(r, 4), e);
        } else {
            e.push_back(3);
        }

        ApplyGates(e);

        for (const Pair& p : M().pendingApply) {
            if (p.value == itemLow && p.slot - 1 != slot0) {
                e.push_back(5);
                break;
            }
        }

        return e;
    }

    // FUN_102E0FE0: CanApplyItem(slot0, item).
    std::vector<uint32_t> CheckApplyItem(uint32_t slot0, uint32_t itemLow) {
        std::vector<uint32_t> e;

        if (!SlotValid(slot0)) {
            e.push_back(4);
        }

        Item item;

        if (!ItemObject(itemLow, item)) {
            e.push_back(2);
        } else {
            Row scroll = ByItem(item.entry);

            if (!scroll) {
                e.push_back(3);
            }

            if (Row cur = BySpell(SlotAt(slot0))) {
                Row r = BySpell(U32(cur, 4));

                if (!r) {
                    e.push_back(0xD);
                } else if (!RealmAllows(r)) {
                    e.push_back(0x12);
                } else if (WildcardMode()) {
                    e.push_back(0x13);
                }

                if (!Affordable(std::vector<Row>{ scroll }, false)) {
                    e.push_back(0x10);
                }
            }
        }

        ApplyGates(e);

        if (Fusion() && !AltarNearby()) {
            e.push_back(0x19);
        }

        return e;
    }

    // FUN_102E18F0: the collection-reforge checks shared by the item and slot forms.
    void ReforgeTargetChecks(uint32_t current, uint32_t spell, std::vector<uint32_t>& e) {
        if (current == spell) {
            e.push_back(9);
        }

        if (Row r = BySpell(spell)) {
            if (!Known(U32(r, 4))) {
                e.push_back(0xB);
            }

            if (!RealmAllows(r)) {
                e.push_back(0x18);
            } else if (WildcardMode()) {
                e.push_back(0x19);
            }
        } else {
            e.push_back(10);
        }

        if (!AltarNearby()) {
            e.push_back(0x16);
        }

        if (Casting()) {
            e.push_back(0x17);
        }
    }

    struct ReforgeCosts {
        uint32_t moneyDelta = 0;
        uint32_t tokenDelta = 0;
        uint32_t money = 0;
        uint32_t tokens = 0;
    };

    void FillCosts(const std::vector<Row>& rows, bool slot, uint32_t money0, uint32_t tokens0, ReforgeCosts& out) {
        Totals(rows, slot, out.money, out.tokens);
        out.moneyDelta = money0 < out.money ? out.money - money0 : 0;
        out.tokenDelta = tokens0 < out.tokens ? out.tokens - tokens0 : 0;
    }

    // FUN_102E1AB0: CanCollectionReforgeItem(item, spell).
    std::vector<uint32_t> CheckCollectionReforgeItem(uint32_t itemLow, uint32_t spell, ReforgeCosts& out) {
        std::vector<uint32_t> e;
        out = ReforgeCosts();
        uint32_t current = 0;
        Item item;

        if (!ItemObject(itemLow, item)) {
            e.push_back(2);
        } else if (item.entry != kMysticScroll) {
            if (Row s = ByItem(item.entry)) {
                if (U32(s, 0x1C)) {
                    e.push_back(4);
                }

                current = U32(s, 4);
            } else {
                e.push_back(3);
            }
        }

        ReforgeTargetChecks(current, spell, e);
        Row r = BySpell(spell);

        if (!r) {
            return e;
        }

        std::vector<Row> rows = PendingReforgeRows();
        uint32_t money0;
        uint32_t tokens0;
        Totals(rows, false, money0, tokens0);
        rows.push_back(r);

        if (!Affordable(rows, false)) {
            e.push_back(0xC);
        }

        if (U32(r, 0x1C)) {
            e.push_back(5);
        }

        FillCosts(rows, false, money0, tokens0, out);
        return e;
    }

    // FUN_102E1E70: CanCollectionReforgeSlot(slot0, spell); pending = the target is already in the pending list (the
    // save check).
    std::vector<uint32_t> CheckCollectionReforgeSlot(uint32_t slot0, uint32_t spell, bool pending, ReforgeCosts& out) {
        std::vector<uint32_t> e;
        out = ReforgeCosts();

        if (!SlotValid(slot0)) {
            e.push_back(6);
        }

        if (DraftSpec()) {
            e.push_back(7);
        }

        ReforgeTargetChecks(SlotAt(slot0), spell, e);
        Row r = BySpell(spell);

        if (!r) {
            return e;
        }

        std::vector<Row> rows = PendingReforgeRows();
        uint32_t money0;
        uint32_t tokens0;
        Totals(rows, true, money0, tokens0);

        if (!pending) {
            rows.push_back(r);
        }

        if (!Affordable(rows, true)) {
            e.push_back(0xC);
        }

        if (HasPlayer() && !ClassAllowed(r, PlayerClass())) {
            e.push_back(8);
        }

        FillCosts(rows, true, money0, tokens0, out);

        if (!Known(U32(r, 4))) {
            e.push_back(0xB);
        }

        uint32_t maxStacks = 0;
        MaxStacks(U32(r, 4), maxStacks);

        if (maxStacks < CountSpell(U32(r, 4), true, slot0, spell, 0xFFFFFFFF)) {
            e.push_back(0xF);
        }

        const uint32_t q = QualityOf(r);

        if (QualityCap(q) < CountQuality(q, true, slot0, spell, 0xFFFFFFFF)) {
            e.push_back(QualityLimitError(q, 0x10));
        }

        if (RareWorldforged(r) && CountRareWorldforged(true, slot0, spell, 0xFFFFFFFF) > 3) {
            e.push_back(0x1A);
        }

        if (!AEMet(r)) {
            e.push_back(0x1B);
        }

        if (!TEMet(r)) {
            e.push_back(0x1C);
        }

        if (HasPlayer() && PlayerLevel() < U32(r, 0x14)) {
            e.push_back(0x1D);
        }

        return e;
    }

    // FUN_102E2750: the disenchant checks for an enchant spell (RE_DISENCHANT_*, 0 = ok).
    uint32_t CheckDisenchant(uint32_t spell) {
        Row r = spell ? BySpell(spell) : nullptr;

        if (!r) {
            return 7;
        }

        if (!RealmAllows(r)) {
            return 0xE;
        }

        if (WildcardMode()) {
            return 0xF;
        }

        if (Known(spell)) {
            return 8;
        }

        if (!AltarNearby()) {
            return 0xC;
        }

        if (Casting()) {
            return 0xD;
        }

        const uint32_t tok = ExtractCost(r);

        if (tok == 0 || tok == 0xFFFFFFFF) {
            return 9;
        }

        return HasPlayer() && HasItems(kMysticExtract, tok) ? 0 : 9;
    }

    // FUN_102E2CC0: the mystic extract purchase (RE_PURCHASE_MYSTIC_EXTRACT_*).
    uint32_t CheckExtractPurchase() {
        return HasTokenCount(kMysticExtract, 1) ? 3 : 0;
    }

    // FUN_102E29F0: can spell be equipped (in slot0, or anywhere for -1) -- RE_EQUIP_*, 0 = ok.
    uint32_t CheckEquip(uint32_t spell, uint32_t slot0) {
        Row r = BySpell(spell);

        if (!r) {
            return 2;
        }

        if (slot0 != 0xFFFFFFFF && !SlotValid(slot0)) {
            return 0xB;
        }

        uint32_t maxStacks = 0;

        if (!MaxStacks(U32(r, 4), maxStacks)) {
            return 3;
        }

        if (CountSpell(U32(r, 4), false, 0, 0, slot0) >= maxStacks) {
            return 3;
        }

        if (HasPlayer() && !ClassAllowed(r, PlayerClass())) {
            return 10;
        }

        if (WildcardMode()) {
            return 0xD;
        }

        if (!AEMet(r)) {
            return 0x10;
        }

        if (!TEMet(r)) {
            return 0x11;
        }

        if (HasPlayer() && PlayerLevel() < U32(r, 0x14)) {
            return 0x12;
        }

        const uint32_t q = QualityOf(r);

        if (QualityCap(q) <= CountQuality(q, false, 0, 0, slot0)) {
            return QualityLimitError(q, 4);
        }

        if (RareWorldforged(r)) {
            return CountRareWorldforged(false, 0, 0, slot0) > 3 ? 0xE : 0;
        }

        return 0;
    }

    // FUN_102E2FB0: the reforge gates (RE_REFORGE_*).
    void ReforgeGates(std::vector<uint32_t>& e) {
        if (!ReforgeAffordable()) {
            e.push_back(7);
        }

        if (!AltarNearby()) {
            e.push_back(9);
        }

        if (HasPlayer()) {
            if (Casting()) {
                e.push_back(10);
            }

            if (IsCoAClass(PlayerClass())) {
                e.push_back(0xB);
            }
        }

        if (WildcardMode()) {
            e.push_back(0xC);
        }
    }

    std::vector<uint32_t> CheckReforgeItem(uint32_t itemLow) {   // FUN_102E30C0
        std::vector<uint32_t> e;
        Item item;

        if (!ItemObject(itemLow, item)) {
            e.push_back(2);
        } else if (item.entry != kMysticScroll) {
            Row r = ByItem(item.entry);

            if (!r) {
                e.push_back(3);
            } else if (U32(r, 0x1C)) {
                e.push_back(4);
            }
        }

        ReforgeGates(e);
        return e;
    }

    std::vector<uint32_t> CheckReforgeSlot(uint32_t slot0) {   // FUN_102E31D0
        std::vector<uint32_t> e;

        if (!SlotValid(slot0)) {
            e.push_back(5);
        }

        if (DraftSpec()) {
            e.push_back(6);
        }

        ReforgeGates(e);
        return e;
    }

    std::vector<uint32_t> CheckDestroy(uint32_t slot0) {   // FUN_102E2570
        std::vector<uint32_t> e;
        const bool valid = SlotValid(slot0);

        if (!valid) {
            e.push_back(2);
        }

        if (!(valid && slot0 < M().slots.size() && M().slots[slot0] != 0)) {
            e.push_back(3);
        }

        if (HasPlayer()) {
            if (Casting()) {
                e.push_back(4);
            }

            if (IsCoAClass(PlayerClass())) {
                e.push_back(5);
            }
        }

        if (DraftSpec()) {
            e.push_back(6);
        }

        return e;
    }

    // The untarnished scroll's buy price from the client's item cache (0x67CA30 record +0x20).
    bool ScrollPrice(uint32_t& price) {
        return s_deps.itemBuyPrice && s_deps.itemBuyPrice(kMysticScroll, price);
    }

    std::vector<uint32_t> CheckPurchaseScroll() {   // FUN_102E2DF0
        std::vector<uint32_t> e;
        uint32_t price = 0;

        if (!ScrollPrice(price)) {
            e.push_back(2);
        } else {
            if (HasPlayer() && Coinage() < price) {
                e.push_back(3);
            }

            if (FreeBagSlots() == 0) {
                e.push_back(4);
            }
        }

        if (!AltarNearby()) {
            e.push_back(5);
        }

        if (HasPlayer()) {
            if (Casting()) {
                e.push_back(6);
            }

            if (IsCoAClass(PlayerClass())) {
                e.push_back(7);
            }
        }

        return e;
    }

    // ---- the enchant-info table (FUN_1009B2F0) -------------------------------------------------------------------
    void SetNumber(lua_State* L, const char* key, double v) {
        lua_pushstring(L, key);
        PushNum(L, v);
        lua_settable(L, -3);
    }

    void PushEnchantInfo(lua_State* L, Row r) {
        const uint32_t spell = U32(r, 4);
        lua_createtable(L, 0, 0);
        lua_checkstack(L, 2);
        SetNumber(L, "SpellID", static_cast<double>(spell));
        std::string name;
        const bool hasSpell = s_deps.spellName && s_deps.spellName(spell, name);
        lua_pushstring(L, "SpellName");

        if (hasSpell) {
            PushStr(L, name.c_str());
        } else {
            lua_pushnil(L);
        }

        lua_settable(L, -3);
        lua_pushstring(L, "MaxStacks");
        uint32_t stacks = 0;

        if (hasSpell && MaxStacks(spell, stacks)) {
            PushInt(L, static_cast<int32_t>(stacks));
        } else {
            lua_pushnil(L);
        }

        lua_settable(L, -3);
        lua_pushstring(L, "Quality");
        PushEnum(L, kReQualities, 9, QualityOf(r));
        lua_settable(L, -3);
        SetNumber(L, "RequiredLevel", static_cast<double>(U32(r, 0x14)));
        SetNumber(L, "ItemID", static_cast<double>(U32(r, 0x18)));
        lua_pushstring(L, "Known");
        PushBool(L, Known(spell));
        lua_settable(L, -3);
        lua_pushstring(L, "IsWorldforged");
        PushBool(L, U32(r, 0x1C) != 0);
        lua_settable(L, -3);
        lua_pushstring(L, "IsAvailableForCurrentClass");
        PushBool(L, HasPlayer() && ClassAllowed(r, PlayerClass()));
        lua_settable(L, -3);

        // ClassRequirements: the three {class, tab, AE, TE} requirements grouped by class in first-seen order, skipping
        // a "None" (1) class or tab (FUN_1009AA70 / FUN_1009DBE0 / FUN_1009DE10).
        struct TabReq {
            uint32_t tab;
            uint32_t ae;
            uint32_t te;
        };
        std::vector<std::pair<uint32_t, std::vector<TabReq>>> groups;

        for (uint32_t i = 0; i < 3; ++i) {
            const uint32_t cls = U32(r, 0x4C + i * 4);
            const uint32_t tab = U32(r, 0x58 + i * 4);

            if (cls == 1 || tab == 1) {
                continue;
            }

            const TabReq t { tab, U32(r, 0x64 + i * 4), U32(r, 0x70 + i * 4) };
            auto it = std::find_if(groups.begin(), groups.end(), [cls](const auto& g) { return g.first == cls; });

            if (it == groups.end()) {
                groups.emplace_back(cls, std::vector<TabReq> { t });
            } else {
                it->second.push_back(t);
            }
        }

        lua_pushstring(L, "ClassRequirements");
        lua_createtable(L, 0, static_cast<int>(groups.size()));
        lua_checkstack(L, 2);

        for (size_t g = 0; g < groups.size(); ++g) {
            PushNum(L, static_cast<double>(g + 1));
            lua_createtable(L, 0, 0);
            lua_checkstack(L, 2);
            lua_pushstring(L, "ClassType");
            const uint32_t cls = groups[g].first;
            PushStr(L, cls - 1 < 0x2F ? kClassTypes[cls - 1] : ("UNEXPECTED_ENUM_VALUE_" + std::to_string(cls)).c_str());
            lua_settable(L, -3);
            lua_pushstring(L, "TabTypes");
            lua_createtable(L, 0, static_cast<int>(groups[g].second.size()));
            lua_checkstack(L, 2);

            for (size_t i = 0; i < groups[g].second.size(); ++i) {
                const TabReq& t = groups[g].second[i];
                PushNum(L, static_cast<double>(i + 1));
                lua_createtable(L, 0, 0);
                lua_checkstack(L, 2);
                lua_pushstring(L, "Tab");
                PushStr(L, t.tab - 1 < 0x5F ? kTabTypes[t.tab - 1] : ("UNEXPECTED_ENUM_VALUE_" + std::to_string(t.tab)).c_str());
                lua_settable(L, -3);
                SetNumber(L, "RequiredAE", static_cast<double>(t.ae));
                SetNumber(L, "RequiredTE", static_cast<double>(t.te));
                lua_settable(L, -3);
            }

            lua_settable(L, -3);
            lua_settable(L, -3);
        }

        lua_settable(L, -3);

        // Spec: the four tag names mapped through the tag table; "NONE" (0) and unknown names drop out.
        std::vector<uint32_t> tags;

        for (uint32_t i = 0; i < 4; ++i) {
            const int t = EnumIndex(kSpecTags, 91, Str(r, 0x3C + i * 4));

            if (t > 0) {
                tags.push_back(static_cast<uint32_t>(t));
            }
        }

        lua_pushstring(L, "Spec");

        if (tags.empty()) {
            lua_pushnil(L);
        } else {
            lua_createtable(L, 0, static_cast<int>(tags.size()));
            lua_checkstack(L, 2);

            for (size_t i = 0; i < tags.size(); ++i) {
                PushNum(L, static_cast<double>(i + 1));
                PushStr(L, kSpecTags[tags[i]]);
                lua_settable(L, -3);
            }
        }

        lua_settable(L, -3);
    }

    // ---- the enchant container virtuals (vtable 0x10B56504) ------------------------------------------------------
    // [0] FUN_102EFFD0: every row the player may see.
    std::vector<Row> EnchantContainer::DoPopulate() {
        std::vector<Row> v;
        CoA::Dbc::Table& t = Dbc();

        if (!t.Loaded()) {
            return v;
        }

        const uint8_t cls = PlayerClass();

        for (uint32_t id = t.MinId(); id <= t.MaxId(); ++id) {
            Row r = t.Row(id);

            if (!r) {
                continue;
            }

            if (U32(r, 0x34) || U32(r, 0x38)) {
                if (IsStockClass(cls) && (U32(r, 0x34) & 0x5FF) == 0) {
                    continue;
                }

                if (cls == 10 && (U32(r, 0x34) & 0x200) == 0) {
                    continue;
                }
            }

            if (!HasSpell(U32(r, 4)) || !RealmAllows(r)) {
                continue;
            }

            if (!WildcardMode()) {
                v.push_back(r);
            }
        }

        return v;
    }

    // [2] FUN_102F0510
    bool EnchantContainer::ArgMatch(uint32_t f, const Row& r) {
        switch (f) {
            case 0: case 1: case 2: case 3: return true;
            case 4: return Known(U32(r, 4));
            case 5: return !Known(U32(r, 4));
            case 6: return QualityOf(r) == 2;
            case 7: return QualityOf(r) == 3;
            case 8: return QualityOf(r) == 4;
            case 9: return QualityOf(r) == 5;
            case 10: return QualityOf(r) == 6;
            case 0xB: return U32(r, 0x1C) != 0;
            case 0xC: return U32(r, 0x1C) == 0;
            case 0x16: return HasClassReq(r, 0xD, true);
            case 0x33: return HasTabReq(r, 0xD, 0x1C, false);
            default: break;
        }

        // RE_FILTER_CLASS_WARRIOR..DRUID (0x0D..0x17, HERO 0x16 above): {CA class type, its reborn type}.
        static const struct {
            uint32_t a;
            uint32_t b;
        } kClass[11] = { { 3, 0x25 }, { 10, 0x2C }, { 2, 0x24 }, { 4, 0x26 }, { 6, 0x28 }, { 0xB, 0x2D }, { 9, 0x2B }, { 5, 0x27 },
            { 7, 0x29 }, { 0, 0 }, { 8, 0x2A } };

        if (f >= 0xD && f <= 0x17) {
            return HasClassReq(r, kClass[f - 0xD].a, true) || HasClassReq(r, kClass[f - 0xD].b, true);
        }

        static const struct {
            uint32_t f;
            uint32_t cls;
            uint32_t reborn;
            uint32_t tab;
        } kSpecs[] = {
            { 0x18, 3, 0x25, 7 }, { 0x19, 3, 0x25, 6 }, { 0x1A, 3, 0x25, 5 }, { 0x1B, 10, 0x2C, 7 }, { 0x1C, 10, 0x2C, 0xE },
            { 0x1D, 10, 0x2C, 0x19 }, { 0x1E, 2, 0x24, 2 }, { 0x1F, 2, 0x24, 4 }, { 0x20, 2, 0x24, 3 }, { 0x21, 4, 0x26, 10 },
            { 0x22, 4, 0x26, 8 }, { 0x23, 4, 0x26, 9 }, { 0x24, 6, 0x28, 0x10 }, { 0x25, 6, 0x28, 0xE }, { 0x26, 6, 0x28, 0xF },
            { 0x27, 0xB, 0x2D, 0x1A }, { 0x28, 0xB, 0x2D, 0xC }, { 0x29, 0xB, 0x2D, 0x1B }, { 0x2A, 9, 0x2B, 0x17 },
            { 0x2B, 9, 0x2B, 0x18 }, { 0x2C, 9, 0x2B, 0x14 }, { 0x2D, 5, 0x27, 0xB }, { 0x2E, 5, 0x27, 0xC }, { 0x2F, 5, 0x27, 0xD },
            { 0x30, 7, 0x29, 0x12 }, { 0x31, 7, 0x29, 0x13 }, { 0x32, 7, 0x29, 0x11 }, { 0x34, 8, 0x2A, 0x14 },
            { 0x35, 8, 0x2A, 0x16 }, { 0x36, 8, 0x2A, 0x15 },
        };

        for (const auto& s : kSpecs) {
            if (s.f == f) {
                return HasTabReq(r, s.cls, s.tab, true) || HasTabReq(r, s.reborn, s.tab, true);
            }
        }

        return false;
    }

    // [4] FUN_102F0D50
    uint32_t EnchantContainer::ArgGroup(uint32_t f) {
        if (f <= 3) {
            return 1;
        }

        if (f <= 5) {
            return 2;
        }

        if (f <= 10) {
            return 3;
        }

        if (f <= 0xC) {
            return 4;
        }

        if (f <= 0x36) {
            return 5;
        }

        return 0;
    }

    // FUN_101BF110: ASCII case-insensitive substring; an empty needle always matches.
    bool ContainsCI(const char* hay, const std::string& needle) {
        if (needle.empty()) {
            return true;
        }

        const size_t n = strlen(hay);

        if (needle.size() > n) {
            return false;
        }

        auto fold = [](char c) { return static_cast<uint8_t>(c + 0xBF) <= 0x19 ? static_cast<char>(c + 0x20) : c; };

        for (size_t i = 0; i + needle.size() <= n; ++i) {
            size_t k = 0;

            while (k < needle.size() && fold(hay[i + k]) == fold(needle[k])) {
                ++k;
            }

            if (k == needle.size()) {
                return true;
            }
        }

        return false;
    }

    // [3] FUN_102F0400: the spell's name, else its description.
    bool EnchantContainer::TextMatch(const std::string&, const Row& r) {
        if (this->lastRawText.empty()) {
            return true;
        }

        const uint32_t spell = U32(r, 4);

        if (s_deps.spellTextName && ContainsCI(s_deps.spellTextName(spell).c_str(), this->lastRawText)) {
            return true;
        }

        return s_deps.spellTextDescription && ContainsCI(s_deps.spellTextDescription(spell).c_str(), this->lastRawText);
    }

    // [8] FUN_102F0DF0: ((7 - quality) << 16 | level) << 32 | id -- best quality, then lowest level first.
    std::string EnchantContainer::FilterKey(const Row& r) {
        const uint8_t q = static_cast<uint8_t>(7 - static_cast<uint8_t>(QualityOf(r)));
        std::string k;
        CoA::Filter::AppendU32(k, (static_cast<uint32_t>(q) << 16) | U32(r, 0x14));
        CoA::Filter::AppendU32(k, U32(r, 0));
        return k;
    }

    // ---- mystic scrolls ------------------------------------------------------------------------------------------
    // FUN_102E63B0: the player's mystic scrolls (the untarnished scroll, or an item flagged 0x400000).
    std::vector<Item> ScrollItems() {
        std::vector<Item> v;

        for (const Item& item : PlayerItems()) {
            const CoA::ItemAddon::Record* rec = CoA::ItemAddon::Find(item.entry);

            if (rec && (item.entry == kMysticScroll || (rec->fields[1] & 0x400000))) {
                v.push_back(item);
            }
        }

        return v;
    }

    ScrollEntry MakeEntry(const Item& item, const std::string& name, uint32_t quality) {   // FUN_102E00D0 / FUN_102DF220
        ScrollEntry e;
        e.guid = item.low;
        e.entry = item.entry;
        e.name = name;
        e.bag = item.bag;
        e.slot = item.slot;
        e.quality = quality;

        if (Row r = ByItem(e.entry)) {
            e.spell = U32(r, 4);
            e.known = Known(e.spell);
        }

        return e;
    }

    // The comparators FUN_102E08C0 / FUN_102E0A00: the untarnished scroll first, then scrolls the player can use (level
    // and class), then bag, then slot.
    bool Usable(const ScrollEntry& e) {
        Row r = ByItem(e.entry);
        return HasPlayer() && r && U32(r, 0x14) <= PlayerLevel() && ClassAllowed(r, PlayerClass());
    }

    bool ScrollLess(const ScrollEntry& a, const ScrollEntry& b) {
        const bool sa = a.entry == kMysticScroll;
        const bool sb = b.entry == kMysticScroll;

        if (sa != sb) {
            return sa;
        }

        const bool ua = Usable(a);
        const bool ub = Usable(b);

        if (ua != ub) {
            return ua;
        }

        if (a.bag != b.bag) {
            return a.bag < b.bag;
        }

        return a.slot < b.slot;
    }

    void SetInt(lua_State* L, const char* key, int32_t v) {
        lua_pushstring(L, key);
        PushInt(L, v);
        lua_settable(L, -3);
    }

    void PushScroll(lua_State* L, const ScrollEntry& e) {   // FUN_1009DA50
        lua_createtable(L, 0, 0);
        lua_checkstack(L, 2);
        SetInt(L, "Guid", static_cast<int32_t>(e.guid));
        SetInt(L, "Entry", static_cast<int32_t>(e.entry));
        lua_pushstring(L, "Name");
        PushStr(L, e.name.c_str());
        lua_settable(L, -3);
        SetInt(L, "Bag", e.bag);
        SetInt(L, "Slot", e.slot);
        SetInt(L, "Quality", static_cast<int32_t>(e.quality));
        SetInt(L, "SpellID", static_cast<int32_t>(e.spell));
        lua_pushstring(L, "Known");
        PushBool(L, e.known);
        lua_settable(L, -3);
    }

    void PushScrollList(lua_State* L, const std::vector<ScrollEntry>& v) {   // FUN_102DF980
        lua_createtable(L, 0, static_cast<int>(v.size()));
        lua_checkstack(L, 2);

        for (size_t i = 0; i < v.size(); ++i) {
            PushNum(L, static_cast<double>(i + 1));
            PushScroll(L, v[i]);
            lua_settable(L, -3);
        }
    }

    std::string LowerAscii(std::string s) {
        for (char& c : s) {
            c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
        }

        return s;
    }

    // FUN_102EF270: rebuild +0xF0 from the player's scrolls, the text and the RE_FILTER list.
    void FilterScrolls(const std::string& rawText, const std::vector<uint32_t>& filters) {
        std::vector<uint32_t> knownF;
        std::vector<uint32_t> qualityF;
        std::vector<uint32_t> worldforgedF;
        std::vector<uint32_t> classF;

        for (uint32_t f : filters) {
            if (f <= 3) {
                continue;
            }

            if (f <= 5) {
                knownF.push_back(f);
            } else if (f <= 10) {
                qualityF.push_back(f);
            } else if (f <= 12) {
                worldforgedF.push_back(f);
            } else {
                classF.push_back(f);
            }
        }

        const std::string text = LowerAscii(rawText);
        Mgr& m = M();
        m.scrolls.clear();
        bool untarnished = false;

        for (const Item& item : PlayerItems()) {
            const CoA::ItemAddon::Record* rec = CoA::ItemAddon::Find(item.entry);

            if (item.entry == kMysticScroll) {
                if (untarnished) {
                    continue;
                }

                untarnished = true;
            } else if (!((rec ? rec->fields[1] : 0) & 0x400000)) {
                continue;
            }

            std::string name = rec && rec->name ? rec->name : std::string();

            if (name.empty()) {
                name = ClientItemName(item.low);
            }

            const ScrollEntry e = MakeEntry(item, name, rec ? rec->Quality() : 0);
            Row r = ByItem(e.entry);

            if (!text.empty() && LowerAscii(e.name).find(text) == std::string::npos) {
                continue;
            }

            auto any = [](const std::vector<uint32_t>& fs, auto pred) {
                if (fs.empty()) {
                    return true;
                }

                for (uint32_t f : fs) {
                    if (pred(f)) {
                        return true;
                    }
                }

                return false;
            };

            if (!any(knownF, [&](uint32_t f) { return r && ((f == 4 && e.known) || (f == 5 && !e.known)); })) {
                continue;
            }

            if (!any(qualityF, [&](uint32_t f) { return e.quality == f - 4; })) {
                continue;
            }

            if (!any(worldforgedF, [&](uint32_t f) { return r && ((f == 0xB && U32(r, 0x1C) != 0) || (f == 0xC && U32(r, 0x1C) == 0)); })) {
                continue;
            }

            if (!any(classF, [&](uint32_t f) { return r && m.container.MatchArg(f, r); })) {
                continue;
            }

            m.scrolls.push_back(e);
        }

        std::sort(m.scrolls.begin(), m.scrolls.end(), ScrollLess);   // FUN_102DEE90
    }

    // ---- Lua argument readers ------------------------------------------------------------------------------------
    // FUN_100BED80 + FUN_100C1370: a table of {name = boolean}; the names whose value is true.
    std::vector<std::string> TrueKeys(lua_State* L, int idx) {
        std::vector<std::string> out;

        if (lua_type(L, idx) != LUA_TTABLE) {
            return out;
        }

        lua_pushnil(L);

        while (lua_next(L, idx)) {
            if (lua_type(L, -2) == LUA_TSTRING && lua_toboolean(L, -1)) {
                out.emplace_back(lua_tolstring(L, -2, nullptr));
            }

            lua_settop(L, -2);
        }

        return out;
    }

    std::vector<uint32_t> FilterIndexes(const std::vector<std::string>& names) {   // FUN_102DF6D0
        std::vector<uint32_t> v;

        for (const std::string& n : names) {
            const int i = EnumIndex(kFilters, 55, n);

            if (i >= 0) {
                v.push_back(static_cast<uint32_t>(i));
            }
        }

        return v;
    }

    // FUN_100E92D0: {number, boolean}.
    bool ReadNumberBool(lua_State* L, uint32_t& n, bool& b) {
        if (!ValidateInput(L, { LUA_TNUMBER, LUA_TBOOLEAN })) {
            return false;
        }

        n = static_cast<uint32_t>(ToInt(CheckNumber(L, 1)));
        b = lua_toboolean(L, 2) != 0;
        return true;
    }

    // ok, then nil or the first error's name.
    int PushCheck(lua_State* L, const std::vector<uint32_t>& e, const char* const* table, uint32_t count) {
        PushBool(L, e.empty());

        if (e.empty()) {
            lua_pushnil(L);
        } else {
            PushEnum(L, table, count, e[0]);
        }

        return 2;
    }

    int PushCheck(lua_State* L, uint32_t e, const char* const* table, uint32_t count) {
        return PushCheck(L, e ? std::vector<uint32_t> { e } : std::vector<uint32_t>(), table, count);
    }

    // ---- natives -------------------------------------------------------------------------------------------------
    // The item-addressed sends: false when the item is gone, else the packet and true.
    template <class F>
    int SendForItem(lua_State* L, uint32_t itemLow, F send) {
        Item item;

        if (!ItemObject(itemLow, item)) {
            PushBool(L, false);
            return 1;
        }

        uint8_t bag;
        uint8_t slot;
        WireBagSlot(item, bag, slot);
        send(bag, slot);
        PushBool(L, true);
        return 1;
    }

    int ReforgeItem(lua_State* L) {   // 0x102EE7D0
        uint32_t itemLow;

        if (!ReadNumber(L, itemLow)) {
            return 0;
        }

        return SendForItem(L, itemLow, [](uint8_t bag, uint8_t slot) { CoA::Packet(0x603).U8(bag).U8(slot).Send(); });
    }

    int CanReforgeItem(lua_State* L) {   // 0x102EBD30
        uint32_t itemLow;

        if (!ReadNumber(L, itemLow)) {
            return 0;
        }

        return PushCheck(L, CheckReforgeItem(itemLow), kReforge, 13);
    }

    int ReforgeSlot(lua_State* L) {   // 0x102EE960
        uint32_t slot;

        if (!ReadNumber(L, slot) || slot == 0) {
            return 0;
        }

        CoA::Packet(0x60E).U32(slot - 1).Send();
        PushBool(L, true);
        return 1;
    }

    int CanReforgeSlot(lua_State* L) {   // 0x102EBE30
        uint32_t slot;

        if (!ReadNumber(L, slot) || slot == 0) {
            return 0;
        }

        return PushCheck(L, CheckReforgeSlot(slot - 1), kReforge, 13);
    }

    int GetReforgeCost(lua_State* L) {   // 0x102EDA70
        PushNum(L, static_cast<double>(ReforgeCost(false)));
        PushNum(L, static_cast<double>(ReforgeCost(true)));
        return 2;
    }

    int CollectionReforgeItem(lua_State* L) {   // 0x102EC720
        int32_t itemLow;
        int32_t spell;

        if (!ReadInt2(L, itemLow, spell)) {
            return 0;
        }

        return SendForItem(L, static_cast<uint32_t>(itemLow), [spell](uint8_t bag, uint8_t slot) {
            CoA::Packet(0x605).U8(bag).U8(slot).U32(static_cast<uint32_t>(spell)).Send();
        });
    }

    // ok, nil or the first error; on NO_MONEY the four costs, else four nils.
    void PushReforgeResult(lua_State* L, const std::vector<uint32_t>& e, const ReforgeCosts& c) {
        PushCheck(L, e, kCollReforge, 30);

        if (!e.empty() && e[0] == 0xC) {
            PushInt(L, static_cast<int32_t>(c.moneyDelta));
            PushInt(L, static_cast<int32_t>(c.tokenDelta));
            PushInt(L, static_cast<int32_t>(c.money));
            PushInt(L, static_cast<int32_t>(c.tokens));
        } else {
            PushNils(L, 4);
        }
    }

    int CanCollectionReforgeItem(lua_State* L) {   // 0x102EAD40
        int32_t itemLow;
        int32_t spell;

        if (!ReadInt2(L, itemLow, spell)) {
            return 0;
        }

        ReforgeCosts c;
        const std::vector<uint32_t> e = CheckCollectionReforgeItem(static_cast<uint32_t>(itemLow), static_cast<uint32_t>(spell), c);
        PushReforgeResult(L, e, c);
        return 6;
    }

    void UpsertPending(std::vector<Pair>& v, uint32_t slot, uint32_t value) {
        for (Pair& p : v) {
            if (p.slot == slot) {
                p.value = value;
                return;
            }
        }

        v.push_back({ slot, value });
    }

    int CollectionReforgeSlot(lua_State* L) {   // 0x102EC8C0
        int32_t slot;
        int32_t spell;

        if (ReadInt2(L, slot, spell) && slot != 0) {
            UpsertPending(M().pendingReforge, static_cast<uint32_t>(slot), static_cast<uint32_t>(spell));
        }

        return 0;
    }

    int CanCollectionReforgeSlot(lua_State* L) {   // 0x102EAE70
        int32_t slot;
        int32_t spell;

        if (!ReadInt2(L, slot, spell) || slot == 0) {
            return 0;
        }

        ReforgeCosts c;
        const std::vector<uint32_t> e = CheckCollectionReforgeSlot(static_cast<uint32_t>(slot) - 1, static_cast<uint32_t>(spell), false, c);
        PushReforgeResult(L, e, c);
        return 6;
    }

    // CanCollectionReforgeAnySlot / CanApplyAnySlot: the first slot with no error wins; otherwise false and
    // {[slot] = {error names}} in the map's own iteration order.
    template <class F>
    int AnySlot(lua_State* L, F check, const char* const* table, uint32_t count) {
        std::unordered_map<uint32_t, std::vector<uint32_t>> errors;

        for (uint32_t slot0 = 0;; ++slot0) {
            errors[slot0 + 1] = check(slot0);

            if (errors[slot0 + 1].empty()) {
                PushBool(L, true);
                lua_pushnil(L);
                return 2;
            }

            if (slot0 + 1 > 0x10) {
                break;
            }
        }

        PushBool(L, false);
        lua_createtable(L, 0, 0);
        lua_checkstack(L, 2);

        for (const auto& kv : errors) {
            PushInt(L, static_cast<int32_t>(kv.first));
            PushEnumList(L, table, count, kv.second);
            lua_settable(L, -3);
        }

        return 2;
    }

    int CanCollectionReforgeAnySlot(lua_State* L) {   // 0x102EAAC0
        uint32_t spell;

        if (!ReadNumber(L, spell)) {
            return 0;
        }

        return AnySlot(
            L,
            [spell](uint32_t slot0) {
                ReforgeCosts c;
                return CheckCollectionReforgeSlot(slot0, spell, false, c);
            },
            kCollReforge, 30);
    }

    void PushSlotSpell(lua_State* L, uint32_t slot, uint32_t spell) {   // FUN_1009E100
        lua_createtable(L, 0, 0);
        lua_checkstack(L, 2);
        SetNumber(L, "Slot", static_cast<double>(slot));
        lua_pushstring(L, "Enchant");

        if (Row r = BySpell(spell)) {
            PushEnchantInfo(L, r);
        } else {
            lua_pushnil(L);
        }

        lua_settable(L, -3);
    }

    void PushSlotItem(lua_State* L, uint32_t slot, uint32_t itemLow) {   // FUN_1009E010
        lua_createtable(L, 0, 0);
        lua_checkstack(L, 2);
        SetNumber(L, "RESlot", static_cast<double>(slot));
        lua_pushstring(L, "Enchant");
        Row r = nullptr;
        Item item;

        if (ItemObject(itemLow, item)) {
            r = ByItem(item.entry);
        }

        if (r) {
            PushEnchantInfo(L, r);
        } else {
            lua_pushnil(L);
        }

        lua_settable(L, -3);
    }

    template <class F>
    int PushPairs(lua_State* L, const std::vector<Pair>& v, F push) {
        lua_createtable(L, 0, static_cast<int>(v.size()));
        lua_checkstack(L, 2);

        for (size_t i = 0; i < v.size(); ++i) {
            PushNum(L, static_cast<double>(i + 1));
            push(L, v[i].slot, v[i].value);
            lua_settable(L, -3);
        }

        return 1;
    }

    int GetCollectionReforgeChanges(lua_State* L) {   // 0x102ED1A0
        return PushPairs(L, M().pendingReforge, PushSlotSpell);
    }

    // FUN_102EFEB0 / FUN_102EFE60: drop the pending entry for slot, or the last one for -1.
    void UndoPending(std::vector<Pair>& v, uint32_t slot) {
        if (slot == 0xFFFFFFFF) {
            if (!v.empty()) {
                v.pop_back();
            }

            return;
        }

        for (auto it = v.begin(); it != v.end(); ++it) {
            if (it->slot == slot) {
                v.erase(it);
                return;
            }
        }
    }

    int UndoCollectionReforge(lua_State* L) {   // 0x102EF200
        uint32_t slot;

        if (ReadNumber(L, slot)) {
            UndoPending(M().pendingReforge, slot);
        }

        return 0;
    }

    int UndoLastCollectionReforge(lua_State*) {   // 0x102EF250
        if (!M().pendingReforge.empty()) {
            M().pendingReforge.pop_back();
        }

        return 0;
    }

    int SaveCollectionReforge(lua_State* L) {   // 0x102EEC50
        const auto& v = M().pendingReforge;

        if (v.empty()) {
            PushBool(L, false);
            return 1;
        }

        CoA::Packet p(0x60F);
        p.U32(static_cast<uint32_t>(v.size()));

        for (const Pair& e : v) {
            p.U32(e.slot - 1).U32(e.value);
        }

        p.Send();
        PushBool(L, true);
        return 0;   // the original pushes true but returns 0
    }

    // The save checks collect every entry's errors into an unordered_set and report the head of its list -- MSVC's
    // insertion keeps the first error ever inserted at the head.
    void AddUnique(std::vector<uint32_t>& all, uint32_t e) {
        if (std::find(all.begin(), all.end(), e) == all.end()) {
            all.push_back(e);
        }
    }

    int CanSaveCollectionReforge(lua_State* L) {   // 0x102EC500
        std::vector<uint32_t> all;

        for (const Pair& p : M().pendingReforge) {
            ReforgeCosts c;

            for (uint32_t e : CheckCollectionReforgeSlot(p.slot - 1, p.value, true, c)) {
                AddUnique(all, e);
            }
        }

        return PushCheck(L, all, kCollReforge, 30);
    }

    int PushCosts(lua_State* L, uint32_t spell, bool slot) {
        Row r = BySpell(spell);

        if (!r) {
            return PushNils(L, 2);
        }

        PushNum(L, static_cast<double>(CollectionReforgeCost(r, false, slot)));
        PushNum(L, static_cast<double>(CollectionReforgeCost(r, true, slot)));
        return 2;
    }

    int GetCollectionReforgeSlotCost(lua_State* L) {   // 0x102ED330
        uint32_t spell;
        return ReadNumber(L, spell) ? PushCosts(L, spell, true) : 0;
    }

    int GetCollectionReforgeItemCost(lua_State* L) {   // 0x102ED260
        uint32_t spell;
        return ReadNumber(L, spell) ? PushCosts(L, spell, false) : 0;
    }

    int GetSaveCollectionReforgeSlotCost(lua_State* L) {   // 0x102EDAF0
        uint32_t money;
        uint32_t tokens;
        Totals(PendingReforgeRows(), true, money, tokens);
        PushNum(L, static_cast<double>(money));
        PushNum(L, static_cast<double>(tokens));
        return 2;
    }

    int DisenchantItem(lua_State* L) {   // 0x102ECA60
        uint32_t itemLow;
        bool extract;

        if (!ReadNumberBool(L, itemLow, extract)) {
            return 0;
        }

        return SendForItem(L, itemLow, [extract](uint8_t bag, uint8_t slot) {
            CoA::Packet(0x607).U8(extract ? 1 : 0).U8(bag).U8(slot).Send();
        });
    }

    // The extract pre-check of CanDisenchantItem / CanDisenchantSlot: (false, reason, cost) when the extract purchase
    // itself is refused.
    bool ExtractRefused(lua_State* L) {
        const uint32_t r = CheckExtractPurchase();

        if (!r) {
            return false;
        }

        PushBool(L, false);
        PushEnum(L, kExtract, 4, r);
        PushInt(L, static_cast<int32_t>(ExtractPurchaseCost()));
        return true;
    }

    int PushDisenchant(lua_State* L, uint32_t err, bool extract) {
        if (err == 0 || (extract && err == 9)) {
            PushBool(L, true);
            lua_pushnil(L);
            return 2;
        }

        PushBool(L, false);
        PushEnum(L, kDisenchant, 16, err);
        return 2;
    }

    int CanDisenchantItem(lua_State* L) {   // 0x102EB1A0
        uint32_t itemLow;
        bool extract;

        if (!ReadNumberBool(L, itemLow, extract)) {
            return 0;
        }

        if (extract && ExtractRefused(L)) {
            return 3;
        }

        uint32_t err;
        Item item;

        if (ItemObject(itemLow, item)) {
            Row r = ByItem(item.entry);
            err = r ? CheckDisenchant(U32(r, 4)) : 3;
        } else {
            err = 2;
        }

        return PushDisenchant(L, err, extract);
    }

    int DisenchantSlot(lua_State* L) {   // 0x102ECC00
        uint32_t slot;
        bool extract;

        if (!ReadNumberBool(L, slot, extract) || slot == 0) {
            return 0;
        }

        CoA::Packet(0x608).U8(extract ? 1 : 0).U32(slot - 1).Send();
        PushBool(L, true);
        return 1;
    }

    int CanDisenchantSlot(lua_State* L) {   // 0x102EB340
        uint32_t slot;
        bool extract;

        if (!ReadNumberBool(L, slot, extract) || slot == 0) {
            return 0;
        }

        if (extract && ExtractRefused(L)) {
            return 3;
        }

        const uint32_t slot0 = slot - 1;
        uint32_t err;

        if (!SlotValid(slot0)) {
            err = 4;
        } else if (DraftSpec()) {
            err = 5;
        } else {
            err = CheckDisenchant(slot0 < M().slots.size() ? M().slots[slot0] : 0);
        }

        return PushDisenchant(L, err, extract);
    }

    int GetDisenchantCost(lua_State* L) {   // 0x102ED400
        uint32_t spell;
        bool extract;

        if (!ReadNumberBool(L, spell, extract)) {
            return 0;
        }

        Row r = BySpell(spell);

        if (!r) {
            return PushNils(L, 2);
        }

        PushNum(L, static_cast<double>(ExtractCost(r)));

        if (extract) {
            PushNum(L, static_cast<double>(ExtractPurchaseCost()));
        } else {
            lua_pushnil(L);
        }

        return 2;
    }

    int ApplyItem(lua_State* L) {   // 0x102EA420
        int32_t slot;
        int32_t itemLow;

        if (!ReadInt2(L, slot, itemLow) || slot == 0) {
            return 0;
        }

        return SendForItem(L, static_cast<uint32_t>(itemLow), [slot](uint8_t bag, uint8_t bslot) {
            CoA::Packet(0x60A).U32(static_cast<uint32_t>(slot) - 1).U8(bag).U8(bslot).Send();
        });
    }

    int CanApplyItem(lua_State* L) {   // 0x102EA8E0
        int32_t slot;
        int32_t itemLow;

        if (!ReadInt2(L, slot, itemLow) || slot == 0) {
            return 0;
        }

        return PushCheck(L, CheckApplyItem(static_cast<uint32_t>(slot) - 1, static_cast<uint32_t>(itemLow)), kApply, 26);
    }

    int ApplySlot(lua_State* L) {   // 0x102EA5B0
        int32_t slot;
        int32_t itemLow;
        Item item;

        if (ReadInt2(L, slot, itemLow) && slot != 0 && ItemObject(static_cast<uint32_t>(itemLow), item)) {
            UpsertPending(M().pendingApply, static_cast<uint32_t>(slot), static_cast<uint32_t>(itemLow));
        }

        return 0;
    }

    int CanApplySlot(lua_State* L) {   // 0x102EA9D0
        int32_t slot;
        int32_t itemLow;

        if (!ReadInt2(L, slot, itemLow) || slot == 0) {
            return 0;
        }

        return PushCheck(L, CheckApplySlot(static_cast<uint32_t>(slot) - 1, static_cast<uint32_t>(itemLow)), kApply, 26);
    }

    int CanApplyAnySlot(lua_State* L) {   // 0x102EA670
        uint32_t itemLow;

        if (!ReadNumber(L, itemLow)) {
            return 0;
        }

        return AnySlot(L, [itemLow](uint32_t slot0) { return CheckApplySlot(slot0, itemLow); }, kApply, 26);
    }

    int GetApplyChanges(lua_State* L) {   // 0x102ECFB0
        return PushPairs(L, M().pendingApply, PushSlotItem);
    }

    int UndoApply(lua_State* L) {   // 0x102EF1D0
        uint32_t slot;

        if (ReadNumber(L, slot)) {
            UndoPending(M().pendingApply, slot);
        }

        return 0;
    }

    int UndoLastApply(lua_State*) {   // 0x102EF230
        if (!M().pendingApply.empty()) {
            M().pendingApply.pop_back();
        }

        return 0;
    }

    int SaveApply(lua_State* L) {   // 0x102EEA70
        const auto& v = M().pendingApply;

        if (v.empty()) {
            PushBool(L, false);
            return 1;
        }

        CoA::Packet p(0x610);
        p.U32(static_cast<uint32_t>(v.size()));
        bool ok = true;

        for (const Pair& e : v) {
            Item item;

            if (!ItemObject(e.value, item)) {
                ok = false;
                break;
            }

            uint8_t bag;
            uint8_t slot;
            WireBagSlot(item, bag, slot);
            p.U32(e.slot - 1).U8(bag).U8(slot);
        }

        if (ok) {
            p.Send();
        }

        PushBool(L, ok);
        return 1;
    }

    int CanSaveApply(lua_State* L) {   // 0x102EBF40
        std::vector<uint32_t> all;
        // Fusion: a slot applied twice is RE_APPLY_DUPLICATE_SLOT, then every entry's errors in order.
        std::vector<uint32_t> seen;

        for (const Pair& p : M().pendingApply) {
            if (Fusion()) {
                if (std::find(seen.begin(), seen.end(), p.slot) != seen.end()) {
                    AddUnique(all, 0x18);
                }

                seen.push_back(p.slot);
            }

            for (uint32_t e : CheckApplySlot(p.slot - 1, p.value)) {
                AddUnique(all, e);
            }
        }

        return PushCheck(L, all, kApply, 26);
    }

    int GetApplyItemCost(lua_State* L) {   // 0x102ED070
        uint32_t slot;

        if (!ReadNumber(L, slot)) {
            return 0;
        }

        if (SlotValid(slot) && slot < M().slots.size() && M().slots[slot] != 0) {
            if (Row r = BySpell(M().slots[slot])) {
                PushNum(L, static_cast<double>(CollectionReforgeCost(r, false, false)));
                PushNum(L, static_cast<double>(CollectionReforgeCost(r, true, false)));
                return 2;
            }
        }

        return PushNils(L, 2);
    }

    int PurchaseMysticScroll(lua_State* L) {   // 0x102EE290
        CoA::Packet(0x611).Send();
        PushBool(L, true);
        return 1;
    }

    int CanPurchaseMysticScroll(lua_State* L) {   // 0x102EBB90
        return PushCheck(L, CheckPurchaseScroll(), kPurchase, 8);
    }

    int GetMysticScrolls(lua_State* L) {   // 0x102ED7B0
        std::vector<ScrollEntry> v;

        for (const Item& item : ScrollItems()) {
            if (item.entry == kMysticScroll &&
                std::any_of(v.begin(), v.end(), [](const ScrollEntry& e) { return e.entry == kMysticScroll; })) {
                continue;
            }

            const CoA::ItemAddon::Record* rec = CoA::ItemAddon::Find(item.entry);
            v.push_back(MakeEntry(item, ClientItemName(item.low), rec ? rec->Quality() : 0));
        }

        std::sort(v.begin(), v.end(), ScrollLess);   // FUN_102DEB40
        PushScrollList(L, v);
        return 1;
    }

    int SetMysticScrollFilter(lua_State* L) {   // 0x102EEDB0
        if (!ValidateInput(L, { LUA_TSTRING, LUA_TTABLE })) {
            return 0;
        }

        const std::string text = CheckString(L, 1);
        FilterScrolls(text, FilterIndexes(TrueKeys(L, 2)));
        return 0;
    }

    int GetNumFilteredMysticScrolls(lua_State* L) {   // 0x102ED930
        if (lua_gettop(L) != 0) {
            return 0;
        }

        PushInt(L, static_cast<int32_t>(M().scrolls.size()));
        return 1;
    }

    int GetFilteredMysticScrollAtIndex(lua_State* L) {   // 0x102ED5C0
        uint32_t i;

        if (!ReadNumber(L, i) || i == 0) {
            return 0;
        }

        if (i - 1 < M().scrolls.size()) {
            PushScroll(L, M().scrolls[i - 1]);
        } else {
            lua_pushnil(L);
        }

        return 1;
    }

    int GetFilteredMysticScrolls(lua_State* L) {   // 0x102ED650 -> FUN_102E4E80
        const int top = lua_gettop(L);

        if (top > 2) {
            return 0;
        }

        uint32_t start = 0;
        uint32_t count = 0x12;

        if (top >= 1 && lua_type(L, 1) > 0) {
            start = static_cast<uint32_t>(ToInt(CheckNumber(L, 1)));
        }

        if (top > 1 && lua_type(L, 2) > 0) {
            count = static_cast<uint32_t>(ToInt(CheckNumber(L, 2)));
        }

        std::vector<ScrollEntry> v;
        const auto& s = M().scrolls;

        if (start < s.size() && count != 0) {
            const uint32_t end = std::min<uint32_t>(start + count, static_cast<uint32_t>(s.size()));
            v.assign(s.begin() + start, s.begin() + end);
        }

        PushScrollList(L, v);
        return 1;
    }

    int GetMysticScrollCost(lua_State* L) {   // 0x102ED760
        uint32_t price;

        if (ScrollPrice(price)) {
            PushInt(L, static_cast<int32_t>(price));
        } else {
            lua_pushnil(L);
        }

        return 1;
    }

    int CanEquipItem(lua_State* L) {   // 0x102EB590
        uint32_t itemLow;

        if (!ReadNumber(L, itemLow)) {
            return 0;
        }

        uint32_t spell = 0;
        Item item;

        if (ItemObject(itemLow, item)) {
            if (Row r = ByItem(item.entry)) {
                spell = U32(r, 4);
            }
        }

        return PushCheck(L, CheckEquip(spell, 0xFFFFFFFF), kEquip, 19);
    }

    int CanEquipSlot(lua_State* L) {   // 0x102EB650
        uint32_t slot;

        if (!ReadNumber(L, slot) || slot == 0) {
            return 0;
        }

        return PushCheck(L, CheckEquip(SlotAt(slot - 1), slot - 1), kEquip, 19);
    }

    int CanEquipEnchant(lua_State* L) {   // 0x102EB520
        uint32_t spell;

        if (!ReadNumber(L, spell)) {
            return 0;
        }

        return PushCheck(L, CheckEquip(spell, 0xFFFFFFFF), kEquip, 19);
    }

    // QueryEnchants(pageSize, page, text, {RE_FILTER_x = true}) -> page of enchant infos, page count.
    int QueryEnchants(lua_State* L) {   // 0x102EE360
        if (!ValidateInput(L, { LUA_TNUMBER, LUA_TNUMBER, LUA_TSTRING, LUA_TTABLE })) {
            return 0;
        }

        const uint32_t pageSize = static_cast<uint32_t>(ToInt(CheckNumber(L, 1)));
        const uint32_t page = static_cast<uint32_t>(ToInt(CheckNumber(L, 2)));
        const std::string text = CheckString(L, 3);
        const std::vector<uint32_t> filters = FilterIndexes(TrueKeys(L, 4));

        if (!CoA::ClientState::InWorld()) {
            lua_createtable(L, 0, 0);
            lua_checkstack(L, 2);
            return 1;
        }

        EnchantContainer& c = M().container;
        std::string parsed = text;
        CoA::Filter::ParseTokens(parsed);
        c.lastRawText = parsed;
        c.ApplyFilter(text, filters, {});
        const std::vector<Row>& all = c.Filtered();
        std::vector<Row> pageRows;
        const uint32_t start = (page - 1) * pageSize;

        for (uint32_t i = start; i < start + pageSize; ++i) {
            if (i < all.size() && all[i]) {
                pageRows.push_back(all[i]);
            }
        }

        const uint32_t pages =
            static_cast<uint32_t>(ToInt(std::ceil(static_cast<double>(static_cast<uint32_t>(all.size())) / static_cast<double>(pageSize))));
        lua_createtable(L, 0, static_cast<int>(pageRows.size()));
        lua_checkstack(L, 2);

        for (size_t i = 0; i < pageRows.size(); ++i) {
            PushNum(L, static_cast<double>(i + 1));
            PushEnchantInfo(L, pageRows[i]);
            lua_settable(L, -3);
        }

        PushNum(L, static_cast<double>(pages));
        return 2;
    }

    int GetEnchantInfoBySpell(lua_State* L) {   // 0x102ED560
        uint32_t spell;

        if (!ReadNumber(L, spell) || spell == 0) {
            return 0;
        }

        if (Row r = BySpell(spell)) {
            PushEnchantInfo(L, r);
        } else {
            lua_pushnil(L);
        }

        return 1;
    }

    int GetEnchantInfoByItem(lua_State* L) {   // 0x102ED500
        uint32_t item;

        if (!ReadNumber(L, item) || item == 0) {
            return 0;
        }

        if (Row r = ByItem(item)) {
            PushEnchantInfo(L, r);
        } else {
            lua_pushnil(L);
        }

        return 1;
    }

    int GetProgress(lua_State* L) {   // 0x102ED980
        PushNum(L, ProgressPercent(M().progress, M().level));
        PushNum(L, static_cast<double>(M().level));
        return 2;
    }

    bool TokenGuid(const char* token, uint64_t& guid) {
        guid = 0;
        return s_deps.tokenGuid && s_deps.tokenGuid(token, guid);
    }

    // The unit readers of GetAppliedEnchant / CanInspect / Inspect: a player guid only.
    bool PlayerGuidOf(const char* token, uint64_t& guid) {
        if (!TokenGuid(token, guid) || guid == 0) {
            return false;
        }

        const uint32_t hi = static_cast<uint32_t>(guid >> 32);
        return !(hi >> 28 != 0 && hi >> 16 != 0);
    }

    bool IsActivePlayer(uint64_t guid) {
        return HasPlayer() && s_deps.activePlayerGuid && s_deps.activePlayerGuid() == guid;
    }

    int GetAppliedEnchant(lua_State* L) {   // 0x102ECD30
        if (!ValidateInput(L, { LUA_TSTRING, LUA_TNUMBER })) {
            return 0;
        }

        const std::string token = CheckString(L, 1);
        const uint32_t slot = static_cast<uint32_t>(ToInt(CheckNumber(L, 2)));

        if (slot == 0) {
            return 0;
        }

        uint64_t guid;

        if (!PlayerGuidOf(token.c_str(), guid)) {
            return 0;
        }

        const uint32_t slot0 = slot - 1;
        uint32_t spell = 0;

        if (IsActivePlayer(guid)) {
            spell = SlotAt(slot0);
        } else {
            auto it = M().inspect.find(GuidLow(guid));

            if (it != M().inspect.end() && slot0 < it->second.applied.size()) {
                spell = it->second.applied[slot0];
            }
        }

        PushInt(L, static_cast<int32_t>(spell));
        return 1;
    }

    std::vector<uint32_t> CheckInspect(uint64_t guid) {   // FUN_102E2C20
        uint8_t cls = 0;

        if (!s_deps.playerClassByGuid || !s_deps.playerClassByGuid(guid, cls)) {
            return { 2 };
        }

        if (IsCoAClass(cls)) {
            return { 4 };
        }

        return {};
    }

    int CanInspect(lua_State* L) {   // 0x102EB710
        if (!ValidateInput(L, { LUA_TSTRING })) {
            return 0;
        }

        uint64_t guid;

        if (!PlayerGuidOf(CheckString(L, 1), guid)) {
            return 0;
        }

        return PushCheck(L, CheckInspect(guid), kInspect, 5);
    }

    void PushList(lua_State* L, const std::vector<uint32_t>& v) {   // FUN_1009A460
        lua_createtable(L, 0, static_cast<int>(v.size()));
        lua_checkstack(L, 2);

        for (size_t i = 0; i < v.size(); ++i) {
            PushNum(L, static_cast<double>(i + 1));
            PushInt(L, static_cast<int32_t>(v[i]));
            lua_settable(L, -3);
        }
    }

    void PushInspect(lua_State* L, const InspectData& d) {   // FUN_1009BBA0
        lua_createtable(L, 0, 0);
        lua_checkstack(L, 2);
        SetNumber(L, "Progress", InspectProgress(d));
        SetInt(L, "Level", static_cast<int32_t>(d.level));
        lua_pushstring(L, "AppliedEnchants");
        PushList(L, d.applied);
        lua_settable(L, -3);
        lua_pushstring(L, "KnownEnchants");
        PushList(L, d.known);
        lua_settable(L, -3);
    }

    // Inspect(unit, refresh): a cached result (true, table), else CMSG 0x613 for a player (true, nil -- one value
    // returned), else false.
    int Inspect(lua_State* L) {   // 0x102EDF00
        if (!ValidateInput(L, { LUA_TSTRING, LUA_TBOOLEAN })) {
            return 0;
        }

        const std::string token = CheckString(L, 1);
        const bool refresh = lua_toboolean(L, 2) != 0;
        uint64_t guid;

        if (TokenGuid(token.c_str(), guid) && guid != 0) {
            if (!refresh) {
                auto it = M().inspect.find(GuidLow(guid));

                if (it != M().inspect.end()) {
                    PushBool(L, true);
                    PushInspect(L, it->second);
                    return 2;
                }
            }

            const uint32_t hi = static_cast<uint32_t>(guid >> 32);

            if (hi < 0x10000000 || (hi >> 16) == 0) {   // FUN_102EA1C0
                CoA::Packet(0x613).U64(guid).Send();
                M().inspectKey = GuidLow(guid);
                PushBool(L, true);
                lua_pushnil(L);
                return 1;
            }
        }

        PushBool(L, false);
        lua_pushnil(L);
        return 1;
    }

    int HasAnyScroll(lua_State* L) {   // 0x102EDDF0
        PushBool(L, !ScrollItems().empty());
        return 1;
    }

    int HasAnyCollected(lua_State* L) {   // 0x102EDDC0
        PushBool(L, !M().known.empty());
        return 1;
    }

    int HasAnySlotEnchanted(lua_State* L) {   // 0x102EDE60
        uint32_t n = 0;

        for (uint32_t i = 0; i < 0x11; ++i) {
            n += SlotAt(i) != 0;
        }

        PushBool(L, n != 0);
        return 1;
    }

    int HasNearbyMysticAltar(lua_State* L) {   // 0x102EDED0
        PushBool(L, AltarNearby());
        return 1;
    }

    int Destroy(lua_State* L) {   // 0x102EC950
        uint32_t slot;

        if (!ReadNumber(L, slot) || slot == 0) {
            return 0;
        }

        CoA::Packet(0x617).U32(slot - 1).Send();
        PushBool(L, true);
        return 1;
    }

    int CanDestroy(lua_State* L) {   // 0x102EAFD0
        uint32_t slot;

        if (!ReadNumber(L, slot) || slot == 0) {
            return 0;
        }

        return PushCheck(L, CheckDestroy(slot - 1), kDestroy, 7);
    }

    int PurchaseMysticExtract(lua_State* L) {   // 0x102EE1C0
        CoA::Packet(0x733).Send();
        PushBool(L, true);
        return 1;
    }

    int CanPurchaseMysticExtract(lua_State* L) {   // 0x102EBA70
        PushCheck(L, CheckExtractPurchase(), kExtract, 4);
        PushInt(L, static_cast<int32_t>(ExtractPurchaseCost()));
        return 3;
    }

    // ---- packets -------------------------------------------------------------------------------------------------
    // A count read from the packet, refused when the rest of the packet cannot hold that many dwords.
    bool ReadCount(CDataStore* msg, uint32_t& n) {
        n = CoA::Read::U32(msg);
        return !CoA::Read::Failed(msg) && n <= CoA::Read::Remaining(msg) / 4;
    }

    bool ReadList(CDataStore* msg, std::vector<uint32_t>& out) {
        uint32_t n;

        if (!ReadCount(msg, n)) {
            return false;
        }

        out.assign(n, 0);

        for (uint32_t& v : out) {
            v = CoA::Read::U32(msg);
        }

        return true;
    }

    int32_t OnUpdateKnown(void*, uint32_t, uint32_t, CDataStore* msg) {   // 0x5F9
        ReadList(msg, M().known);
        return 1;
    }

    int32_t OnAddKnown(void*, uint32_t, uint32_t, CDataStore* msg) {   // 0x5FA
        const uint32_t id = CoA::Read::U32(msg);
        const uint8_t silent = CoA::Read::U8(msg);
        M().known.push_back(id);

        if (!silent) {
            CoA::Events::Signal("MYSTIC_ENCHANT_LEARNED", "%u", id);
        }

        return 1;
    }

    int32_t OnRemoveKnown(void*, uint32_t, uint32_t, CDataStore* msg) {   // 0x5FB
        const uint32_t id = CoA::Read::U32(msg);
        auto& k = M().known;
        k.erase(std::remove(k.begin(), k.end(), id), k.end());
        CoA::Events::Signal("MYSTIC_ENCHANT_UNLEARNED", "%u", id);
        return 1;
    }

    int32_t OnUpdateData(void*, uint32_t, uint32_t, CDataStore* msg) {   // 0x5FC
        M().progress = CoA::Read::U64(msg);
        M().level = CoA::Read::U32(msg);
        return 1;
    }

    int32_t OnUpdateSlots(void*, uint32_t, uint32_t, CDataStore* msg) {   // 0x5FD
        if (ReadList(msg, M().slots)) {
            M().slotsAux.assign(M().slots.size(), 0);
        }

        return 1;
    }

    int32_t OnUpdateSlot(void*, uint32_t, uint32_t, CDataStore* msg) {   // 0x5FE
        const uint32_t slot = CoA::Read::U32(msg);

        if (slot >= M().slots.size()) {
            return 1;
        }

        const uint32_t before = M().slots[slot];
        M().slots[slot] = CoA::Read::U32(msg);

        if (before != M().slots[slot]) {
            CoA::Events::Signal("MYSTIC_ENCHANT_SLOT_UPDATE", "%u", slot + 1);
        }

        return 1;
    }

    // FUN_100CF140: the progress multipliers of auras 0x14B60 / 0x14B61 ((points + 1) / 100 + 1 each).
    void ScaleProgress(uint64_t& v) {
        if (!HasPlayer() || !s_deps.auraPoints) {
            return;
        }

        for (uint32_t aura : { 0x14B60u, 0x14B61u }) {
            int32_t points = 0;

            if (!s_deps.auraPoints(aura, points)) {
                continue;
            }

            const float mult = static_cast<float>(points + 1) / 100.0f + 1.0f;
            v = static_cast<uint64_t>(static_cast<float>(static_cast<int64_t>(v)) * mult);
        }
    }

    int32_t OnReforgeResult(void*, uint32_t, uint32_t, CDataStore* msg) {   // 0x604
        const std::string result = CoA::Read::Str(msg);
        const uint32_t spell = CoA::Read::U32(msg);

        if (Row r = BySpell(spell)) {
            uint64_t gain;

            switch (QualityIndex(Str(r, 0xC))) {
                case 2: gain = 0x3C; break;
                case 3: gain = 0x50; break;
                case 4: gain = 100; break;
                case 5: case 6: gain = 200; break;
                default: gain = 0;
            }

            ScaleProgress(gain);

            if (gain) {
                Mgr& m = M();
                m.progress += gain;
                uint32_t level = m.level;

                if (LevelProgress(level) <= m.progress) {
                    do {
                        ++level;
                    } while (LevelProgress(level) <= m.progress);

                    m.level = level;
                }
            }
        }

        CoA::Events::Signal("MYSTIC_ENCHANT_REFORGE_RESULT", "%s%u", result.c_str(), spell);
        return 1;
    }

    // 0x60B / 0x606: an RE_APPLY_* / RE_COLLECTION_REFORGE_* result; OK clears the pending list.
    void OnResult(CDataStore* msg, const char* const* table, uint32_t count, std::vector<Pair>& pending, const char* ev) {
        const std::string result = CoA::Read::Str(msg);
        const int i = EnumIndex(table, count, result);

        if (i < 0) {
            CoA::Log::Printf("Unexpected Value: %s", result.c_str());
        } else if (i == 0) {
            pending.clear();
        }

        CoA::Events::Signal(ev, "%s", result.c_str());
    }

    int32_t OnApplyResult(void*, uint32_t, uint32_t, CDataStore* msg) {   // 0x60B
        OnResult(msg, kApply, 26, M().pendingApply, "MYSTIC_ENCHANT_APPLY_RESULT");
        return 1;
    }

    int32_t OnCollectionReforgeResult(void*, uint32_t, uint32_t, CDataStore* msg) {   // 0x606
        OnResult(msg, kCollReforge, 30, M().pendingReforge, "MYSTIC_ENCHANT_COLLECTION_REFORGE_RESULT");
        return 1;
    }

    void StringEvent(CDataStore* msg, const char* ev) {
        const std::string s = CoA::Read::Str(msg);
        CoA::Events::Signal(ev, "%s", s.c_str());
    }

    int32_t OnDestroyResult(void*, uint32_t, uint32_t, CDataStore* msg) {   // 0x618
        StringEvent(msg, "MYSTIC_ENCHANT_DESTROY_RESULT");
        return 1;
    }

    int32_t OnDisenchantResult(void*, uint32_t, uint32_t, CDataStore* msg) {   // 0x609
        StringEvent(msg, "MYSTIC_ENCHANT_DISENCHANT_RESULT");
        return 1;
    }

    int32_t OnPurchaseResult(void*, uint32_t, uint32_t, CDataStore* msg) {   // 0x612
        StringEvent(msg, "MYSTIC_ENCHANT_PURCHASE_RESULT");
        return 1;
    }

    int32_t OnPurchaseExtractResult(void*, uint32_t, uint32_t, CDataStore* msg) {   // 0x734
        StringEvent(msg, "MYSTIC_ENCHANT_PURCHASE_MYSTIC_EXTRACT_RESULT");
        return 1;
    }

    int32_t OnInspectResult(void*, uint32_t, uint32_t, CDataStore* msg) {   // 0x614
        const std::string result = CoA::Read::Str(msg);
        InspectData d;
        d.progress = CoA::Read::U64(msg);
        d.level = CoA::Read::U32(msg);

        if (ReadList(msg, d.applied)) {
            ReadList(msg, d.known);
        }

        M().inspect.emplace(M().inspectKey, std::move(d));   // an existing key keeps its old result
        M().inspectKey = 0;
        CoA::Events::Signal("MYSTIC_ENCHANT_INSPECT_RESULT", "%s", result.c_str());
        return 1;
    }

    int32_t OnProgress(void*, uint32_t, uint32_t, CDataStore* msg) {   // 0x619
        M().progress = CoA::Read::U64(msg);
        M().level = CoA::Read::U32(msg);
        CoA::Events::Signal("MYSTIC_ENCHANT_PROGRESS_UPDATE", "%f%u", ProgressPercent(M().progress, M().level), M().level);
        return 1;
    }

    // 0x56B SMSG_PATCH_MYSTIC_ENCHANT (FUN_102E8990): a 0x7C-byte row, then its +0x0C / +0x10 quality names and its four
    // +0x3C..+0x48 tag names; upsert (FUN_10133AC0), the two index inserts (FUN_100BD860), reset the container for a new
    // id, MYSTIC_ENCHANT_PATCHED.
    int32_t OnPatch(void*, uint32_t, uint32_t, CDataStore* msg) {
        std::vector<uint8_t> row(0x7C);
        CoA::Read::Data(msg, row.data(), 0x7C);
        CoA::Dbc::Table& t = Dbc();
        static const uint32_t kOffsets[6] = { 0xC, 0x10, 0x3C, 0x40, 0x44, 0x48 };

        for (uint32_t off : kOffsets) {
            const uint32_t s = t.AddString(CoA::Read::Str(msg).c_str());
            memcpy(row.data() + off, &s, 4);
        }

        if (CoA::Read::Failed(msg)) {
            return 1;
        }

        uint32_t id;
        memcpy(&id, row.data(), 4);
        const bool existed = t.Row(id) != nullptr;
        t.Upsert(id, row);

        if (Row r = t.Row(id)) {
            Index& idx = Idx();
            idx.bySpell[U32(r, 4)] = r;
            idx.byItem[U32(r, 0x18)] = r;
        }

        if (!existed) {
            M().container.Reset();
        }

        CoA::Events::Signal("MYSTIC_ENCHANT_PATCHED", nullptr);
        return 1;
    }

    // ---- lifecycle -----------------------------------------------------------------------------------------------
    void OnEnterWorld() {   // 0x102EA2A0: prefetch the untarnished scroll's item record
        if (s_deps.cacheItem) {
            s_deps.cacheItem(kMysticScroll);
        }
    }

    void OnGlueScreen() {   // 0x102E32D0
        Mgr& m = M();
        m.known.clear();
        m.slots.clear();
        m.progress = 0;
        m.level = 0;
        m.pendingReforge.clear();
        m.pendingApply.clear();
        m.inspectKey = 0;
        m.inspect.clear();
        m.altarOpen = false;
        m.container.Reset();
    }

    void OnTick() {   // 0x102EFF00: once a second while the altar is open, close it when out of range
        Mgr& m = M();

        if (!m.altarOpen) {
            return;
        }

        const __time64_t now = _time64(nullptr);

        if (now - m.altarCheck < 1) {
            return;
        }

        m.altarCheck = now;

        if (AltarNearby()) {
            return;
        }

        m.altarOpen = false;
        CoA::Events::Signal("MYSTIC_ALTAR_CLOSED", nullptr);
    }

    const CoA::Binding s_bindings[] = {
        COA_NATIVE("C_MysticEnchant", "ReforgeItem", ReforgeItem),
        COA_NATIVE("C_MysticEnchant", "CanReforgeItem", CanReforgeItem),
        COA_NATIVE("C_MysticEnchant", "ReforgeSlot", ReforgeSlot),
        COA_NATIVE("C_MysticEnchant", "CanReforgeSlot", CanReforgeSlot),
        COA_NATIVE("C_MysticEnchant", "GetReforgeCost", GetReforgeCost),
        COA_NATIVE("C_MysticEnchant", "CollectionReforgeItem", CollectionReforgeItem),
        COA_NATIVE("C_MysticEnchant", "CanCollectionReforgeItem", CanCollectionReforgeItem),
        COA_NATIVE("C_MysticEnchant", "CollectionReforgeSlot", CollectionReforgeSlot),
        COA_NATIVE("C_MysticEnchant", "CanCollectionReforgeSlot", CanCollectionReforgeSlot),
        COA_NATIVE("C_MysticEnchant", "CanCollectionReforgeAnySlot", CanCollectionReforgeAnySlot),
        COA_NATIVE("C_MysticEnchant", "GetCollectionReforgeChanges", GetCollectionReforgeChanges),
        COA_NATIVE("C_MysticEnchant", "UndoCollectionReforge", UndoCollectionReforge),
        COA_NATIVE("C_MysticEnchant", "UndoLastCollectionReforge", UndoLastCollectionReforge),
        COA_NATIVE("C_MysticEnchant", "SaveCollectionReforge", SaveCollectionReforge),
        COA_NATIVE("C_MysticEnchant", "CanSaveCollectionReforge", CanSaveCollectionReforge),
        COA_NATIVE("C_MysticEnchant", "GetCollectionReforgeSlotCost", GetCollectionReforgeSlotCost),
        COA_NATIVE("C_MysticEnchant", "GetCollectionReforgeItemCost", GetCollectionReforgeItemCost),
        COA_NATIVE("C_MysticEnchant", "GetSaveCollectionReforgeSlotCost", GetSaveCollectionReforgeSlotCost),
        COA_NATIVE("C_MysticEnchant", "DisenchantItem", DisenchantItem),
        COA_NATIVE("C_MysticEnchant", "CanDisenchantItem", CanDisenchantItem),
        COA_NATIVE("C_MysticEnchant", "DisenchantSlot", DisenchantSlot),
        COA_NATIVE("C_MysticEnchant", "CanDisenchantSlot", CanDisenchantSlot),
        COA_NATIVE("C_MysticEnchant", "GetDisenchantCost", GetDisenchantCost),
        COA_NATIVE("C_MysticEnchant", "ApplyItem", ApplyItem),
        COA_NATIVE("C_MysticEnchant", "CanApplyItem", CanApplyItem),
        COA_NATIVE("C_MysticEnchant", "ApplySlot", ApplySlot),
        COA_NATIVE("C_MysticEnchant", "CanApplySlot", CanApplySlot),
        COA_NATIVE("C_MysticEnchant", "CanApplyAnySlot", CanApplyAnySlot),
        COA_NATIVE("C_MysticEnchant", "GetApplyChanges", GetApplyChanges),
        COA_NATIVE("C_MysticEnchant", "UndoApply", UndoApply),
        COA_NATIVE("C_MysticEnchant", "UndoLastApply", UndoLastApply),
        COA_NATIVE("C_MysticEnchant", "SaveApply", SaveApply),
        COA_NATIVE("C_MysticEnchant", "CanSaveApply", CanSaveApply),
        COA_NATIVE("C_MysticEnchant", "GetApplyItemCost", GetApplyItemCost),
        COA_NATIVE("C_MysticEnchant", "PurchaseMysticScroll", PurchaseMysticScroll),
        COA_NATIVE("C_MysticEnchant", "CanPurchaseMysticScroll", CanPurchaseMysticScroll),
        COA_NATIVE("C_MysticEnchant", "GetMysticScrolls", GetMysticScrolls),
        COA_NATIVE("C_MysticEnchant", "SetMysticScrollFilter", SetMysticScrollFilter),
        COA_NATIVE("C_MysticEnchant", "GetNumFilteredMysticScrolls", GetNumFilteredMysticScrolls),
        COA_NATIVE("C_MysticEnchant", "GetFilteredMysticScrollAtIndex", GetFilteredMysticScrollAtIndex),
        COA_NATIVE("C_MysticEnchant", "GetFilteredMysticScrolls", GetFilteredMysticScrolls),
        COA_NATIVE("C_MysticEnchant", "GetMysticScrollCost", GetMysticScrollCost),
        COA_NATIVE("C_MysticEnchant", "CanEquipItem", CanEquipItem),
        COA_NATIVE("C_MysticEnchant", "CanEquipSlot", CanEquipSlot),
        COA_NATIVE("C_MysticEnchant", "CanEquipEnchant", CanEquipEnchant),
        COA_NATIVE("C_MysticEnchant", "QueryEnchants", QueryEnchants),
        COA_NATIVE("C_MysticEnchant", "GetEnchantInfoBySpell", GetEnchantInfoBySpell),
        COA_NATIVE("C_MysticEnchant", "GetEnchantInfoByItem", GetEnchantInfoByItem),
        COA_NATIVE("C_MysticEnchant", "GetProgress", GetProgress),
        COA_NATIVE("C_MysticEnchant", "GetAppliedEnchant", GetAppliedEnchant),
        COA_NATIVE("C_MysticEnchant", "Inspect", Inspect),
        COA_NATIVE("C_MysticEnchant", "CanInspect", CanInspect),
        COA_NATIVE("C_MysticEnchant", "HasAnyScroll", HasAnyScroll),
        COA_NATIVE("C_MysticEnchant", "HasAnyCollected", HasAnyCollected),
        COA_NATIVE("C_MysticEnchant", "HasAnySlotEnchanted", HasAnySlotEnchanted),
        COA_NATIVE("C_MysticEnchant", "HasNearbyMysticAltar", HasNearbyMysticAltar),
        COA_NATIVE("C_MysticEnchant", "Destroy", Destroy),
        COA_NATIVE("C_MysticEnchant", "CanDestroy", CanDestroy),
        COA_NATIVE("C_MysticEnchant", "PurchaseMysticExtract", PurchaseMysticExtract),
        COA_NATIVE("C_MysticEnchant", "CanPurchaseMysticExtract", CanPurchaseMysticExtract),
    };
}

CoA::MysticEnchant::Deps& CoA::MysticEnchant::Dependencies() {
    return s_deps;
}

uint32_t CoA::MysticEnchant::SlotEnchant(uint32_t slot0) {
    return SlotAt(slot0);
}

std::vector<uint32_t> CoA::MysticEnchant::Slots() {
    return M().slots;
}

bool CoA::MysticEnchant::Slotted(uint32_t spell) {
    return std::find(M().slots.begin(), M().slots.end(), spell) != M().slots.end();
}

bool CoA::MysticEnchant::Collected(uint32_t spell) {
    return Known(spell);
}

bool CoA::MysticEnchant::Worldforged(uint32_t spell) {
    Row r = BySpell(spell);
    return r && U32(r, 0x1C) != 0;
}

void CoA::MysticEnchant::OnGameObjectUse(uint32_t entry) {   // FUN_102EA1A0 / FUN_102EA2C0
    if (!IsAltarEntry(entry)) {
        return;
    }

    M().altarOpen = true;
    CoA::Events::Signal("MYSTIC_ALTAR_USED", nullptr);
}

void CoA::MysticEnchant::OnItemUse(uint32_t entry) {
    if (entry == kMysticScroll) {
        CoA::Events::Signal("MYSTIC_SCROLL_USED", "%u", entry);   // FUN_102EA3A0
    } else if (entry == kPresetUnlockToken) {
        CoA::Events::Signal("MYSTIC_ENCHANT_UNLOCK_PRESET_USED", nullptr);   // FUN_102EA330
    }
}

void CoA::MysticEnchant::Init() {   // the tail of FUN_102E9850
    CoA::LuaReg::AddModule(s_bindings, sizeof(s_bindings) / sizeof(s_bindings[0]));
    COA_PACKET(0x5F9, OnUpdateKnown);
    COA_PACKET(0x5FA, OnAddKnown);
    COA_PACKET(0x5FB, OnRemoveKnown);
    COA_PACKET(0x5FC, OnUpdateData);
    COA_PACKET(0x5FD, OnUpdateSlots);
    COA_PACKET(0x5FE, OnUpdateSlot);
    COA_PACKET(0x604, OnReforgeResult);
    COA_PACKET(0x606, OnCollectionReforgeResult);
    COA_PACKET(0x609, OnDisenchantResult);
    COA_PACKET(0x60B, OnApplyResult);
    COA_PACKET(0x612, OnPurchaseResult);
    COA_PACKET(0x614, OnInspectResult);
    COA_PACKET(0x618, OnDestroyResult);
    COA_PACKET(0x56B, OnPatch);
    COA_PACKET(0x619, OnProgress);
    COA_PACKET(0x734, OnPurchaseExtractResult);
    CoA::Lifecycle::OnGlueScreen(OnGlueScreen);
    CoA::Lifecycle::OnAfterIdle(OnTick);
    COA_EVENT("MYSTIC_ENCHANT_LEARNED");
    COA_EVENT("MYSTIC_ENCHANT_UNLEARNED");
    COA_EVENT("MYSTIC_ENCHANT_SLOT_UPDATE");
    COA_EVENT("MYSTIC_ENCHANT_REFORGE_RESULT");
    COA_EVENT("MYSTIC_ENCHANT_APPLY_RESULT");
    COA_EVENT("MYSTIC_ENCHANT_COLLECTION_REFORGE_RESULT");
    COA_EVENT("MYSTIC_ENCHANT_DESTROY_RESULT");
    COA_EVENT("MYSTIC_ENCHANT_DISENCHANT_RESULT");
    COA_EVENT("MYSTIC_ENCHANT_PURCHASE_RESULT");
    COA_EVENT("MYSTIC_ENCHANT_PURCHASE_MYSTIC_EXTRACT_RESULT");
    COA_EVENT("MYSTIC_ENCHANT_INSPECT_RESULT");
    COA_EVENT("MYSTIC_ENCHANT_PROGRESS_UPDATE");
    COA_EVENT("MYSTIC_ENCHANT_PATCHED");
    COA_EVENT("MYSTIC_ALTAR_USED");
    COA_EVENT("MYSTIC_ALTAR_CLOSED");
    COA_EVENT("MYSTIC_SCROLL_USED");
    COA_EVENT("MYSTIC_ENCHANT_UNLOCK_PRESET_USED");
}

void CoA::MysticEnchant::Reset() {
    s_index = Index();
    OnGlueScreen();
    M().scrolls.clear();
    M().altarCheck = 0;
    s_deps = Deps();
}

void CoA::MysticEnchant::EnterWorld() {
    OnEnterWorld();
}

bool CoA::MysticEnchant::IsAltar(uint32_t entry) {
    return IsAltarEntry(entry);
}

bool CoA::MysticEnchant::PlayerCasting() {
    return Casting();
}

bool CoA::MysticEnchant::PlayerIsCoAClass() {
    return HasPlayer() && IsCoAClass(PlayerClass());
}

uint32_t CoA::MysticEnchant::BagItemCount(uint32_t entry) {   // FUN_10308470
    uint32_t n = 0;

    for (const Item& item : PlayerItems()) {
        if (item.slot != 0 && item.entry == entry) {
            n += item.count;
        }
    }

    return n;
}
