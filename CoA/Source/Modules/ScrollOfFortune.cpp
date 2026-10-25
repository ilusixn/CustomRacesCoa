// AscScrollOfFortune.cpp: C_WildcardRewards, the Scroll of Fortune rewards (manager FUN_10a38f20 -> static 0x10D3D608),
// the wildcard twin of C_DraftRewards (DraftMode.cpp, whose client questions it shares).
//
// SMSG_SCROLL_OF_FORTUNE_REWARDS_LIST 0x6AB (FUN_10a38a30): list and both maps cleared, then a CompressedDataStore: u32 n
//   x 0x31-byte records {u32 tier, u8 flag, u32 level, u32 item[4], u32 count[4], u32 cost, u32 release}, indexed by tier
//   in the map for their flag (+0x0C when set, +0x2C when clear). Always fires SCROLL_OF_FORTUNE_REWARDS_LIST_UPDATED.
// SMSG_COLLECTED_SCROLL_OF_FORTUNE_REWARDS_LIST 0x6B8 (FUN_10a388a0): claimed list (+0x4C) replaced by u32 n x 12 bytes
//   {u32, u32 claimed (flag), u32 claimed (no flag)}; limits (+0x58) replaced by u32 n x 28 bytes {u32 tier, u32 x6};
//   fires SCROLL_OF_FORTUNE_LIMITS_UPDATED.
// SMSG_COLLECT_SCROLL_OF_FORTUNE_REWARDS_RESULT 0x6B6 (FUN_10a38630): cstring; on _OK the last claim (+0x64 tier, +0x68
//   count, +0x6C flag) is added to the claimed list; then COLLECT_SCROLL_OF_FORTUNE_REWARDS_RESULT("%s").
// ClaimScrollOfFortuneRewards sends CMSG 0x6B5 {u32 tier, u32 count, u8 flag}. Every glue screen empties everything
// (FUN_10a38070).
#include "CoA/Source/Modules/DraftMode.h"
#include "CoA/Source/Modules/GameEvents.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/Events.h"
#include "CoA/Source/Runtime/Lifecycle.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Packet.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Source/Runtime/Read.h"
#include "CoA/Source/Runtime/Script.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

namespace {
#pragma pack(push, 1)
    struct Reward {
        uint32_t tier;
        uint8_t flag;
        uint32_t level;
        uint32_t items[4];
        uint32_t counts[4];
        uint32_t cost;
        uint32_t release;
    };
#pragma pack(pop)

    static_assert(sizeof(Reward) == 0x31, "scroll of fortune reward");

    struct Claimed {
        uint32_t v[3];
    };

    struct Limit {
        uint32_t tier;
        uint32_t v[6];
    };

    struct State {
        std::vector<Reward> rewards;
        std::unordered_map<uint32_t, std::vector<uint32_t>> maps[2];   // [flag] -> tier -> reward indices
        std::vector<Claimed> claimed;
        std::vector<Limit> limits;
        uint32_t lastTier = 0;
        uint32_t lastCount = 0;
        bool lastFlag = false;
    };

    State s_sof;

    void ResetState() {   // FUN_10a38070
        s_sof = State();
    }

    int32_t OnRewardsList(void*, uint32_t, uint32_t, CDataStore* msg) {
        s_sof.rewards.clear();
        s_sof.maps[0].clear();
        s_sof.maps[1].clear();
        std::vector<uint8_t> data;

        if (CoA::Read::Inflate(msg, data) && data.size() >= 4) {
            uint32_t n;
            memcpy(&n, data.data(), 4);
            size_t at = 4;

            for (uint32_t i = 0; i < n && at + sizeof(Reward) <= data.size(); ++i, at += sizeof(Reward)) {
                Reward r;
                memcpy(&r, data.data() + at, sizeof(r));
                s_sof.rewards.push_back(r);
                s_sof.maps[r.flag != 0][r.tier].push_back(static_cast<uint32_t>(s_sof.rewards.size() - 1));
            }
        }

        CoA::Events::Signal("SCROLL_OF_FORTUNE_REWARDS_LIST_UPDATED", nullptr);
        return 1;
    }

