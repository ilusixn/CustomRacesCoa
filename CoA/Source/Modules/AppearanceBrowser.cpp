// AscAppearance.cpp's collection browser (AppearancesEntryContainer, manager +0x0C), the outfit view
// (AppearanceOutfitEntryContainer, +0xC8), C_AppearanceCollection, C_AppearanceOutfit and the seasonal lists
// (SeasonalAppearances.dbc: +4 appearance, +8 season, +0xC chapter, +0x10 order, +0x14..+0x24 realm gates).
#include "CoA/Source/Modules/AppearanceInternal.h"
#include "CoA/Source/Modules/Config.h"
#include "CoA/Source/Modules/RealmInfo.h"
#include "CoA/Source/Runtime/FilterContainer.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Packet.h"
#include "CoA/Source/Runtime/Script.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <algorithm>
#include <map>
#include <unordered_set>

using namespace CoA::Appearance;
using namespace CoA::Appearance::detail;
using CoA::Script::CheckNumber;
using CoA::Script::PushBool;
using CoA::Script::PushStr;
using CoA::Script::ReadInt2;
using CoA::Script::ReadNumber;
using CoA::Script::ReadString;
using CoA::Script::ToInt;
using CoA::Script::ValidateInput;

namespace {
    const char* const kFilters[38] = { "FILTER_NONE", "FILTER_COLLECTED", "FILTER_UNCOLLECTED", "FILTER_WEAPON_AXE", "FILTER_WEAPON_AXE2",
        "FILTER_WEAPON_BOW", "FILTER_WEAPON_GUN", "FILTER_WEAPON_MACE", "FILTER_WEAPON_MACE2", "FILTER_WEAPON_POLEARM", "FILTER_WEAPON_SWORD",
        "FILTER_WEAPON_SWORD2", "FILTER_WEAPON_STAFF", "FILTER_WEAPON_FIST", "FILTER_WEAPON_DAGGER", "FILTER_WEAPON_SPEAR",
        "FILTER_WEAPON_CROSSBOW", "FILTER_WEAPON_WAND", "FILTER_WEAPON_FISHING_POLE", "FILTER_WEAPON_THROWN", "FILTER_ARMOR_CLOTH",
        "FILTER_ARMOR_LEATHER", "FILTER_ARMOR_MAIL", "FILTER_ARMOR_PLATE", "FILTER_ARMOR_SHIELD", "FILTER_INVTYPE_MAINHAND",
        "FILTER_INVTYPE_OFFHAND", "FILTER_INVTYPE_HOLDABLE", "FILTER_AVAILABLE_ON_THE_WEB_STORE", "FILTER_AVAILABLE_ON_THE_ETHEREAL_BAZAAR",
        "FILTER_ITEM_QUALITY_POOR", "FILTER_ITEM_QUALITY_NORMAL", "FILTER_ITEM_QUALITY_UNCOMMON", "FILTER_ITEM_QUALITY_RARE",
        "FILTER_ITEM_QUALITY_EPIC", "FILTER_ITEM_QUALITY_LEGENDARY", "FILTER_ITEM_QUALITY_ARTIFACT", "FILTER_ITEM_QUALITY_HEIRLOOM" };   // 0x10B29C70
    const char* const kSorts[7] = { "SORT_NONE", "SORT_RECENTLY_COLLECTED_ASC", "SORT_RECENTLY_COLLECTED_DESC", "SORT_ALPHABETICAL_ASC",
        "SORT_ALPHABETICAL_DESC", "SORT_ID_ASC", "SORT_ID_DESC" };   // 0x10B29A74
    const uint32_t kPageSize = 18;

    // The browser's item text caches (manager +0x7C item -> lower-case name, +0x9C item -> quality), filled by the
    // prefetch FUN_101b4a70 before each query. Only ever cleared by the item addon refresh FUN_101b5420, which the
    // shipped client never lets run.
    std::unordered_map<uint32_t, std::string> s_itemNames;
    std::unordered_map<uint32_t, uint32_t> s_itemQualities;

    std::string ItemNameLower(uint32_t item) {
        auto it = s_itemNames.find(item);
        return it == s_itemNames.end() ? std::string() : it->second;
    }

    int ItemQuality(uint32_t item) {
        auto it = s_itemQualities.find(item);
        return it == s_itemQualities.end() ? -1 : static_cast<int>(it->second);
    }

