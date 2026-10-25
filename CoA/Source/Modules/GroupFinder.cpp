// AscGroupFinder.cpp (see GroupFinder.h).
#include "CoA/Source/Modules/GroupFinder.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/Dbc.h"
#include "CoA/Source/Runtime/Events.h"
#include "CoA/Source/Runtime/Lifecycle.h"
#include "CoA/Source/Runtime/Log.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Packet.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Source/Runtime/Read.h"
#include "CoA/Source/Runtime/Script.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <algorithm>
#include <string>
#include <vector>

namespace {
    struct Group {
        std::string id;
        std::vector<uint32_t> activities;
        uint32_t minItemLevel = 0;
        bool tank = false;
        bool healer = false;
        bool dps = false;
        std::string title;
        std::string description;
        bool newPlayerFriendly = false;
        bool groupLeader = false;
        std::string leaderName;
        uint32_t memberCount = 0;
        uint32_t groupSize = 0;
        uint32_t leaderClass = 0;
        uint32_t leaderLevel = 0;
        bool autoAccept = false;
    };

    Group s_own;                  // manager +0x20
    std::vector<Group> s_groups;  // the map, in insertion order
    CoA::GroupFinder::Deps s_deps;

    CoA::Dbc::Table& Activities() {
        return CoA::Dbc::Get("DBFilesClient\\LFGActivities.dbc");
    }

    CoA::Dbc::Table& Categories() {
        return CoA::Dbc::Get("DBFilesClient\\LFGActivityCategories.dbc");
    }

    CoA::Dbc::Table& GroupTypes() {
        return CoA::Dbc::Get("DBFilesClient\\LFGActivityGroupType.dbc");
    }

    Group* FindGroup(const std::string& id) {
        for (Group& g : s_groups) {
            if (g.id == id) {
                return &g;
            }
        }

        return nullptr;
    }

    Group& UpsertGroup(const std::string& id) {
        if (Group* g = FindGroup(id)) {
            return *g;
        }

        s_groups.emplace_back();
        s_groups.back().id = id;
        return s_groups.back();
    }

    uint32_t PlayerLevel() {
        return s_deps.playerLevel ? s_deps.playerLevel() : 0;
    }

    void PutSized(CoA::Packet& p, const std::string& s) {
        p.U32(static_cast<uint32_t>(s.size()));

        if (!s.empty()) {
            p.Data(s.data(), static_cast<uint32_t>(s.size()));
        }
    }

    // ---- Lua ----
    void PushIdArray(lua_State* L, const std::vector<uint32_t>& v) {   // FUN_1009a460
        lua_createtable(L, 0, static_cast<int>(v.size()));
        lua_checkstack(L, 2);

        for (uint32_t i = 0; i < v.size(); ++i) {
            lua_pushnumber(L, static_cast<double>(i + 1));
            lua_pushinteger(L, static_cast<int>(v[i]));
            lua_settable(L, -3);
        }
    }

    void SetStr(lua_State* L, const char* k, const std::string& v) {
        lua_pushstring(L, k);
        CoA::Script::PushStr(L, v.c_str());
        lua_settable(L, -3);
    }

    void SetNum(lua_State* L, const char* k, uint32_t v) {
        lua_pushstring(L, k);
        lua_pushnumber(L, static_cast<double>(static_cast<int32_t>(v)));
        lua_settable(L, -3);
    }

    void SetBool(lua_State* L, const char* k, bool v) {
        lua_pushstring(L, k);
        CoA::Script::PushBool(L, v);
        lua_settable(L, -3);
    }

