// AscVanityCollection.cpp: C_VanityCollection.
//
// Manager FUN_1032e9b0 (0x10D3C030): +0x00 the known vanity item ids, +0x10 an unordered_set of the same ids. CMSG
// 0x523 CUSTOM_ASCENSION_POINT_SPEND_REQUEST {u8 kind, u32 item} buys (kind 2 = web shop / delivery).
//
// The item browser (FUN_1032c140 builder, FUN_1032fcd0 query) reads the bridge store's VanityCollection records:
// 0 ID, 1 itemid, 2 category, 3 artwork (str), 4 dp, 5 sp, 6 bt, 7..11 Live / Seasonal / League / PTR / Dev, 0xC
// flags, 0xD..0xE class mask (u64), 0xF race mask, 0x10 creature preview, 0x11 spell preview, 0x12..0x29 contents,
// 0x2A description (str), 0x2B additional text (str), 0x2C learned spell. Item names, descriptions, quality and
// flags come from the ItemAddon records (FUN_1020ef10).
#include "CoA/Source/Modules/VanityCollection.h"
#include "CoA/Source/Modules/BridgeStore.h"
#include "CoA/Source/Modules/RealmInfo.h"
#include "CoA/Source/Runtime/ClientState.h"
#include "CoA/Source/Runtime/Events.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Packet.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Source/Runtime/Read.h"
#include "CoA/Source/Runtime/Script.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace {
    using CoA::Script::CheckNumber;
    using CoA::Script::CheckString;
    using CoA::Script::PushBool;
    using CoA::Script::ToInt;
    using CoA::BridgeStore::VanityRecord;

    std::vector<uint32_t> s_known;              // +0x00
    std::unordered_set<uint32_t> s_knownSet;    // +0x10

    uint32_t Rec(const VanityRecord* row, uint32_t i) {
        return row->f[i];
    }

    std::string RecStr(const VanityRecord* row, uint32_t i) {
        const char* s = row->Str(i);
        return s ? s : "";
    }

    // The 0xE8-byte VanityCollectionItem (FUN_1032d3e0, names / quality / flags by FUN_1032bee0).
    struct Item {
        uint32_t row = 0;
        uint32_t id = 0;
        uint32_t itemId = 0;
        uint32_t category = 0;
        uint32_t mask = 0;
        uint32_t dp = 0;
        uint32_t sp = 0;
        uint32_t bt = 0;
        uint32_t flags = 0;
        uint64_t classMask = 0;
        uint32_t raceMask = 0;
        uint32_t contents[24] = {};
        uint32_t learnedSpell = 0;
        std::string search;   // lower-case
        std::string name;
        std::string itemDescription;
        uint32_t quality = 0;
        uint32_t itemFlags = 0;
    };

    std::vector<Item> s_items;        // +0x5C
    std::vector<Item*> s_itemList;    // +0x104
    bool s_built = false;             // +0x58
    std::vector<Item*> s_results;     // +0xF8
    std::vector<Item*> s_page;        // +0x110

    // FUN_10331970
    std::string Lower(std::string s) {
        for (char& c : s) {
            c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
        }

        return s;
    }

    // FUN_1021a1a0: not hidden (flags bit 3), and offered on one of this realm's kinds.
    bool Offered(const VanityRecord* row) {
        if (Rec(row, 0xC) & 8) {
            return false;
        }

        const CoA::RealmInfo::Info& r = CoA::RealmInfo::Get();
        return (Rec(row, 7) && r.Live()) || (Rec(row, 8) && r.Seasonal()) || (Rec(row, 9) && r.League()) || (Rec(row, 10) && r.PTR()) ||
            (Rec(row, 11) && r.Dev());
    }

    // rec[2] | 0x20000000 (sp cost) | 0x40000000 (flags 0x20)
    uint32_t CategoryMask(const VanityRecord* row) {
        uint32_t m = Rec(row, 2);

        if (Rec(row, 5)) {
            m |= 0x20000000;
        }

        if (Rec(row, 0xC) & 0x20) {
            m |= 0x40000000;
        }

        return m;
    }

    // FUN_1032c140: every record index, fetched 1,000 at a time (FUN_1032dc10). A failed batch abandons the build
    // with an empty list; records not found or not offered are skipped.
    void Build() {
        s_items.clear();
        s_itemList.clear();
        uint32_t n = CoA::BridgeStore::VanityCount();
        std::vector<uint32_t> indexes(n);

        for (uint32_t i = 0; i < n; ++i) {
            indexes[i] = i;
        }

        std::vector<CoA::BridgeStore::VanityEntry> batch;

        for (uint32_t done = 0; done < n; done += 1000) {
            uint32_t take = std::min<uint32_t>(n - done, 1000);

            if (!CoA::BridgeStore::VanityRecords(indexes.data() + done, take, batch)) {
                s_items.clear();
                s_built = true;
                return;
            }

            for (const CoA::BridgeStore::VanityEntry& v : batch) {
                if (!v.found || !Offered(&v.record)) {
                    continue;
                }

                const VanityRecord* row = &v.record;
                Item e;
                e.row = v.index;
                e.id = Rec(row, 0);
                e.itemId = Rec(row, 1);
                e.category = Rec(row, 2);
                e.mask = CategoryMask(row);
                e.dp = Rec(row, 4);
                e.sp = Rec(row, 5);
                e.bt = Rec(row, 6);
                e.flags = Rec(row, 0xC);
                e.classMask = static_cast<uint64_t>(Rec(row, 0xD)) | (static_cast<uint64_t>(Rec(row, 0xE)) << 32);
                e.raceMask = Rec(row, 0xF);

                for (uint32_t c = 0; c < 24; ++c) {
                    e.contents[c] = Rec(row, 0x12 + c);
                }

                e.learnedSpell = Rec(row, 0x2C);
                e.search = RecStr(row, 0x2A) + " " + RecStr(row, 0x2B) + " " + std::to_string(e.itemId);   // FUN_1032d3e0
                s_items.push_back(std::move(e));
            }
        }

        // FUN_1020ef10 over the unique item ids (its result is not checked), then FUN_1032bee0 per item: the item
        // text when found, and the search text rebuilt either way.
        std::vector<uint32_t> ids;
        std::unordered_set<uint32_t> seen;

        for (const Item& e : s_items) {
            if (seen.insert(e.itemId).second) {
                ids.push_back(e.itemId);
            }
        }

        std::unordered_map<uint32_t, CoA::ItemAddon::Info> info;
        CoA::ItemAddon::FetchInfo(ids.data(), static_cast<uint32_t>(ids.size()), info);

        for (Item& e : s_items) {
            auto it = info.find(e.itemId);

            if (it != info.end()) {
                e.name = it->second.name;
                e.itemDescription = it->second.description;
                e.quality = it->second.quality;
                e.itemFlags = it->second.flags;
            }

            e.search = Lower(e.name + " " + e.itemDescription + " " + e.search);
        }

        for (Item& e : s_items) {
            s_itemList.push_back(&e);
        }

        s_built = true;
    }

    std::vector<Item*>& Items() {
        if (!s_built) {
            Build();
        }

        return s_itemList;
    }

    // FUN_1032e220
    Item* ByItemId(uint32_t item) {
        for (Item* e : Items()) {
            if (e->itemId == item) {
                return e;
            }
        }

        return nullptr;
    }

    void SetNumber(lua_State* L, const char* key, uint32_t value) {
        lua_pushstring(L, key);
        lua_pushinteger(L, static_cast<int32_t>(value));
        lua_settable(L, -3);
    }

    void SetString(lua_State* L, const char* key, const std::string& value) {
        lua_pushstring(L, key);
        lua_pushstring(L, value.c_str());
        lua_settable(L, -3);
    }

    // FUN_1032f6e0: the Lua table, re-read from the record (empty when it is gone).
    void PushItem(lua_State* L, const Item* e) {
        const VanityRecord* row = CoA::BridgeStore::VanityAt(e->row, e->itemId);   // FUN_1032e090

        if (!row) {
            lua_createtable(L, 0, 0);
            return;
        }

        uint32_t item = Rec(row, 1);
        CoA::ItemAddon::Info info = CoA::ItemAddon::GetInfo(item);   // FUN_10330740 -> FUN_1020e9c0
        lua_createtable(L, 0, 0);
        lua_checkstack(L, 2);
        SetNumber(L, "id", Rec(row, 0));
        SetNumber(L, "itemid", item);
        SetNumber(L, "itemID", item);
        SetString(L, "name", info.name);
        SetNumber(L, "quality", info.quality);
        SetString(L, "description", RecStr(row, 0x2A));
        uint32_t mask = CategoryMask(row);
        SetNumber(L, "group", mask);
        SetNumber(L, "categoryMask", mask);
        SetString(L, "additionalText", RecStr(row, 0x2B));
        // Item.dbc display id -> ItemDisplayInfo inventory icon.
        SetString(L, "icon", CoA::ClientState::ItemIcon(item));
        SetString(L, "artwork", RecStr(row, 3));
        SetNumber(L, "dpCost", Rec(row, 4));
        SetNumber(L, "spCost", Rec(row, 5));
        SetNumber(L, "btCost", Rec(row, 6));
        SetNumber(L, "creaturePreview", Rec(row, 0x10));
        SetNumber(L, "spellPreview", Rec(row, 0x11));
        lua_pushstring(L, "contentsPreview");
        lua_createtable(L, 0, 0x18);
        lua_checkstack(L, 2);

        for (uint32_t i = 0; i < 24; ++i) {
            if (Rec(row, 0x12 + i) != 0) {
                lua_pushnumber(L, static_cast<double>(i + 1));
                lua_pushinteger(L, static_cast<int32_t>(Rec(row, 0x12 + i)));
                lua_settable(L, -3);
            }
        }

        lua_settable(L, -3);
        SetNumber(L, "flags", Rec(row, 0xC));
        SetNumber(L, "learnedSpell", Rec(row, 0x2C));
        SetNumber(L, "allowableClass", Rec(row, 0xD));
        SetNumber(L, "allowableRace", Rec(row, 0xF));
        lua_pushstring(L, "owned");
        PushBool(L, CoA::VanityCollection::Owned(item));
        lua_settable(L, -3);
    }

    // ---- BuildVanityCollectionIndex filters (lambda_1..8) and sorts (lambda_9..13) ------------------------------
    const uint32_t kCategoryMasks[] = {   // DAT_10b5cc20
        0x2000000, 0x4000000, 0x8000000, 0x10000000, 0x20000000, 0x40000000, 0x80000000,
        0x100001, 0x100002, 0x100004, 0x100008, 0x100010, 0x100020, 0x100040, 0x100080, 0x100100,
        0x100200, 0x100400, 0x100800, 0x101000, 0x102000, 0x104000, 0x108000, 0x110000, 0x120000,
        0x200001, 0x200002, 0x200004, 0x200008, 0x200010, 0x200020, 0x200040, 0x200080, 0x200100,
        0x200200, 0x200400, 0x200800, 0x201000, 0x202000, 0x400001, 0x400002, 0x400004, 0x800001,
        0x800002, 0x800004, 0x800008, 0x1000001, 0x1000002, 0x1000004, 0x1000008, 0x1000010,
    };

    // 0x10331CC0
    bool CategoryFilter(const Item& e, uint32_t arg) {
        if (arg == 0 || arg == 0xFFFFFFFF) {
            return true;
        }

        for (uint32_t m : kCategoryMasks) {
            if ((e.mask & m) == m && (m & arg) == m) {
                return true;
            }
        }

        return false;
    }

    // 0x10331D40 (always applied): the selected character's class and race.
    bool CurrentCharacterFilter(const Item& e) {
        if (e.classMask != 0) {
            uint32_t c = static_cast<uint32_t>(CoA::ClientState::CharacterClass()) - 1;

            if (c > 0x3F || !(e.classMask & (1ull << c))) {
                return false;
            }
        }

        if (e.raceMask != 0) {
            uint32_t r = static_cast<uint32_t>(CoA::ClientState::CharacterRace()) - 1;

            if (r > 0x1F || !(e.raceMask & (1u << r))) {
                return false;
            }
        }

        return true;
    }

    // 0x10331F30: bit 0 owned, bit 1 not owned.
    bool OwnershipFilter(const Item& e, uint32_t arg) {
        if ((arg & 1) == ((arg >> 1) & 1)) {
            return true;
        }

        return CoA::VanityCollection::Owned(e.itemId) == ((arg & 1) != 0);
    }

    enum SortKind { SORT_NAME, SORT_QUALITY, SORT_COST, SORT_OWNED_FIRST, SORT_UNOWNED_FIRST };

    // 0x10332090: the lowest non-zero price.
    uint32_t CostKey(const Item& e) {
        uint32_t k = 0xFFFFFFFF;

        for (uint32_t v : { e.dp, e.sp, e.bt }) {
            if (v != 0 && v < k) {
                k = v;
            }
        }

        return k == 0xFFFFFFFF ? 0 : k;
    }

    // FUN_1032fcd0
    uint32_t Query(const std::string& text, uint32_t category, uint32_t filters, uint32_t sort, uint32_t offset, uint32_t count) {
        std::string needle = Lower(text);
        s_results.clear();

        for (Item* e : Items()) {
            if (!needle.empty() && e->search.find(needle) == std::string::npos) {   // vanity_raw_search_filter
                continue;
            }

            if (!CurrentCharacterFilter(*e) || !CategoryFilter(*e, category)) {
                continue;
            }

            if ((filters & 3) && !OwnershipFilter(*e, filters)) {
                continue;
            }

            if ((filters & 4) && e->dp == 0 && e->sp == 0 && e->bt == 0) {   // purchasable
                continue;
            }

            if ((filters & 8) && (e->flags & 4)) {   // deliverable
                continue;
            }

            if ((filters & 0x10) && e->quality != 7) {   // heirloom
                continue;
            }

            if ((filters & 0x20) && !(e->itemFlags & 0x200) && Lower(e->itemDescription).find("manastorm") == std::string::npos) {
                continue;
            }

            s_results.push_back(e);
        }

        bool desc = false;
        int kind = -1;

        switch (sort) {
        case 1: kind = SORT_NAME; break;
        case 2: kind = SORT_NAME; desc = true; break;
        case 3: kind = SORT_QUALITY; desc = true; break;
        case 4: kind = SORT_QUALITY; break;
        case 5: kind = SORT_COST; break;
        case 6: kind = SORT_COST; desc = true; break;
        case 7: kind = SORT_OWNED_FIRST; break;
        case 8: kind = SORT_UNOWNED_FIRST; break;
        }

        if (kind >= 0) {
            std::stable_sort(s_results.begin(), s_results.end(), [kind, desc](const Item* a, const Item* b) {
                if (kind == SORT_NAME) {
                    std::string x = Lower(a->name);
                    std::string y = Lower(b->name);
                    return desc ? y < x : x < y;
                }

                uint32_t x = 0;
                uint32_t y = 0;

                if (kind == SORT_QUALITY) {
                    x = a->quality;
                    y = b->quality;
                } else if (kind == SORT_COST) {
                    x = CostKey(*a);
                    y = CostKey(*b);
                } else if (kind == SORT_OWNED_FIRST) {
                    x = !CoA::VanityCollection::Owned(a->itemId);
                    y = !CoA::VanityCollection::Owned(b->itemId);
                } else {
                    x = CoA::VanityCollection::Owned(a->itemId);
                    y = CoA::VanityCollection::Owned(b->itemId);
                }

                return desc ? y < x : x < y;
            });
        }

        s_page.clear();
        uint32_t total = static_cast<uint32_t>(s_results.size());

        if (offset < total && count != 0) {
            uint32_t n = std::min(count, total - offset);
            s_page.assign(s_results.begin() + offset, s_results.begin() + offset + n);
        }

        return total;
    }

    int PushList(lua_State* L, const std::vector<Item*>& list) {
        lua_createtable(L, 0, static_cast<int>(list.size()));
        lua_checkstack(L, 2);

        for (size_t i = 0; i < list.size(); ++i) {
            lua_pushinteger(L, static_cast<int32_t>(i + 1));
            PushItem(L, list[i]);
            lua_settable(L, -3);
        }

        return 1;
    }

    // ---- natives ------------------------------------------------------------------------------------------------
    // IsCollectionItemOwned / IsOwned (handler_IsCollectionItemOwned): exactly one argument.
    int IsCollectionItemOwned(lua_State* L) {
        if (lua_gettop(L) != 1) {
            return 0;
        }

        PushBool(L, CoA::VanityCollection::Owned(static_cast<uint32_t>(ToInt(CheckNumber(L, 1)))));
        return 1;
    }

    // FUN_103311d0 (also AscBindings' C_VanityCollection.Purchase): (item, kind).
    int PurchaseCollectionItem(lua_State* L) {
        if (lua_gettop(L) != 2) {
            return 0;
        }

        int32_t item = ToInt(CheckNumber(L, 1));
        uint8_t kind = static_cast<uint8_t>(static_cast<int>(CheckNumber(L, 2)));
        CoA::Packet(0x523).U8(kind).U32(static_cast<uint32_t>(item)).Send();
        return 0;
    }

    // FUN_103312f0: (item) -> true
    int PurchaseWebShopItem(lua_State* L) {
        if (lua_gettop(L) != 1) {
            return 0;
        }

        CoA::Packet(0x523).U8(2).U32(static_cast<uint32_t>(ToInt(CheckNumber(L, 1)))).Send();
        PushBool(L, true);
        return 1;
    }

    // FUN_103317c0: (item)
    int RequestDelivery(lua_State* L) {
        if (lua_gettop(L) != 1) {
            return 0;
        }

        CoA::Packet(0x523).U8(2).U32(static_cast<uint32_t>(ToInt(CheckNumber(L, 1)))).Send();
        return 0;
    }

    // AscBindings' IsPurchaseInProgress: returns nothing.
    int IsPurchaseInProgress(lua_State* L) {
        return 0;
    }

    // FUN_10330f60: SpellCustomAttr +0x18 bit 5, and the spell's mechanic (+0xC) is not 21 (mount).
    int IsConsolidatedVanityBuff(lua_State* L) {
        if (!CoA::Script::ValidateInput(L, { LUA_TNUMBER })) {
            return 0;
        }

        uint32_t spell = static_cast<uint32_t>(ToInt(CheckNumber(L, 1)));
        auto attr = CoA::VanityCollection::Dependencies().spellCustomAttr18;
        bool yes = false;

        if (attr && attr(spell, 0x20)) {
            int32_t mechanic = CoA::ClientState::SpellMechanic(spell);
            yes = mechanic >= 0 && mechanic != 0x15;
        }

        PushBool(L, yes);
        return 1;
    }

    // FUN_10331420: ([text [, category [, filters [, sort [, offset [, count = 9]]]]]]) -> {total, items}; at most
    // six arguments, nil ones default.
    int QueryItems(lua_State* L) {
        int top = lua_gettop(L);

        if (top >= 7) {
            return 0;
        }

        auto given = [L, top](int i) { return top >= i && lua_type(L, i) > 0; };
        std::string text;

        if (given(1)) {
            text = CheckString(L, 1);
        }

        uint32_t category = given(2) ? static_cast<uint32_t>(ToInt(CheckNumber(L, 2))) : 0;
        uint32_t filters = given(3) ? static_cast<uint32_t>(ToInt(CheckNumber(L, 3))) : 0;
        uint32_t sort = given(4) ? static_cast<uint32_t>(ToInt(CheckNumber(L, 4))) : 0;
        uint32_t offset = given(5) ? static_cast<uint32_t>(ToInt(CheckNumber(L, 5))) : 0;
        uint32_t count = given(6) ? static_cast<uint32_t>(ToInt(CheckNumber(L, 6))) : 9;

        if (sort > 8) {
            sort = 0;
        }

        uint32_t total = Query(text, category, filters, sort, offset, count);
        lua_createtable(L, 0, 2);
        lua_checkstack(L, 3);
        lua_pushstring(L, "total");
        lua_pushinteger(L, static_cast<int32_t>(total));
        lua_settable(L, -3);
        lua_pushstring(L, "items");
        PushList(L, s_page);
        lua_settable(L, -3);
        return 1;
    }

    // handler_GetAllItems: no arguments.
    int GetAllItems(lua_State* L) {
        if (lua_gettop(L) != 0) {
            return 0;
        }

        return PushList(L, Items());
    }

    // handler_GetItem: (itemid)
    int GetItem(lua_State* L) {
        if (lua_gettop(L) != 1) {
            return 0;
        }

        const Item* e = ByItemId(static_cast<uint32_t>(ToInt(CheckNumber(L, 1))));

        if (!e) {
            return 0;
        }

        PushItem(L, e);
        return 1;
    }

    // handler_GetItemByLearnedSpell: (spell)
    int GetItemByLearnedSpell(lua_State* L) {
        if (lua_gettop(L) != 1) {
            return 0;
        }

        uint32_t spell = static_cast<uint32_t>(ToInt(CheckNumber(L, 1)));

        for (const Item* e : Items()) {
            if (e->learnedSpell == spell) {
                PushItem(L, e);
                return 1;
            }
        }

        return 0;
    }

    // handler_GetOwnerByContentItem: (item in contents)
    int GetOwnerByContentItem(lua_State* L) {
        if (lua_gettop(L) != 1) {
            return 0;
        }

        uint32_t item = static_cast<uint32_t>(ToInt(CheckNumber(L, 1)));

        for (const Item* e : Items()) {
            for (uint32_t c : e->contents) {
                if (c == item) {
                    PushItem(L, e);
                    return 1;
                }
            }
        }

        return 0;
    }

    // handler_GetRandomItem
    int GetRandomItem(lua_State* L) {
        if (lua_gettop(L) != 0) {
            return 0;
        }

        std::vector<Item*>& items = Items();

        if (items.empty()) {
            return 0;
        }

        PushItem(L, items[static_cast<uint32_t>(rand()) % items.size()]);
        return 1;
    }

    // FUN_10330d70 / FUN_1032de20: flags bit 4.
    int GetSeasonalShowcaseItems(lua_State* L) {
        if (lua_gettop(L) != 0) {
            return 0;
        }

        std::vector<Item*> list;

        for (Item* e : Items()) {
            if (e->flags & 0x10) {
                list.push_back(e);
            }
        }

        return PushList(L, list);
    }

    // handler_GetDPPrice: (itemid) -> +0x14, 0 when unknown.
    int GetDPPrice(lua_State* L) {
        if (lua_gettop(L) != 1) {
            return 0;
        }

        const Item* e = ByItemId(static_cast<uint32_t>(ToInt(CheckNumber(L, 1))));
        lua_pushinteger(L, e ? static_cast<int32_t>(e->dp) : 0);
        return 1;
    }

    // handler_GetVPPrice: (itemid) -> +0x18, 0 when unknown.
    int GetVPPrice(lua_State* L) {
        if (lua_gettop(L) != 1) {
            return 0;
        }

        const Item* e = ByItemId(static_cast<uint32_t>(ToInt(CheckNumber(L, 1))));
        lua_pushinteger(L, e ? static_cast<int32_t>(e->sp) : 0);
        return 1;
    }

    const CoA::Binding s_bindings[] = {
        COA_NATIVE("C_VanityCollection", "IsCollectionItemOwned", IsCollectionItemOwned),
        COA_NATIVE("", "IsCollectionItemOwned", IsCollectionItemOwned),   // FUN_10332760 also sets it as a global
        COA_NATIVE("C_VanityCollection", "IsOwned", IsCollectionItemOwned),
        COA_NATIVE("C_VanityCollection", "PurchaseCollectionItem", PurchaseCollectionItem),
        COA_NATIVE("C_VanityCollection", "PurchaseWebShopItem", PurchaseWebShopItem),
        COA_NATIVE("C_VanityCollection", "RequestDelivery", RequestDelivery),
        COA_NATIVE("C_VanityCollection", "IsConsolidatedVanityBuff", IsConsolidatedVanityBuff),
        COA_NATIVE("C_VanityCollection", "QueryItems", QueryItems),
        COA_NATIVE("C_VanityCollection", "GetAllItems", GetAllItems),
        COA_NATIVE("C_VanityCollection", "GetItem", GetItem),
        COA_NATIVE("C_VanityCollection", "GetItemByLearnedSpell", GetItemByLearnedSpell),
        COA_NATIVE("C_VanityCollection", "GetOwnerByContentItem", GetOwnerByContentItem),
        COA_NATIVE("C_VanityCollection", "GetRandomItem", GetRandomItem),
        COA_NATIVE("C_VanityCollection", "GetSeasonalShowcaseItems", GetSeasonalShowcaseItems),
        COA_NATIVE("C_VanityCollection", "GetDPPrice", GetDPPrice),
        COA_NATIVE("C_VanityCollection", "GetVPPrice", GetVPPrice),
        // AscBindings.cpp
        COA_NATIVE("C_VanityCollection", "Purchase", PurchaseCollectionItem),
        COA_NATIVE("C_VanityCollection", "IsPurchaseInProgress", IsPurchaseInProgress),
    };
}

