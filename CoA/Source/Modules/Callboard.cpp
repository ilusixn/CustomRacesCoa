// AscCallboard.cpp (see Callboard.h): the Callboard view, SMSG 0x589 / 0x58A and C_Callboard.GetView; its completions
// feed C_TemporalContracts.ActivateAll* (TemporalContracts' forEachCompletion).
#include "CoA/Source/Modules/Callboard.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Modules/TemporalContracts.h"
#include "CoA/Source/Runtime/Events.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Source/Runtime/Read.h"
#include "CoA/Source/Runtime/Script.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <string>
#include <utility>
#include <vector>

using CoA::Script::PushStr;

namespace {
    struct Reward {
        uint32_t money = 0, honor = 0, arena = 0, cachePoints = 0;
        std::vector<std::pair<uint32_t, uint32_t>> items;   // {ItemID, Amount}
    };

    struct SubCategory {
        std::string type, name;   // +0x00, +0x18
        uint32_t remaining = 0;   // +0x30
        uint32_t moneyCost = 0, tokenCost = 0, minimumLevel = 0;
        Reward reward;
    };

    struct Category {
        std::string contentType, name;   // +0x00, +0x18
        uint32_t remaining = 0, moneyCost = 0, tokenCost = 0;
        Reward reward;
        std::vector<SubCategory> subCategories;   // +0x58
    };

    std::vector<Category> s_view;   // +4
    bool s_loaded = false;          // +0x10

    Reward ReadReward(CDataStore* msg) {   // FUN_10116af0
        Reward r;
        r.money = CoA::Read::U32(msg);
        r.honor = CoA::Read::U32(msg);
        r.arena = CoA::Read::U32(msg);
        r.cachePoints = CoA::Read::U32(msg);

        for (uint32_t n = CoA::Read::U32(msg); n && CoA::Read::Remaining(msg) != 0; --n) {   // FUN_10117120
            const uint32_t item = CoA::Read::U32(msg);
            r.items.emplace_back(item, CoA::Read::U32(msg));
        }

        return r;
    }

    int32_t OnView(void*, uint32_t, uint32_t, CDataStore* msg) {   // 0x589 FUN_101194e0
        std::vector<Category> view;

        for (uint32_t n = CoA::Read::U32(msg); n && CoA::Read::Remaining(msg) != 0; --n) {
            Category c;
            c.contentType = CoA::Read::Str(msg);
            c.name = CoA::Read::Str(msg);
            c.remaining = CoA::Read::U32(msg);
            c.moneyCost = CoA::Read::U32(msg);
            c.tokenCost = CoA::Read::U32(msg);
            c.reward = ReadReward(msg);

            for (uint32_t m = CoA::Read::U32(msg); m && CoA::Read::Remaining(msg) != 0; --m) {
                SubCategory s;
                s.type = CoA::Read::Str(msg);
                s.name = CoA::Read::Str(msg);
                s.remaining = CoA::Read::U32(msg);
                s.moneyCost = CoA::Read::U32(msg);
                s.tokenCost = CoA::Read::U32(msg);
                s.minimumLevel = CoA::Read::U32(msg);
                s.reward = ReadReward(msg);
                c.subCategories.push_back(std::move(s));
            }

            view.push_back(std::move(c));
        }

        s_view = std::move(view);
        s_loaded = true;
        CoA::Events::Signal("CALLBOARD_VIEW_UPDATED", nullptr);
        return 1;
    }

    int32_t OnRequestResult(void*, uint32_t, uint32_t, CDataStore* msg) {   // 0x58A FUN_101197c0
        const std::string result = CoA::Read::Str(msg);
        CoA::Events::Signal("CALLBOARD_VIEW_REQUEST_RESULT", "%s", result.c_str());
        return 1;
    }

    // ---- Lua (arrays: createtable(0, n), keys lua_pushnumber(i)) ------------------------------------------------
    void SetStr(lua_State* L, const char* k, const std::string& v) {
        lua_pushstring(L, k);
        PushStr(L, v.c_str());
        lua_settable(L, -3);
    }

    void SetU32(lua_State* L, const char* k, uint32_t v) {
        lua_pushstring(L, k);
        lua_pushinteger(L, static_cast<int32_t>(v));
        lua_settable(L, -3);
    }