    void PushGroup(lua_State* L, const Group& g) {
        lua_createtable(L, 0, 0);
        lua_checkstack(L, 2);
        SetStr(L, "GroupID", g.id);
        lua_pushstring(L, "ActivityIDs");
        PushIdArray(L, g.activities);
        lua_settable(L, -3);
        SetStr(L, "LeaderName", g.leaderName);
        SetStr(L, "Title", g.title);
        SetStr(L, "Description", g.description);
        SetNum(L, "MinItemLevel", g.minItemLevel);
        SetBool(L, "NeedsTank", g.tank);
        SetBool(L, "NeedsHealer", g.healer);
        SetBool(L, "NeedsDPS", g.dps);
        SetBool(L, "NewPlayerFriendly", g.newPlayerFriendly);
        SetNum(L, "MemberCount", g.memberCount);
        SetNum(L, "GroupSize", g.groupSize);
        SetBool(L, "GroupLeader", g.groupLeader);
        lua_pushstring(L, "LeaderClass");   // ChrClasses +0x1C (FUN_100b13c0)
        const char* token = s_deps.classToken ? s_deps.classToken(g.leaderClass) : nullptr;
        lua_pushstring(L, token ? token : "UNKNOWN");
        lua_settable(L, -3);
        lua_pushstring(L, "LeaderLevel");
        lua_pushinteger(L, static_cast<int>(g.leaderLevel));
        lua_settable(L, -3);
        SetBool(L, "AutoAccept", g.autoAccept);
    }

    // FUN_1011f230 (lua_toarray): the numbers in a table, in lua_next order; any non-number value logs and yields an empty
    // list.
    std::vector<uint32_t> ToArray(lua_State* L, int idx) {
        std::vector<uint32_t> out;

        if (lua_type(L, idx) != LUA_TTABLE) {
            CoA::Log::Printf("FrameScript::lua_toarray Value at index %d is not a table", idx);
            return out;
        }

        lua_pushnil(L);

        while (lua_next(L, idx)) {
            if (!lua_isnumber(L, -1)) {
                lua_settop(L, -3);
                CoA::Log::Printf("FrameScript::lua_toarray Invalid lua value type");
                return {};
            }

            out.push_back(static_cast<uint32_t>(CoA::Script::ToInt(CoA::Script::CheckNumber(L, -1))));
            lua_settop(L, -2);
        }

        return out;
    }

    // FUN_1026d5e0: {table, number, bool x3, string x2, bool x2, number, bool}.
    int SendListing(lua_State* L, uint32_t opcode) {
        if (!CoA::Script::ValidateInput(L, { LUA_TTABLE, LUA_TNUMBER, LUA_TBOOLEAN, LUA_TBOOLEAN, LUA_TBOOLEAN, LUA_TSTRING, LUA_TSTRING, LUA_TBOOLEAN,
                LUA_TBOOLEAN, LUA_TNUMBER, LUA_TBOOLEAN })) {
            return 0;
        }

        const std::vector<uint32_t> activities = ToArray(L, 1);
        const uint32_t minItemLevel = static_cast<uint32_t>(CoA::Script::ToInt(CoA::Script::CheckNumber(L, 2)));
        const bool tank = lua_toboolean(L, 3) != 0;
        const bool healer = lua_toboolean(L, 4) != 0;
        const bool dps = lua_toboolean(L, 5) != 0;
        const std::string title = CoA::Script::CheckString(L, 6);
        const std::string description = CoA::Script::CheckString(L, 7);
        const bool b8 = lua_toboolean(L, 8) != 0;
        const bool b9 = lua_toboolean(L, 9) != 0;
        const uint32_t groupSize = static_cast<uint32_t>(CoA::Script::ToInt(CoA::Script::CheckNumber(L, 10)));
        const bool autoAccept = lua_toboolean(L, 11) != 0;

        s_own.activities = activities;
        s_own.minItemLevel = minItemLevel;
        s_own.tank = tank;
        s_own.healer = healer;
        s_own.dps = dps;
        s_own.title = title;
        s_own.description = description;
        s_own.newPlayerFriendly = b8;
        s_own.groupLeader = b9;
        s_own.leaderName = s_deps.playerName ? s_deps.playerName() : "Unknown";
        uint32_t a = 0;
        uint32_t b = 0;

        if (s_deps.partyCounts) {
            s_deps.partyCounts(a, b);
        }

        s_own.memberCount = std::max(a, b ? b : 1u);
        s_own.leaderClass = 10;
        s_own.groupSize = groupSize;
        s_own.leaderLevel = PlayerLevel();
        s_own.autoAccept = autoAccept;

        CoA::Packet p(opcode);
        p.U32(static_cast<uint32_t>(activities.size()));

        for (uint32_t id : activities) {
            p.U32(id);
        }

        p.U32(minItemLevel).U8(tank).U8(healer).U8(dps);
        PutSized(p, title);
        PutSized(p, description);
        p.U8(b8).U8(b9).U32(groupSize).U8(autoAccept);
        p.Send();
        CoA::Script::PushBool(L, true);
        return 1;
    }

