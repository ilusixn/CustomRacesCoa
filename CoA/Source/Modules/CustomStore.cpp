// AscCustomStore.cpp (see CustomStore.h): the custom store manager, its packets and the C_CustomStore natives.
#include "CoA/Source/Modules/CustomStore.h"
#include "CoA/Source/Modules/BridgeStore.h"
#include "CoA/Source/Modules/GameEvents.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Modules/VanityCollection.h"
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
#include <cmath>
#include <cstring>
#include <map>
#include <string>
#include <vector>

using CoA::Script::CheckNumber;
using CoA::Script::CheckString;
using CoA::Script::PushBool;
using CoA::Script::PushStr;
using CoA::Script::ToInt;
using CoA::Script::ValidateInput;

namespace {
    const char* const kQuery[6] = { "QUERY_CUSTOM_STORE_OK", "QUERY_CUSTOM_STORE_UNKNOWN", "QUERY_CUSTOM_STORE_BAD_STORE",
        "QUERY_CUSTOM_STORE_NOT_FOUND", "QUERY_CUSTOM_STORE_NPC_NOT_IN_RANGE",
        "QUERY_CUSTOM_STORE_GO_NOT_IN_RANGE" };   // 0x10B39D10
    const char* const kPurchase[14] = { "PURCHASE_CUSTOM_STORE_ITEM_OK", "PURCHASE_CUSTOM_STORE_ITEM_UNKNOWN",
        "PURCHASE_CUSTOM_STORE_ITEM_NOT_FOUND", "PURCHASE_CUSTOM_STORE_ITEM_STORE_NOT_FOUND",
        "PURCHASE_CUSTOM_STORE_ITEM_NPC_NOT_IN_RANGE", "PURCHASE_CUSTOM_STORE_ITEM_GO_NOT_IN_RANGE",
        "PURCHASE_CUSTOM_STORE_ITEM_COUNT_TOO_LOW", "PURCHASE_CUSTOM_STORE_ITEM_COUNT_MONEY_OUT_OF_RANGE",
        "PURCHASE_CUSTOM_STORE_ITEM_NOT_ENOUGH_MONEY", "PURCHASE_CUSTOM_STORE_ITEM_COUNT_ITEMS_OUT_OF_RANGE",
        "PURCHASE_CUSTOM_STORE_ITEM_NOT_ENOUGH_ITEMS", "PURCHASE_CUSTOM_STORE_ITEM_CONDITIONS",
        "PURCHASE_CUSTOM_STORE_ITEM_GAME_EVENT", "PURCHASE_CUSTOM_STORE_ITEM_ACHIEVEMENT" };   // 0x10B3A4A8
    const char* const kFilters[31] = { "FILTER_NONE", "FILTER_CAN_AFFORD", "FILTER_CANNOT_AFFORD", "FILTER_VANITY_OWNED",
        "FILTER_VANITY_UNOWNED", "FILTER_QUALITY_POOR", "FILTER_QUALITY_NORMAL", "FILTER_QUALITY_UNCOMMON",
        "FILTER_QUALITY_RARE", "FILTER_QUALITY_EPIC", "FILTER_QUALITY_LEGENDARY", "FILTER_QUALITY_ARTIFACT",
        "FILTER_QUALITY_HEIRLOOM", "FILTER_SKILL_CARD_OWNED", "FILTER_SKILL_CARD_UNOWNED",
        "FILTER_SKILL_CARD_QUALITY_COMMON", "FILTER_SKILL_CARD_QUALITY_UNCOMMON", "FILTER_SKILL_CARD_QUALITY_RARE",
        "FILTER_SKILL_CARD_QUALITY_EPIC", "FILTER_SKILL_CARD_QUALITY_LEGENDARY", "FILTER_SKILL_CARD_SPELL_FAMILY_GENERIC",
        "FILTER_SKILL_CARD_SPELL_FAMILY_MAGE", "FILTER_SKILL_CARD_SPELL_FAMILY_WARRIOR",
        "FILTER_SKILL_CARD_SPELL_FAMILY_WARLOCK", "FILTER_SKILL_CARD_SPELL_FAMILY_PRIEST",
        "FILTER_SKILL_CARD_SPELL_FAMILY_DRUID", "FILTER_SKILL_CARD_SPELL_FAMILY_ROGUE",
        "FILTER_SKILL_CARD_SPELL_FAMILY_HUNTER", "FILTER_SKILL_CARD_SPELL_FAMILY_PALADIN",
        "FILTER_SKILL_CARD_SPELL_FAMILY_SHAMAN", "FILTER_SKILL_CARD_SPELL_FAMILY_DEATH_KNIGHT" };   // 0x10B39C18
    const uint32_t kFamilies[11] = { 0, 3, 4, 5, 6, 7, 8, 9, 10, 11, 15 };   // filters 20..30 (SpellFamilyName)