    // FUN_101b3450: queue `item` unless its name is cached or it is already queued.
    void QueueItem(std::vector<uint32_t>& queue, std::unordered_set<uint32_t>& queued, uint32_t item) {
        if (s_itemNames.count(item) == 0 && queued.insert(item).second) {
            queue.push_back(item);
        }
    }

    bool InCategory(const uint8_t* a, uint32_t category) {
        for (uint32_t k = 0; k < 3; ++k) {
            if (U32(a, 0x14 + k * 4) == category) {
                return true;
            }
        }

        return false;
    }

    // AppearancesEntryContainer (vtable 0x10B2A398), entries are Appearances rows.
    class AppearanceBrowser : public CoA::Filter::Container<const uint8_t*> {
    protected:
        // FUN_101b41b0 -> FUN_101b4a70: before the query, fetch the text the query will need. Runs when there is filter
        // text or a name sort (3 / 4), a quality filter (0x1E..0x25), or an item filter (3..0x1B). For every item-display
        // appearance that is not an item set and, when a category is selected (manager +0x144 > 0), lies in it: with
        // text / name sort its own item (+4), and with text, name sort or quality filter every ItemAppearances row's item.
        // One FetchInfo batch, then the names (lower-cased) and qualities go into the caches.
        void BeforeFilter(const std::string& text, const std::vector<uint32_t>& args, const std::vector<uint32_t>& sorts) override {
            bool byText = !text.empty();

            for (uint32_t s : sorts) {
                byText = byText || s == 3 || s == 4;
            }

            bool byQuality = false;
            bool byItem = false;

            for (uint32_t f : args) {
                byQuality = byQuality || (f >= 0x1E && f <= 0x25);
                byItem = byItem || (f >= 3 && f <= 0x1B);
            }

            if (!byText && !byQuality && !byItem) {
                return;
            }

            const int32_t category = static_cast<int32_t>(M().unk144);
            std::vector<uint32_t> queue;
            std::unordered_set<uint32_t> queued;

            for (const uint8_t* e : this->All()) {
                if (!e || HasCategoryOfType(e, TYPE_ITEM_SET) || !IsItemDisplay(e)) {
                    continue;
                }

                if (category > 0 && !InCategory(e, static_cast<uint32_t>(category))) {
                    continue;
                }

                if (byText) {
                    QueueItem(queue, queued, U32(e, 4));
                } else if (!byQuality) {
                    continue;
                }

                if (const std::vector<const uint8_t*>* rows = ItemsForAppearance(U32(e, 0))) {
                    for (const uint8_t* r : *rows) {
                        if (r) {
                            QueueItem(queue, queued, U32(r, 4));
                        }
                    }
                }
            }

            const Deps& d = Dependencies();
            std::unordered_map<uint32_t, ItemInfo> info;

            if (!d.fetchItemInfo || !d.fetchItemInfo(queue.data(), static_cast<uint32_t>(queue.size()), info)) {
                return;
            }

            for (const auto& i : info) {
                s_itemNames[i.first] = CoA::Filter::Lower(i.second.name);
                s_itemQualities[i.first] = i.second.quality;
            }
        }

        // [0] 0x101B5930: every realm-appropriate appearance, less thrown weapons unless enabled.
        std::vector<const uint8_t*> DoPopulate() override {
            const bool* thrownCfg = CoA::Config::Bool("CONFIG_THROWN_WEAPON_APPEARANCES_ENABLED");
            const bool thrown = thrownCfg && *thrownCfg;
            std::vector<const uint8_t*> out;
            CoA::Dbc::Table& t = Appearances();

            for (uint32_t id = t.MinId(); t.Count() && id <= t.MaxId(); ++id) {
                const uint8_t* a = t.Row(id);

                if (!a || !RealmAppropriate(a)) {
                    continue;
                }

                if (!thrown) {
                    bool skip = false;

                    for (uint32_t k = 0; k < 3; ++k) {
                        const uint32_t c = U32(a, 0x14 + k * 4);

                        if (c && CategoryIs(c, TYPE_ITEM)) {
                            ItemRecord item;
                            skip = FetchItem(U32(a, 0xC), item) && item.classId == 2 && item.subclassId == 0x10;
                            break;
                        }
                    }

                    if (skip) {
                        continue;
                    }
                }

                out.push_back(a);
            }

            return out;
        }