    template <class T>
    void ReadRecords(CDataStore* msg, std::vector<T>& out) {
        out.clear();

        for (uint32_t n = CoA::Read::U32(msg); n && CoA::Read::Remaining(msg) >= sizeof(T); --n) {
            T v;
            CoA::Read::Data(msg, &v, sizeof(v));
            out.push_back(v);
        }
    }

    int32_t OnClaimedList(void*, uint32_t, uint32_t, CDataStore* msg) {
        ReadRecords(msg, s_sof.claimed);
        ReadRecords(msg, s_sof.limits);
        CoA::Events::Signal("SCROLL_OF_FORTUNE_LIMITS_UPDATED", nullptr);
        return 1;
    }

    const char* const kResults[8] = { "COLLECT_SCROLL_OF_FORTUNE_REWARDS_OK", "COLLECT_SCROLL_OF_FORTUNE_REWARDS_UNKNOWN",
        "COLLECT_SCROLL_OF_FORTUNE_REWARDS_SPECIALIZATION_OUT_OF_RANGE", "COLLECT_SCROLL_OF_FORTUNE_REWARDS_COUNT_TOO_HIGH",
        "COLLECT_SCROLL_OF_FORTUNE_REWARDS_INVALID_COUNT", "COLLECT_SCROLL_OF_FORTUNE_REWARDS_NOT_ENOUGH_MARKS_OF_ASCENSION",
        "COLLECT_SCROLL_OF_FORTUNE_REWARDS_TOO_LOW_LEVEL", "COLLECT_SCROLL_OF_FORTUNE_REWARDS_NOT_RELEASED" };

    int32_t OnClaimResult(void*, uint32_t, uint32_t, CDataStore* msg) {
        const std::string result = CoA::Read::Str(msg);

        if (result == kResults[0]) {
            if (s_sof.claimed.size() <= s_sof.lastTier) {
                s_sof.claimed.resize(s_sof.lastTier + 1, Claimed {});
            }

            s_sof.claimed[s_sof.lastTier].v[s_sof.lastFlag ? 1 : 2] += s_sof.lastCount;
        }

        CoA::Events::Signal("COLLECT_SCROLL_OF_FORTUNE_REWARDS_RESULT", "%s", result.c_str());
        return 1;
    }

    uint32_t ClaimedCount(uint32_t tier, bool flag) {
        return tier < s_sof.claimed.size() ? s_sof.claimed[tier].v[flag ? 1 : 2] : 0;
    }

    // FUN_10a38400: consecutive rewards of the tier from the 1-based `start`, stopping at a gap.
    std::vector<const Reward*> NextRewards(uint32_t tier, uint32_t start, uint32_t count, bool flag) {
        std::vector<const Reward*> out;
        const auto& m = s_sof.maps[flag];
        const auto it = m.find(tier);

        for (uint32_t i = start; i < start + count; ++i) {
            if (i == 0 || it == m.end() || i - 1 >= it->second.size()) {
                break;
            }

            out.push_back(&s_sof.rewards[it->second[i - 1]]);
        }

        return out;
    }

    // FUN_10a380e0
    std::vector<const Reward*> Totals(uint32_t tier, uint32_t count, bool flag, std::unordered_map<uint32_t, uint32_t>& items, uint32_t& cost) {
        cost = 0;
        const std::vector<const Reward*> recs = NextRewards(tier, ClaimedCount(tier, flag) + 1, count, flag);

        for (const Reward* r : recs) {
            cost += r->cost;

            for (int k = 0; k < 4; ++k) {
                if (r->items[k] && r->counts[k]) {
                    items[r->items[k]] += r->counts[k];
                }
            }
        }

        return recs;
    }

    int64_t Now() {
        const CoA::DraftMode::Deps& d = CoA::DraftMode::Dependencies();
        return d.now ? d.now() : 0;
    }

