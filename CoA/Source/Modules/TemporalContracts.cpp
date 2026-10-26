// AscTemporalContracts.cpp (see TemporalContracts.h): the TemporalContractMgr, its queued activations and the
// C_TemporalContracts natives, transcribed as the decompile has them.
#include "CoA/Source/Modules/TemporalContracts.h"
#include "CoA/Source/Modules/BridgeStore.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Modules/RealmInfo.h"
#include "CoA/Source/Runtime/ClientHooks.h"
#include "CoA/Source/Runtime/Events.h"
#include "CoA/Source/Runtime/Lifecycle.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Packet.h"
#include "CoA/Source/Runtime/Script.h"
#include "CoA/Source/Runtime/Timer.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <cstring>
#include <deque>
#include <utility>

using CoA::Script::CheckNumber;
using CoA::Script::PushBool;
using CoA::Script::ToInt;
using CoA::Script::ValidateInput;
using CoA::TemporalContracts::ContractType;
using CoA::TemporalContracts::QuestRecord;

namespace {
#include "CoA/Source/Modules/QuestCategories.generated.inc"

    struct Activation {   // 0x20 bytes
        std::string type;
        uint8_t flag;
        uint32_t count;
    };

    CoA::TemporalContracts::Deps s_deps;
    std::vector<ContractType> s_types;   // +0x24
    std::deque<Activation> s_queue;      // +0x30
    uint32_t s_timer = 0;                // 0x10D3C1DC

    uint32_t U32(const uint8_t* p, uint32_t off) {
        uint32_t v;
        memcpy(&v, p + off, 4);
        return v;
    }

    bool Quest(uint32_t id, QuestRecord& out) {   // 0x67DE90 on the quest cache 0xC5DA48
        return s_deps.questRecord && s_deps.questRecord(id, out);
    }

    bool HasQuest(uint32_t id) {
        QuestRecord r;
        return Quest(id, r);
    }

    // FUN_10212c10: the first quest of the group the client has in its quest cache.
    const uint8_t* FirstCachedQuest(uint32_t group) {
        for (const uint8_t* r : CoA::BridgeStore::QuestsInGroup(group)) {   // FUN_10212ad0
            if (HasQuest(U32(r, 0))) {
                return r;
            }
        }

        return nullptr;
    }

    // FUN_103376a0: the first non-zero group of a type with a cached quest.
    const uint8_t* FirstCachedQuest(const ContractType& t) {
        for (uint32_t g : t.groups) {
            if (g != 0) {
                if (const uint8_t* r = FirstCachedQuest(g)) {
                    return r;
                }
            }
        }

        return nullptr;
    }

    const ContractType* FindByName(const std::string& name) {   // FUN_10338f20
        for (const ContractType& t : s_types) {
            if (t.name == name) {
                return &t;
            }
        }

        return nullptr;
    }

    // FUN_10338fe0: the priced types (MoneyCost and TokenCost both set) of a content type.
    std::vector<const ContractType*> ByContentType(const std::string& contentType) {
        std::vector<const ContractType*> out;

        for (const ContractType& t : s_types) {
            if (t.moneyCost != 0 && t.tokenCost != 0 && t.contentType == contentType) {
                out.push_back(&t);
            }
        }

        return out;
    }

    void Enqueue(const std::string& type, bool flag, uint32_t count) {   // FUN_10337620
        s_queue.push_back({ type, static_cast<uint8_t>(flag), count });
    }

    // FUN_10339320: one activation per tick.
    void SendNext() {
        if (s_queue.empty()) {
            return;
        }

        const Activation a = s_queue.front();
        s_queue.pop_front();
        CoA::Packet(0x55B).U32(static_cast<uint32_t>(a.type.size())).Data(a.type.data(), static_cast<uint32_t>(a.type.size()))
            .U8(a.flag).U32(a.count).Send();
    }

