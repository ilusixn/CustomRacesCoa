// C_CustomStore (CustomStore.cpp): the query / purchase CMSGs byte-exact, SMSG 0x6BA / 0x6BC / 0x6BD and their events,
// every QUERY_ / PURCHASE_ result code (the money comparison kept the original's way round), the filter container's
// argument groups and text, pages, the glue reset and short packets.
#include "CoA/Source/Modules/BridgeStore.h"
#include "CoA/Source/Modules/CustomStore.h"
#include "CoA/Source/Modules/GameEvents.h"
#include "CoA/Source/Modules/VanityCollection.h"
#include "CoA/Source/Runtime/Dbc.h"
#include "CoA/Source/Runtime/Lifecycle.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/MemoryBridge.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Tests/Support.h"
#include "CoA/Tests/TestMain.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace {
    using V = std::vector<std::string>;
    using Packets = std::vector<std::vector<uint8_t>>;

    // Strings: 1 "Red Sword", 11 "Blue Hat", 20 "Green Ring".
    const std::string kStrings = std::string("Red Sword") + '\0' + "Blue Hat" + '\0' + "Green Ring" + '\0';

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
        if (strstr(path, "ItemAddon.dbc")) {
            out = CoATest::Wdbc({ ItemAddonRow(1, 9001, 1, 4), ItemAddonRow(2, 9002, 11, 2), ItemAddonRow(3, 9003, 20, 4) }, kStrings);
            return true;
        }

        return false;
    }

    bool s_player = true;
    uint32_t s_money = 0;
    std::map<uint32_t, uint32_t> s_held;
    std::map<uint32_t, bool> s_nearby;   // entry -> in range
    std::map<uint32_t, bool> s_achievements;

    lua_State* Start() {
        CoA::Dbc::SetFileReader(ReadTables);
        CoA::MemoryBridge::Reset();
        CoA::BridgeStore::Reset();
        CoA::BridgeStore::LoadTables();
        CoA::BridgeStore::BuildIndexes();
        CoA::Dbc::SetFileReader(nullptr);
        CoA::GameEvents::Reset();
        CoA::VanityCollection::Reset();
        CoA::LuaReg::Reset();
        CoA::Packets::Clear();
        CoA::Lifecycle::Reset();
        CoA::CustomStore::Reset();
        CoA::CustomStore::Init();
        s_player = true;
        s_money = 0;
        s_held.clear();
        s_nearby.clear();
        s_achievements.clear();
        CoA::CustomStore::Deps& d = CoA::CustomStore::Dependencies();
        d.hasPlayer = [] { return s_player; };
        d.playerMoney = [] { return s_money; };
        d.heldCount = [](uint32_t item) { return s_held[item]; };
        d.nearby = [](uint32_t entry, bool go) { return s_nearby[entry + (go ? 1000000u : 0u)]; };
        d.achievementDone = [](uint32_t id) { return s_achievements[id]; };
        return CoATest::NewState(false);
    }

    void Stop(lua_State* L) {
        lua_close(L);
        CoATest::StopRecordingPackets();
        CoATest::StopRecordingSignals();
        CoA::CustomStore::Reset();
        CoA::Lifecycle::Reset();
        CoA::Packets::Clear();
        CoA::GameEvents::Reset();
        CoA::VanityCollection::Reset();
        CoA::BridgeStore::Reset();
    }

    V Dispatch(uint32_t opcode, CoATest::Bytes& b) {
        CoATest::RecordSignals();
        CDataStore msg(b.data.data(), static_cast<uint32_t>(b.data.size()));
        int32_t result = 0;
        CoA::Packets::Dispatch(static_cast<int32_t>(opcode), 0, &msg, result);
        CoATest::StopRecordingSignals();
        return CoATest::Signals();
    }

    V Run(lua_State* L, const std::string& chunk) {
        V out;
        const int base = lua_gettop(L);

        if (luaL_loadstring(L, chunk.c_str()) != 0 || lua_pcall(L, 0, LUA_MULTRET, 0) != 0) {
            out.push_back(std::string("<error> ") + lua_tostring(L, -1));
            lua_settop(L, base);
            return out;
        }

        for (int i = base + 1; i <= lua_gettop(L); ++i) {
            out.push_back(lua_isnil(L, i) ? "nil" : lua_isboolean(L, i) ? (lua_toboolean(L, i) ? "true" : "false")
                : lua_istable(L, i) ? "table" : lua_tostring(L, i));
        }

        lua_settop(L, base);
        return out;
    }

    Packets Sent(lua_State* L, const std::string& chunk, V* results = nullptr) {
        CoATest::RecordPackets();
        V r = Run(L, chunk);
        CoATest::StopRecordingPackets();

        if (results) {
            *results = r;
        }

        return CoATest::SentPackets();
    }

    struct StoreItem {
        uint32_t id, type, item, money;
        std::vector<uint32_t> tokens, counts;
        uint32_t gameEvent, achievement;
    };

    void PutItem(CoATest::Bytes& b, const StoreItem& it) {
        b.U32(it.id).U32(it.type).U32(it.item).U32(it.money);

        for (int k = 0; k < 5; ++k) {
            b.U32(k < static_cast<int>(it.tokens.size()) ? it.tokens[k] : 0);
        }

        for (int k = 0; k < 5; ++k) {
            b.U32(k < static_cast<int>(it.counts.size()) ? it.counts[k] : 0);
        }

        b.U32(it.gameEvent).U32(it.achievement);
    }

    V Query(const char* result, const std::vector<StoreItem>& items) {
        CoATest::Bytes b;
        b.Str(result).U32(static_cast<uint32_t>(items.size()));

        for (const StoreItem& it : items) {
            PutItem(b, it);
        }

        return Dispatch(0x6BA, b);
    }

    struct StoreType {
        uint32_t id, creature, go;
        const char* first;
        const char* second;
    };

    void Types(const std::vector<StoreType>& types) {
        CoATest::Bytes b;
        b.U32(static_cast<uint32_t>(types.size()));

        for (const StoreType& t : types) {
            b.U32(t.id).U32(t.creature).U32(t.go).Str(t.first).Str(t.second);
        }

        Dispatch(0x6BD, b);
    }

    void Event(uint16_t id, bool active) {
        CoATest::Bytes b;
        uint8_t raw[0x34] = {};
        memcpy(raw, &id, 2);
        raw[0x13] = active ? 1 : 0;
        b.Raw(raw, sizeof(raw));
        CoATest::Deliver(CoA::GameEvents::OnGameEventInfo, 0x9BD, b);
    }

    V CanBuy(lua_State* L, uint32_t id, uint32_t count) {
        return Run(L, "return C_CustomStore.CanPurchaseCustomStoreItem(" + std::to_string(id) + ", " + std::to_string(count) + ")");
    }

    V Code(const char* name) {
        return V{ "false", std::string("PURCHASE_CUSTOM_STORE_ITEM_") + name };
    }

    // The filtered item ids of page 1 (page size as last applied).
    std::string Page(lua_State* L, int page = 1) {
        const V r = Run(L, "local t = C_CustomStore.GetCustomStoreData(" + std::to_string(page) + ") local s = '' "
            "for i = 1, #t do s = s .. (i > 1 and ',' or '') .. t[i] end return s");
        return r.empty() ? "<none>" : r[0];
    }

    std::string Filter(lua_State* L, const std::string& text, const std::string& filters, int size = 0) {
        Run(L, "C_CustomStore.ApplyCustomStoreFilter('" + text + "', {" + filters + "}, {}, " + std::to_string(size) + ")");
        return Page(L);
    }
}

