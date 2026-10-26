// AscRecoveryService.cpp (see RecoveryService.h): the six recovery categories, their packets and the C_RecoveryService
// natives, checked against the genuine DLL (0x10306A10 recover, FUN_103060b0 result).
#include "CoA/Source/Modules/RecoveryService.h"
#include "CoA/Source/Modules/BridgeStore.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Modules/Token.h"
#include "CoA/Source/Runtime/Events.h"
#include "CoA/Source/Runtime/FilterContainer.h"
#include "CoA/Source/Runtime/Log.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Packet.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Source/Runtime/Read.h"
#include "CoA/Source/Runtime/Script.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using CoA::Script::CheckNumber;
using CoA::Script::CheckString;
using CoA::Script::PushBool;
using CoA::Script::PushStr;
using CoA::Script::ToInt;
using CoA::Script::ValidateInput;

namespace {
    const char* const kCategories[8] = { "RECOVERY_SERVICE_CATEGORY_NONE",
        "RECOVERY_SERVICE_CATEGORY_DISENCHANTED_ITEM", "RECOVERY_SERVICE_CATEGORY_DELETED_CHARACTER",
        "RECOVERY_SERVICE_CATEGORY_VENDORED_ITEM", "RECOVERY_SERVICE_CATEGORY_DELETED_ITEM",
        "RECOVERY_SERVICE_CATEGORY_MYTHIC_RECYCLING", "RECOVERY_SERVICE_CATEGORY_WORLDFORGED_ITEM",
        "RECOVERY_SERVICE_CATEGORY_MAX" };   // 0x10B5A990
    const uint32_t kQueryOpcode[7] = { 0, 0x5D2, 0x5DA, 0x5DE, 0x5E2, 0x5E6, 0x682 };
    const uint32_t kRecoverOpcode[7] = { 0, 0x5D4, 0x5DC, 0x5E0, 0x5E4, 0x5E8, 0x684 };
    const char* const kRecoveryOk[7] = { nullptr, "DISENCHANTED_ITEM_RECOVERY_OK", "DELETED_CHARACTER_RECOVERY_OK",
        "VENDORED_ITEM_RECOVERY_OK", "DELETED_ITEM_RECOVERY_OK", "MYTHIC_RECYCLING_RECOVERY_OK",
        "WORLDFORGED_ITEM_RECOVERY_OK" };

    enum class Kind { Character, Item, Mythic, Worldforged };

    struct Record {
        Kind kind = Kind::Item;
        std::string id;                                          // +0x08
        bool recovered = false;                                  // +0x20
        uint64_t date = 0;                                       // +0x28
        uint64_t cost = 0;                                       // +0x30
        std::vector<std::pair<uint32_t, uint32_t>> itemCosts;    // +0x38 unordered_map, insertion order
        std::vector<std::pair<uint32_t, uint32_t>> tokenCosts;   // +0x58 token index -> first value
        std::string name, race, sex, cls;                        // character
        uint32_t level = 0;
        uint32_t itemEntry = 0, itemCount = 0, refundedItem = 0; // item / worldforged
        std::string extraStr;
        uint32_t extraU32 = 0;
    };

    CoA::RecoveryService::Deps s_deps;

    std::string Lower(std::string s) {
        for (char& c : s) {
            c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
        }

        return s;
    }

    // FUN_102ffa20: drop a leading prefix, '_' -> ' ', lowercase.
    std::string Normalize(std::string s, const char* prefix) {
        const size_t n = strlen(prefix);

        if (n && s.size() >= n && s.compare(0, n, prefix) == 0) {
            s.erase(0, n);
        }

        std::replace(s.begin(), s.end(), '_', ' ');
        return Lower(s);
    }

    bool Has(const std::string& field, const std::string& search) {
        return field.find(search) != std::string::npos;
    }