    int CreateListing(lua_State* L) {
        return SendListing(L, 0x57C);
    }

    int UpdateListing(lua_State* L) {
        return SendListing(L, 0x57D);
    }

    int SendEmpty(lua_State* L, uint32_t opcode) {
        CoA::Packet(opcode).Send();
        CoA::Script::PushBool(L, true);
        return 1;
    }

    int RemoveListing(lua_State* L) {
        return SendEmpty(L, 0x57E);
    }

    int QueryListedActivities(lua_State* L) {
        return SendEmpty(L, 0x57F);
    }

    // The two bare globals FUN_10274e60 registers after the C_GroupFinder table: one string, sent as {u32 length, bytes}.
    // SuggestInvite = FUN_10274610 (CMSG 0x584), the global RequestInvite = FUN_10274490 (CMSG 0x586) -- not
    // C_GroupFinder.RequestInvite, a different function.
    int SendName(lua_State* L, uint32_t opcode) {
        std::string name;

        if (!CoA::Script::ReadString(L, name)) {
            return 0;
        }

        CoA::Packet p(opcode);
        PutSized(p, name);
        p.Send();
        CoA::Script::PushBool(L, true);
        return 1;
    }

    int SuggestInvite(lua_State* L) {
        return SendName(L, 0x584);
    }

    int RequestInviteByName(lua_State* L) {
        return SendName(L, 0x586);
    }

    // FUN_1026d2a0: {string, number, number, string, string, number}; a class without "CLASS_" gets it.
    int RequestInvite(lua_State* L) {
        if (!CoA::Script::ValidateInput(L, { LUA_TSTRING, LUA_TNUMBER, LUA_TNUMBER, LUA_TSTRING, LUA_TSTRING, LUA_TNUMBER })) {
            return 0;
        }

        const std::string group = CoA::Script::CheckString(L, 1);
        const uint8_t b2 = static_cast<uint8_t>(CoA::Script::ToInt(CoA::Script::CheckNumber(L, 2)));
        const uint32_t n3 = static_cast<uint32_t>(CoA::Script::ToInt(CoA::Script::CheckNumber(L, 3)));
        const std::string s4 = CoA::Script::CheckString(L, 4);
        std::string cls = CoA::Script::CheckString(L, 5);
        const uint8_t b6 = static_cast<uint8_t>(CoA::Script::ToInt(CoA::Script::CheckNumber(L, 6)));

        if (!(cls.size() > 5 && cls.find("CLASS_") != std::string::npos)) {
            cls = "CLASS_" + cls;
        }

        CoA::Packet p(0x59E);
        PutSized(p, group);
        p.U8(b2).U32(n3);
        PutSized(p, s4);
        PutSized(p, cls);
        p.U8(b6);
        p.Send();
        CoA::Script::PushBool(L, true);
        return 1;
    }

    int GetListedGroupID(lua_State* L) {
        if (s_own.id.empty()) {
            lua_pushnil(L);
        } else {
            CoA::Script::PushStr(L, s_own.id.c_str());
        }

        return 1;
    }

    int GetGroupInfo(lua_State* L) {
        std::string id;

        if (!CoA::Script::ReadString(L, id)) {
            return 0;
        }

        if (const Group* g = FindGroup(id)) {
            PushGroup(L, *g);
        } else {
            lua_pushnil(L);
        }

        return 1;
    }