// QueryCustomStore / PurchaseCustomStoreItem: CMSG 0x6B9 {u32 store} / 0x6BB {u32 item, u32 count}, true; wrong
// arguments send nothing and return nothing.
COA_TEST(CustomStore_QueryAndPurchasePackets) {
    lua_State* L = Start();
    V r;
    CHECK(Sent(L, "return C_CustomStore.QueryCustomStore(77)", &r) == Packets{ CoATest::Expected(0x6B9, CoATest::Bytes().U32(77)) });
    CHECK(r == V{ "true" });
    CHECK(Sent(L, "return C_CustomStore.PurchaseCustomStoreItem(5, 3)", &r) ==
        Packets{ CoATest::Expected(0x6BB, CoATest::Bytes().U32(5).U32(3)) });
    CHECK(r == V{ "true" });
    CHECK(Sent(L, "return C_CustomStore.PurchaseCustomStoreItem(-1, 2.9)", &r) ==
        Packets{ CoATest::Expected(0x6BB, CoATest::Bytes().U32(0xFFFFFFFF).U32(2)) });
    CHECK(Sent(L, "return select('#', C_CustomStore.QueryCustomStore('x'))", &r).empty());
    CHECK(r == V{ "0" });
    CHECK(Sent(L, "return select('#', C_CustomStore.PurchaseCustomStoreItem(5))", &r).empty());
    CHECK(r == V{ "0" });
    Stop(L);
}

