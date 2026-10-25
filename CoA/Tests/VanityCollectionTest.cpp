#include "CoA/Source/Modules/BridgeStore.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Modules/RealmInfo.h"
#include "CoA/Source/Modules/VanityCollection.h"
#include "CoA/Source/Runtime/Dbc.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/MemoryBridge.h"
#include "CoA/Tests/Support.h"
#include "CoA/Tests/TestMain.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <cstring>

namespace {
    // Strings: 1 "Alpha Pet", 11 "Beta Mount", 22 "Gamma", 28 "a pet", 34 "more".
    const std::string kStrings = std::string("Alpha Pet") + '\0' + "Beta Mount" + '\0' + "Gamma" + '\0' + "a pet" + '\0' + "more" + '\0';

    std::vector<uint32_t> VanityRow(uint32_t id, uint32_t item, uint32_t category, uint32_t dp, uint32_t live, uint32_t flags, uint32_t learned) {
        std::vector<uint32_t> row(0x2A, 0);
        row[0] = id;
        row[1] = item;
        row[2] = category;
        row[4] = dp;
        row[7] = live;
        row[0xC] = flags;
        row[0x12] = item + 100;   // contents[0]
        std::vector<uint32_t> description = CoATest::Loc(28);
        std::vector<uint32_t> additional = CoATest::Loc(34);
        row.insert(row.end(), description.begin(), description.end());
        row.insert(row.end(), additional.begin(), additional.end());
        row.push_back(learned);
        return row;
    }

    std::vector<uint32_t> ItemAddonRow(uint32_t rowId, uint32_t item, uint32_t name, uint32_t quality) {
        std::vector<uint32_t> row = { rowId, item };
        std::vector<uint32_t> n = CoATest::Loc(name);
        std::vector<uint32_t> d = CoATest::Loc(0);
        row.insert(row.end(), n.begin(), n.end());
        row.insert(row.end(), d.begin(), d.end());
        std::vector<uint32_t> fields(12, 0);
        fields[0] = quality;
        row.insert(row.end(), fields.begin(), fields.end());
        return row;
    }

    bool ReadTables(const char* path, std::vector<uint8_t>& out) {
        if (strstr(path, "VanityCollection.dbc")) {
            out = CoATest::Wdbc({ VanityRow(1, 5001, 0x100001, 10, 1, 0, 7001), VanityRow(2, 5002, 0x200001, 0, 1, 0x10, 7002),
                VanityRow(3, 5003, 0x100001, 5, 0, 0, 7003) }, kStrings, 0x4C);
            return true;
        }

        if (strstr(path, "ItemAddon.dbc")) {
            out = CoATest::Wdbc({ ItemAddonRow(1, 5001, 1, 3), ItemAddonRow(2, 5002, 11, 4), ItemAddonRow(3, 5003, 22, 2) }, kStrings);
            return true;
        }

        return false;
    }

    void LiveRealm() {
        CoATest::Bytes b;
        b.U32(1).U32(0).F32(0).F32(0).F32(0).U32(0).F32(0).F32(0).U32(0);

        for (int i = 0; i < 8; ++i) {
            b.U8(i == 0 ? 1 : 0);
        }

        b.Str("").Str("Live").U8(0);
        CoATest::Deliver(CoA::RealmInfo::OnRealmInfo, 0x9BC, b);
    }

    lua_State* Setup() {
        CoA::Dbc::SetFileReader(ReadTables);
        CoA::MemoryBridge::Reset();
        CoA::BridgeStore::Reset();
        CoA::BridgeStore::LoadTables();
        CoA::BridgeStore::BuildIndexes();
        CoA::Dbc::SetFileReader(nullptr);
        CoA::RealmInfo::Reset();
        LiveRealm();
        CoA::VanityCollection::Reset();
        CoA::LuaReg::Reset();
        CoA::VanityCollection::Init();
        return CoATest::NewState(true);
    }

    // Runs chunk and returns its results as strings.
    std::vector<std::string> Run(lua_State* L, const char* chunk) {
        std::vector<std::string> out;
        int base = lua_gettop(L);

        if (luaL_loadstring(L, chunk) != 0 || lua_pcall(L, 0, LUA_MULTRET, 0) != 0) {
            out.push_back(std::string("<error> ") + lua_tostring(L, -1));
            lua_settop(L, base);
            return out;
        }

        for (int i = base + 1; i <= lua_gettop(L); ++i) {
            out.push_back(lua_isnil(L, i) ? "nil" : lua_isboolean(L, i) ? (lua_toboolean(L, i) ? "true" : "false") : lua_tostring(L, i));
        }

        lua_settop(L, base);
        return out;
    }
}