    // 0x1033B840: the next tick is armed without storing its id.
    int32_t OnTick(const void*, void*) {
        SendNext();
        CoA::Timer::Schedule(0x32, OnTick, nullptr);
        return 0;
    }

    void OnEnterWorld() {   // FUN_1033b820
        s_timer = CoA::Timer::Schedule(0x32, OnTick, nullptr);
    }

    void OnGlueScreen() {   // FUN_10337120: the map, the types and the queue emptied
        s_types.clear();
        s_queue.clear();
    }

    void OnGlueRegistered() {   // FUN_103392f0
        if (s_timer) {
            CoA::Timer::Cancel(s_timer, OnTick, nullptr);
            s_timer = 0;
        }
    }

    // ---- Lua writers -----------------------------------------------------------------------------------------
    void SetInt(lua_State* L, const char* k, int32_t v) {
        lua_pushstring(L, k);
        lua_pushinteger(L, v);
        lua_settable(L, -3);
    }

    void SetStr(lua_State* L, const char* k, const std::string& v) {
        lua_pushstring(L, k);
        lua_pushstring(L, v.c_str());
        lua_settable(L, -3);
    }

    // The quests of every non-zero group, one array numbered across groups.
    void PushQuests(lua_State* L, const ContractType* t) {
        lua_createtable(L, 0, 0);
        lua_checkstack(L, 2);

        if (!t) {
            return;
        }

        int32_t n = 1;

        for (uint32_t g : t->groups) {
            if (g != 0) {
                for (const uint8_t* r : CoA::BridgeStore::QuestsInGroup(g)) {
                    lua_pushinteger(L, n++);
                    lua_pushinteger(L, static_cast<int32_t>(U32(r, 0)));
                    lua_settable(L, -3);
                }
            }
        }
    }

    int32_t ScaledHonor(uint32_t raw) {   // cvt u32 -> float * RealmInfo +0x10, truncated (0x10ae6220)
        const float scaled = static_cast<float>(static_cast<double>(raw)) * CoA::RealmInfo::Get().rate1;
        return static_cast<int32_t>(static_cast<int64_t>(scaled));
    }

    std::string ArgString(lua_State* L, int idx) {   // FUN_100bf440
        const char* s = lua_tolstring(L, idx, nullptr);
        return s ? s : "";
    }

    bool IsBoolArg(lua_State* L, int idx) {   // nil, boolean or number
        const int t = lua_type(L, idx);
        return t == LUA_TNIL || t == LUA_TBOOLEAN || t == LUA_TNUMBER;
    }

    bool OneString(lua_State* L) {
        return lua_gettop(L) == 1 && lua_isstring(L, 1);
    }

    struct Totals {
        uint32_t money = 0;
        uint32_t arena = 0;
        uint32_t honor = 0;
        uint32_t cachePoints = 0;
        std::vector<std::pair<uint32_t, uint32_t>> items;   // FUN_10336aa0: the item map in list order

        void AddItem(uint32_t item, uint32_t amount, bool accumulate) {
            for (auto& e : this->items) {
                if (e.first == item) {
                    e.second = accumulate ? e.second + amount : amount;
                    return;
                }
            }

            this->items.emplace_back(item, amount);
        }
    };

    uint32_t CachePoints(uint32_t quest, bool& found) {   // AscQuestAddon::Get(quest)->points
        uint32_t points = 0;
        found = s_deps.questAddonPoints && s_deps.questAddonPoints(quest, points);
        return found ? points : 0;
    }

    // One cached quest's rewards, each times `times`.
    void AddQuest(Totals& t, const uint8_t* questRow, uint32_t times, bool accumulate) {
        QuestRecord rec;

        if (!Quest(U32(questRow, 0), rec)) {
            return;
        }

        t.arena += rec.arena * times;
        t.money += rec.money * times;
        t.honor += rec.honor * times;

        for (uint32_t i = 0; i < 4; ++i) {
            const uint32_t item = rec.items[i];
            const uint32_t amount = rec.amounts[i] * times;

            if (item != 0 && amount != 0) {
                t.AddItem(item, amount, accumulate);
            }
        }

        bool found;
        const uint32_t points = CachePoints(U32(questRow, 0), found);

        if (found) {
            t.cachePoints += points * times;
        }
    }