    const char* SubclassToken(uint32_t cls, uint32_t sub) {
        static const char* const k0[9] = { "ITEM_SUBCLASS_CONSUMABLE", "ITEM_SUBCLASS_POTION", "ITEM_SUBCLASS_ELIXIR",
            "ITEM_SUBCLASS_FLASK", "ITEM_SUBCLASS_SCROLL", "ITEM_SUBCLASS_FOOD", "ITEM_SUBCLASS_ITEM_ENHANCEMENT",
            "ITEM_SUBCLASS_BANDAGE", "ITEM_SUBCLASS_CONSUMABLE_OTHER" };
        static const char* const k1[9] = { "ITEM_SUBCLASS_CONTAINER", "ITEM_SUBCLASS_SOUL_CONTAINER",
            "ITEM_SUBCLASS_HERB_CONTAINER", "ITEM_SUBCLASS_ENCHANTING_CONTAINER", "ITEM_SUBCLASS_ENGINEERING_CONTAINER",
            "ITEM_SUBCLASS_GEM_CONTAINER", "ITEM_SUBCLASS_MINING_CONTAINER", "ITEM_SUBCLASS_LEATHERWORKING_CONTAINER",
            "ITEM_SUBCLASS_INSCRIPTION_CONTAINER" };
        static const char* const k2[22] = { "ITEM_SUBCLASS_WEAPON_AXE", "ITEM_SUBCLASS_WEAPON_AXE2",
            "ITEM_SUBCLASS_WEAPON_BOW", "ITEM_SUBCLASS_WEAPON_GUN", "ITEM_SUBCLASS_WEAPON_MACE",
            "ITEM_SUBCLASS_WEAPON_MACE2", "ITEM_SUBCLASS_WEAPON_POLEARM", "ITEM_SUBCLASS_WEAPON_SWORD",
            "ITEM_SUBCLASS_WEAPON_SWORD2", "ITEM_SUBCLASS_WEAPON_obsolete", "ITEM_SUBCLASS_WEAPON_STAFF",
            "ITEM_SUBCLASS_WEAPON_EXOTIC", "ITEM_SUBCLASS_WEAPON_EXOTIC2", "ITEM_SUBCLASS_WEAPON_FIST",
            "ITEM_SUBCLASS_WEAPON_MISC", "ITEM_SUBCLASS_WEAPON_DAGGER", "ITEM_SUBCLASS_WEAPON_THROWN",
            "ITEM_SUBCLASS_WEAPON_SPEAR", "ITEM_SUBCLASS_WEAPON_CROSSBOW", "ITEM_SUBCLASS_WEAPON_WAND",
            "ITEM_SUBCLASS_WEAPON_FISHING_POLE", "ITEM_SUBCLASS_WEAPON_MAX" };
        static const char* const k3[9] = { "ITEM_SUBCLASS_GEM_RED", "ITEM_SUBCLASS_GEM_BLUE", "ITEM_SUBCLASS_GEM_YELLOW",
            "ITEM_SUBCLASS_GEM_PURPLE", "ITEM_SUBCLASS_GEM_GREEN", "ITEM_SUBCLASS_GEM_ORANGE", "ITEM_SUBCLASS_GEM_META",
            "ITEM_SUBCLASS_GEM_SIMPLE", "ITEM_SUBCLASS_GEM_PRISMATIC" };
        static const char* const k4[12] = { "ITEM_SUBCLASS_ARMOR_MISC", "ITEM_SUBCLASS_ARMOR_CLOTH",
            "ITEM_SUBCLASS_ARMOR_LEATHER", "ITEM_SUBCLASS_ARMOR_MAIL", "ITEM_SUBCLASS_ARMOR_PLATE",
            "ITEM_SUBCLASS_ARMOR_BUCKLER", "ITEM_SUBCLASS_ARMOR_SHIELD", "ITEM_SUBCLASS_ARMOR_LIBRAM",
            "ITEM_SUBCLASS_ARMOR_IDOL", "ITEM_SUBCLASS_ARMOR_TOTEM", "ITEM_SUBCLASS_ARMOR_SIGIL",
            "ITEM_SUBCLASS_ARMOR_MAX" };
        static const char* const k6[5] = { "ITEM_SUBCLASS_WAND", "ITEM_SUBCLASS_BOLT", "ITEM_SUBCLASS_ARROW",
            "ITEM_SUBCLASS_BULLET", "ITEM_SUBCLASS_THROWN" };
        static const char* const k7[16] = { "ITEM_SUBCLASS_TRADE_GOODS", "ITEM_SUBCLASS_PARTS",
            "ITEM_SUBCLASS_EXPLOSIVES", "ITEM_SUBCLASS_DEVICES", "ITEM_SUBCLASS_JEWELCRAFTING", "ITEM_SUBCLASS_CLOTH",
            "ITEM_SUBCLASS_LEATHER", "ITEM_SUBCLASS_METAL_STONE", "ITEM_SUBCLASS_MEAT", "ITEM_SUBCLASS_HERB",
            "ITEM_SUBCLASS_ELEMENTAL", "ITEM_SUBCLASS_TRADE_GOODS_OTHER", "ITEM_SUBCLASS_ENCHANTING",
            "ITEM_SUBCLASS_MATERIAL", "ITEM_SUBCLASS_ARMOR_ENCHANTMENT", "ITEM_SUBCLASS_WEAPON_ENCHANTMENT" };
        static const char* const k9[12] = { "ITEM_SUBCLASS_BOOK", "ITEM_SUBCLASS_LEATHERWORKING_PATTERN",
            "ITEM_SUBCLASS_TAILORING_PATTERN", "ITEM_SUBCLASS_ENGINEERING_SCHEMATIC", "ITEM_SUBCLASS_BLACKSMITHING",
            "ITEM_SUBCLASS_COOKING_RECIPE", "ITEM_SUBCLASS_ALCHEMY_RECIPE", "ITEM_SUBCLASS_FIRST_AID_MANUAL",
            "ITEM_SUBCLASS_ENCHANTING_FORMULA", "ITEM_SUBCLASS_FISHING_MANUAL", "ITEM_SUBCLASS_JEWELCRAFTING_RECIPE",
            "ITEM_SUBCLASS_WOODCUTTING_RECEIPE" };
        static const char* const k11[4] = { "ITEM_SUBCLASS_QUIVER0", "ITEM_SUBCLASS_QUIVER1", "ITEM_SUBCLASS_QUIVER",
            "ITEM_SUBCLASS_AMMO_POUCH" };
        static const char* const k13[2] = { "ITEM_SUBCLASS_KEY", "ITEM_SUBCLASS_LOCKPICK" };
        static const char* const k15[6] = { "ITEM_SUBCLASS_JUNK", "ITEM_SUBCLASS_JUNK_REAGENT", "ITEM_SUBCLASS_JUNK_PET",
            "ITEM_SUBCLASS_JUNK_HOLIDAY", "ITEM_SUBCLASS_JUNK_OTHER", "ITEM_SUBCLASS_JUNK_MOUNT" };
        static const uint32_t kGlyphIds[10] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 11 };
        static const char* const kGlyph[10] = { "ITEM_SUBCLASS_GLYPH_WARRIOR", "ITEM_SUBCLASS_GLYPH_PALADIN",
            "ITEM_SUBCLASS_GLYPH_HUNTER", "ITEM_SUBCLASS_GLYPH_ROGUE", "ITEM_SUBCLASS_GLYPH_PRIEST",
            "ITEM_SUBCLASS_GLYPH_DEATH_KNIGHT", "ITEM_SUBCLASS_GLYPH_SHAMAN", "ITEM_SUBCLASS_GLYPH_MAGE",
            "ITEM_SUBCLASS_GLYPH_WARLOCK", "ITEM_SUBCLASS_GLYPH_DRUID" };