    struct Type {
        uint32_t id = 0, creature = 0, gameObject = 0;
        std::string first, second;
    };

    struct Item {
        uint32_t id = 0, type = 0, item = 0, money = 0;
        uint32_t tokens[5] = {}, counts[5] = {};
        uint32_t gameEvent = 0, achievement = 0;
    };

    CoA::CustomStore::Deps s_deps;
    std::vector<Type> s_types;    // +0x00
    uint32_t s_pageSize = 0x12;   // +0x0C
    std::vector<Item> s_items;    // +0x10

    const Type* FindType(uint32_t id) {
        for (const Type& t : s_types) {
            if (t.id == id) {
                return &t;
            }
        }

        return nullptr;
    }

    const Item* FindItem(uint32_t id) {
        for (const Item& i : s_items) {
            if (i.id == id) {
                return &i;
            }
        }

        return nullptr;
    }

    bool HasPlayer() {
        return s_deps.hasPlayer && s_deps.hasPlayer();
    }

    uint32_t Money() {
        return s_deps.playerMoney ? s_deps.playerMoney() : 0;
    }

    uint32_t Held(uint32_t item) {
        return s_deps.heldCount ? s_deps.heldCount(item) : 0;
    }

    bool Nearby(uint32_t entry, bool gameObject) {
        return s_deps.nearby && s_deps.nearby(entry, gameObject);
    }

    bool Completed(uint32_t id) {
        return s_deps.achievementDone && s_deps.achievementDone(id);
    }

    // FUN_101a3a20: PURCHASE_CUSTOM_STORE_ITEM_* for `count` of item `id`.
    uint32_t CanPurchase(uint32_t id, uint32_t count) {
        const Item* it = FindItem(id);

        if (!it) {
            return 2;
        }

        const Type* t = FindType(it->type);

        if (!t) {
            return 3;
        }

        if (t->creature && !Nearby(t->creature, false)) {
            return 4;
        }

        if (t->gameObject && !Nearby(t->gameObject, true)) {
            return 5;
        }

        if (count == 0) {
            return 6;
        }

        const uint32_t limit = static_cast<uint32_t>(0x7FFFFFFFull / count);

        if (limit < it->money) {
            return 7;
        }

        if (HasPlayer() && it->money * count < Money()) {   // (sic: the original compares this way round)
            return 8;
        }

        for (uint32_t k = 0; k < 5; ++k) {
            if (it->tokens[k] && it->counts[k]) {
                if (limit < it->counts[k]) {
                    return 9;
                }

                if (Held(it->tokens[k]) < it->counts[k] * count) {
                    return 10;
                }
            }
        }

        if (it->gameEvent && !CoA::GameEvents::IsActive(static_cast<uint16_t>(it->gameEvent))) {
            return 12;
        }

        if (it->achievement && !Completed(it->achievement)) {
            return 13;
        }

        return 0;
    }

