#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/Dbc.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Tests/Support.h"
#include "CoA/Tests/TestMain.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <cstring>
#include <string>
#include <vector>

namespace {
    using V = std::vector<std::string>;
    using Packets = std::vector<std::vector<uint8_t>>;

    // Strings: 1 "Common", 8 "Rare Box", 17 "Desc", 22 "Icon".
    const std::string kStrings = std::string("Common") + '\0' + "Rare Box" + '\0' + "Desc" + '\0' + "Icon" + '\0';

    uint32_t Bits(float f) {
        uint32_t v;
        memcpy(&v, &f, 4);
        return v;
    }

    std::vector<uint32_t> ItemRow(uint32_t id, uint32_t type, uint32_t a, uint32_t b, float f) {
        std::vector<uint32_t> row = { id, type, a, b, Bits(f) };

        for (uint32_t i = 0; i < 20; ++i) {
            row.push_back(id * 100 + i);
        }

        row.push_back(0);   // the 0x68-byte row's last dword
        return row;
    }

    std::vector<uint32_t> TypeRow(uint32_t id, uint32_t a, uint32_t b, uint32_t s1, uint32_t s2, uint32_t s3) {
        std::vector<uint32_t> row = { id, a, b, s1 };
        std::vector<uint32_t> l2 = CoATest::Loc(s2);
        std::vector<uint32_t> l3 = CoATest::Loc(s3);
        row.insert(row.end(), l2.begin(), l2.end());
        row.insert(row.end(), l3.begin(), l3.end());
        return row;
    }

    std::vector<uint32_t> RarityRow(uint32_t id, uint32_t flag, uint32_t s) {
        std::vector<uint32_t> row = { id, flag };
        std::vector<uint32_t> l = CoATest::Loc(s);
        row.insert(row.end(), l.begin(), l.end());
        return row;
    }

    bool s_withRates = true;

    bool ReadTables(const char* path, std::vector<uint8_t>& out) {
        if (strstr(path, "CollectorCacheItems.dbc")) {
            out = CoATest::Wdbc({ ItemRow(3, 1, 30, 31, 0.5f), ItemRow(5, 2, 50, 51, 1.5f), ItemRow(9, 1, 90, 91, 2.0f) }, kStrings);
            return true;
        }

        if (strstr(path, "CollectorCacheTypes.dbc")) {
            out = CoATest::Wdbc({ TypeRow(1, 7, 8, 8, 17, 22), TypeRow(2, 0, 0, 0, 0, 0) }, kStrings);
            return true;
        }

        if (strstr(path, "CollectorCacheRarityTypes.dbc")) {
            out = CoATest::Wdbc({ RarityRow(4, 1, 1), RarityRow(6, 0, 0) }, kStrings);
            return true;
        }

        if (strstr(path, "CollectorCacheRarityRates.dbc") && s_withRates) {
            out = CoATest::Wdbc({ { 1, 0, 2, Bits(0.25f) }, { 2, 0, 1, Bits(0.75f) }, { 3, 0, 1, Bits(0.1f) } }, kStrings);
            return true;
        }

        return false;
    }

    void Unload() {
        for (const char* t : { "CollectorCacheItems", "CollectorCacheTypes", "CollectorCacheRarityTypes", "CollectorCacheRarityRates" }) {
            CoA::Dbc::Get((std::string("DBFilesClient\\") + t + ".dbc").c_str()).Unload();
        }
    }

    lua_State* Start(bool rates = true) {
        s_withRates = rates;
        CoA::Dbc::SetFileReader(ReadTables);
        Unload();
        CoA::LuaReg::Reset();
        CoA::CollectorCache::Init();
        return CoATest::NewState(false);
    }

    void Stop(lua_State* L) {
        lua_close(L);
        CoATest::StopRecordingPackets();
        Unload();
        CoA::Dbc::SetFileReader(nullptr);
        CoA::LuaReg::Reset();
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
}

// GetCollectorCacheItemInfo (FUN_10194310): +8, +0xC, the float, the 20 values, then the first rarity rate with +8 == 1.
COA_TEST(CollectorCache_ItemInfo) {
    lua_State* L = Start();
    CHECK((Run(L, "local a, b, c, t, r = C_CollectorCache.GetCollectorCacheItemInfo(5) return a, b, c, #t, t[1], t[20], r") ==
        V{ "50", "51", "1.5", "20", "500", "519", "0.75" }));
    CHECK((Run(L, "return select('#', C_CollectorCache.GetCollectorCacheItemInfo(4))") == V{ "0" }));
    CHECK((Run(L, "return select('#', C_CollectorCache.GetCollectorCacheItemInfo('x'))") == V{ "0" }));
    Stop(L);
}

// Without a rate row the original still returns 5 with four values pushed: Lua sees the argument first.
COA_TEST(CollectorCache_ItemInfoWithoutRate) {
    lua_State* L = Start(false);
    CHECK((Run(L, "local a, b, c, d, t = C_CollectorCache.GetCollectorCacheItemInfo(3) return a, b, c, d, type(t)") ==
        V{ "3", "30", "31", "0.5", "table" }));
    Stop(L);
}

// The items of a type in id order, their count, the type / rarity rows (strings, the flag as a boolean).
COA_TEST(CollectorCache_ItemsAndTypes) {
    lua_State* L = Start();
    CHECK((Run(L, "local t = C_CollectorCache.GetCollectorCacheItems(1) return #t, t[1], t[2]") == V{ "2", "3", "9" }));
    CHECK((Run(L, "return #C_CollectorCache.GetCollectorCacheItems(7), C_CollectorCache.GetNumCollectorCacheItems(1), "
                  "C_CollectorCache.GetNumCollectorCacheItems(2), C_CollectorCache.GetNumCollectorCacheItems(8)") == V{ "0", "2", "1", "0" }));
    CHECK((Run(L, "return select('#', C_CollectorCache.GetCollectorCacheItems()), select('#', C_CollectorCache.GetNumCollectorCacheItems({}))") ==
        V{ "0", "0" }));
    CHECK((Run(L, "return C_CollectorCache.GetCollectorCacheTypeInfo(1)") == V{ "7", "8", "Rare Box", "Desc", "Icon" }));
    CHECK((Run(L, "return C_CollectorCache.GetCollectorCacheTypeInfo(2)") == V{ "0", "0", "", "", "" }));
    CHECK((Run(L, "return select('#', C_CollectorCache.GetCollectorCacheTypeInfo(3))") == V{ "0" }));
    CHECK((Run(L, "return C_CollectorCache.GetCollectorCacheRarityTypeInfo(4)") == V{ "true", "Common" }));
    CHECK((Run(L, "return C_CollectorCache.GetCollectorCacheRarityTypeInfo(6)") == V{ "false", "" }));
    CHECK((Run(L, "return select('#', C_CollectorCache.GetCollectorCacheRarityTypeInfo(5))") == V{ "0" }));
    Stop(L);
}

// OpenCollectorCache: CMSG_OPEN_COLLECTOR_CACHE 0x6DF {u32 id}, true; a non-number sends nothing.
COA_TEST(CollectorCache_Open) {
    lua_State* L = Start();
    CoATest::RecordPackets();
    CHECK((Run(L, "return C_CollectorCache.OpenCollectorCache(77.6)") == V{ "true" }));
    CHECK((Run(L, "return select('#', C_CollectorCache.OpenCollectorCache('x'))") == V{ "0" }));
    CHECK((CoATest::SentPackets() == Packets{ CoATest::Expected(0x6DF, CoATest::Bytes().U32(77)) }));
    Stop(L);
}