// SMSG 0x6BC: PURCHASE_CUSTOM_STORE_ITEM_RESULT("%s").
COA_TEST(CustomStore_PurchaseResult) {
    lua_State* L = Start();
    CoATest::Bytes b;
    b.Str("PURCHASE_CUSTOM_STORE_ITEM_OK");
    CHECK(Dispatch(0x6BC, b) == V{ "PURCHASE_CUSTOM_STORE_ITEM_RESULT:PURCHASE_CUSTOM_STORE_ITEM_OK" });
    CoATest::Bytes e;
    CHECK(Dispatch(0x6BC, e) == V{ "PURCHASE_CUSTOM_STORE_ITEM_RESULT:" });
    Stop(L);
}

// SMSG 0x6BD replaces the store types; GetCustomStoreTypeInfo is (second, first | nil).
COA_TEST(CustomStore_TypesAndTypeInfo) {
    lua_State* L = Start();
    Types({ { 1, 10, 0, "Vendor", "Bob's Shop" }, { 2, 0, 20, "", "Chest" } });
    CHECK(Run(L, "return C_CustomStore.GetCustomStoreTypeInfo(1)") == (V{ "Bob's Shop", "Vendor" }));
    CHECK(Run(L, "return C_CustomStore.GetCustomStoreTypeInfo(2)") == (V{ "Chest", "nil" }));
    CHECK(Run(L, "return C_CustomStore.GetCustomStoreTypeInfo(3)") == (V{ "nil", "nil" }));
    CHECK(Run(L, "return select('#', C_CustomStore.GetCustomStoreTypeInfo('x'))") == V{ "0" });
    Types({ { 3, 0, 0, "A", "B" } });
    CHECK(Run(L, "return C_CustomStore.GetCustomStoreTypeInfo(1)") == (V{ "nil", "nil" }));
    CHECK(Run(L, "return C_CustomStore.GetCustomStoreTypeInfo(3)") == (V{ "B", "A" }));
    Stop(L);
}

// SMSG 0x6BA: the items are cleared on every result and loaded only on QUERY_CUSTOM_STORE_OK; the event carries the
// string. GetCustomStoreItemInfo: item, money, the paired token items / counts, event | nil, achievement | nil.
COA_TEST(CustomStore_QueryResultAndItemInfo) {
    lua_State* L = Start();
    CHECK(Query("QUERY_CUSTOM_STORE_OK", { { 5, 1, 9001, 250, { 7, 0, 8, 9 }, { 2, 3, 0, 4 }, 0, 0 },
        { 6, 1, 9002, 0, {}, {}, 33, 44 } }) == V{ "QUERY_CUSTOM_STORE_RESULT:QUERY_CUSTOM_STORE_OK" });
    const std::string dump = "local function arr(t) local s = '' for i = 1, #t do s = s .. (i > 1 and ',' or '') .. t[i] end return s end "
        "local a, b, c, d, e, f = C_CustomStore.GetCustomStoreItemInfo(";
    CHECK(Run(L, dump + "5) return a, b, arr(c), arr(d), e, f") == (V{ "9001", "250", "7,9", "2,4", "nil", "nil" }));
    CHECK(Run(L, dump + "6) return a, b, arr(c), arr(d), e, f") == (V{ "9002", "0", "", "", "33", "44" }));
    CHECK(Run(L, "return select('#', C_CustomStore.GetCustomStoreItemInfo(7)), C_CustomStore.GetCustomStoreItemInfo(7)") ==
        (V{ "6", "nil", "nil", "nil", "nil", "nil", "nil" }));
    CHECK(Run(L, "return select('#', C_CustomStore.GetCustomStoreItemInfo())") == V{ "0" });

    // A failure result clears the items without reading any.
    CHECK(Query("QUERY_CUSTOM_STORE_NOT_FOUND", { { 8, 1, 9001, 1, {}, {}, 0, 0 } }) ==
        V{ "QUERY_CUSTOM_STORE_RESULT:QUERY_CUSTOM_STORE_NOT_FOUND" });
    CHECK(Run(L, "return (C_CustomStore.GetCustomStoreItemInfo(5)), (C_CustomStore.GetCustomStoreItemInfo(8))") == (V{ "nil", "nil" }));

    // An unknown string: cleared, logged, still signalled.
    Query("QUERY_CUSTOM_STORE_OK", { { 5, 1, 9001, 250, {}, {}, 0, 0 } });
    CHECK(Query("SOMETHING", { { 9, 1, 9001, 1, {}, {}, 0, 0 } }) == V{ "QUERY_CUSTOM_STORE_RESULT:SOMETHING" });
    CHECK(Run(L, "return (C_CustomStore.GetCustomStoreItemInfo(5)), (C_CustomStore.GetCustomStoreItemInfo(9))") == (V{ "nil", "nil" }));
    Stop(L);
}