    // FUN_10a37eb0
    uint32_t CheckClaim(uint32_t tier, uint32_t count, bool flag) {
        if (tier >= s_sof.claimed.size()) {
            return 2;
        }

        if (count > 100) {
            return 3;
        }

        const std::vector<const Reward*> recs = NextRewards(tier, ClaimedCount(tier, flag) + 1, count, flag);

        if (recs.size() != count) {
            return 4;
        }

        std::unordered_map<uint32_t, uint32_t> items;
        uint32_t cost;
        Totals(tier, count, flag, items, cost);
        const CoA::DraftMode::Deps& d = CoA::DraftMode::Dependencies();

        if ((d.itemCount ? d.itemCount(0x5B9D2) : 0) < cost) {
            return 5;
        }

        const int64_t now = Now();

        for (const Reward* r : recs) {
            if (d.hasPlayer && d.hasPlayer() && (d.playerLevel ? d.playerLevel() : 0) < r->level) {
                return 6;
            }

            if (now < static_cast<int64_t>(r->release)) {
                return 7;
            }
        }

        return 0;
    }

    bool ReadClaimArgs(lua_State* L, uint32_t& tier, uint32_t& count, bool& flag) {   // FUN_10221e50
        if (!CoA::Script::ValidateInput(L, { LUA_TNUMBER, LUA_TNUMBER, LUA_TBOOLEAN })) {
            return false;
        }

        tier = static_cast<uint32_t>(static_cast<int64_t>(CoA::Script::CheckNumber(L, 1)));
        count = static_cast<uint32_t>(static_cast<int64_t>(CoA::Script::CheckNumber(L, 2)));
        flag = lua_toboolean(L, 3) != 0;
        return true;
    }

    bool ReadTierFlag(lua_State* L, uint32_t& tier, bool& flag) {   // FUN_100e92d0
        if (!CoA::Script::ValidateInput(L, { LUA_TNUMBER, LUA_TBOOLEAN })) {
            return false;
        }

        tier = static_cast<uint32_t>(static_cast<int64_t>(CoA::Script::CheckNumber(L, 1)));
        flag = lua_toboolean(L, 2) != 0;
        return true;
    }

    void PushItems(lua_State* L, const std::unordered_map<uint32_t, uint32_t>& items) {   // FUN_102222e0
        lua_createtable(L, 0, static_cast<int>(items.size()));
        lua_checkstack(L, 2);

        for (const auto& e : items) {
            CoA::Script::PushInt(L, static_cast<int32_t>(e.first));
            CoA::Script::PushInt(L, static_cast<int32_t>(e.second));
            lua_settable(L, -3);
        }
    }

    // GetMarkOfAscensionCost(tier, count, flag) (0x10a39880): the Marks the next `count` claims cost.
    int GetMarkOfAscensionCost(lua_State* L) {
        uint32_t tier;
        uint32_t count;
        bool flag;

        if (!ReadClaimArgs(L, tier, count, flag) || tier == 0) {
            return 0;
        }

        std::unordered_map<uint32_t, uint32_t> items;
        uint32_t cost;
        Totals(tier - 1, count, flag, items, cost);
        CoA::Script::PushInt(L, static_cast<int32_t>(cost));
        return 1;
    }

    // GetRewards(tier, count, flag) (0x10a39980): {item = count} the next `count` claims give.
    int GetRewards(lua_State* L) {
        uint32_t tier;
        uint32_t count;
        bool flag;

        if (!ReadClaimArgs(L, tier, count, flag) || tier == 0) {
            return 0;
        }

        std::unordered_map<uint32_t, uint32_t> items;
        uint32_t cost;
        Totals(tier - 1, count, flag, items, cost);
        PushItems(L, items);
        return 1;
    }