    // FUN_101a6f50 cases 1 / 2: money and every token covered.
    bool Affordable(const Item& it) {
        if (HasPlayer() && Money() < it.money) {
            return false;
        }

        for (uint32_t k = 0; k < 5; ++k) {
            if (it.tokens[k] && it.counts[k] && Held(it.tokens[k]) < it.counts[k]) {
                return false;
            }
        }

        return true;
    }

    // FUN_101a5d50: the ItemAddon record's quality (+0x10) is `q`.
    bool ItemQuality(uint32_t item, uint32_t q) {
        const CoA::ItemAddon::Record* r = CoA::ItemAddon::Find(item);
        return r && r->Quality() == q;
    }

    class ItemContainer : public CoA::Filter::Container<const Item*> {
    protected:
        std::vector<const Item*> DoPopulate() override {   // FUN_101a6d00
            std::vector<const Item*> out;

            for (const Item& i : s_items) {
                out.push_back(&i);
            }

            return out;
        }

        bool ArgMatch(uint32_t f, const Item* const& it) override {   // FUN_101a6f50
            switch (f) {
            case 1: return Affordable(*it);
            case 2: return !Affordable(*it);
            case 3: return CoA::VanityCollection::Owned(it->item);
            case 4: return !CoA::VanityCollection::Owned(it->item);
            default: break;
            }

            if (f >= 5 && f <= 12) {
                return ItemQuality(it->item, f - 5);
            }

            if (f < 13 || f > 30) {
                return false;
            }

            CoA::CustomStore::SkillCard c;

            if (!s_deps.skillCardByItem || !s_deps.skillCardByItem(it->item, c)) {
                return false;
            }

            if (f == 13 || f == 14) {
                const bool owned = s_deps.skillCardCollectedAtLeast && s_deps.skillCardCollectedAtLeast(c.cardId, c.rank);
                return owned == (f == 13);
            }

            if (f <= 19) {
                return s_deps.skillCardQualityIndex && s_deps.skillCardQualityIndex(c.quality) == static_cast<int32_t>(f - 15);
            }

            uint32_t family = 0;
            return s_deps.spellFamily && s_deps.spellFamily(c.spell, family) && family == kFamilies[f - 20];
        }

        uint32_t ArgGroup(uint32_t f) override {   // FUN_101a75c0
            if (f == 1 || f == 2) {
                return 1;
            }

            if (f == 3 || f == 4) {
                return 2;
            }

            if (f >= 5 && f <= 12) {
                return 3;
            }

            if (f == 13 || f == 14) {
                return 4;
            }

            if (f >= 15 && f <= 19) {
                return 5;
            }

            if (f >= 20 && f <= 30) {
                return 6;
            }

            return 0;
        }

        // FUN_101a6da0: the item's name (ItemAddon record) contains the text; no text matches all.
        bool TextMatch(const std::string& lower, const Item* const& it) override {
            if (lower.empty()) {
                return true;
            }

            const CoA::ItemAddon::Record* r = CoA::ItemAddon::Find(it->item);

            if (!r) {
                return false;
            }

            return CoA::Filter::Lower(r->name ? r->name : "").find(lower) != std::string::npos;
        }
    };

    ItemContainer s_view;   // +0x1C

    // ---- natives ------------------------------------------------------------------------------------------------
    // FUN_101a5de0: (text, {FILTER_* = true}, {sorts, unused}, page size -- 0 means 18).
    int ApplyCustomStoreFilter(lua_State* L) {
        if (!ValidateInput(L, { LUA_TSTRING, LUA_TTABLE, LUA_TTABLE, LUA_TNUMBER })) {
            return 0;
        }

        const std::string text = CheckString(L, 1);
        std::map<std::string, bool> names;
        CoA::Script::ToBoolMap(L, 2, names);   // FUN_100bed80

        std::vector<uint32_t> filters;

        for (const auto& n : names) {
            if (n.second) {
                for (uint32_t i = 0; i < 31; ++i) {
                    if (n.first == kFilters[i]) {
                        filters.push_back(i);
                        break;
                    }
                }
            }
        }

        const int32_t size = ToInt(CheckNumber(L, 4));
        s_view.ApplyFilter(text, filters, {});   // FUN_101a4170
        s_pageSize = size == 0 ? 0x12 : static_cast<uint32_t>(size);
        return 0;
    }