        switch (cls) {
        case 0: return sub < 9 ? k0[sub] : "";
        case 1: return sub < 9 ? k1[sub] : "";
        case 2: return sub < 22 ? k2[sub] : "";
        case 3: return sub < 9 ? k3[sub] : "";
        case 4: return sub < 12 ? k4[sub] : "";
        case 5: return sub == 0 ? "ITEM_SUBCLASS_REAGENT" : "";
        case 6: return sub < 5 ? k6[sub] : "";
        case 7: return sub < 16 ? k7[sub] : "";
        case 8: return sub == 0 ? "ITEM_SUBCLASS_GENERIC" : "";
        case 9: return sub < 12 ? k9[sub] : "";
        case 10: return sub == 0 ? "ITEM_SUBCLASS_MONEY" : "";
        case 11: return sub < 4 ? k11[sub] : "";
        case 12: return sub == 0 ? "ITEM_SUBCLASS_QUEST" : "";
        case 13: return sub < 2 ? k13[sub] : "";
        case 14: return sub == 0 ? "ITEM_SUBCLASS_PERMANENT" : "";
        case 15: return sub < 6 ? k15[sub] : "";
        case 16:
            for (uint32_t i = 0; i < 10; ++i) {
                if (kGlyphIds[i] == sub) {
                    return kGlyph[i];
                }
            }

            return "";
        default: return "ITEM_SUBCLASS_NONE";
        }
    }

    // FUN_103004e0: name, description, quality, class, subclass or inventory type contains the search. No Item.dbc row
    // or no ItemAddon record: never a match.
    bool ItemMatches(const std::string& search, uint32_t entry) {
        static const char* const kQuality[9] = { "ITEM_QUALITY_POOR", "ITEM_QUALITY_NORMAL", "ITEM_QUALITY_UNCOMMON",
            "ITEM_QUALITY_RARE", "ITEM_QUALITY_EPIC", "ITEM_QUALITY_LEGENDARY", "ITEM_QUALITY_ARTIFACT",
            "ITEM_QUALITY_HEIRLOOM", "MAX_ITEM_QUALITY" };
        static const char* const kClass[17] = { "ITEM_CLASS_CONSUMABLE", "ITEM_CLASS_CONTAINER", "ITEM_CLASS_WEAPON",
            "ITEM_CLASS_GEM", "ITEM_CLASS_ARMOR", "ITEM_CLASS_REAGENT", "ITEM_CLASS_PROJECTILE",
            "ITEM_CLASS_TRADE_GOODS", "ITEM_CLASS_GENERIC", "ITEM_CLASS_RECIPE", "ITEM_CLASS_MONEY",
            "ITEM_CLASS_QUIVER", "ITEM_CLASS_QUEST", "ITEM_CLASS_KEY", "ITEM_CLASS_PERMANENT", "ITEM_CLASS_MISC",
            "ITEM_CLASS_GLYPH" };
        static const char* const kInvType[29] = { "INVTYPE_NON_EQUIP", "INVTYPE_HEAD", "INVTYPE_NECK",
            "INVTYPE_SHOULDERS", "INVTYPE_BODY", "INVTYPE_CHEST", "INVTYPE_WAIST", "INVTYPE_LEGS", "INVTYPE_FEET",
            "INVTYPE_WRISTS", "INVTYPE_HANDS", "INVTYPE_FINGER", "INVTYPE_TRINKET", "INVTYPE_WEAPON",
            "INVTYPE_SHIELD", "INVTYPE_RANGED", "INVTYPE_CLOAK", "INVTYPE_2HWEAPON", "INVTYPE_BAG", "INVTYPE_TABARD",
            "INVTYPE_ROBE", "INVTYPE_WEAPONMAINHAND", "INVTYPE_WEAPONOFFHAND", "INVTYPE_HOLDABLE", "INVTYPE_AMMO",
            "INVTYPE_THROWN", "INVTYPE_RANGEDRIGHT", "INVTYPE_QUIVER", "INVTYPE_RELIC" };

        if (entry == 0) {
            return false;
        }

        CoA::RecoveryService::ItemRow row;

        if (!s_deps.itemRow || !s_deps.itemRow(entry, row)) {
            return false;
        }

        const CoA::ItemAddon::Record* q = CoA::ItemAddon::Find(entry);

        if (!q) {
            return false;
        }

        if (search.empty()) {
            return true;
        }

        const std::string s = Normalize(search, "");
        const uint32_t quality = q->Quality();

        if (Has(Normalize(q->name ? q->name : "", ""), s) || Has(Normalize(q->description ? q->description : "", ""), s)) {
            return true;
        }

        if (quality < 9 && Has(Normalize(kQuality[quality], "ITEM_QUALITY_"), s)) {
            return true;
        }

        if (row.classId < 17 && Has(Normalize(kClass[row.classId], "ITEM_CLASS_"), s)) {
            return true;
        }

        if (Has(Normalize(SubclassToken(row.classId, row.subclassId), "ITEM_SUBCLASS_"), s)) {
            return true;
        }

        return row.inventoryType < 29 && Has(Normalize(kInvType[row.inventoryType], "INVTYPE_"), s);
    }

    // FUN_102ffbf0: name, race name, sex, class name or level contains the search.
    bool CharacterMatches(const std::string& search, const Record& r) {
        static const char* const kRaces[11] = { "RACE_NONE", "RACE_HUMAN", "RACE_ORC", "RACE_DWARF", "RACE_NIGHTELF",
            "RACE_UNDEAD_PLAYER", "RACE_TAUREN", "RACE_GNOME", "RACE_TROLL", "RACE_BLOODELF", "RACE_DRAENEI" };
        static const uint32_t kRaceIds[11] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 10, 11 };
        static const char* const kClasses[33] = { "CLASS_NONE", "CLASS_WARRIOR", "CLASS_PALADIN", "CLASS_HUNTER",
            "CLASS_ROGUE", "CLASS_PRIEST", "CLASS_DEATH_KNIGHT", "CLASS_SHAMAN", "CLASS_MAGE", "CLASS_WARLOCK",
            "CLASS_HERO", "CLASS_DRUID", "CLASS_BARBARIAN", "CLASS_WITCH_DOCTOR", "CLASS_DEMON_HUNTER",
            "CLASS_WITCH_HUNTER", "CLASS_STORMBRINGER", "CLASS_KNIGHT_OF_XOROTH", "CLASS_GUARDIAN", "CLASS_MONK",
            "CLASS_SON_OF_ARUGAL", "CLASS_RANGER", "CLASS_CHRONOMANCER", "CLASS_NECROMANCER", "CLASS_PYROMANCER",
            "CLASS_CULTIST", "CLASS_STARCALLER", "CLASS_SUN_CLERIC", "CLASS_TINKER", "CLASS_VENOMANCER",
            "CLASS_REAPER", "CLASS_PRIMALIST", "CLASS_RUNEMASTER" };

        if (search.empty()) {
            return true;
        }

        const std::string name = Lower(r.name);
        std::string race = "none";
        uint32_t raceId = 0;

        for (uint32_t i = 0; i < 11; ++i) {
            if (r.race == kRaces[i]) {
                raceId = kRaceIds[i];
            }
        }

        if (const char* n = s_deps.raceName ? s_deps.raceName(raceId) : nullptr) {
            race = Lower(n);
        }

        std::string sex = r.sex;
        const size_t g = sex.find("GENDER_");

        if (g != std::string::npos) {
            sex.erase(g, 7);
        }

        std::replace(sex.begin(), sex.end(), '_', ' ');
        sex = Lower(sex);
        std::string cls = "none";
        uint32_t classId = 0;

        for (uint32_t i = 0; i < 33; ++i) {
            if (r.cls == kClasses[i]) {
                classId = i;
            }
        }

        if (const char* n = s_deps.className ? s_deps.className(classId) : nullptr) {
            cls = Lower(n);
        }

        const std::string level = std::to_string(r.level);
        const std::string s = Lower(search);
        return Has(name, s) || Has(race, s) || Has(sex, s) || Has(cls, s) || Has(level, s);
    }

    // One category's FilterableContainerBase: populated from the last query response, in wire order.
    class CategoryContainer : public CoA::Filter::Container<Record*> {
    public:
        std::vector<std::unique_ptr<Record>> records;   // what the response loaded (the container's +4 list)

    protected:
        std::vector<Record*> DoPopulate() override {
            std::vector<Record*> out;

            for (const auto& r : this->records) {
                out.push_back(r.get());
            }

            return out;
        }

        // FUN_10307270: a recovered record never shows; otherwise the type's own search.
        bool TextMatch(const std::string& lower, Record* const& r) override {
            if (r->recovered) {
                return false;
            }

            return r->kind == Kind::Character ? CharacterMatches(lower, *r) : ItemMatches(lower, r->itemEntry);
        }
    };

    CategoryContainer s_containers[7];
    uint32_t s_pendingCategory = 0;            // +0x2E8
    uint32_t s_pendingPosition = 0xFFFFFFFF;   // +0x2EC

    // FUN_102fe640: the category's index, 0 for NONE or an unknown name.
    uint32_t CategoryIndex(const std::string& s) {
        for (uint32_t i = 0; i < 8; ++i) {
            if (s == kCategories[i]) {
                return i;
            }
        }

        CoA::Log::Printf("RecoveryService: Unexpected Value: %s", s.c_str());
        return 0;
    }

    void MapSet(std::vector<std::pair<uint32_t, uint32_t>>& m, uint32_t key, uint32_t value) {
        for (auto& kv : m) {
            if (kv.first == key) {
                kv.second = value;
                return;
            }
        }

        m.emplace_back(key, value);
    }

    // The tail every non-worldforged record shares: u8 recovered, u64 date, u64 cost, then the two cost maps (entries
    // with a zero on the wrong side are dropped).
    void ReadCommonTail(CDataStore* msg, Record& r) {
        r.recovered = CoA::Read::U8(msg) != 0;
        r.date = CoA::Read::U64(msg);
        r.cost = CoA::Read::U64(msg);

        for (uint32_t n = CoA::Read::U32(msg); n && CoA::Read::Remaining(msg) != 0; --n) {
            const uint32_t item = CoA::Read::U32(msg);
            const uint32_t count = CoA::Read::U32(msg);

            if (item != 0 && count != 0) {
                MapSet(r.itemCosts, item, count);
            }
        }

        for (uint32_t n = CoA::Read::U32(msg); n && CoA::Read::Remaining(msg) != 0; --n) {
            const std::string token = CoA::Read::Str(msg);
            const uint32_t a = CoA::Read::U32(msg);
            const uint32_t b = CoA::Read::U32(msg);
            uint32_t index = 0;

            if (CoA::Token::TypeIndex(token.c_str(), index) && (a != 0 || b != 0)) {
                MapSet(r.tokenCosts, index, a);
            }
        }
    }

    std::unique_ptr<Record> ReadItem(CDataStore* msg, Kind kind) {   // FUN_10301a70
        auto r = std::make_unique<Record>();
        r->kind = kind;
        r->id = CoA::Read::Str(msg);
        r->itemEntry = CoA::Read::U32(msg);
        r->itemCount = CoA::Read::U32(msg);
        r->extraStr = CoA::Read::Str(msg);
        r->extraU32 = CoA::Read::U32(msg);
        ReadCommonTail(msg, *r);
        return r;
    }

    std::unique_ptr<Record> ReadCharacter(CDataStore* msg) {   // FUN_10300ff0
        auto r = std::make_unique<Record>();
        r->kind = Kind::Character;
        r->id = CoA::Read::Str(msg);
        r->name = CoA::Read::Str(msg);
        r->race = CoA::Read::Str(msg);
        r->sex = CoA::Read::Str(msg);
        r->cls = CoA::Read::Str(msg);
        r->level = CoA::Read::U32(msg);
        ReadCommonTail(msg, *r);
        return r;
    }

    std::unique_ptr<Record> ReadWorldforged(CDataStore* msg) {   // FUN_10302200
        auto r = std::make_unique<Record>();
        r->kind = Kind::Worldforged;
        r->id = CoA::Read::Str(msg);
        r->itemEntry = CoA::Read::U32(msg);      // RequiredItemId
        r->refundedItem = CoA::Read::U32(msg);   // RefundedItemId
        r->itemCount = CoA::Read::U32(msg);
        r->extraStr = CoA::Read::Str(msg);
        r->extraU32 = CoA::Read::U32(msg);
        r->recovered = CoA::Read::U8(msg) != 0;
        r->date = CoA::Read::U64(msg);

        if (const uint32_t n = CoA::Read::U32(msg)) {
            MapSet(r->itemCosts, 0x5B9D2, n);
        }

        return r;
    }

    // ---- Lua ----------------------------------------------------------------------------------------------------
    void SetStr(lua_State* L, const char* k, const std::string& v) {
        lua_pushstring(L, k);
        PushStr(L, v.c_str());
        lua_settable(L, -3);
    }

    void SetInt(lua_State* L, const char* k, uint32_t v) {
        lua_pushstring(L, k);
        lua_pushinteger(L, static_cast<int32_t>(v));
        lua_settable(L, -3);
    }

    void SetNum(lua_State* L, const char* k, int32_t v) {
        lua_pushstring(L, k);
        lua_pushnumber(L, static_cast<double>(v));
        lua_settable(L, -3);
    }

    void PushItemCosts(lua_State* L, const Record& r) {   // FUN_102222e0
        lua_createtable(L, 0, static_cast<int>(r.itemCosts.size()));
        lua_checkstack(L, 2);

        for (const auto& kv : r.itemCosts) {
            lua_pushinteger(L, static_cast<int32_t>(kv.first));
            lua_pushinteger(L, static_cast<int32_t>(kv.second));
            lua_settable(L, -3);
        }
    }

    void PushTokenCosts(lua_State* L, const Record& r) {   // FUN_102fe830
        lua_createtable(L, 0, static_cast<int>(r.tokenCosts.size()));
        lua_checkstack(L, 2);

        for (const auto& kv : r.tokenCosts) {
            const char* name = CoA::Token::TypeName(kv.first);
            PushStr(L, name ? name : ("UNEXPECTED_ENUM_VALUE_" + std::to_string(kv.first)).c_str());
            lua_pushinteger(L, static_cast<int32_t>(kv.second));
            lua_settable(L, -3);
        }
    }

    // Money when positive, else the item costs, else the token costs, else nil.
    void SetRecoveryCost(lua_State* L, const char* k, const Record& r) {
        lua_pushstring(L, k);

        if (static_cast<int64_t>(r.cost) > 0) {
            lua_pushinteger(L, static_cast<int32_t>(static_cast<uint32_t>(r.cost)));
        } else if (!r.itemCosts.empty()) {
            PushItemCosts(L, r);
        } else if (!r.tokenCosts.empty()) {
            PushTokenCosts(L, r);
        } else {
            lua_pushnil(L);
        }

        lua_settable(L, -3);
    }

    void PushRecord(lua_State* L, const Record& r) {   // the record's vtable +4
        lua_createtable(L, 0, 0);
        lua_checkstack(L, 2);
        SetStr(L, "Id", r.id);

        switch (r.kind) {
        case Kind::Character:   // FUN_10305b20
            SetStr(L, "Name", r.name);
            SetStr(L, "Race", r.race);
            SetStr(L, "Sex", r.sex);
            SetStr(L, "Class", r.cls);
            SetNum(L, "Level", static_cast<int32_t>(r.level));
            SetRecoveryCost(L, "RecoveryCost", r);
            break;
        case Kind::Item:        // FUN_10305d00
        case Kind::Mythic:      // FUN_10305e30
            SetInt(L, "ItemEntry", r.itemEntry);
            SetInt(L, "ItemCount", r.itemCount);
            SetRecoveryCost(L, r.kind == Kind::Mythic ? "RecoveryCosts" : "RecoveryCost", r);
            break;
        case Kind::Worldforged: // FUN_10305f60
            SetInt(L, "RequiredItemId", r.itemEntry);
            SetInt(L, "RefundedItemId", r.refundedItem);
            SetInt(L, "ItemCount", r.itemCount);
            lua_pushstring(L, "RecoveryCosts");
            lua_createtable(L, 0, 0);
            lua_checkstack(L, 2);
            lua_pushinteger(L, static_cast<int32_t>(r.itemEntry));
            lua_pushinteger(L, static_cast<int32_t>(r.itemCount));
            lua_settable(L, -3);
            lua_settable(L, -3);
            lua_pushstring(L, "AdditionalRefund");
            PushItemCosts(L, r);
            lua_settable(L, -3);
            break;
        }

        SetNum(L, "Date", static_cast<int32_t>(static_cast<uint32_t>(r.date)));
    }

    // FUN_10306f80 -> FUN_102499c0(search, {}, {}): an empty category leaves the view alone.
    void ApplyFilter(uint32_t category, const std::string& search) {
        if (category >= 1 && category <= 6) {
            s_containers[category].ApplyFilter(search, {}, {});
        }
    }

    // ---- natives ------------------------------------------------------------------------------------------------
    int QueryCategory(lua_State* L) {   // 0x10306880
        std::string name;

        if (!CoA::Script::ReadString(L, name)) {
            return 0;
        }

        const uint32_t c = CategoryIndex(name);

        if (c == 0) {
            return 0;
        }

        if (c > 6) {
            PushBool(L, false);
            return 1;
        }

        CoA::Packet(kQueryOpcode[c]).Send();
        PushBool(L, true);
        return 1;
    }

    int GetNumItemsInCategory(lua_State* L) {   // 0x10306700
        std::string name;

        if (!CoA::Script::ReadString(L, name)) {
            return 0;
        }

        const uint32_t c = CategoryIndex(name);

        if (c == 0) {
            return 0;
        }

        lua_pushinteger(L, c <= 6 ? static_cast<int32_t>(s_containers[c].Size()) : 0);
        return 1;
    }

    int GetCategoryItemAtIndex(lua_State* L) {   // 0x103064b0
        if (!ValidateInput(L, { LUA_TSTRING, LUA_TNUMBER })) {
            return 0;
        }

        const std::string name = CheckString(L, 1);
        const int32_t index = ToInt(CheckNumber(L, 2));

        if (index == 0) {
            return 0;
        }

        const uint32_t c = CategoryIndex(name);

        if (c == 0) {
            return 0;
        }

        Record* r = c <= 6 ? s_containers[c].At(static_cast<uint32_t>(index - 1)) : nullptr;

        if (r) {
            PushRecord(L, *r);
        } else {
            lua_pushnil(L);
        }

        return 1;
    }

    int UpdateFilter(lua_State* L) {   // 0x10306e50
        if (!ValidateInput(L, { LUA_TSTRING, LUA_TSTRING })) {
            return 0;
        }

        const std::string name = CheckString(L, 1);
        const std::string search = CheckString(L, 2);
        ApplyFilter(CategoryIndex(name), search);
        return 0;
    }

    // RecoverCategoryItemAtIndex(category, index, text) (0x10306A10; FUN_102fe420 reads {string, number, string}).
    int RecoverCategoryItemAtIndex(lua_State* L) {
        if (!ValidateInput(L, { LUA_TSTRING, LUA_TNUMBER, LUA_TSTRING })) {
            return 0;
        }

        const std::string name = CheckString(L, 1);
        const int32_t index = ToInt(CheckNumber(L, 2));
        const std::string text = CheckString(L, 3);

        if (index == 0) {
            return 0;
        }

        const uint32_t c = CategoryIndex(name);

        if (c == 0) {
            return 0;
        }

        int32_t position = -1;

        if (c <= 6) {
            CategoryContainer& ct = s_containers[c];
            const Record* r = ct.At(static_cast<uint32_t>(index - 1));   // FUN_10305a00..: the view entry

            for (size_t i = 0; r && i < ct.records.size(); ++i) {       // std::find over +4
                if (ct.records[i].get() == r) {
                    position = static_cast<int32_t>(i);
                    break;
                }
            }
        }

        if (position < 0) {
            PushBool(L, false);
            return 1;
        }

        CoA::Packet p(kRecoverOpcode[c]);
        p.U32(static_cast<uint32_t>(position)).Str(text.c_str());
        p.Send();
        s_pendingCategory = c;
        s_pendingPosition = static_cast<uint32_t>(position);
        PushBool(L, true);
        return 1;
    }

    // ---- packets ------------------------------------------------------------------------------------------------
    template <uint32_t Category>
    int32_t OnQueryResponse(void*, uint32_t, uint32_t, CDataStore* msg) {
        CategoryContainer& c = s_containers[Category];
        c.Reset();   // FUN_10306370
        c.records.clear();

        for (uint32_t n = CoA::Read::U32(msg); n && CoA::Read::Remaining(msg) != 0; --n) {
            switch (Category) {
            case 2: c.records.push_back(ReadCharacter(msg)); break;
            case 5: c.records.push_back(ReadItem(msg, Kind::Mythic)); break;
            case 6: c.records.push_back(ReadWorldforged(msg)); break;
            default: c.records.push_back(ReadItem(msg, Kind::Item)); break;
            }
        }

        CoA::Events::Signal("RECOVERY_QUERY_RESULT", "%s", kCategories[Category]);
        return 1;
    }

    // FUN_103060b0: success marks the pending position of the pending category's VIEW recovered (the position is in
    // the all list; the original indexes the view with it) and refilters with the container's last text.
    template <uint32_t Category>
    int32_t OnRecoverResult(void*, uint32_t, uint32_t, CDataStore* msg) {
        const std::string result = CoA::Read::Str(msg);

        if (result == kRecoveryOk[Category]) {
            if (s_pendingCategory >= 1 && s_pendingCategory <= 6) {
                CategoryContainer& c = s_containers[s_pendingCategory];

                if (s_pendingPosition < c.Size()) {
                    if (Record* r = c.Filtered()[s_pendingPosition]) {
                        r->recovered = true;
                    }
                }

                const std::string text = c.LastText();
                ApplyFilter(s_pendingCategory, text);
            }
        }

        s_pendingCategory = 0;
        s_pendingPosition = 0xFFFFFFFF;
        CoA::Events::Signal("RECOVERY_RESULT", "%s%s", kCategories[Category], result.c_str());
        return 1;
    }

    const CoA::Binding s_bindings[] = {
        COA_NATIVE("C_RecoveryService", "GetCategoryItemAtIndex", GetCategoryItemAtIndex),
        COA_NATIVE("C_RecoveryService", "GetNumItemsInCategory", GetNumItemsInCategory),
        COA_NATIVE("C_RecoveryService", "QueryCategory", QueryCategory),
        COA_NATIVE("C_RecoveryService", "RecoverCategoryItemAtIndex", RecoverCategoryItemAtIndex),
        COA_NATIVE("C_RecoveryService", "UpdateFilter", UpdateFilter),
    };
}