    // GetLevelingInfo(tier) (0x10a39330): the tier's flagged rewards up to the server's max level, as {Level, Rewards =
    // {item = count} (later slots overwrite a repeated item), IsClaimed}.
    int GetLevelingInfo(lua_State* L) {
        uint32_t tier;

        if (!CoA::Script::ReadNumber(L, tier) || tier == 0) {
            return 0;
        }

        const uint32_t t = tier - 1;

        struct Entry {
            uint32_t level;
            std::unordered_map<uint32_t, uint32_t> items;
            bool claimed;
        };

        std::vector<Entry> entries;
        const auto& m = s_sof.maps[1];
        const auto it = m.find(t);

        if (it != m.end()) {
            for (size_t i = 0; i < it->second.size(); ++i) {
                const Reward& r = s_sof.rewards[it->second[i]];

                if (r.level > CoA::GameEvents::ServerMaxLevel()) {
                    continue;
                }

                Entry e { r.level, {}, false };

                for (int k = 0; k < 4; ++k) {
                    if (r.items[k] && r.counts[k]) {
                        e.items[r.items[k]] = r.counts[k];
                    }
                }

                const uint32_t claimed = t < s_sof.claimed.size() ? s_sof.claimed[t].v[1] : 0;
                e.claimed = i + 1 <= claimed;
                entries.push_back(std::move(e));
            }
        }

        lua_createtable(L, 0, 0);
        lua_checkstack(L, 2);

        for (size_t i = 0; i < entries.size(); ++i) {
            CoA::Script::PushInt(L, static_cast<int32_t>(i + 1));
            lua_createtable(L, 0, 0);
            lua_checkstack(L, 2);
            lua_pushstring(L, "Level");
            CoA::Script::PushInt(L, static_cast<int32_t>(entries[i].level));
            lua_settable(L, -3);
            lua_pushstring(L, "Rewards");
            PushItems(L, entries[i].items);
            lua_settable(L, -3);
            lua_pushstring(L, "IsClaimed");
            CoA::Script::PushBool(L, entries[i].claimed);
            lua_settable(L, -3);
            lua_settable(L, -3);
        }

        return 1;
    }

    int CanClaimScrollOfFortuneRewards(lua_State* L) {
        uint32_t tier;
        uint32_t count;
        bool flag;

        if (!ReadClaimArgs(L, tier, count, flag) || tier == 0) {
            return 0;
        }

        const uint32_t code = CheckClaim(tier - 1, count, flag);
        CoA::Script::PushBool(L, code == 0);

        if (code != 0) {
            CoA::Script::PushStr(L, code < 8 ? kResults[code] : ("UNEXPECTED_ENUM_VALUE_" + std::to_string(code)).c_str());
        } else {
            lua_pushnil(L);
        }

        return 2;
    }

    int ClaimScrollOfFortuneRewards(lua_State* L) {
        uint32_t tier;
        uint32_t count;
        bool flag;

        if (!ReadClaimArgs(L, tier, count, flag) || tier == 0) {
            return 0;
        }

        if (CheckClaim(tier - 1, count, flag) != 0) {
            CoA::Script::PushBool(L, false);
            return 1;
        }

        CoA::Packet(0x6B5).U32(tier - 1).U32(count).U8(flag ? 1 : 0).Send();
        s_sof.lastTier = tier - 1;
        s_sof.lastCount = count;
        s_sof.lastFlag = flag;
        CoA::Script::PushBool(L, true);
        return 1;
    }

    int GetNumClaimableScrollOfFortuneRewards(lua_State* L) {
        uint32_t tier;
        bool flag;

        if (!ReadTierFlag(L, tier, flag) || tier == 0) {
            return 0;
        }

        uint32_t n = 0;

        while (n < 0x4000 && CheckClaim(tier - 1, n + 1, flag) == 0) {
            ++n;
        }

        CoA::Script::PushInt(L, static_cast<int32_t>(n));
        return 1;
    }

    int GetNumClaimedScrollOfFortuneRewards(lua_State* L) {
        uint32_t tier;
        bool flag;

        if (!ReadTierFlag(L, tier, flag) || tier == 0) {
            return 0;
        }

        CoA::Script::PushInt(L, static_cast<int32_t>(ClaimedCount(tier - 1, flag)));
        return 1;
    }

    // handler_GetScrollOfFortuneLimits: {ability = {used, cap}, talent = ..., generic = ...} for the limits entry of the
    // tier; nothing when there is none.
    void SetLimit(lua_State* L, const char* k, uint32_t used, uint32_t cap) {
        lua_pushstring(L, k);
        lua_createtable(L, 0, 2);
        lua_pushstring(L, "used");
        CoA::Script::PushInt(L, static_cast<int32_t>(used));
        lua_settable(L, -3);
        lua_pushstring(L, "cap");
        CoA::Script::PushInt(L, static_cast<int32_t>(cap));
        lua_settable(L, -3);
        lua_settable(L, -3);
    }