    void PushTotals(lua_State* L, const Totals& t) {
        lua_createtable(L, 0, 0);
        lua_checkstack(L, 2);
        SetInt(L, "RewardMoney", static_cast<int32_t>(t.money));
        SetInt(L, "RewardArenaPoints", static_cast<int32_t>(t.arena));
        SetInt(L, "RewardHonor", ScaledHonor(t.honor));
        lua_pushstring(L, "RewardItems");
        lua_createtable(L, 0, 0);
        lua_checkstack(L, 2);

        for (size_t i = 0; i < t.items.size(); ++i) {
            lua_pushinteger(L, static_cast<int32_t>(i + 1));
            lua_pushinteger(L, static_cast<int32_t>(t.items[i].first));
            lua_settable(L, -3);
        }

        lua_settable(L, -3);
        lua_pushstring(L, "RewardAmount");
        lua_createtable(L, 0, 0);
        lua_checkstack(L, 2);

        for (size_t i = 0; i < t.items.size(); ++i) {
            lua_pushinteger(L, static_cast<int32_t>(i + 1));
            lua_pushinteger(L, static_cast<int32_t>(t.items[i].second));
            lua_settable(L, -3);
        }

        lua_settable(L, -3);
        lua_pushstring(L, "CallboardCachePoints");
        lua_pushnumber(L, static_cast<double>(t.cachePoints));
        lua_settable(L, -3);
    }

    // ---- natives -----------------------------------------------------------------------------------------------
    // FUN_10339ff0: {[Name] = {remainingCompletions, moneyCost, tokenCost, minimumLevel, name, quests}} for every type
    // with completions left and both costs set.
    int GetCompletableTemporalContracts(lua_State* L) {
        lua_createtable(L, 0, 0);
        lua_checkstack(L, 2);

        for (const ContractType& t : s_types) {
            if (t.remaining == 0 || t.moneyCost == 0 || t.tokenCost == 0) {
                continue;
            }

            lua_pushstring(L, t.name.c_str());
            lua_createtable(L, 0, 0);
            lua_checkstack(L, 2);
            SetInt(L, "remainingCompletions", static_cast<int32_t>(t.remaining));
            SetInt(L, "moneyCost", static_cast<int32_t>(t.moneyCost));
            SetInt(L, "tokenCost", static_cast<int32_t>(t.tokenCost));
            SetInt(L, "minimumLevel", static_cast<int32_t>(t.minimumLevel));
            SetStr(L, "name", t.displayName);
            lua_pushstring(L, "quests");
            PushQuests(L, &t);
            lua_settable(L, -3);
            lua_settable(L, -3);
        }

        return 1;
    }

    // FUN_103396a0: (type, flag [, count]) -- queued only while nothing else is pending.
    int ActivateTemporalContract(lua_State* L) {
        if (lua_gettop(L) > 1 && lua_isstring(L, 1) && IsBoolArg(L, 2) && s_queue.empty()) {
            const std::string type = ArgString(L, 1);
            const bool flag = lua_toboolean(L, 2) != 0;
            uint32_t count = 0;

            if (lua_gettop(L) >= 3 && lua_isnumber(L, 3)) {
                count = static_cast<uint32_t>(ToInt(CheckNumber(L, 3)));
            }

            Enqueue(type, flag, count);
            PushBool(L, true);
            return 1;
        }

        PushBool(L, false);
        return 1;
    }