CoA::RecoveryService::Deps& CoA::RecoveryService::Dependencies() {
    return s_deps;
}

void CoA::RecoveryService::Init() {   // FUN_102fea30
    CoA::LuaReg::AddModule(s_bindings, sizeof(s_bindings) / sizeof(s_bindings[0]));
    COA_EVENT("RECOVERY_QUERY_RESULT");
    COA_EVENT("RECOVERY_RESULT");
    COA_PACKET(0x5D3, OnQueryResponse<1>);
    COA_PACKET(0x5DB, OnQueryResponse<2>);
    COA_PACKET(0x5DF, OnQueryResponse<3>);
    COA_PACKET(0x5E3, OnQueryResponse<4>);
    COA_PACKET(0x5E7, OnQueryResponse<5>);
    COA_PACKET(0x683, OnQueryResponse<6>);
    COA_PACKET(0x5D5, OnRecoverResult<1>);
    COA_PACKET(0x5DD, OnRecoverResult<2>);
    COA_PACKET(0x5E1, OnRecoverResult<3>);
    COA_PACKET(0x5E5, OnRecoverResult<4>);
    COA_PACKET(0x5E9, OnRecoverResult<5>);
    COA_PACKET(0x685, OnRecoverResult<6>);
}

void CoA::RecoveryService::Reset() {
    for (CategoryContainer& c : s_containers) {
        c.Reset();
        c.records.clear();
    }

    s_pendingCategory = 0;
    s_pendingPosition = 0xFFFFFFFF;
    s_deps = Deps();
}