bool CoA::VanityCollection::Owned(uint32_t item) {
    return s_knownSet.count(item) != 0 || std::find(s_known.begin(), s_known.end(), item) != s_known.end();
}

// 0x6F7: u32 n, n x u32
int32_t CoA::VanityCollection::OnList(void* param, uint32_t opcode, uint32_t time, CDataStore* msg) {
    s_known.clear();
    s_knownSet.clear();
    uint32_t n = CoA::Read::U32(msg);

    for (uint32_t i = 0; i < n && !CoA::Read::Failed(msg); ++i) {
        uint32_t id = CoA::Read::U32(msg);

        if (CoA::Read::Failed(msg)) {
            break;
        }

        s_known.push_back(id);
        s_knownSet.insert(id);
    }

    return 1;
}

// 0x6F8: u32
int32_t CoA::VanityCollection::OnAdd(void* param, uint32_t opcode, uint32_t time, CDataStore* msg) {
    uint32_t id = CoA::Read::U32(msg);
    s_known.push_back(id);
    s_knownSet.insert(id);
    return 1;
}

// 0x6F9: u32
int32_t CoA::VanityCollection::OnRemove(void* param, uint32_t opcode, uint32_t time, CDataStore* msg) {
    uint32_t id = CoA::Read::U32(msg);
    s_known.erase(std::remove(s_known.begin(), s_known.end(), id), s_known.end());
    s_knownSet.erase(id);
    return 1;
}