        // [2] 0x101B6100
        bool ArgMatch(uint32_t f, const uint8_t* const& a) override {
            static const uint32_t kWeaponSub[17] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 10, 13, 15, 17, 18, 19, 20, 16 };
            static const uint32_t kArmorSub[5] = { 1, 2, 3, 4, 6 };
            auto itemIs = [&](uint32_t cls, uint32_t sub) {   // FUN_101b4730
                ItemRecord item;
                return IsItemDisplay(a) && FetchItem(U32(a, 4), item) && item.classId == cls && item.subclassId == sub;
            };
            auto invIs = [&](uint32_t inv) {   // FUN_101b4770
                ItemRecord item;
                return IsItemDisplay(a) && FetchItem(U32(a, 4), item) && item.inventoryType == inv;
            };
            const uint32_t id = U32(a, 0);

            if (f == 1) {
                return IsCollected(id);
            }

            if (f == 2) {
                return !IsCollected(id);
            }

            if (f >= 3 && f <= 0x13) {
                return itemIs(2, kWeaponSub[f - 3]);
            }

            if (f >= 0x14 && f <= 0x18) {
                return itemIs(4, kArmorSub[f - 0x14]);
            }

            if (f >= 0x19 && f <= 0x1B) {
                return invIs(0x15 + (f - 0x19));
            }

            if (f == 0x1C || f == 0x1D) {
                const uint8_t* d = DetailsFor(id);
                return d && U32(d, f == 0x1C ? 0xC : 0x10) != 0;
            }

            if (f >= 0x1E && f <= 0x25 && IsItemDisplay(a)) {
                if (const std::vector<const uint8_t*>* rows = ItemsForAppearance(id)) {
                    for (const uint8_t* r : *rows) {
                        if (ItemQuality(U32(r, 4)) == static_cast<int>(f - 0x1E)) {
                            return true;
                        }
                    }
                }
            }

            return false;
        }

        // [4] 0x101B6630
        uint32_t ArgGroup(uint32_t f) override {
            if (f == 1 || f == 2) {
                return 1;
            }

            if (f >= 3 && f <= 0x18) {
                return 2;
            }

            if (f == 0x19 || f == 0x1A) {
                return 3;
            }

            if (f == 0x1C || f == 0x1D) {
                return 4;
            }

            if (f >= 0x1E && f <= 0x25) {
                return 5;
            }

            return 0;
        }

        // [3] 0x101B5D10: in the selected category; then the item set's / an alternate item's name.
        bool TextMatch(const std::string& lower, const uint8_t* const& a) override {
            const int32_t category = static_cast<int32_t>(M().unk144);

            if (category < 1 || !InCategory(a, static_cast<uint32_t>(category))) {
                return false;
            }

            if (lower.empty()) {
                return true;
            }

            const uint32_t id = U32(a, 0);

            if (HasCategoryOfType(a, TYPE_ITEM_SET)) {
                for (uint32_t setId : SetsWithAppearance(id)) {
                    ItemSetRecord set;

                    if (FetchItemSet(setId, set) && CoA::Filter::Lower(set.name).find(lower) != std::string::npos) {
                        return true;
                    }
                }

                return false;
            }

            if (const std::vector<const uint8_t*>* rows = ItemsForAppearance(id)) {
                for (const uint8_t* r : *rows) {
                    const std::string name = ItemNameLower(U32(r, 4));

                    if (!name.empty() && name.find(lower) != std::string::npos) {
                        return true;
                    }
                }
            }

            return false;
        }

        // [8] 0x101B66F0: [!item set:1][!collected:1][set incompletion:16][+0x2C:32].
        std::string FilterKey(const uint8_t* const& a) override {
            const uint32_t id = U32(a, 0);
            const bool isSet = HasCategoryOfType(a, TYPE_ITEM_SET);
            uint16_t setKey = 0;
            ItemSetRecord set;

            if (isSet && FetchItemSet(U32(a, 0x20), set)) {
                std::vector<uint32_t> parts;
                ItemSetAppearanceList(set, parts);
                uint32_t total = 0;
                uint32_t have = 0;

                for (uint32_t p : parts) {
                    if (p) {
                        ++total;
                    }

                    if (IsCollected(p)) {
                        ++have;
                    }
                }

                if (total) {
                    setKey = static_cast<uint16_t>(~((have * 0xFFFFu) / total));
                }
            }

            std::string k;
            CoA::Filter::AppendByte(k, isSet ? 0 : 1);
            CoA::Filter::AppendByte(k, IsCollected(id) ? 0 : 1);
            CoA::Filter::AppendByte(k, static_cast<uint8_t>(setKey >> 8));
            CoA::Filter::AppendByte(k, static_cast<uint8_t>(setKey));
            CoA::Filter::AppendU32(k, U32(a, 0x2C));
            return k;
        }