    // FUN_1033a780: (type) -> {remainingCompletions, moneyCost, tokenCost, quests}, zeros when unknown.
    int GetTemporalContractInfo(lua_State* L) {
        if (!OneString(L)) {
            lua_pushnil(L);
            return 1;
        }

        const ContractType* t = FindByName(ArgString(L, 1));
        lua_createtable(L, 0, 0);
        lua_checkstack(L, 2);
        SetInt(L, "remainingCompletions", t ? static_cast<int32_t>(t->remaining) : 0);
        SetInt(L, "moneyCost", t ? static_cast<int32_t>(t->moneyCost) : 0);
        SetInt(L, "tokenCost", t ? static_cast<int32_t>(t->tokenCost) : 0);
        lua_pushstring(L, "quests");
        PushQuests(L, t);
        lua_settable(L, -3);
        return 1;
    }

    // FUN_10339d90: (type) -> the type's quest ids, or nil.
    int GetCompletableQuestsByCategory(lua_State* L) {
        const ContractType* t = OneString(L) ? FindByName(ArgString(L, 1)) : nullptr;

        if (!t) {
            lua_pushnil(L);
            return 1;
        }

        PushQuests(L, t);
        return 1;
    }

    // FUN_103376d0: the built-in id-range table; the second name applies from expansion 1 on.
    const char* BuiltinCategory(uint32_t quest) {
        for (const auto& c : kQuestCategories) {
            if (quest >= c.first && quest <= c.last) {
                return CoA::RealmInfo::Get().ruleset >= 1 ? c.tbc : c.name;
            }
        }

        return nullptr;
    }

    // 0x1033a280: (quest) -> the built-in category, else the first type owning one of the quest's groups, else nil.
    int GetQuestCategory(lua_State* L) {
        if (lua_gettop(L) == 1 && lua_isnumber(L, 1)) {
            const uint32_t quest = static_cast<uint32_t>(ToInt(lua_tonumber(L, 1)));

            if (const char* c = BuiltinCategory(quest)) {
                lua_pushstring(L, c);
                return 1;
            }

            if (const uint8_t* row = CoA::BridgeStore::QuestById(quest)) {   // FUN_101dc700
                for (const ContractType& t : s_types) {
                    for (uint32_t g : t.groups) {
                        for (uint32_t off : { 8u, 12u, 16u }) {
                            if (U32(row, off) == g) {
                                lua_pushstring(L, t.name.c_str());
                                return 1;
                            }
                        }
                    }
                }
            }
        }

        lua_pushnil(L);
        return 1;
    }

    // FUN_1033aa80: {[content type] = {type names...}}.
    int GetTemporalContractTypes(lua_State* L) {
        std::vector<std::pair<std::string, std::vector<std::string>>> byContent;

        for (const ContractType& t : s_types) {
            auto it = byContent.begin();

            while (it != byContent.end() && it->first != t.contentType) {
                ++it;
            }

            if (it == byContent.end()) {
                byContent.emplace_back(t.contentType, std::vector<std::string>());
                it = byContent.end() - 1;
            }

            it->second.push_back(t.name);
        }

        lua_createtable(L, 0, 0);
        lua_checkstack(L, 2);

        for (const auto& e : byContent) {
            lua_pushstring(L, e.first.c_str());
            lua_createtable(L, 0, 0);
            lua_checkstack(L, 2);

            for (size_t i = 0; i < e.second.size(); ++i) {
                lua_pushinteger(L, static_cast<int32_t>(i + 1));
                lua_pushstring(L, e.second[i].c_str());
                lua_settable(L, -3);
            }

            lua_settable(L, -3);
        }

        return 1;
    }

    void EnqueueCompletion(const std::string& type, void* flag) {
        Enqueue(type, *static_cast<bool*>(flag), 0);
    }

    bool ForEachCompletion(const std::string* contentType, bool* flag) {   // FUN_10337270
        return s_deps.forEachCompletion && s_deps.forEachCompletion(contentType, EnqueueCompletion, flag);
    }

