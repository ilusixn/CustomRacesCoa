#include "CoA/Source/Modules/GroupFinder.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/Dbc.h"
#include "CoA/Source/Runtime/Lifecycle.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Tests/Support.h"
#include "CoA/Tests/TestMain.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <cstring>
#include <string>
#include <vector>

namespace {
    using V = std::vector<std::string>;

    // LFGActivities.dbc: id, category, min level, max level, name (string), +0x14. Activity 5: category 2, levels 10..20.
    bool ReadTables(const char* path, std::vector<uint8_t>& out) {
        if (strstr(path, "LFGActivities.dbc")) {
            out = CoATest::Wdbc({ { 5, 2, 10, 20, 1, 0 }, { 6, 2, 30, 40, 1, 0 } }, "Raid");
            return true;
        }

        return false;
    }

    lua_State* Setup() {
        CoA::LuaReg::Reset();
        CoA::Packets::Clear();
        CoA::Lifecycle::Reset();
        CoA::Dbc::SetFileReader(ReadTables);
        CoA::GroupFinder::Reset();
        CoA::GroupFinder::Init();
        CoA::GroupFinder::Deps& deps = CoA::GroupFinder::Dependencies();
        deps.playerLevel = [] { return 15u; };
        deps.playerName = [] { return "Helix"; };
        deps.partyCounts = [](uint32_t& a, uint32_t& b) {
            a = 0;
            b = 3;
        };
        deps.classToken = [](uint32_t id) -> const char* { return id == 6 ? "DEATHKNIGHT" : nullptr; };
        return CoATest::NewState(false);
    }

    V Feed(uint32_t opcode, CoATest::Bytes bytes) {
        CoATest::RecordSignals();
        CDataStore msg(bytes.data.data(), static_cast<uint32_t>(bytes.data.size()));
        int32_t result = 0;
        CoA::Packets::Dispatch(static_cast<int32_t>(opcode), 0, &msg, result);
        CoATest::StopRecordingSignals();
        return CoATest::Signals();
    }

    void Group(CoATest::Bytes& b, const char* id, uint32_t activity, uint32_t cls) {
        b.Name(id).U32(1).U32(activity).U32(200).U8(1).U8(0).U8(1).Name("title").Name("desc").U8(1).U8(0).Name("Leader").U32(2).U32(5)
            .U32(cls).U32(15).U8(1);
    }

    std::string Field(lua_State* L, const char* id, const char* key) {
        lua_getglobal(L, "C_GroupFinder");
        lua_getfield(L, -1, "GetGroupInfo");
        lua_pushstring(L, id);
        lua_call(L, 1, 1);
        std::string out = "nil";

        if (lua_istable(L, -1)) {
            lua_getfield(L, -1, key);
            out = lua_isstring(L, -1) ? lua_tostring(L, -1) : (lua_isboolean(L, -1) ? (lua_toboolean(L, -1) ? "true" : "false") : "nil");
            lua_pop(L, 1);
        }

        lua_pop(L, 2);
        return out;
    }
}