    // Groups for the given activities (all when the list is empty; always the player's own), whose first activity's level
    // range holds the player.
    int GetListedGroups(lua_State* L) {
        if (!CoA::Script::ValidateInput(L, { LUA_TTABLE })) {
            return 0;
        }

        const std::vector<uint32_t> filter = ToArray(L, 1);
        const uint32_t level = PlayerLevel();
        std::vector<const Group*> hits;

        for (const Group& g : s_groups) {
            bool include = filter.empty() || g.id == s_own.id;

            for (size_t i = 0; !include && i < g.activities.size(); ++i) {
                include = std::find(filter.begin(), filter.end(), g.activities[i]) != filter.end();
            }

            if (!include || g.activities.empty()) {
                continue;
            }

            const uint8_t* row = Activities().Row(g.activities[0]);

            if (row && CoA::Dbc::Table::U32(row, 8) <= level && level <= CoA::Dbc::Table::U32(row, 0xC)) {
                hits.push_back(&g);
            }
        }

        lua_createtable(L, 0, 0);
        lua_checkstack(L, 2);

        for (uint32_t i = 0; i < hits.size(); ++i) {
            lua_pushnumber(L, static_cast<double>(i + 1));
            PushGroup(L, *hits[i]);
            lua_settable(L, -3);
        }

        return 1;
    }

    // ---- DBC lists ----
    int GetActivityInfo(lua_State* L) {
        uint32_t id;

        if (!CoA::Script::ReadNumber(L, id)) {
            return 0;
        }

        if (const uint8_t* row = Activities().Row(id)) {
            CoA::Script::PushStr(L, Activities().Str(row, 0x10));
            lua_pushnumber(L, CoA::Dbc::Table::I32(row, 8));
            lua_pushnumber(L, CoA::Dbc::Table::I32(row, 0xC));
            lua_pushnumber(L, CoA::Dbc::Table::I32(row, 4));
            return 4;
        }

        CoA::Script::PushStr(L, "");
        lua_pushnumber(L, 0);
        lua_pushnumber(L, 0);
        lua_pushnumber(L, 0);
        return 4;
    }

    int GetGroupTypeInfo(lua_State* L) {
        uint32_t id;

        if (!CoA::Script::ReadNumber(L, id)) {
            return 0;
        }

        const uint8_t* row = GroupTypes().Row(id);
        const char* name = row ? GroupTypes().Str(row, 4) : nullptr;
        CoA::Script::PushStr(L, name ? name : "");
        return 1;
    }

    // C_GroupFinder.GetCategoryInfo(id) (0x10273100): LFGActivityCategories +8 name, +4 int; "" and 0 for an unknown id.
    int GetCategoryInfo(lua_State* L) {
        uint32_t id;

        if (!CoA::Script::ReadNumber(L, id)) {
            return 0;
        }

        CoA::Dbc::Table& t = Categories();

        if (const uint8_t* row = t.Row(id)) {
            CoA::Script::PushStr(L, t.Str(row, 8));
            CoA::Script::PushInt(L, CoA::Dbc::Table::I32(row, 4));
            return 2;
        }

        CoA::Script::PushStr(L, "");
        CoA::Script::PushInt(L, 0);
        return 2;
    }

    // The insertion-sort predicates FUN_1026df70 / FUN_1026e110 / FUN_1026e270, literally.
    bool ActivityBefore(uint32_t a, uint32_t b) {
        const uint8_t* A = Activities().Row(a);
        const uint8_t* B = Activities().Row(b);

        if (!A || !B) {
            return false;
        }

        auto f = [](const uint8_t* r, uint32_t i) { return CoA::Dbc::Table::U32(r, i * 4); };
        const bool notBefore = f(A, 2) <= f(B, 2) &&
            (f(A, 2) != f(B, 2) || ((f(B, 3) <= f(A, 3) && (f(A, 3) != f(B, 3) || f(B, 5) <= f(A, 5))) && (f(A, 5) != f(B, 5) || f(B, 0) <= f(A, 0))));
        return !notBefore;
    }

    bool CategoryBefore(uint32_t a, uint32_t b) {
        const uint8_t* A = Categories().Row(a);
        const uint8_t* B = Categories().Row(b);

        if (!A || !B) {
            return false;
        }

        const uint32_t a3 = CoA::Dbc::Table::U32(A, 0xC);
        const uint32_t b3 = CoA::Dbc::Table::U32(B, 0xC);
        return !(b3 <= a3 && (a3 != b3 || b <= a));
    }