// CanQueryCustomStore: an item whose id equals the queried id must exist (else BAD_STORE), then the type with that id
// (NOT_FOUND), then its creature / game object in range.
COA_TEST(CustomStore_CanQuery) {
    lua_State* L = Start();
    auto can = [&](uint32_t id) { return Run(L, "return C_CustomStore.CanQueryCustomStore(" + std::to_string(id) + ")"); };
    CHECK(can(1) == (V{ "false", "QUERY_CUSTOM_STORE_BAD_STORE" }));
    Query("QUERY_CUSTOM_STORE_OK", { { 1, 9, 9001, 0, {}, {}, 0, 0 }, { 2, 9, 9001, 0, {}, {}, 0, 0 },
        { 3, 9, 9001, 0, {}, {}, 0, 0 }, { 4, 9, 9001, 0, {}, {}, 0, 0 } });
    CHECK(can(1) == (V{ "false", "QUERY_CUSTOM_STORE_NOT_FOUND" }));
    CHECK(can(9) == (V{ "false", "QUERY_CUSTOM_STORE_BAD_STORE" }));   // a type id, but no item carries it as its id
    Types({ { 1, 0, 0, "", "" }, { 2, 50, 0, "", "" }, { 3, 0, 60, "", "" }, { 4, 50, 60, "", "" } });
    CHECK(can(1) == (V{ "true", "nil" }));
    CHECK(can(2) == (V{ "false", "QUERY_CUSTOM_STORE_NPC_NOT_IN_RANGE" }));
    CHECK(can(3) == (V{ "false", "QUERY_CUSTOM_STORE_GO_NOT_IN_RANGE" }));
    s_nearby[50] = true;
    CHECK(can(2) == (V{ "true", "nil" }));
    CHECK(can(4) == (V{ "false", "QUERY_CUSTOM_STORE_GO_NOT_IN_RANGE" }));
    s_nearby[1000060] = true;
    CHECK(can(4) == (V{ "true", "nil" }));
    s_nearby[50] = false;
    CHECK(can(4) == (V{ "false", "QUERY_CUSTOM_STORE_NPC_NOT_IN_RANGE" }));
    CHECK(Run(L, "return select('#', C_CustomStore.CanQueryCustomStore())") == V{ "0" });
    Stop(L);
}

