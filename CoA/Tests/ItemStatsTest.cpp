#include "CoA/Source/Modules/AttachLua.h"
#include "CoA/Source/Modules/ItemStats.h"
#include "CoA/Source/Runtime/LuaReg.h"
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

    struct Auction {
        uint32_t item;
        uint32_t level;
    };

    std::map<int, std::vector<Auction>> s_lists;
    std::map<uint32_t, int32_t> s_itemLevels;
    int32_t s_roll = 0;

    void Start() {
        CoA::LuaReg::Reset();
        CoA::Packets::Clear();
        CoA::AttachLua::clientCacheVersion.Clear();
        CoA::ItemStats::Reset();
        CoA::ItemStats::Init();
        s_lists.clear();
        s_itemLevels.clear();
        s_roll = 0;
        CoA::ItemStats::Deps& d = CoA::ItemStats::Dependencies();
        d.auctionList = [](int list, uint32_t& count) {
            auto it = s_lists.find(list);

            if (it == s_lists.end()) {
                return false;
            }

            count = static_cast<uint32_t>(it->second.size());
            return true;
        };
        d.auctionEntry = [](int list, uint32_t index, uint32_t& item, uint32_t& level) {
            const std::vector<Auction>& v = s_lists[list];
            item = index < v.size() ? v[index].item : 0;
            level = index < v.size() ? v[index].level : 0;
        };
        d.itemRequiredLevel = [](uint32_t item, int32_t& level) {
            auto it = s_itemLevels.find(item);

            if (it == s_itemLevels.end()) {
                return false;
            }

            level = it->second;
            return true;
        };
        d.nextLootRollId = [] { return s_roll; };
    }

    void Stop() {
        CoATest::StopRecordingPackets();
        CoATest::StopRecordingSignals();
        CoA::ItemStats::Reset();
        CoA::AttachLua::clientCacheVersion.Clear();
        CoA::Packets::Clear();
        CoA::LuaReg::Reset();
    }

    void Deliver(uint32_t opcode, const CoATest::Bytes& b) {
        std::vector<uint8_t> data = b.data;
        CDataStore msg(data.data(), static_cast<uint32_t>(data.size()));
        int32_t result = 0;
        CHECK(CoA::Packets::Dispatch(static_cast<int32_t>(opcode), 0, &msg, result));
        CHECK(result == 1);
    }

    std::vector<uint8_t> Query(uint32_t item, uint32_t level) {
        return CoATest::Expected(0x6FF, CoATest::Bytes().U64(static_cast<uint64_t>(level) << 32 | item));
    }

    // A record: key, 2 u32, 10 stat pairs, 2 damage triples, armor, armor reborn, six resistances, block, random
    // property, required level, sell price.
    CoATest::Bytes Record(uint32_t item, uint32_t level, int32_t requiredLevel) {
        CoATest::Bytes b;
        b.U64(static_cast<uint64_t>(level) << 32 | item).U32(0xAA).U32(0xBB);

        for (uint32_t i = 0; i < 10; ++i) {
            b.I32(static_cast<int32_t>(i + 1)).I32(-static_cast<int32_t>(i) * 10);
        }

        b.F32(1.5f).F32(2.25f).U32(0).F32(3.0f).F32(4.5f).U32(0);
        b.I32(100).I32(-5).I32(1).I32(2).I32(3).I32(4).I32(5).I32(6).U32(7).I32(-8).I32(requiredLevel).U32(12345);
        return b;
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
            out.push_back(lua_isnil(L, i) ? "nil" : lua_istable(L, i) ? "table" : lua_tostring(L, i));
        }

        lua_settop(L, base);
        return out;
    }
}