    bool GroupTypeBefore(uint32_t a, uint32_t b) {
        const uint8_t* A = GroupTypes().Row(a);
        const uint8_t* B = GroupTypes().Row(b);

        if (!A || !B) {
            return false;
        }

        const uint32_t a2 = CoA::Dbc::Table::U32(A, 8);
        const uint32_t b2 = CoA::Dbc::Table::U32(B, 8);
        return !(b2 <= a2 && (a2 != b2 || b <= a));
    }

    int GetAvailableActivities(lua_State* L) {
        uint32_t category;

        if (!CoA::Script::ReadNumber(L, category)) {
            return 0;
        }

        const uint32_t level = PlayerLevel();
        std::vector<uint32_t> ids;
        CoA::Dbc::Table& t = Activities();

        for (uint32_t id = t.MinId(); id <= t.MaxId(); ++id) {
            const uint8_t* row = t.Row(id);

            if (row && CoA::Dbc::Table::U32(row, 4) == category && CoA::Dbc::Table::U32(row, 8) <= level && level <= CoA::Dbc::Table::U32(row, 0xC)) {
                ids.push_back(id);
            }
        }

        std::stable_sort(ids.begin(), ids.end(), ActivityBefore);
        PushIdArray(L, ids);
        return 1;
    }

    int GetAvailableCategories(lua_State* L) {
        uint32_t type;

        if (!CoA::Script::ReadNumber(L, type)) {
            return 0;
        }

        std::vector<uint32_t> ids;
        CoA::Dbc::Table& t = Categories();

        for (uint32_t id = t.MinId(); id <= t.MaxId(); ++id) {
            const uint8_t* row = t.Row(id);

            if (row && CoA::Dbc::Table::U32(row, 4) == type) {
                ids.push_back(id);
            }
        }

        std::stable_sort(ids.begin(), ids.end(), CategoryBefore);
        PushIdArray(L, ids);
        return 1;
    }

    int GetAvailableGroupTypes(lua_State* L) {
        std::vector<uint32_t> ids;
        CoA::Dbc::Table& t = GroupTypes();

        for (uint32_t id = t.MinId(); id <= t.MaxId(); ++id) {
            if (t.Row(id)) {
                ids.push_back(id);
            }
        }

        std::stable_sort(ids.begin(), ids.end(), GroupTypeBefore);
        PushIdArray(L, ids);
        return 1;
    }

    // ---- packets ----
    template <uint32_t N>
    std::string ResultName(uint32_t r, const char* const (&names)[N]) {
        return r < N ? std::string(names[r]) : "UNEXPECTED_ENUM_VALUE_" + std::to_string(r);
    }

    int32_t OnCreateResponse(void*, uint32_t, uint32_t, CDataStore* msg) {   // 0x580
        static const char* const names[5] = {
            "CREATE_LISTING_OK", "CREATE_LISTING_ALREADY_LISTED", "CREATE_LISTING_NOT_LEADER", "CREATE_LISTING_IN_GROUP", "CREATE_LISTING_INVALID_LISTING",
        };

        const uint32_t r = CoA::Read::U32(msg);

        if (r == 0) {
            s_own.id = CoA::Read::Name(msg);
            UpsertGroup(s_own.id) = s_own;
            CoA::Events::Signal("GROUP_FINDER_ACTIVITY_CREATED", "%s", s_own.id.c_str());
            return 1;
        }

        CoA::Events::Signal("GROUP_FINDER_ACTIVITY_CREATION_FAILED", "%s", ResultName(r, names).c_str());
        return 1;
    }

    int32_t OnUpdateResponse(void*, uint32_t, uint32_t, CDataStore* msg) {   // 0x581
        static const char* const names[5] = {
            "UPDATE_LISTING_OK", "UPDATE_LISTING_NOT_LISTED", "UPDATE_LISTING_NOT_LEADER", "UPDATE_LISTING_IN_GROUP", "UPDATE_LISTING_INVALID_LISTING",
        };

        const uint32_t r = CoA::Read::U32(msg);

        if (r == 0) {
            s_own.id = CoA::Read::Name(msg);
            UpsertGroup(s_own.id) = s_own;
            CoA::Events::Signal("GROUP_FINDER_ACTIVITY_UPDATED", "%s", s_own.id.c_str());
            return 1;
        }

        CoA::Events::Signal("GROUP_FINDER_ACTIVITY_UPDATE_FAILED", "%s", ResultName(r, names).c_str());
        return 1;
    }