// CanPurchaseCustomStoreItem: every code in the original's order; the money check fires when the cost is BELOW the
// player's money (kept) and only with a player.
COA_TEST(CustomStore_CanPurchase) {
    lua_State* L = Start();
    CHECK(CanBuy(L, 1, 1) == Code("NOT_FOUND"));
    Query("QUERY_CUSTOM_STORE_OK", { { 1, 7, 9001, 100, { 500 }, { 3 }, 0, 0 }, { 2, 8, 9001, 0, {}, {}, 0, 0 },
        { 3, 9, 9001, 0, {}, {}, 0, 0 }, { 4, 1, 9001, 0x40000000, {}, {}, 0, 0 }, { 5, 1, 9001, 0, { 600 }, { 0x40000000 }, 0, 0 },
        { 6, 1, 9001, 0, { 0, 700 }, { 5, 0 }, 12, 0 }, { 7, 1, 9001, 0, {}, {}, 0, 99 }, { 8, 1, 9001, 0x3FFFFFFF, {}, {}, 0, 0 } });
    CHECK(CanBuy(L, 1, 1) == Code("STORE_NOT_FOUND"));
    Types({ { 1, 0, 0, "", "" }, { 7, 0, 0, "", "" }, { 8, 40, 0, "", "" }, { 9, 0, 41, "", "" } });
    CHECK(CanBuy(L, 2, 1) == Code("NPC_NOT_IN_RANGE"));
    CHECK(CanBuy(L, 3, 1) == Code("GO_NOT_IN_RANGE"));
    s_nearby[40] = true;
    s_nearby[1000041] = true;
    CHECK(CanBuy(L, 2, 1) == (V{ "true", "nil" }));
    CHECK(CanBuy(L, 3, 1) == (V{ "true", "nil" }));
    CHECK(CanBuy(L, 1, 0) == Code("COUNT_TOO_LOW"));
    CHECK(CanBuy(L, 4, 1) == (V{ "true", "nil" }));
    CHECK(CanBuy(L, 4, 2) == Code("COUNT_MONEY_OUT_OF_RANGE"));   // 0x7FFFFFFF / 2 < 0x40000000
    CHECK(CanBuy(L, 8, 2) == (V{ "true", "nil" }));               // 0x3FFFFFFF is not below the limit

    // Money: 100 x 2 = 200 < 300 -> NOT_ENOUGH_MONEY; 200 >= 200 passes; no player skips the check.
    s_held[500] = 6;
    s_money = 300;
    CHECK(CanBuy(L, 1, 2) == Code("NOT_ENOUGH_MONEY"));
    s_money = 200;
    CHECK(CanBuy(L, 1, 2) == (V{ "true", "nil" }));
    s_money = 0;
    CHECK(CanBuy(L, 1, 2) == (V{ "true", "nil" }));
    s_money = 1000;
    s_player = false;
    CHECK(CanBuy(L, 1, 2) == (V{ "true", "nil" }));
    s_player = true;
    s_money = 0;

    // Tokens: count x n held, out of range per count.
    CHECK(CanBuy(L, 1, 3) == Code("NOT_ENOUGH_ITEMS"));   // 3 x 3 = 9 > 6
    s_held[500] = 9;
    CHECK(CanBuy(L, 1, 3) == (V{ "true", "nil" }));
    CHECK(CanBuy(L, 5, 1) == Code("NOT_ENOUGH_ITEMS"));
    CHECK(CanBuy(L, 5, 2) == Code("COUNT_ITEMS_OUT_OF_RANGE"));

    // An unpaired token slot is skipped; then the game event, then the achievement.
    CHECK(CanBuy(L, 6, 1) == Code("GAME_EVENT"));
    Event(12, true);
    CHECK(CanBuy(L, 6, 1) == (V{ "true", "nil" }));
    CHECK(CanBuy(L, 7, 1) == Code("ACHIEVEMENT"));
    s_achievements[99] = true;
    CHECK(CanBuy(L, 7, 1) == (V{ "true", "nil" }));
    CHECK(Run(L, "return select('#', C_CustomStore.CanPurchaseCustomStoreItem(1))") == V{ "0" });
    Stop(L);
}

// IsItemLockedDueToAchievement / GameEvent: an item with the condition set and not met.
COA_TEST(CustomStore_Locks) {
    lua_State* L = Start();
    Query("QUERY_CUSTOM_STORE_OK", { { 1, 1, 9001, 0, {}, {}, 12, 99 }, { 2, 1, 9001, 0, {}, {}, 0, 0 } });
    CHECK(Run(L, "return C_CustomStore.IsItemLockedDueToAchievement(1), C_CustomStore.IsItemLockedDueToGameEvent(1)") == (V{ "true", "true" }));
    CHECK(Run(L, "return C_CustomStore.IsItemLockedDueToAchievement(2), C_CustomStore.IsItemLockedDueToGameEvent(2)") == (V{ "false", "false" }));
    CHECK(Run(L, "return C_CustomStore.IsItemLockedDueToAchievement(3), C_CustomStore.IsItemLockedDueToGameEvent(3)") == (V{ "false", "false" }));
    s_achievements[99] = true;
    Event(12, true);
    CHECK(Run(L, "return C_CustomStore.IsItemLockedDueToAchievement(1), C_CustomStore.IsItemLockedDueToGameEvent(1)") == (V{ "false", "false" }));
    CHECK(Run(L, "return select('#', C_CustomStore.IsItemLockedDueToAchievement('x')), select('#', C_CustomStore.IsItemLockedDueToGameEvent())") ==
        (V{ "0", "0" }));
    Stop(L);
}