    int GetCustomStoreData(lua_State* L) {   // FUN_101a6560: (page) -> {item ids}
        uint32_t page = 0;

        if (!CoA::Script::ReadNumber(L, page) || page == 0) {
            return 0;
        }

        const std::vector<const Item*>& f = s_view.Filtered();
        lua_createtable(L, 0, 0);
        lua_checkstack(L, 2);
        int32_t n = 0;

        for (uint32_t i = s_pageSize * (page - 1); i < s_pageSize * page && i < f.size() && f[i]; ++i) {
            lua_pushnumber(L, ++n);
            lua_pushinteger(L, static_cast<int32_t>(f[i]->id));
            lua_settable(L, -3);
        }

        return 1;
    }

    int GetCustomStoreMaxPages(lua_State* L) {   // 0x101A6850: ceil(filtered / page size), unsigned -> float
        const float n = static_cast<float>(static_cast<double>(static_cast<uint32_t>(s_view.Size())));
        const float size = static_cast<float>(static_cast<double>(s_pageSize));
        lua_pushnumber(L, static_cast<float>(std::ceil(static_cast<double>(n / size))));
        return 1;
    }

    void PushIntArray(lua_State* L, const std::vector<uint32_t>& v) {   // FUN_1009a460
        lua_createtable(L, 0, 0);
        lua_checkstack(L, 2);

        for (size_t i = 0; i < v.size(); ++i) {
            lua_pushnumber(L, static_cast<double>(i + 1));
            lua_pushinteger(L, static_cast<int32_t>(v[i]));
            lua_settable(L, -3);
        }
    }

    // FUN_101a66a0: item entry, money, {token items}, {token counts}, game event | nil, achievement | nil.
    int GetCustomStoreItemInfo(lua_State* L) {
        uint32_t id = 0;

        if (!CoA::Script::ReadNumber(L, id)) {
            return 0;
        }

        const Item* it = FindItem(id);

        if (!it) {
            return CoA::Script::PushNils(L, 6);
        }

        lua_pushinteger(L, static_cast<int32_t>(it->item));
        lua_pushinteger(L, static_cast<int32_t>(it->money));
        std::vector<uint32_t> tokens;
        std::vector<uint32_t> counts;

        for (uint32_t k = 0; k < 5; ++k) {
            if (it->tokens[k] && it->counts[k]) {
                tokens.push_back(it->tokens[k]);
                counts.push_back(it->counts[k]);
            }
        }

        PushIntArray(L, tokens);
        PushIntArray(L, counts);

        if (it->gameEvent) {
            lua_pushinteger(L, static_cast<int32_t>(it->gameEvent));
        } else {
            lua_pushnil(L);
        }

        if (it->achievement) {
            lua_pushinteger(L, static_cast<int32_t>(it->achievement));
        } else {
            lua_pushnil(L);
        }

        return 6;
    }

    int GetCustomStoreTypeInfo(lua_State* L) {   // (the second string, the first | nil)
        uint32_t id = 0;

        if (!CoA::Script::ReadNumber(L, id)) {
            return 0;
        }

        const Type* t = FindType(id);

        if (!t) {
            return CoA::Script::PushNils(L, 2);
        }

        PushStr(L, t->second.c_str());

        if (t->first.empty()) {
            lua_pushnil(L);
        } else {
            PushStr(L, t->first.c_str());
        }

        return 2;
    }