// 0x9D0: clears the character-customization unlock lists (+0x2C / +0x3C), which nothing here reads.
int32_t CoA::VanityCollection::OnCustomizationUnlocks(void* param, uint32_t opcode, uint32_t time, CDataStore* msg) {
    return 1;
}

// SMSG 0x573 (FUN_101e55d0): the record's fields in order with its three strings as C strings in place, then three
// more C strings (read and dropped). FUN_1032ec60 patches the store; only on success does it rebuild the browser and
// signal HOT_PATCH_VANITY_COLLECTION("%u", item). The bridge refuses the patch, so neither happens.
int32_t CoA::VanityCollection::OnPatchVanity(void* param, uint32_t opcode, uint32_t time, CDataStore* msg) {
    VanityRecord r = {};
    std::string strings[3];

    for (uint32_t i = 0; i < 3; ++i) {
        r.f[i] = CoA::Read::U32(msg);
    }

    strings[0] = CoA::Read::Str(msg);

    for (uint32_t i = 4; i <= 0x29; ++i) {
        r.f[i] = CoA::Read::U32(msg);
    }

    strings[1] = CoA::Read::Str(msg);
    strings[2] = CoA::Read::Str(msg);
    r.f[0x2C] = CoA::Read::U32(msg);

    for (uint32_t i = 0; i < 3; ++i) {
        CoA::Read::Str(msg);
    }

    for (uint32_t i = 0; i < 3; ++i) {
        r.strings[i] = strings[i].c_str();
    }

    if (CoA::BridgeStore::PatchVanity(r)) {
        Build();
        CoA::Events::Signal("HOT_PATCH_VANITY_COLLECTION", "%u", r.f[1]);
    }

    return 1;
}

CoA::VanityCollection::Deps& CoA::VanityCollection::Dependencies() {
    static Deps deps;
    return deps;
}

void CoA::VanityCollection::Reset() {
    s_known.clear();
    s_knownSet.clear();
    s_items.clear();
    s_itemList.clear();
    s_results.clear();
    s_page.clear();
    s_built = false;
}

void CoA::VanityCollection::Init() {
    CoA::LuaReg::AddModule(s_bindings, sizeof(s_bindings) / sizeof(s_bindings[0]));
    COA_PACKET(0x6F7, CoA::VanityCollection::OnList);
    COA_PACKET(0x6F8, CoA::VanityCollection::OnAdd);
    COA_PACKET(0x6F9, CoA::VanityCollection::OnRemove);
    COA_PACKET(0x9D0, CoA::VanityCollection::OnCustomizationUnlocks);
    COA_PACKET(0x573, CoA::VanityCollection::OnPatchVanity);
    COA_EVENT("HOT_PATCH_VANITY_COLLECTION");
}