    int32_t OnRemoveResponse(void*, uint32_t, uint32_t, CDataStore* msg) {   // 0x582
        static const char* const names[4] = {
            "REMOVE_LISTING_OK", "REMOVE_LISTING_NOT_LISTED", "REMOVE_LISTING_NOT_LEADER", "REMOVE_LISTING_IN_GROUP",
        };

        const uint32_t r = CoA::Read::U32(msg);

        if (r == 0) {
            const std::string id = CoA::Read::Name(msg);
            s_own.id.clear();
            s_groups.erase(std::remove_if(s_groups.begin(), s_groups.end(), [&](const Group& g) { return g.id == id; }), s_groups.end());
            CoA::Events::Signal("GROUP_FINDER_ACTIVITY_DELISTED", "%s", id.c_str());
            return 1;
        }

        CoA::Events::Signal("GROUP_FINDER_ACTIVITY_DELIST_FAILED", "%s", ResultName(r, names).c_str());
        return 1;
    }

    // 0x583: the whole map. (Stops at the packet's end; the original reads on for a count past it.)
    int32_t OnQueryResponse(void*, uint32_t, uint32_t, CDataStore* msg) {
        s_groups.clear();   // FUN_10274e00

        for (uint32_t n = CoA::Read::U32(msg); n; --n) {
            Group g;
            g.id = CoA::Read::Name(msg);

            for (uint32_t k = CoA::Read::U32(msg); k && !CoA::Read::Failed(msg); --k) {
                g.activities.push_back(CoA::Read::U32(msg));
            }

            g.minItemLevel = CoA::Read::U32(msg);
            g.tank = CoA::Read::U8(msg) != 0;
            g.healer = CoA::Read::U8(msg) != 0;
            g.dps = CoA::Read::U8(msg) != 0;
            g.title = CoA::Read::Name(msg);
            g.description = CoA::Read::Name(msg);
            g.newPlayerFriendly = CoA::Read::U8(msg) != 0;
            g.groupLeader = CoA::Read::U8(msg) != 0;
            g.leaderName = CoA::Read::Name(msg);
            g.memberCount = CoA::Read::U32(msg);
            g.groupSize = CoA::Read::U32(msg);
            g.leaderClass = CoA::Read::U32(msg);
            g.leaderLevel = CoA::Read::U32(msg);
            g.autoAccept = CoA::Read::U8(msg) != 0;

            if (CoA::Read::Failed(msg)) {
                break;
            }

            UpsertGroup(g.id) = g;
        }

        CoA::Events::Signal("GROUP_FINDER_LISTED_ACTIVITIES_UPDATED", nullptr);
        return 1;
    }

    int32_t OnSuggestedInvite(void*, uint32_t, uint32_t, CDataStore* msg) {   // 0x585
        const std::string first = CoA::Read::Name(msg);
        const std::string second = CoA::Read::Name(msg);
        CoA::Events::Signal("SUGGESTED_GROUP_INVITE", "%s%s", second.c_str(), first.c_str());
        return 1;
    }

    int32_t OnRequestInvite(void*, uint32_t, uint32_t, CDataStore* msg) {   // 0x587
        const std::string name = CoA::Read::Name(msg);
        CoA::Events::Signal("REQUEST_GROUP_INVITE", "%s", name.c_str());
        return 1;
    }