// FUN_10280330: 0/0 is nothing; an unknown key asks CMSG 0x6FF {u64 level << 32 | item} once; a pending key queues the
// load callback again, so SMSG 0x700 fires the four events once per miss. A record for a key nobody asked for is dropped.
COA_TEST(ItemStats_GetAndRecord) {
    Start();
    CoATest::RecordPackets();
    CoATest::RecordSignals();
    CHECK(CoA::ItemStats::Get(0, 0) == nullptr);
    CHECK(CoATest::SentPackets().empty());
    CHECK(CoA::ItemStats::Get(0, 5) == nullptr);   // only 0 / 0 is skipped
    CHECK(CoA::ItemStats::Get(7, 60) == nullptr);
    CHECK(CoA::ItemStats::Get(7, 60) == nullptr);
    CHECK((CoATest::SentPackets() == Packets{ Query(0, 5), Query(7, 60) }));
    Deliver(0x700, Record(7, 60, 55));
    const V once = { "GET_ITEM_INFO_RECEIVED:7,true", "AUCTION_ITEM_LIST_UPDATE", "AUCTION_BIDDER_LIST_UPDATE", "AUCTION_OWNER_LIST_UPDATE" };
    V twice = once;
    twice.insert(twice.end(), once.begin(), once.end());
    CHECK(CoATest::Signals() == twice);
    const uint8_t* r = CoA::ItemStats::Get(7, 60);
    CHECK(r != nullptr);

    if (r) {
        int32_t required;
        memcpy(&required, r + 0xA0, 4);
        CHECK(required == 55);
        CHECK(r[8] == 0xAA && r[0xC] == 0xBB);
    }

    CHECK(CoATest::Signals().size() == 8);   // a loaded record queues nothing
    CHECK(CoA::ItemStats::Count() == 2);
    Deliver(0x700, Record(9, 1, 3));          // not asked for
    CHECK(CoA::ItemStats::Count() == 2);
    // A short record reads zeros past the end.
    Deliver(0x700, CoATest::Bytes().U64(5ull << 32).U32(0x11));
    const uint8_t* s = CoA::ItemStats::Get(0, 5);
    CHECK(s && s[8] == 0x11 && s[0xA0] == 0);
    Stop();
}

// GetScalingItemStats(item, level): nil (and the query) until cached, then the record as FUN_102817e0's JSON table.
COA_TEST(ItemStats_ScalingItemStats) {
    Start();
    lua_State* L = CoATest::NewState(false);
    CoATest::RecordPackets();
    CHECK((CoATest::Call(L, "", "GetScalingItemStats", { "n:7.9", "n:60" }) == V{ "nil" }));
    CHECK((CoATest::SentPackets() == Packets{ Query(7, 60) }));
    Deliver(0x700, Record(7, 60, 55));
    CHECK((Run(L, "local t = GetScalingItemStats(7, 60) return t.ItemStatsType0, t.ItemStatsValue0, t.ItemStatsType9, "
        "t.ItemStatsValue9, t.ItemDamageMin0, t.ItemDamageMax0, t.ItemDamageMin1, t.ItemDamageMax1, t.ItemArmor, "
        "t.ItemArmorReborn, t.ItemHolyRes, t.ItemArcaneRes, t.ItemBlock, t.RandomProperty, t.RequiredLevel, t.SellPrice")
        == V{ "1", "0", "10", "-90", "1.5", "2.25", "3", "4.5", "100", "-5", "1", "6", "7", "-8", "55", "12345" }));
    CHECK(Run(L, "return GetScalingItemStats(7)")[0].compare(0, 7, "<error>") == 0);   // luaL_checknumber
    lua_close(L);
    Stop();
}

// GetAuctionItemRequiredLevel(list, index): "bidder" / "owner" / "list"; the scaled record's required level, else the
// item cache's, else nil. index - 1 < count + 1 lets the entry one past the end through.
COA_TEST(ItemStats_AuctionRequiredLevel) {
    Start();
    lua_State* L = CoATest::NewState(false);
    s_lists[0] = { { 7, 60 }, { 8, 0 } };
    s_lists[2] = { { 9, 70 } };
    s_itemLevels = { { 8, 33 }, { 9, 44 }, { 0, 1 } };
    Deliver(0x700, Record(7, 60, 55));   // not requested yet: dropped
    CHECK((CoATest::Call(L, "", "GetAuctionItemRequiredLevel", { "s:bidder", "n:1" }) == V{ "nil" }));   // requested now
    Deliver(0x700, Record(7, 60, 55));
    CHECK((CoATest::Call(L, "", "GetAuctionItemRequiredLevel", { "s:bidder", "n:1" }) == V{ "55" }));
    CHECK((CoATest::Call(L, "", "GetAuctionItemRequiredLevel", { "s:bidder", "n:2" }) == V{ "33" }));    // 8 / 0: no scaled record
    CHECK((CoATest::Call(L, "", "GetAuctionItemRequiredLevel", { "s:bidder", "n:3" }) == V{ "1" }));     // one past: item 0
    CHECK((CoATest::Call(L, "", "GetAuctionItemRequiredLevel", { "s:bidder", "n:4" }) == V{ "nil" }));
    CHECK((CoATest::Call(L, "", "GetAuctionItemRequiredLevel", { "s:bidder", "n:0" }) == V{ "nil" }));
    CHECK((CoATest::Call(L, "", "GetAuctionItemRequiredLevel", { "s:list", "n:1" }) == V{ "44" }));
    CHECK((CoATest::Call(L, "", "GetAuctionItemRequiredLevel", { "s:owner", "n:1" }) == V{ "nil" }));    // no data
    CHECK((CoATest::Call(L, "", "GetAuctionItemRequiredLevel", { "s:other", "n:1" }) == V{ "nil" }));
    s_lists[1] = {};
    CHECK((CoATest::Call(L, "", "GetAuctionItemRequiredLevel", { "s:owner", "n:1" }) == V{ "nil" }));    // count 0
    s_itemLevels.erase(9);
    CHECK((CoATest::Call(L, "", "GetAuctionItemRequiredLevel", { "s:list", "n:1" }) == V{ "nil" }));
    lua_close(L);
    Stop();
}