    template <size_t N>
    int PushCode(lua_State* L, uint32_t code, const char* const (&names)[N]) {
        PushBool(L, code == 0);

        if (code == 0) {
            lua_pushnil(L);
        } else {
            PushStr(L, code < N ? names[code] : ("UNEXPECTED_ENUM_VALUE_" + std::to_string(code)).c_str());
        }

        return 2;
    }

    // FUN_101a63a0: some loaded item must carry the id (else BAD_STORE), then the store type of that id.
    int CanQueryCustomStore(lua_State* L) {
        uint32_t id = 0;

        if (!CoA::Script::ReadNumber(L, id)) {
            return 0;
        }

        uint32_t code = 2;

        if (FindItem(id)) {
            const Type* t = FindType(id);

            if (!t) {
                code = 3;
            } else if (t->creature && !Nearby(t->creature, false)) {
                code = 4;
            } else if (t->gameObject && !Nearby(t->gameObject, true)) {
                code = 5;
            } else {
                code = 0;
            }
        }

        return PushCode(L, code, kQuery);
    }

    int CanPurchaseCustomStoreItem(lua_State* L) {   // FUN_101a6220: (item, count)
        int32_t id = 0;
        int32_t count = 0;

        if (!CoA::Script::ReadInt2(L, id, count)) {
            return 0;
        }

        return PushCode(L, CanPurchase(static_cast<uint32_t>(id), static_cast<uint32_t>(count)), kPurchase);
    }

    int QueryCustomStore(lua_State* L) {   // FUN_101a6c00
        uint32_t id = 0;

        if (!CoA::Script::ReadNumber(L, id)) {
            return 0;
        }

        CoA::Packet(0x6B9).U32(id).Send();
        PushBool(L, true);
        return 1;
    }

    int PurchaseCustomStoreItem(lua_State* L) {   // FUN_101a6af0
        int32_t id = 0;
        int32_t count = 0;

        if (!CoA::Script::ReadInt2(L, id, count)) {
            return 0;
        }

        CoA::Packet(0x6BB).U32(static_cast<uint32_t>(id)).U32(static_cast<uint32_t>(count)).Send();
        PushBool(L, true);
        return 1;
    }

    int IsItemLockedDueToAchievement(lua_State* L) {
        uint32_t id = 0;

        if (!CoA::Script::ReadNumber(L, id)) {
            return 0;
        }

        const Item* it = FindItem(id);
        PushBool(L, it && it->achievement && !Completed(it->achievement));
        return 1;
    }

    int IsItemLockedDueToGameEvent(lua_State* L) {
        uint32_t id = 0;

        if (!CoA::Script::ReadNumber(L, id)) {
            return 0;
        }

        const Item* it = FindItem(id);
        PushBool(L, it && it->gameEvent && !CoA::GameEvents::IsActive(static_cast<uint16_t>(it->gameEvent)));
        return 1;
    }

    // ---- packets ------------------------------------------------------------------------------------------------
    // 0x6BA (FUN_101a5590): str result; the items and the view cleared; on QUERY_CUSTOM_STORE_OK, u32 n x 0x40 bytes.
    int32_t OnQueryResult(void*, uint32_t, uint32_t, CDataStore* msg) {
        const std::string r = CoA::Read::Str(msg);
        s_items.clear();
        s_view.Reset();   // FUN_101016d0
        int32_t code = -1;

        for (int32_t i = 0; i < 6 && code < 0; ++i) {
            if (r == kQuery[i]) {
                code = i;
            }
        }

        if (code < 0) {
            CoA::Log::Printf("Unexpected Value: %s", r.c_str());
        }

        if (code == 0) {
            for (uint32_t n = CoA::Read::U32(msg); n && CoA::Read::Remaining(msg) != 0; --n) {
                uint32_t raw[16];
                CoA::Read::Data(msg, raw, sizeof(raw));
                Item it;
                it.id = raw[0];
                it.type = raw[1];
                it.item = raw[2];
                it.money = raw[3];
                memcpy(it.tokens, raw + 4, sizeof(it.tokens));
                memcpy(it.counts, raw + 9, sizeof(it.counts));
                it.gameEvent = raw[14];
                it.achievement = raw[15];
                s_items.push_back(it);
            }
        }

        CoA::Events::Signal("QUERY_CUSTOM_STORE_RESULT", "%s", r.c_str());
        return 1;
    }