    // 0x10339510: (flag) over the whole Callboard view.
    int ActivateAllTemporalContracts(lua_State* L) {
        if (lua_gettop(L) == 1 && IsBoolArg(L, 1) && s_queue.empty()) {
            bool flag = lua_toboolean(L, 1) != 0;
            PushBool(L, ForEachCompletion(nullptr, &flag));
            return 1;
        }

        PushBool(L, false);
        return 1;
    }

    // FUN_103395a0: (content type, flag).
    int ActivateAllTemporalContractsByContentType(lua_State* L) {
        if (lua_gettop(L) == 2 && lua_isstring(L, 1) && IsBoolArg(L, 2) && s_queue.empty()) {
            const std::string contentType = ArgString(L, 1);
            bool flag = lua_toboolean(L, 2) != 0;
            PushBool(L, ForEachCompletion(&contentType, &flag));
            return 1;
        }

        PushBool(L, false);
        return 1;
    }

    // FUN_10339800: (type) -> {rewards (the first cached quest's, unscaled by completions), remainingCompletions,
    // category, moneyCost, tokenCost, minimumLevel, name}, or nil.
    int GetCategoryData(lua_State* L) {
        const ContractType* t = OneString(L) ? FindByName(ArgString(L, 1)) : nullptr;

        if (!t) {
            lua_pushnil(L);
            return 1;
        }

        lua_createtable(L, 0, 0);
        lua_checkstack(L, 2);
        lua_pushstring(L, "rewards");
        lua_createtable(L, 0, 0);
        lua_checkstack(L, 2);

        for (uint32_t g : t->groups) {
            const uint8_t* row = g ? FirstCachedQuest(g) : nullptr;
            QuestRecord rec;

            if (!row || !Quest(U32(row, 0), rec)) {
                continue;
            }

            SetInt(L, "RewardMoney", static_cast<int32_t>(rec.money));
            SetInt(L, "RewardArenaPoints", static_cast<int32_t>(rec.arena));
            SetInt(L, "RewardHonor", ScaledHonor(rec.honor));

            for (const char* key : { "RewardItems", "RewardAmount" }) {
                const uint32_t* values = key[6] == 'I' ? rec.items : rec.amounts;
                lua_pushstring(L, key);
                lua_createtable(L, 0, 0);
                lua_checkstack(L, 2);

                for (uint32_t i = 0; i < 4; ++i) {
                    lua_pushinteger(L, static_cast<int32_t>(i + 1));
                    lua_pushinteger(L, static_cast<int32_t>(values[i]));
                    lua_settable(L, -3);
                }

                lua_settable(L, -3);
            }

            lua_pushstring(L, "CallboardCachePoints");
            bool found;
            const uint32_t points = CachePoints(U32(row, 0), found);
            lua_pushnumber(L, found ? static_cast<double>(points) : 0.0);
            lua_settable(L, -3);
            break;
        }

        lua_settable(L, -3);
        SetInt(L, "remainingCompletions", static_cast<int32_t>(t->remaining));
        SetStr(L, "category", t->contentType);
        SetInt(L, "moneyCost", static_cast<int32_t>(t->moneyCost));
        SetInt(L, "tokenCost", static_cast<int32_t>(t->tokenCost));
        SetInt(L, "minimumLevel", static_cast<int32_t>(t->minimumLevel));
        SetStr(L, "name", t->displayName);
        return 1;
    }

    // FUN_10339cb0: (type) -> its display name, or nil.
    int GetCategoryName(lua_State* L) {
        const ContractType* t = OneString(L) ? FindByName(ArgString(L, 1)) : nullptr;

        if (!t) {
            lua_pushnil(L);
            return 1;
        }

        lua_pushstring(L, t->displayName.c_str());
        return 1;
    }