        // [9] 0x101B6B10. Names become big integers byte by byte, so a longer name is larger: the key leads with the
        // length to keep that order.
        std::string SortKey(uint32_t s, const uint8_t* const& a) override {
            const uint32_t id = U32(a, 0);
            std::string k;

            switch (s) {
                case 1: CoA::Filter::AppendU64(k, CollectedTime(id)); break;
                case 2: CoA::Filter::AppendU64(k, ~CollectedTime(id)); break;
                case 3:
                case 4: {
                    const std::string name = CoA::Filter::Lower(AppearanceName(a));
                    CoA::Filter::AppendU32(k, static_cast<uint32_t>(name.size()));

                    for (char c : name) {
                        CoA::Filter::AppendByte(k, s == 3 ? static_cast<uint8_t>(c) : static_cast<uint8_t>(0xFF - static_cast<uint8_t>(c)));
                    }

                    break;
                }
                case 5: CoA::Filter::AppendU32(k, id); break;
                case 6: CoA::Filter::AppendU32(k, ~id); break;
                default: CoA::Filter::AppendU32(k, 0); break;
            }

            return k;
        }
    };

    struct OutfitEntry {
        std::string name;
        std::vector<uint32_t> list;
    };

    // The GlobalStrings NEW_OUTFIT text (DAT_10bdb270): +8 tag, +0xC text.
    const uint8_t* NewOutfitRow() {
        CoA::Dbc::Table& gs = CoA::Dbc::Get("DBFilesClient\\GlobalStrings.dbc");

        for (uint32_t id = gs.MinId(); gs.Count() && id <= gs.MaxId(); ++id) {
            if (const uint8_t* r = gs.Row(id)) {
                if (strcmp(gs.Str(r, 8), "NEW_OUTFIT") == 0) {
                    return r;
                }
            }
        }

        return nullptr;
    }

    std::string NewOutfitText() {
        const uint8_t* r = NewOutfitRow();
        return r ? CoA::Dbc::Get("DBFilesClient\\GlobalStrings.dbc").Str(r, 0xC) : std::string();
    }

    // AppearanceOutfitEntryContainer (vtable 0x10B2A3F8): the NEW_OUTFIT entry, then every outfit.
    class OutfitBrowser : public CoA::Filter::Container<OutfitEntry> {
    protected:
        std::vector<OutfitEntry> DoPopulate() override {   // 0x100CBBC0
            std::vector<OutfitEntry> out;
            out.push_back({ NewOutfitText(), {} });

            for (const auto& o : M().outfits) {
                out.push_back({ o.first, o.second });
            }

            return out;
        }

        bool TextMatch(const std::string& lower, const OutfitEntry& e) override {   // 0x100CC250
            return e.name == NewOutfitText() || CoA::Filter::Lower(e.name).find(lower) != std::string::npos;
        }
    };

    AppearanceBrowser* s_browser = nullptr;   // manager +0x0C
    OutfitBrowser* s_outfitBrowser = nullptr; // manager +0xC8

    AppearanceBrowser& Browser() {
        if (!s_browser) {
            s_browser = new AppearanceBrowser();
        }

        return *s_browser;
    }

    OutfitBrowser& Outfits() {
        if (!s_outfitBrowser) {
            s_outfitBrowser = new OutfitBrowser();
        }

        return *s_outfitBrowser;
    }

    std::vector<std::string> ReadFlagNames(lua_State* L, int idx) {   // FUN_100bed80: {name = true}
        std::map<std::string, bool> map;
        CoA::Script::ToBoolMap(L, idx, map);

        std::vector<std::string> out;

        for (const auto& p : map) {
            if (p.second) {
                out.push_back(p.first);
            }
        }

        return out;
    }

    // FUN_100c67f0: (category, text, {FILTER_* = true}, {SORT_* = true}). A category above 0 filters the appearances; -2
    // filters the outfits.
    int ApplyCategoryFilter(lua_State* L) {
        if (!ValidateInput(L, { LUA_TNUMBER, LUA_TSTRING, LUA_TTABLE, LUA_TTABLE })) {
            return 0;
        }

        const int32_t category = ToInt(CheckNumber(L, 1));
        const char* t = lua_tostring(L, 2);
        const std::string text = t ? t : "";
        const std::vector<std::string> filterNames = ReadFlagNames(L, 3);
        const std::vector<std::string> sortNames = ReadFlagNames(L, 4);

        if (category >= 1) {
            std::vector<uint32_t> filters;
            std::vector<uint32_t> sorts;

            for (const std::string& n : filterNames) {
                for (uint32_t i = 0; i < 38; ++i) {
                    if (n == kFilters[i]) {
                        filters.push_back(i);
                    }
                }
            }

            for (const std::string& n : sortNames) {
                for (uint32_t i = 0; i < 7; ++i) {
                    if (n == kSorts[i]) {
                        sorts.push_back(i);
                    }
                }
            }

            M().unk144 = static_cast<uint32_t>(category);

            if (BrowserDirty()) {
                Browser().Reset();
                BrowserDirty() = false;
            }

            Browser().ApplyFilter(text, filters, sorts);
        } else if (category == -2) {
            M().unk144 = static_cast<uint32_t>(-2);
            Outfits().Reset();
            Outfits().ApplyFilter(text);
        }

        return 0;
    }

    uint32_t MaxPages() {   // FUN_100c3ce0
        const int32_t c = static_cast<int32_t>(M().unk144);

        if (c > 0) {
            return static_cast<uint32_t>((Browser().Size() + kPageSize - 1) / kPageSize);
        }

        if (c == -2) {
            return static_cast<uint32_t>((Outfits().Size() + kPageSize - 1) / kPageSize);
        }

        return 0;
    }

    int GetCategoryMaxPages(lua_State* L) {   // handler_GetCategoryMaxPages
        lua_pushinteger(L, static_cast<int>(MaxPages()));
        return 1;
    }

    // FUN_100c98d0: (page, 1-based) -> 18 appearance ids, or 18 outfit names for -2; nil otherwise.
    int GetCategoryAppearances(lua_State* L) {
        uint32_t page;

        if (!ReadNumber(L, page) || page == 0) {
            return 0;
        }

        const uint32_t first = (page - 1) * kPageSize;
        const int32_t c = static_cast<int32_t>(M().unk144);

        if (c >= 1) {
            std::vector<uint32_t> ids;

            if (page - 1 < MaxPages()) {
                for (uint32_t i = first; i < first + kPageSize && i < Browser().Size(); ++i) {
                    if (const uint8_t* a = Browser().At(i)) {
                        ids.push_back(U32(a, 0));
                    }
                }
            }

            PushIntList(L, ids);
            return 1;
        }

        if (c == -2) {
            std::vector<std::string> names;

            if (page - 1 < MaxPages()) {
                for (uint32_t i = first; i < first + kPageSize && i < Outfits().Size(); ++i) {
                    names.push_back(Outfits().At(i).name);
                }
            }

            lua_createtable(L, 0, static_cast<int>(names.size()));
            lua_checkstack(L, 2);

            for (uint32_t i = 0; i < names.size(); ++i) {
                lua_pushnumber(L, static_cast<double>(i + 1));
                PushStr(L, names[i].c_str());
                lua_settable(L, -3);
            }

            return 1;
        }

        lua_pushnil(L);
        return 1;
    }

    // FUN_100c9c00: (category) -> collected appearances in it, all appearances in it.
    int GetCollectedCount(lua_State* L) {
        if (!ValidateInput(L, { LUA_TNUMBER })) {
            return 0;
        }

        const int32_t category = ToInt(CheckNumber(L, 1));
        auto inCategory = [category](const uint8_t* a) {
            for (uint32_t k = 0; k < 3; ++k) {
                if (I32(a, 0x14 + k * 4) == category) {
                    return true;
                }
            }

            return false;
        };
        int have = 0;
        int total = 0;

        for (const Collected& c : M().collected) {
            if (const uint8_t* a = Appearances().Row(c.id)) {
                if (inCategory(a)) {
                    ++have;
                }
            }
        }

        if (category > 0) {
            CoA::Dbc::Table& t = Appearances();

            for (uint32_t id = t.MinId(); t.Count() && id <= t.MaxId(); ++id) {
                if (const uint8_t* a = t.Row(id)) {
                    if (inCategory(a)) {
                        ++total;
                    }
                }
            }
        }

        lua_pushinteger(L, have);
        lua_pushinteger(L, total);
        return 2;
    }

    void PushStringList(lua_State* L, const std::vector<std::string>& v) {   // FUN_100beb90
        lua_createtable(L, 0, static_cast<int>(v.size()));
        lua_checkstack(L, 2);

        for (uint32_t i = 0; i < v.size(); ++i) {
            lua_pushnumber(L, static_cast<double>(i + 1));
            PushStr(L, v[i].c_str());
            lua_settable(L, -3);
        }
    }

    // FUN_100c85e0: the APPEARANCE_TYPE_* names that have a category the player's class may use, then
    // APPEARANCE_TYPE_OUTFIT.
    int GetAppearanceTypes(lua_State* L) {
        std::vector<std::string> names;
        CoA::Dbc::Table& cats = Categories();

        for (int type = 0; type < 12; ++type) {
            for (uint32_t id = cats.MinId(); cats.Count() && id <= cats.MaxId(); ++id) {
                const uint8_t* c = cats.Row(id);

                if (c && CategoryType(c) == type && CategoryAllowsClass(c)) {
                    names.push_back(kAppearanceTypes[type]);
                    break;
                }
            }
        }

        names.push_back("APPEARANCE_TYPE_OUTFIT");
        PushStringList(L, names);
        return 1;
    }

    // handler_GetCategoryInfo: name (+8), icon (+0x18), type (+4); three nils if unknown.
    int GetCategoryInfo(lua_State* L) {
        uint32_t category;

        if (!ReadNumber(L, category)) {
            return 0;
        }

        const uint8_t* c = Categories().Row(category);

        if (!c) {
            return CoA::Script::PushNils(L, 3);
        }

        PushStr(L, Categories().Str(c, 8));
        PushStr(L, Categories().Str(c, 0x18));
        PushStr(L, Categories().Str(c, 4));
        return 3;
    }

    // FUN_100c92f0: (type name) -> the ids of its categories the class may use, ascending; {-2} for
    // APPEARANCE_TYPE_OUTFIT.
    int GetCategoriesForType(lua_State* L) {
        std::string type;

        if (!ReadString(L, type)) {
            return 0;
        }

        std::vector<int32_t> ids;

        if (type == "APPEARANCE_TYPE_OUTFIT") {
            ids.push_back(-2);
        } else {
            CoA::Dbc::Table& cats = Categories();

            for (uint32_t id = cats.MinId(); cats.Count() && id <= cats.MaxId(); ++id) {
                if (const uint8_t* c = cats.Row(id)) {
                    if (type == cats.Str(c, 4) && CategoryAllowsClass(c)) {
                        ids.push_back(I32(c, 0));
                    }
                }
            }
        }

        std::sort(ids.begin(), ids.end());
        lua_createtable(L, 0, static_cast<int>(ids.size()));
        lua_checkstack(L, 2);

        for (uint32_t i = 0; i < ids.size(); ++i) {
            lua_pushnumber(L, static_cast<double>(i + 1));
            lua_pushinteger(L, ids[i]);
            lua_settable(L, -3);
        }

        return 1;
    }

    // FUN_100c7910: (item guid string) -> false for an unknown item, else CMSG 0x6EA {u64 guid}, true.
    int CollectItemAppearance(lua_State* L) {
        std::string guidText;

        if (!ReadString(L, guidText)) {
            return 0;
        }

        const Deps& d = Dependencies();
        uint64_t itemGuid = 0;

        if (!d.itemGuid || !d.itemGuid(guidText.c_str(), itemGuid)) {
            PushBool(L, false);
            return 1;
        }

        CoA::Packet(0x6EA).U64(itemGuid).Send();
        PushBool(L, true);
        return 1;
    }

    // ---- seasonal ------------------------------------------------------------------------------------------------
    CoA::Dbc::Table& Seasonal() {
        return CoA::Dbc::Get("DBFilesClient/SeasonalAppearances.dbc");
    }

    bool SeasonalAvailable(const uint8_t* r) {
        const CoA::RealmInfo::Info& ri = CoA::RealmInfo::Get();
        return (U32(r, 0x14) && ri.Live()) || (U32(r, 0x18) && ri.Seasonal()) || (U32(r, 0x1C) && ri.League()) || (U32(r, 0x20) && ri.PTR()) ||
            (U32(r, 0x24) && ri.Dev());
    }

    int GetAvailableSeasons(lua_State* L) {   // FUN_100c8dc0: distinct +8, ascending
        std::vector<uint32_t> out;
        CoA::Dbc::Table& t = Seasonal();

        for (uint32_t id = t.MinId(); t.Count() && id <= t.MaxId(); ++id) {
            if (const uint8_t* r = t.Row(id)) {
                if (SeasonalAvailable(r) && std::find(out.begin(), out.end(), U32(r, 8)) == out.end()) {
                    out.push_back(U32(r, 8));
                }
            }
        }

        std::sort(out.begin(), out.end());
        PushIntList(L, out);
        return 1;
    }

    int GetAvailableChapters(lua_State* L) {   // FUN_100c8c10: (season, 0 = any) -> distinct +0xC, ascending
        uint32_t season;

        if (!ReadNumber(L, season)) {
            return 0;
        }

        std::vector<uint32_t> out;
        CoA::Dbc::Table& t = Seasonal();

        for (uint32_t id = t.MinId(); t.Count() && id <= t.MaxId(); ++id) {
            if (const uint8_t* r = t.Row(id)) {
                if (SeasonalAvailable(r) && (season == 0 || U32(r, 8) == season) && std::find(out.begin(), out.end(), U32(r, 0xC)) == out.end()) {
                    out.push_back(U32(r, 0xC));
                }
            }
        }

        std::sort(out.begin(), out.end());
        PushIntList(L, out);
        return 1;
    }

    // FUN_100ca810: (season, chapter; 0 = any) -> distinct appearances ordered by +0x10.
    int GetSeasonalItems(lua_State* L) {
        int32_t season;
        int32_t chapter;

        if (!ReadInt2(L, season, chapter)) {
            return 0;
        }

        std::vector<const uint8_t*> rows;
        CoA::Dbc::Table& t = Seasonal();

        for (uint32_t id = t.MinId(); t.Count() && id <= t.MaxId(); ++id) {
            if (const uint8_t* r = t.Row(id)) {
                if (SeasonalAvailable(r) && (season == 0 || I32(r, 8) == season) && (chapter == 0 || I32(r, 0xC) == chapter)) {
                    rows.push_back(r);
                }
            }
        }

        std::stable_sort(rows.begin(), rows.end(), [](const uint8_t* a, const uint8_t* b) { return U32(a, 0x10) < U32(b, 0x10); });
        std::vector<uint32_t> out;

        for (const uint8_t* r : rows) {
            if (std::find(out.begin(), out.end(), U32(r, 4)) == out.end()) {
                out.push_back(U32(r, 4));
            }
        }

        PushIntList(L, out);
        return 1;
    }

    // ---- C_AppearanceOutfit --------------------------------------------------------------------------------------
    // FUN_100c84d0 -> FUN_100c3a20: the outfit names in the map's iteration order.
    int GetAppearanceOutfits(lua_State* L) {
        std::vector<std::string> names;

        for (const auto& o : M().outfits) {
            names.push_back(o.first);
        }

        PushStringList(L, names);
        return 1;
    }

    // FUN_100ca4b0: (name) -> the outfit's appearances without index 0, as a list (empty if unknown).
    int GetOutfitInfo(lua_State* L) {
        std::string name;

        if (!ReadString(L, name)) {
            return 0;
        }

        std::vector<uint32_t> list;
        auto it = M().outfits.find(name);

        if (it != M().outfits.end()) {
            list = it->second;
        }

        if (!list.empty()) {
            list.erase(list.begin());
        }

        PushIntList(L, list);
        return 1;
    }

    // FUN_100cb580: (name) -- a known outfit becomes the pending set.
    int SetPendingOutfit(lua_State* L) {
        std::string name;

        if (!ReadString(L, name)) {
            return 0;
        }

        auto it = M().outfits.find(name);

        if (it != M().outfits.end() && it->second != M().pending) {
            M().pending = it->second;
            PendingChanged();
        }

        return 0;
    }

    // FUN_100caef0: (name) -- false for the localized "NEW_OUTFIT" name (GlobalStrings +8 tag, +0xC text), else remember
    // the name and the pending set, CMSG 0x69E {str name, u32 n, u32 x n}, true.
    int SaveOutfit(lua_State* L) {
        std::string name;

        if (!ReadString(L, name)) {
            return 0;
        }

        CoA::Dbc::Table& gs = CoA::Dbc::Get("DBFilesClient\\GlobalStrings.dbc");

        for (uint32_t id = gs.MinId(); gs.Count() && id <= gs.MaxId(); ++id) {
            const uint8_t* r = gs.Row(id);

            if (r && strcmp(gs.Str(r, 8), "NEW_OUTFIT") == 0 && name == gs.Str(r, 0xC)) {
                PushBool(L, false);
                return 1;
            }
        }

        M().pendingOutfitName = name;
        M().pendingOutfit = M().pending;
        CoA::Packet p(0x69E);
        p.Str(name.c_str()).U32(static_cast<uint32_t>(M().pendingOutfit.size()));

        for (uint32_t a : M().pendingOutfit) {
            p.U32(a);
        }

        p.Send();
        PushBool(L, true);
        return 1;
    }

    // FUN_100c7ab0: (name) -> CMSG 0x6A0 {str}, true; the answer is SMSG 0x6A1.
    int DeleteOutfit(lua_State* L) {
        std::string name;

        if (!ReadString(L, name)) {
            return 0;
        }

        M().pendingOutfitName = name;
        CoA::Packet(0x6A0).Str(name.c_str()).Send();
        PushBool(L, true);
        return 1;
    }

    // FUN_100c9f70: the first outfit (iteration order) equal to the pending set, else nil.
    int GetCurrentOutfitName(lua_State* L) {
        for (const auto& o : M().outfits) {
            if (o.second == M().pending) {
                if (o.first.empty()) {
                    break;
                }

                PushStr(L, o.first.c_str());
                return 1;
            }
        }

        lua_pushnil(L);
        return 1;
    }

    int IsAppearanceCollected(lua_State* L) {   // handler_IsAppearanceCollected
        uint32_t appearance;

        if (!ReadNumber(L, appearance)) {
            return 0;
        }

        PushBool(L, IsCollected(appearance));
        return 1;
    }

    const CoA::Binding s_bindings[] = {
        COA_NATIVE("C_AppearanceCollection", "IsAppearanceCollected", IsAppearanceCollected),
        COA_NATIVE("C_AppearanceCollection", "GetAppearanceTypes", GetAppearanceTypes),
        COA_NATIVE("C_AppearanceCollection", "GetCategoryInfo", GetCategoryInfo),
        COA_NATIVE("C_AppearanceCollection", "GetCategoriesForType", GetCategoriesForType),
        COA_NATIVE("C_AppearanceCollection", "ApplyCategoryFilter", ApplyCategoryFilter),
        COA_NATIVE("C_AppearanceCollection", "GetCategoryAppearances", GetCategoryAppearances),
        COA_NATIVE("C_AppearanceCollection", "GetCategoryMaxPages", GetCategoryMaxPages),
        COA_NATIVE("C_AppearanceCollection", "GetCollectedCount", GetCollectedCount),
        COA_NATIVE("C_AppearanceCollection", "CollectItemAppearance", CollectItemAppearance),
        COA_NATIVE("C_AppearanceCollection", "GetSeasonalItems", GetSeasonalItems),
        COA_NATIVE("C_AppearanceCollection", "GetAvailableSeasons", GetAvailableSeasons),
        COA_NATIVE("C_AppearanceCollection", "GetAvailableChapters", GetAvailableChapters),
        COA_NATIVE("C_AppearanceOutfit", "GetAppearanceOutfits", GetAppearanceOutfits),
        COA_NATIVE("C_AppearanceOutfit", "GetOutfitInfo", GetOutfitInfo),
        COA_NATIVE("C_AppearanceOutfit", "SetPendingOutfit", SetPendingOutfit),
        COA_NATIVE("C_AppearanceOutfit", "SaveOutfit", SaveOutfit),
        COA_NATIVE("C_AppearanceOutfit", "DeleteOutfit", DeleteOutfit),
        COA_NATIVE("C_AppearanceOutfit", "GetCurrentOutfitName", GetCurrentOutfitName),
    };
}

void CoA::Appearance::detail::InitBrowser() {
    CoA::LuaReg::AddModule(s_bindings, sizeof(s_bindings) / sizeof(s_bindings[0]));
}

void CoA::Appearance::detail::ResetBrowser() {
    delete s_browser;
    s_browser = nullptr;
    delete s_outfitBrowser;
    s_outfitBrowser = nullptr;
    s_itemNames.clear();
    s_itemQualities.clear();
}