    // SMSG 0x59D (FUN_102719f0): sized name, u8, u32, sized string, sized class, u8. A class naming "CLASS_" anywhere loses
    // its first six characters. -> REQUEST_GROUP_FINDER_INVITE("%s%u%u%s%s%u").
    int32_t OnGroupFinderInvite(void*, uint32_t, uint32_t, CDataStore* msg) {
        const std::string name = CoA::Read::Name(msg);
        const uint32_t a = CoA::Read::U8(msg);
        const uint32_t b = CoA::Read::U32(msg);
        const std::string c = CoA::Read::Name(msg);
        std::string cls = CoA::Read::Name(msg);
        const uint32_t d = CoA::Read::U8(msg);

        if (cls.size() >= 6 && cls.find("CLASS_") != std::string::npos) {
            cls = cls.substr(6);
        }

        CoA::Events::Signal("REQUEST_GROUP_FINDER_INVITE", "%s%u%u%s%s%u", name.c_str(), a, b, c.c_str(), cls.c_str(), d);
        return 1;
    }

    // FUN_1026d0e0 (every glue screen): the listed groups cleared and the player's own listing replaced by a default record.
    void ResetState() {
        s_groups.clear();
        s_own = Group{};
    }

    const CoA::Binding s_bindings[] = {
        COA_NATIVE("C_GroupFinder", "GetCategoryInfo", GetCategoryInfo),
        COA_NATIVE("C_GroupFinder", "CreateListing", CreateListing),
        COA_NATIVE("C_GroupFinder", "GetActivityInfo", GetActivityInfo),
        COA_NATIVE("C_GroupFinder", "GetAvailableActivities", GetAvailableActivities),
        COA_NATIVE("C_GroupFinder", "GetAvailableCategories", GetAvailableCategories),
        COA_NATIVE("C_GroupFinder", "GetAvailableGroupTypes", GetAvailableGroupTypes),
        COA_NATIVE("C_GroupFinder", "GetGroupInfo", GetGroupInfo),
        COA_NATIVE("C_GroupFinder", "GetGroupTypeInfo", GetGroupTypeInfo),
        COA_NATIVE("C_GroupFinder", "GetListedGroupID", GetListedGroupID),
        COA_NATIVE("C_GroupFinder", "GetListedGroups", GetListedGroups),
        COA_NATIVE("C_GroupFinder", "QueryListedActivities", QueryListedActivities),
        COA_NATIVE("C_GroupFinder", "RemoveListing", RemoveListing),
        COA_NATIVE("C_GroupFinder", "RequestInvite", RequestInvite),
        COA_NATIVE("C_GroupFinder", "UpdateListing", UpdateListing),
        COA_NATIVE("", "SuggestInvite", SuggestInvite),
        COA_NATIVE("", "RequestInvite", RequestInviteByName),
    };
}

CoA::GroupFinder::Deps& CoA::GroupFinder::Dependencies() {
    return s_deps;
}

void CoA::GroupFinder::Init() {
    CoA::LuaReg::AddModule(s_bindings, sizeof(s_bindings) / sizeof(s_bindings[0]));
    CoA::Lifecycle::OnGlueScreen(ResetState);
    COA_PACKET(0x59D, OnGroupFinderInvite);
    COA_PACKET(0x580, OnCreateResponse);
    COA_PACKET(0x581, OnUpdateResponse);
    COA_PACKET(0x582, OnRemoveResponse);
    COA_PACKET(0x583, OnQueryResponse);
    COA_PACKET(0x585, OnSuggestedInvite);
    COA_PACKET(0x587, OnRequestInvite);
    COA_EVENT("GROUP_FINDER_ACTIVITY_CREATED");
    COA_EVENT("GROUP_FINDER_ACTIVITY_CREATION_FAILED");
    COA_EVENT("GROUP_FINDER_ACTIVITY_UPDATED");
    COA_EVENT("GROUP_FINDER_ACTIVITY_UPDATE_FAILED");
    COA_EVENT("GROUP_FINDER_ACTIVITY_DELISTED");
    COA_EVENT("GROUP_FINDER_ACTIVITY_DELIST_FAILED");
    COA_EVENT("GROUP_FINDER_LISTED_ACTIVITIES_UPDATED");
    COA_EVENT("SUGGESTED_GROUP_INVITE");
    COA_EVENT("REQUEST_GROUP_INVITE");
    COA_EVENT("REQUEST_GROUP_FINDER_INVITE");
}

void CoA::GroupFinder::Reset() {
    ResetState();
}