    int32_t OnPurchaseResult(void*, uint32_t, uint32_t, CDataStore* msg) {   // 0x6BC
        const std::string r = CoA::Read::Str(msg);
        CoA::Events::Signal("PURCHASE_CUSTOM_STORE_ITEM_RESULT", "%s", r.c_str());
        return 1;
    }

    // 0x6BD (FUN_101a51a0): u32 n x {u32 id, u32 creature, u32 game object, str, str}, replacing the types.
    int32_t OnTypes(void*, uint32_t, uint32_t, CDataStore* msg) {
        s_types.clear();

        for (uint32_t n = CoA::Read::U32(msg); n && CoA::Read::Remaining(msg) != 0; --n) {
            Type t;
            t.id = CoA::Read::U32(msg);
            t.creature = CoA::Read::U32(msg);
            t.gameObject = CoA::Read::U32(msg);
            t.first = CoA::Read::Str(msg);
            t.second = CoA::Read::Str(msg);
            s_types.push_back(t);
        }

        return 1;
    }

    void OnGlueScreen() {   // 0x101A3EB0
        s_types.clear();
        s_pageSize = 0x12;
        s_items.clear();
        s_view.Reset();
    }

    const CoA::Binding s_bindings[] = {
        COA_NATIVE("C_CustomStore", "QueryCustomStore", QueryCustomStore),
        COA_NATIVE("C_CustomStore", "PurchaseCustomStoreItem", PurchaseCustomStoreItem),
        COA_NATIVE("C_CustomStore", "ApplyCustomStoreFilter", ApplyCustomStoreFilter),
        COA_NATIVE("C_CustomStore", "GetCustomStoreData", GetCustomStoreData),
        COA_NATIVE("C_CustomStore", "GetCustomStoreMaxPages", GetCustomStoreMaxPages),
        COA_NATIVE("C_CustomStore", "GetCustomStoreItemInfo", GetCustomStoreItemInfo),
        COA_NATIVE("C_CustomStore", "GetCustomStoreTypeInfo", GetCustomStoreTypeInfo),
        COA_NATIVE("C_CustomStore", "CanQueryCustomStore", CanQueryCustomStore),
        COA_NATIVE("C_CustomStore", "CanPurchaseCustomStoreItem", CanPurchaseCustomStoreItem),
        COA_NATIVE("C_CustomStore", "IsItemLockedDueToGameEvent", IsItemLockedDueToGameEvent),
        COA_NATIVE("C_CustomStore", "IsItemLockedDueToAchievement", IsItemLockedDueToAchievement),
    };
}

CoA::CustomStore::Deps& CoA::CustomStore::Dependencies() {
    return s_deps;
}

void CoA::CustomStore::Init() {   // 0x101A5950
    CoA::LuaReg::AddModule(s_bindings, sizeof(s_bindings) / sizeof(s_bindings[0]));
    COA_EVENT("QUERY_CUSTOM_STORE_RESULT");
    COA_EVENT("PURCHASE_CUSTOM_STORE_ITEM_RESULT");
    COA_PACKET(0x6BA, OnQueryResult);
    COA_PACKET(0x6BC, OnPurchaseResult);
    COA_PACKET(0x6BD, OnTypes);
    CoA::Lifecycle::OnGlueScreen(OnGlueScreen);
}

void CoA::CustomStore::Reset() {
    OnGlueScreen();
    s_deps = Deps();
}