// SetInspectedSlot(slot): 1..20 -> slot - 1, others ignored. SMSG 0x73F: the level for the client's next loot-roll
// index, the vector grown to fit.
COA_TEST(ItemStats_InspectAndLootRoll) {
    Start();
    lua_State* L = CoATest::NewState(false);
    CHECK(CoATest::Call(L, "", "SetInspectedSlot", { "n:20" }).empty());
    CHECK(CoA::ItemStats::InspectedSlot() == 19);
    CoATest::Call(L, "", "SetInspectedSlot", { "n:21" });
    CoATest::Call(L, "", "SetInspectedSlot", { "n:0" });
    CHECK(CoA::ItemStats::InspectedSlot() == 19);
    CoATest::Call(L, "", "SetInspectedSlot", { "n:1" });
    CHECK(CoA::ItemStats::InspectedSlot() == 0);
    s_roll = 3;
    Deliver(0x73F, CoATest::Bytes().I32(64));
    CHECK((CoA::ItemStats::LootRollLevels() == std::vector<uint32_t>{ 0, 0, 0, 64 }));
    s_roll = 1;
    Deliver(0x73F, CoATest::Bytes().I32(12));
    Deliver(0x73F, CoATest::Bytes());   // GetInt32 fails: 0
    CHECK((CoA::ItemStats::LootRollLevels() == std::vector<uint32_t>{ 0, 0, 0, 64 }));
    s_roll = 2;
    Deliver(0x73F, CoATest::Bytes().I32(12));
    CHECK((CoA::ItemStats::LootRollLevels() == std::vector<uint32_t>{ 0, 0, 12, 64 }));
    lua_close(L);
    Stop();
}

// FUN_101b0070 on AttachLua's 0x635710 list: a different version empties the store and becomes its own; the same one
// keeps it. The cache-file helpers keep loaded records only.
COA_TEST(ItemStats_CacheVersion) {
    Start();
    CoA::ItemStats::Get(1, 1);
    CoA::ItemStats::Get(2, 2);
    Deliver(0x700, Record(1, 1, 5));
    CoA::AttachLua::ClientCacheVersionSet(0);
    CHECK(CoA::ItemStats::Count() == 2);
    CoA::ItemStats::PruneUnloaded();
    CHECK(CoA::ItemStats::Count() == 1);
    uint32_t items = 0;
    CoA::ItemStats::ForEachRecord([](const uint8_t* r, void* ctx) { *static_cast<uint32_t*>(ctx) += r[0]; }, &items);
    CHECK(items == 1);
    CoA::AttachLua::ClientCacheVersionSet(12340);
    CHECK(CoA::ItemStats::Count() == 0 && CoA::ItemStats::Version() == 12340);
    std::vector<uint8_t> rec = Record(3, 4, 9).data;
    CoA::ItemStats::InsertLoaded(rec.data());
    CoA::AttachLua::ClientCacheVersionSet(12340);
    CHECK(CoA::ItemStats::Count() == 1 && CoA::ItemStats::Get(3, 4) != nullptr);
    Stop();
}