// ApplyCustomStoreFilter: the name text, the FILTER_* argument groups (any of a group, every group), FILTER_NONE
// rejecting all, false / unknown names ignored; the skill-card filters through Deps.
COA_TEST(CustomStore_Filters) {
    lua_State* L = Start();
    Query("QUERY_CUSTOM_STORE_OK", { { 1, 1, 9001, 100, {}, {}, 0, 0 }, { 2, 1, 9002, 0, { 500 }, { 2 }, 0, 0 },
        { 3, 1, 9003, 50, {}, {}, 0, 0 }, { 4, 1, 9999, 0, {}, {}, 0, 0 } });
    CHECK(Page(L) == "");   // nothing until a filter is applied
    CHECK(Filter(L, "", "") == "1,2,3,4");
    CHECK(Filter(L, "RED", "") == "1");
    CHECK(Filter(L, "ring", "") == "3");
    CHECK(Filter(L, "e", "") == "1,2,3");   // 9999 has no ItemAddon record
    CHECK(Filter(L, "$x1", "") == "");
    s_money = 60;
    CHECK(Filter(L, "", "FILTER_CAN_AFFORD = true") == "3,4");
    CHECK(Filter(L, "", "FILTER_CANNOT_AFFORD = true") == "1,2");
    s_held[500] = 2;
    CHECK(Filter(L, "", "FILTER_CAN_AFFORD = true") == "2,3,4");
    s_player = false;
    CHECK(Filter(L, "", "FILTER_CAN_AFFORD = true") == "1,2,3,4");
    s_player = true;
    CHECK(Filter(L, "", "FILTER_QUALITY_EPIC = true") == "1,3");
    CHECK(Filter(L, "", "FILTER_QUALITY_EPIC = true, FILTER_QUALITY_UNCOMMON = true") == "1,2,3");
    CHECK(Filter(L, "", "FILTER_QUALITY_EPIC = true, FILTER_CAN_AFFORD = true") == "3");
    CHECK(Filter(L, "", "FILTER_QUALITY_EPIC = false") == "1,2,3,4");
    CHECK(Filter(L, "", "FILTER_BOGUS = true") == "1,2,3,4");
    CHECK(Filter(L, "", "FILTER_NONE = true") == "");

    CoATest::Bytes owned;
    owned.U32(1).U32(9002);
    CoATest::Deliver(CoA::VanityCollection::OnList, 0x6F7, owned);
    CHECK(Filter(L, "", "FILTER_VANITY_OWNED = true") == "2");
    CHECK(Filter(L, "", "FILTER_VANITY_UNOWNED = true") == "1,3,4");

    // Skill cards (AscSkillCard through Deps): none without the Dep.
    CHECK(Filter(L, "", "FILTER_SKILL_CARD_UNOWNED = true") == "");
    CoA::CustomStore::Deps& d = CoA::CustomStore::Dependencies();
    d.skillCardByItem = [](uint32_t item, CoA::CustomStore::SkillCard& c) {
        if (item != 9001 && item != 9003) {
            return false;
        }

        c = { item, 2, item == 9001 ? 7u : 9u, item + 1 };
        return true;
    };
    CHECK(Filter(L, "", "FILTER_SKILL_CARD_UNOWNED = true") == "1,3");
    CHECK(Filter(L, "", "FILTER_SKILL_CARD_OWNED = true") == "");
    d.skillCardCollectedAtLeast = [](uint32_t card, uint32_t rank) { return card == 9003 && rank == 2; };
    CHECK(Filter(L, "", "FILTER_SKILL_CARD_OWNED = true") == "3");
    CHECK(Filter(L, "", "FILTER_SKILL_CARD_QUALITY_RARE = true") == "");
    d.skillCardQualityIndex = [](uint32_t q) { return q == 7 ? 2 : q == 9 ? 4 : -1; };
    CHECK(Filter(L, "", "FILTER_SKILL_CARD_QUALITY_RARE = true") == "1");
    CHECK(Filter(L, "", "FILTER_SKILL_CARD_QUALITY_LEGENDARY = true") == "3");
    CHECK(Filter(L, "", "FILTER_SKILL_CARD_SPELL_FAMILY_GENERIC = true") == "");
    d.spellFamily = [](uint32_t spell, uint32_t& family) { family = spell == 9002 ? 0u : 15u; return true; };
    CHECK(Filter(L, "", "FILTER_SKILL_CARD_SPELL_FAMILY_GENERIC = true") == "1");
    CHECK(Filter(L, "", "FILTER_SKILL_CARD_SPELL_FAMILY_DEATH_KNIGHT = true") == "3");
    CHECK(Filter(L, "", "FILTER_SKILL_CARD_SPELL_FAMILY_MAGE = true") == "");
    CHECK(Run(L, "return select('#', C_CustomStore.ApplyCustomStoreFilter('', {}, {}))") == V{ "0" });
    Stop(L);
}