    // FUN_1033ae00: (content type) -> the rewards of every remaining completion of its priced types, one cached quest
    // per group.
    int GetTotalContentTypeRewards(lua_State* L) {
        if (!OneString(L)) {
            lua_pushnil(L);
            return 1;
        }

        Totals total;

        for (const ContractType* t : ByContentType(ArgString(L, 1))) {
            if (t->remaining != 0) {
                for (uint32_t g : t->groups) {
                    if (g != 0) {
                        if (const uint8_t* row = FirstCachedQuest(g)) {
                            AddQuest(total, row, t->remaining, true);
                        }
                    }
                }
            }
        }

        PushTotals(L, total);
        return 1;
    }

    // FUN_1033b410: (type) -> its first cached quest's rewards times the remaining completions, or nil.
    int GetTotalContractTypeRewards(lua_State* L) {
        const ContractType* t = OneString(L) ? FindByName(ArgString(L, 1)) : nullptr;

        if (!t) {
            lua_pushnil(L);
            return 1;
        }

        Totals total;
        const uint8_t* row = FirstCachedQuest(*t);

        if (row && t->moneyCost != 0 && t->tokenCost != 0) {
            AddQuest(total, row, t->remaining, false);
        }

        PushTotals(L, total);
        return 1;
    }

    // FUN_1033ac60: (content type) -> {moneyCost, tokenCost} over its priced types' completions.
    int GetTotalContentTypeCost(lua_State* L) {
        if (!OneString(L)) {
            lua_pushnil(L);
            return 1;
        }

        uint32_t money = 0;
        uint32_t token = 0;

        for (const ContractType* t : ByContentType(ArgString(L, 1))) {
            if (t->remaining != 0) {
                money += t->moneyCost * t->remaining;
                token += t->tokenCost * t->remaining;
            }
        }

        lua_createtable(L, 0, 0);
        lua_checkstack(L, 2);
        SetInt(L, "moneyCost", static_cast<int32_t>(money));
        SetInt(L, "tokenCost", static_cast<int32_t>(token));
        return 1;
    }

    // FUN_1033b2e0: (type) -> {moneyCost, tokenCost} for its remaining completions, zeros when unknown.
    int GetTotalContractTypeCost(lua_State* L) {
        if (!OneString(L)) {
            lua_pushnil(L);
            return 1;
        }

        uint32_t money = 0;
        uint32_t token = 0;
        const ContractType* t = FindByName(ArgString(L, 1));

        if (t && t->moneyCost != 0 && t->tokenCost != 0) {
            money = t->moneyCost * t->remaining;
            token = t->tokenCost * t->remaining;
        }

        lua_createtable(L, 0, 0);
        lua_checkstack(L, 2);
        SetInt(L, "moneyCost", static_cast<int32_t>(money));
        SetInt(L, "tokenCost", static_cast<int32_t>(token));
        return 1;
    }

    // FUN_1033a3b0: (quest, includeZone) -> [the cached quest's ZoneOrSort when asked and set,] then the Quest.dbc row's
    // non-zero sort ids (+0x68..+0x70). Nothing while either record is missing.
    int GetQuestSortIDs(lua_State* L) {
        if (!ValidateInput(L, { LUA_TNUMBER, LUA_TBOOLEAN })) {
            return 0;
        }

        const uint32_t quest = static_cast<uint32_t>(ToInt(CheckNumber(L, 1)));
        const bool includeZone = lua_toboolean(L, 2) != 0;
        QuestRecord rec;
        const bool cached = Quest(quest, rec);
        const uint8_t* row = cached ? CoA::BridgeStore::QuestById(quest) : nullptr;

        if (!row) {
            return 0;
        }

        std::vector<uint32_t> ids;

        if (includeZone && rec.zoneOrSort != 0) {
            ids.push_back(rec.zoneOrSort);
        }

        for (uint32_t off = 0x68; off != 0x74; off += 4) {
            if (U32(row, off) != 0) {
                ids.push_back(U32(row, off));
            }
        }

        lua_createtable(L, 0, static_cast<int>(ids.size()));
        lua_checkstack(L, 2);

        for (size_t i = 0; i < ids.size(); ++i) {
            lua_pushnumber(L, static_cast<double>(i + 1));
            lua_pushinteger(L, static_cast<int32_t>(ids[i]));   // FUN_1009b2a0
            lua_settable(L, -3);
        }

        return 1;
    }