COA_TEST(Vanity_OwnedAfterInfoPacket) {
    lua_State* L = Setup();
    CoATest::Bytes list;
    list.U32(1).U32(5002);
    CoATest::Deliver(CoA::VanityCollection::OnList, 0x6F7, list);
    CHECK(CoATest::Call(L, "C_VanityCollection", "IsCollectionItemOwned", { "n:5002" }) == std::vector<std::string>{ "true" });
    CHECK(CoATest::Call(L, "", "IsCollectionItemOwned", { "n:5001" }) == std::vector<std::string>{ "false" });
    CoATest::Bytes add;
    add.U32(5001);
    CoATest::Deliver(CoA::VanityCollection::OnAdd, 0x6F8, add);
    CHECK(CoATest::Call(L, "C_VanityCollection", "IsOwned", { "n:5001" }) == std::vector<std::string>{ "true" });
    CoATest::Bytes remove;
    remove.U32(5002);
    CoATest::Deliver(CoA::VanityCollection::OnRemove, 0x6F9, remove);
    CHECK(CoATest::Call(L, "C_VanityCollection", "IsOwned", { "n:5002" }) == std::vector<std::string>{ "false" });
    lua_close(L);
}

COA_TEST(Vanity_QueryItemsFilters) {
    lua_State* L = Setup();
    // Item 5003 is not offered on a live realm.
    CHECK(Run(L, "local r = C_VanityCollection.QueryItems() return r.total, #r.items") == (std::vector<std::string>{ "2", "2" }));
    CHECK(Run(L, "local r = C_VanityCollection.QueryItems('BETA') return r.total, r.items[1].itemID, r.items[1].name") ==
        (std::vector<std::string>{ "1", "5002", "Beta Mount" }));
    // Purchasable (4): only 5001 has a price.
    CHECK(Run(L, "local r = C_VanityCollection.QueryItems(nil, nil, 4) return r.total, r.items[1].dpCost") == (std::vector<std::string>{ "1", "10" }));
    // Category 0x200000 group.
    CHECK(Run(L, "local r = C_VanityCollection.QueryItems(nil, 0x200001) return r.total, r.items[1].itemid") == (std::vector<std::string>{ "1", "5002" }));
    // Name descending, one per page from offset 1.
    CHECK(Run(L, "local r = C_VanityCollection.QueryItems(nil, nil, nil, 2, 1, 1) return r.total, #r.items, r.items[1].name") ==
        (std::vector<std::string>{ "2", "1", "Alpha Pet" }));
    // Owned (1) after the list packet.
    CoATest::Bytes list;
    list.U32(1).U32(5001);
    CoATest::Deliver(CoA::VanityCollection::OnList, 0x6F7, list);
    CHECK(Run(L, "local r = C_VanityCollection.QueryItems(nil, nil, 1) return r.total, r.items[1].owned") == (std::vector<std::string>{ "1", "true" }));
    lua_close(L);
}

COA_TEST(Vanity_ItemLookups) {
    lua_State* L = Setup();
    CHECK(Run(L, "local i = C_VanityCollection.GetItem(5001) return i.id, i.description, i.additionalText, i.contentsPreview[1], i.quality") ==
        (std::vector<std::string>{ "1", "a pet", "more", "5101", "3" }));
    CHECK(Run(L, "return C_VanityCollection.GetItemByLearnedSpell(7002).itemID") == std::vector<std::string>{ "5002" });
    CHECK(Run(L, "return C_VanityCollection.GetOwnerByContentItem(5101).itemID") == std::vector<std::string>{ "5001" });
    CHECK(Run(L, "local s = C_VanityCollection.GetSeasonalShowcaseItems() return #s, s[1].itemID") == (std::vector<std::string>{ "1", "5002" }));
    CHECK(Run(L, "return #C_VanityCollection.GetAllItems()") == std::vector<std::string>{ "2" });
    CHECK(Run(L, "return C_VanityCollection.GetDPPrice(5001), C_VanityCollection.GetVPPrice(5001), C_VanityCollection.GetDPPrice(9)") ==
        (std::vector<std::string>{ "10", "0", "0" }));
    CHECK(Run(L, "return C_VanityCollection.GetItem(5003)") == std::vector<std::string>{});
    lua_close(L);
}

COA_TEST(Vanity_PurchaseSendsRecon) {
    lua_State* L = Setup();
    CoATest::RecordPackets();
    CoATest::Call(L, "C_VanityCollection", "Purchase", { "n:5001", "n:1" });
    CHECK(CoATest::Call(L, "C_VanityCollection", "PurchaseWebShopItem", { "n:5002" }) == std::vector<std::string>{ "true" });
    CoATest::Call(L, "C_VanityCollection", "RequestDelivery", { "n:5001" });
    CoATest::Call(L, "C_VanityCollection", "PurchaseCollectionItem", { "n:5002", "n:0" });
    // One argument is not enough.
    CoATest::Call(L, "C_VanityCollection", "Purchase", { "n:5001" });
    CHECK(CoATest::Call(L, "C_VanityCollection", "IsPurchaseInProgress") == std::vector<std::string>{});
    CoATest::StopRecordingPackets();
    std::vector<std::vector<uint8_t>> expected = {
        CoATest::Expected(0x523, CoATest::Bytes().U8(1).U32(5001)),
        CoATest::Expected(0x523, CoATest::Bytes().U8(2).U32(5002)),
        CoATest::Expected(0x523, CoATest::Bytes().U8(2).U32(5001)),
        CoATest::Expected(0x523, CoATest::Bytes().U8(0).U32(5002)),
    };
    CHECK(CoATest::SentPackets() == expected);
    lua_close(L);
}