// Pages: the page size from the filter call (0 means 18), GetCustomStoreData(page) and GetCustomStoreMaxPages.
COA_TEST(CustomStore_Pages) {
    lua_State* L = Start();
    std::vector<StoreItem> items;

    for (uint32_t i = 1; i <= 20; ++i) {
        items.push_back({ i, 1, 9001, 0, {}, {}, 0, 0 });
    }

    Query("QUERY_CUSTOM_STORE_OK", items);
    CHECK(Run(L, "return C_CustomStore.GetCustomStoreMaxPages()") == V{ "0" });
    Filter(L, "", "");
    CHECK(Page(L, 1) == "1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18");
    CHECK(Page(L, 2) == "19,20");
    CHECK(Page(L, 3) == "");
    CHECK(Run(L, "return C_CustomStore.GetCustomStoreMaxPages()") == V{ "2" });
    Filter(L, "", "", 7);
    CHECK(Page(L, 3) == "15,16,17,18,19,20");
    CHECK(Run(L, "return C_CustomStore.GetCustomStoreMaxPages()") == V{ "3" });
    Filter(L, "", "", 5);
    CHECK(Run(L, "return C_CustomStore.GetCustomStoreMaxPages()") == V{ "4" });
    CHECK(Run(L, "return select('#', C_CustomStore.GetCustomStoreData(0)), select('#', C_CustomStore.GetCustomStoreData('x'))") ==
        (V{ "0", "0" }));

    // The glue screen: types, items, the view and the page size reset.
    Types({ { 1, 0, 0, "a", "b" } });
    CoA::Lifecycle::RunGlueScreen();
    CHECK(Run(L, "return (C_CustomStore.GetCustomStoreTypeInfo(1)), (C_CustomStore.GetCustomStoreItemInfo(1))") == (V{ "nil", "nil" }));
    CHECK(Page(L, 1) == "");
    Query("QUERY_CUSTOM_STORE_OK", items);
    Run(L, "C_CustomStore.ApplyCustomStoreFilter('', {}, {}, 0)");
    CHECK(Run(L, "return C_CustomStore.GetCustomStoreMaxPages()") == V{ "2" });
    Query("QUERY_CUSTOM_STORE_OK", items);   // a new result resets the view
    CHECK(Page(L, 1) == "");
    Stop(L);
}

// Short packets: the item / type counts stop at the packet's end.
COA_TEST(CustomStore_ShortPackets) {
    lua_State* L = Start();
    CoATest::Bytes b;
    b.Str("QUERY_CUSTOM_STORE_OK").U32(100);
    PutItem(b, { 1, 1, 9001, 5, {}, {}, 0, 0 });
    b.U32(2).U32(1);   // a cut second item: what CDataStore's array getter copied, the rest zero
    CHECK(Dispatch(0x6BA, b).size() == 1);
    CHECK(Run(L, "return C_CustomStore.GetCustomStoreItemInfo(1)") == (V{ "9001", "5", "table", "table", "nil", "nil" }));
    CHECK(Run(L, "return C_CustomStore.GetCustomStoreItemInfo(2)") == (V{ "0", "0", "table", "table", "nil", "nil" }));
    CoATest::Bytes t;
    t.U32(50).U32(3).U32(0).U32(0).Str("x").Str("y");
    Dispatch(0x6BD, t);
    CHECK(Run(L, "return C_CustomStore.GetCustomStoreTypeInfo(3)") == (V{ "y", "x" }));
    CoATest::Bytes e;
    CHECK(Dispatch(0x6BA, e) == V{ "QUERY_CUSTOM_STORE_RESULT:" });
    Stop(L);
}