    // FUN_1033a5f0: (sort) -> AreaTable name (+0x2C) for a positive id, QuestSort name (+4) for a negative one, else
    // "Unknown".
    int GetQuestSortText(lua_State* L) {
        if (!ValidateInput(L, { LUA_TNUMBER })) {
            return 0;
        }

        const int32_t sort = ToInt(CheckNumber(L, 1));
        const char* text = "Unknown";

        if (sort >= 1) {
            if (const char* name = s_deps.areaName ? s_deps.areaName(static_cast<uint32_t>(sort)) : nullptr) {
                text = name;
            }
        } else if (sort < 0) {
            if (const char* name = s_deps.questSortName ? s_deps.questSortName(static_cast<uint32_t>(-sort)) : nullptr) {
                text = name;
            }
        }

        lua_pushstring(L, text);
        return 1;
    }

    const CoA::Binding s_bindings[] = {
        COA_NATIVE("C_TemporalContracts", "GetCompletableTemporalContracts", GetCompletableTemporalContracts),
        COA_NATIVE("C_TemporalContracts", "ActivateTemporalContract", ActivateTemporalContract),
        COA_NATIVE("C_TemporalContracts", "GetTemporalContractInfo", GetTemporalContractInfo),
        COA_NATIVE("C_TemporalContracts", "GetCompletableQuestsByCategory", GetCompletableQuestsByCategory),
        COA_NATIVE("C_TemporalContracts", "GetQuestCategory", GetQuestCategory),
        COA_NATIVE("C_TemporalContracts", "GetTemporalContractTypes", GetTemporalContractTypes),
        COA_NATIVE("C_TemporalContracts", "ActivateAllTemporalContracts", ActivateAllTemporalContracts),
        COA_NATIVE("C_TemporalContracts", "ActivateAllTemporalContractsByContentType", ActivateAllTemporalContractsByContentType),
        COA_NATIVE("C_TemporalContracts", "GetCategoryData", GetCategoryData),
        COA_NATIVE("C_TemporalContracts", "GetCategoryName", GetCategoryName),
        COA_NATIVE("C_TemporalContracts", "GetTotalContentTypeRewards", GetTotalContentTypeRewards),
        COA_NATIVE("C_TemporalContracts", "GetTotalContractTypeRewards", GetTotalContractTypeRewards),
        COA_NATIVE("C_TemporalContracts", "GetTotalContentTypeCost", GetTotalContentTypeCost),
        COA_NATIVE("C_TemporalContracts", "GetTotalContractTypeCost", GetTotalContractTypeCost),
        COA_NATIVE("C_TemporalContracts", "GetQuestSortIDs", GetQuestSortIDs),
        COA_NATIVE("C_TemporalContracts", "GetQuestSortText", GetQuestSortText),
    };
}

CoA::TemporalContracts::Deps& CoA::TemporalContracts::Dependencies() {
    return s_deps;
}

void CoA::TemporalContracts::Init() {   // FUN_10339120
    CoA::LuaReg::AddModule(s_bindings, sizeof(s_bindings) / sizeof(s_bindings[0]));   // 0x1033bcb0
    CoA::Lifecycle::OnGlueScreen(OnGlueScreen);
    // TEMPORAL_CONTRACTS_UPDATED is registered here (queued with every DLL name by CoA::Events::RegisterNames); only the
    // VM-protected filler fires it.
    CoA::ClientHooks::enterWorld.Add(OnEnterWorld);
    CoA::LuaReg::OnGlueRegistered(OnGlueRegistered);
}

void CoA::TemporalContracts::Reset() {
    OnGlueScreen();
    s_timer = 0;
    s_deps = Deps();
}

void CoA::TemporalContracts::SetTypes(const std::vector<ContractType>& types) {
    s_types = types;
}