    int GetScrollOfFortuneLimits(lua_State* L) {
        uint32_t tier;

        if (!CoA::Script::ReadNumber(L, tier) || tier == 0) {
            return 0;
        }

        for (const Limit& l : s_sof.limits) {
            if (l.tier == tier - 1) {
                lua_createtable(L, 0, 3);
                lua_checkstack(L, 3);
                SetLimit(L, "ability", l.v[3], l.v[0]);
                SetLimit(L, "talent", l.v[4], l.v[1]);
                SetLimit(L, "generic", l.v[5], l.v[2]);
                return 1;
            }
        }

        return 0;
    }

    // FUN_10a39a80: (released, total, seconds until the next release) for the tier's no-flag rewards.
    int GetScrollOfFortuneRewardsReleaseInfo(lua_State* L) {
        uint32_t tier;

        if (!CoA::Script::ReadNumber(L, tier) || tier == 0) {
            return 0;
        }

        const auto it = s_sof.maps[0].find(tier - 1);

        if (it == s_sof.maps[0].end() || it->second.empty()) {
            return CoA::Script::PushNils(L, 3);
        }

        const uint32_t now = static_cast<uint32_t>(Now());
        uint32_t released = 0;
        uint32_t next = 0;

        for (uint32_t idx : it->second) {
            next = s_sof.rewards[idx].release;

            if (next > now) {
                break;
            }

            ++released;
        }

        CoA::Script::PushInt(L, static_cast<int32_t>(released));
        CoA::Script::PushInt(L, static_cast<int32_t>(it->second.size()));

        if (released < it->second.size()) {
            CoA::Script::PushInt(L, static_cast<int32_t>(next - now));
        } else {
            lua_pushnil(L);
        }

        return 3;
    }

    const CoA::Binding s_bindings[] = {
        COA_NATIVE("C_WildcardRewards", "CanClaimScrollOfFortuneRewards", CanClaimScrollOfFortuneRewards),
        COA_NATIVE("C_WildcardRewards", "ClaimScrollOfFortuneRewards", ClaimScrollOfFortuneRewards),
        COA_NATIVE("C_WildcardRewards", "GetNumClaimableScrollOfFortuneRewards", GetNumClaimableScrollOfFortuneRewards),
        COA_NATIVE("C_WildcardRewards", "GetNumClaimedScrollOfFortuneRewards", GetNumClaimedScrollOfFortuneRewards),
        COA_NATIVE("C_WildcardRewards", "GetScrollOfFortuneLimits", GetScrollOfFortuneLimits),
        COA_NATIVE("C_WildcardRewards", "GetScrollOfFortuneRewardsReleaseInfo", GetScrollOfFortuneRewardsReleaseInfo),
        COA_NATIVE("C_WildcardRewards", "GetMarkOfAscensionCost", GetMarkOfAscensionCost),
        COA_NATIVE("C_WildcardRewards", "GetRewards", GetRewards),
        COA_NATIVE("C_WildcardRewards", "GetLevelingInfo", GetLevelingInfo),
    };
}

void CoA::ScrollOfFortune::Init() {
    CoA::LuaReg::AddModule(s_bindings, sizeof(s_bindings) / sizeof(s_bindings[0]));
    CoA::Lifecycle::OnGlueScreen(ResetState);
    COA_PACKET(0x6AB, OnRewardsList);
    COA_PACKET(0x6B6, OnClaimResult);
    COA_PACKET(0x6B8, OnClaimedList);
    COA_EVENT("SCROLL_OF_FORTUNE_REWARDS_LIST_UPDATED");
    COA_EVENT("SCROLL_OF_FORTUNE_LIMITS_UPDATED");
    COA_EVENT("COLLECT_SCROLL_OF_FORTUNE_REWARDS_RESULT");
}

void CoA::ScrollOfFortune::Reset() {
    ResetState();
}