// CreateListing sends the original's CMSG 0x57C bytes; 0x580 OK stores the id and fires CREATED; a failure names the
// result; 0x582 OK delists.
COA_TEST(GroupFinder_CreateUpdateRemove) {
    lua_State* L = Setup();
    lua_newtable(L);
    lua_pushnumber(L, 5);
    lua_rawseti(L, -2, 1);
    lua_setglobal(L, "acts");
    CoATest::RecordPackets();
    luaL_dostring(L, "C_GroupFinder.CreateListing(acts, 200, true, false, true, 'title', 'desc', true, false, 5, true)");
    CoATest::StopRecordingPackets();
    CHECK(CoATest::SentPackets().size() == 1);
    CHECK(CoATest::SentPackets()[0] == CoATest::Expected(0x57C, CoATest::Bytes().U32(1).U32(5).U32(200).U8(1).U8(0).U8(1).Name("title").Name("desc")
        .U8(1).U8(0).U32(5).U8(1)));

    CHECK(Feed(0x580, CoATest::Bytes().U32(0).Name("G1")) == V{ "GROUP_FINDER_ACTIVITY_CREATED:G1" });
    CHECK(CoATest::Call(L, "C_GroupFinder", "GetListedGroupID")[0] == "G1");
    CHECK(Field(L, "G1", "LeaderName") == "Helix");
    CHECK(Field(L, "G1", "MemberCount") == "3");
    CHECK(Feed(0x580, CoATest::Bytes().U32(2)) == V{ "GROUP_FINDER_ACTIVITY_CREATION_FAILED:CREATE_LISTING_NOT_LEADER" });
    CHECK(Feed(0x581, CoATest::Bytes().U32(9)) == V{ "GROUP_FINDER_ACTIVITY_UPDATE_FAILED:UNEXPECTED_ENUM_VALUE_9" });
    CHECK(Feed(0x582, CoATest::Bytes().U32(0).Name("G1")) == V{ "GROUP_FINDER_ACTIVITY_DELISTED:G1" });
    CHECK(CoATest::Call(L, "C_GroupFinder", "GetListedGroupID")[0] == "nil");
    CoA::Dbc::SetFileReader(nullptr);
}

// 0x583 replaces the listed groups; GetListedGroups keeps the groups whose first activity's level range holds the player.
COA_TEST(GroupFinder_ListedGroups) {
    lua_State* L = Setup();
    CoATest::Bytes b;
    b.U32(2);
    Group(b, "A", 5, 6);
    Group(b, "B", 6, 1);
    CHECK(Feed(0x583, b) == V{ "GROUP_FINDER_LISTED_ACTIVITIES_UPDATED" });
    CHECK(Field(L, "A", "LeaderClass") == "DEATHKNIGHT");
    CHECK(Field(L, "B", "LeaderClass") == "UNKNOWN");
    CHECK(Field(L, "A", "NeedsTank") == "true");
    luaL_dostring(L, "n = #C_GroupFinder.GetListedGroups({})");
    lua_getglobal(L, "n");
    CHECK(lua_tonumber(L, -1) == 1);
    lua_pop(L, 1);
    CoA::Dbc::SetFileReader(nullptr);
}

// 0x585 / 0x587 / 0x59D: the invite events (0x585 swaps its two names; 0x59D strips "CLASS_").
COA_TEST(GroupFinder_Invites) {
    Setup();
    CHECK(Feed(0x585, CoATest::Bytes().Name("first").Name("second")) == V{ "SUGGESTED_GROUP_INVITE:second,first" });
    CHECK(Feed(0x587, CoATest::Bytes().Name("Bob")) == V{ "REQUEST_GROUP_INVITE:Bob" });
    CHECK((Feed(0x59D, CoATest::Bytes().Name("Bob").U8(1).U32(7).Name("x").Name("CLASS_MAGE").U8(2)) ==
        V{ "REQUEST_GROUP_FINDER_INVITE:Bob,1,7,x,MAGE,2" }));
    CoA::Dbc::SetFileReader(nullptr);
}

// A 0x583 count past the packet stops at its end instead of reading on.
COA_TEST(GroupFinder_ShortPacket) {
    lua_State* L = Setup();
    CoATest::Bytes b;
    b.U32(1000);
    Group(b, "A", 5, 6);
    Feed(0x583, b);
    CHECK(Field(L, "A", "Title") == "title");
    CoA::Dbc::SetFileReader(nullptr);
}

// The glue screen clears the listed groups and the player's own listing.
COA_TEST(GroupFinder_GlueResets) {
    lua_State* L = Setup();
    Feed(0x580, CoATest::Bytes().U32(0).Name("G1"));
    CoA::Lifecycle::RunGlueScreen();
    CHECK(CoATest::Call(L, "C_GroupFinder", "GetListedGroupID")[0] == "nil");
    CHECK(Field(L, "G1", "Title") == "nil");
    CoA::Dbc::SetFileReader(nullptr);
}