    void PushReward(lua_State* L, const Reward& r) {   // FUN_10118030 -> FUN_101186d0
        lua_createtable(L, 0, 5);
        lua_checkstack(L, 2);
        SetU32(L, "RewardMoney", r.money);
        SetU32(L, "RewardHonor", r.honor);
        SetU32(L, "RewardArenaPoints", r.arena);
        SetU32(L, "CallboardCachePoints", r.cachePoints);
        lua_pushstring(L, "Items");   // FUN_10118110
        lua_createtable(L, 0, static_cast<int>(r.items.size()));
        lua_checkstack(L, 2);

        for (size_t i = 0; i < r.items.size(); ++i) {
            lua_pushnumber(L, static_cast<double>(i + 1));
            lua_createtable(L, 0, 2);
            lua_checkstack(L, 2);
            SetU32(L, "ItemID", r.items[i].first);
            SetU32(L, "Amount", r.items[i].second);
            lua_settable(L, -3);
        }

        lua_settable(L, -3);
    }

    void PushCategory(lua_State* L, const Category& c) {   // FUN_101184d0
        SetStr(L, "ContentType", c.contentType);
        SetStr(L, "Name", c.name);
        SetU32(L, "RemainingCompletions", c.remaining);
        SetU32(L, "MoneyCost", c.moneyCost);
        SetU32(L, "TokenCost", c.tokenCost);
        lua_pushstring(L, "Rewards");
        PushReward(L, c.reward);
        lua_settable(L, -3);
        lua_pushstring(L, "SubCategories");   // FUN_10117e70
        lua_createtable(L, 0, static_cast<int>(c.subCategories.size()));
        lua_checkstack(L, 2);

        for (size_t i = 0; i < c.subCategories.size(); ++i) {
            const SubCategory& s = c.subCategories[i];
            lua_pushnumber(L, static_cast<double>(i + 1));
            lua_createtable(L, 0, 7);   // FUN_10118a40
            lua_checkstack(L, 2);
            SetStr(L, "Type", s.type);
            SetStr(L, "Name", s.name);
            SetU32(L, "RemainingCompletions", s.remaining);
            SetU32(L, "MoneyCost", s.moneyCost);
            SetU32(L, "TokenCost", s.tokenCost);
            SetU32(L, "MinimumLevel", s.minimumLevel);
            lua_pushstring(L, "Rewards");
            PushReward(L, s.reward);
            lua_settable(L, -3);
            lua_settable(L, -3);
        }

        lua_settable(L, -3);
    }

    // FUN_10119980: nil until the first view arrives, else {Categories = [...]}.
    int GetView(lua_State* L) {
        if (!s_loaded) {
            lua_pushnil(L);
            return 1;
        }

        lua_createtable(L, 0, 1);
        lua_checkstack(L, 2);
        lua_pushstring(L, "Categories");   // FUN_10117cb0
        lua_createtable(L, 0, static_cast<int>(s_view.size()));
        lua_checkstack(L, 2);

        for (size_t i = 0; i < s_view.size(); ++i) {
            lua_pushnumber(L, static_cast<double>(i + 1));
            lua_createtable(L, 0, 7);
            lua_checkstack(L, 2);
            PushCategory(L, s_view[i]);
            lua_settable(L, -3);
        }

        lua_settable(L, -3);
        return 1;
    }

    const CoA::Binding s_bindings[] = {
        COA_NATIVE("C_Callboard", "GetView", GetView),
    };
}

bool CoA::Callboard::ForEachCompletion(const std::string* contentType, void (*fn)(const std::string& type, void* ctx),
    void* ctx) {
    bool any = false;

    if (!s_loaded) {
        return any;
    }

    for (const Category& c : s_view) {
        if (contentType && c.contentType != *contentType && c.name != *contentType) {
            continue;
        }

        for (const SubCategory& s : c.subCategories) {
            for (uint32_t i = 0; i < s.remaining; ++i) {
                fn(s.type, ctx);
                any = true;
            }
        }
    }

    return any;
}

void CoA::Callboard::Init() {   // FUN_10119420
    CoA::LuaReg::AddModule(s_bindings, sizeof(s_bindings) / sizeof(s_bindings[0]));
    COA_EVENT("CALLBOARD_VIEW_UPDATED");
    COA_EVENT("CALLBOARD_VIEW_REQUEST_RESULT");
    COA_PACKET(0x589, OnView);
    COA_PACKET(0x58A, OnRequestResult);
    CoA::TemporalContracts::Dependencies().forEachCompletion = CoA::Callboard::ForEachCompletion;
}

void CoA::Callboard::Reset() {
    s_view.clear();
    s_loaded = false;
}
