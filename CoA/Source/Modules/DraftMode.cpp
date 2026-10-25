// AscDraftMode.cpp (see DraftMode.h).
#include "CoA/Source/Modules/DraftMode.h"
#include "CoA/Source/Modules/GameEvents.h"
#include "CoA/Source/Modules/GameMode.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/Events.h"
#include "CoA/Source/Runtime/Lifecycle.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Packet.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Source/Runtime/Read.h"
#include "CoA/Source/Runtime/Script.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <algorithm>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

namespace {
    CoA::DraftMode::Deps s_deps;

    struct Draft {
        uint32_t selected = 0;                // +0x04
        uint32_t field8 = 0;                  // +0x08
        std::vector<uint32_t> picks;          // +0x0C
        std::vector<uint32_t> hof;            // +0x18
        std::vector<uint32_t> sacrifice;      // +0x24
        uint8_t bytes[0x3A] = {};             // +0x30..+0x69

        uint8_t& Byte(uint32_t off) {
            return this->bytes[off - 0x30];
        }

        uint8_t Byte(uint32_t off) const {
            return this->bytes[off - 0x30];
        }

        uint32_t Dword(uint32_t off) const {
            uint32_t v;
            memcpy(&v, &this->bytes[off - 0x30], 4);
            return v;
        }

        void SetDword(uint32_t off, uint32_t v) {
            memcpy(&this->bytes[off - 0x30], &v, 4);
        }

        bool Same(const Draft& o, uint32_t from, uint32_t to) const {
            return memcmp(&this->bytes[from - 0x30], &o.bytes[from - 0x30], to - from + 1) == 0;
        }
    };

    Draft s_draft;

    // FUN_102226a0 + FUN_10224ad0 and the metadata clear shared by the reset and the packet.
    void ResetState() {
        s_draft = Draft();
    }

    // {u32 spell, u8 a, u8 b} x n; the first three set their a / b bytes at the two offset pairs.
    void ReadPicks(CDataStore* msg, std::vector<uint32_t>& out, uint32_t a1, uint32_t b1, uint32_t a2, uint32_t b2) {
        Draft& d = s_draft;
        const uint8_t n = CoA::Read::U8(msg);

        for (uint8_t i = 0; i < n && !CoA::Read::Failed(msg); ++i) {
            out.push_back(CoA::Read::U32(msg));
            const uint8_t a = CoA::Read::U8(msg);
            const uint8_t b = CoA::Read::U8(msg);

            if (i < 3) {
                d.Byte(a1 + i) = a;
                d.Byte(b1 + i) = b;
                d.Byte(a2 + i) = a;
                d.Byte(b2 + i) = b;
            }
        }
    }

    int32_t OnDraftState(void*, uint32_t, uint32_t, CDataStore* msg) {
        const Draft old = s_draft;
        ResetState();
        Draft& d = s_draft;
        d.field8 = CoA::Read::U32(msg);
        d.Byte(0x30) = CoA::Read::U8(msg);
        d.SetDword(0x34, CoA::Read::U32(msg));
        d.SetDword(0x38, CoA::Read::U32(msg));
        d.Byte(0x3C) = CoA::Read::U8(msg);
        d.SetDword(0x40, CoA::Read::U32(msg));

        for (uint32_t off = 0x44; off <= 0x49; ++off) {
            d.Byte(off) = CoA::Read::U8(msg);
        }

        d.Byte(0x58) = d.Byte(0x3C);
        d.Byte(0x5F) = d.Byte(0x44);
        d.Byte(0x66) = d.Byte(0x45);
        d.Byte(0x67) = d.Byte(0x46);
        d.Byte(0x68) = d.Byte(0x47);
        d.Byte(0x69) = d.Byte(0x48);
        ReadPicks(msg, d.picks, 0x59, 0x5C, 0x4A, 0x4D);
        ReadPicks(msg, d.hof, 0x60, 0x63, 0x50, 0x53);
        const uint8_t sacrifices = CoA::Read::U8(msg);

        for (uint8_t i = 0; i < sacrifices && !CoA::Read::Failed(msg); ++i) {
            d.sacrifice.push_back(CoA::Read::U32(msg));
        }

        const uint64_t skip = static_cast<uint64_t>(CoA::Read::U32(msg)) * 8;

        if (!CoA::Read::Failed(msg)) {
            msg->m_read = skip <= CoA::Read::Remaining(msg) ? msg->m_read + static_cast<uint32_t>(skip) : msg->m_size + 1;
        }

        const uint32_t sel = d.Dword(0x34);
        uint32_t index = static_cast<uint32_t>(d.hof.size());

        if (sel != 0) {
            index = static_cast<uint32_t>(std::find(d.hof.begin(), d.hof.end(), sel) - d.hof.begin());
        }

        d.selected = sel;

        if ((d.Byte(0x68) || d.Byte(0x69)) && index < d.hof.size() && index < 3) {
            d.Byte(0x68) = d.Byte(0x60 + index);
            d.Byte(0x69) = d.Byte(0x63 + index);
        }

        const bool picksChanged = old.picks != d.picks;
        const bool hofChanged = old.hof != d.hof || old.Byte(0x30) != d.Byte(0x30) || old.Dword(0x38) != d.Dword(0x38) || old.Dword(0x34) != d.Dword(0x34);
        const bool sacrificeChanged = old.sacrifice != d.sacrifice;
        const bool selectedChanged = old.selected != d.selected;
        const bool pickMetaChanged = !old.Same(d, 0x58, 0x5E);
        const bool hofMetaChanged = !old.Same(d, 0x5F, 0x65);
        const bool sacrificeMetaChanged = old.Byte(0x66) != d.Byte(0x66);
        const bool selectedMetaChanged = !old.Same(d, 0x67, 0x69);

        if (picksChanged) {
            CoA::Events::Signal("ASCENSION_DRAFTMODE_DRAFT_PICK_CHANGED", nullptr);
        }

        if (hofChanged) {
            CoA::Events::Signal("ASCENSION_DRAFTMODE_HAND_OF_FATE_CHANGED", nullptr);
        }

        if (sacrificeChanged) {
            CoA::Events::Signal("ASCENSION_DRAFTMODE_HAND_OF_FATE_SACRIFICE_CHANGED", nullptr);
        }

        if (selectedChanged) {
            CoA::Events::Signal("ASCENSION_DRAFTMODE_HAND_OF_FATE_SELECTED_CHANGED", nullptr);
        }

        if (pickMetaChanged) {
            CoA::Events::Signal("ASCENSION_DRAFTMODE_DRAFT_PICK_METADATA_CHANGED", nullptr);
        }

        if (hofMetaChanged) {
            CoA::Events::Signal("ASCENSION_DRAFTMODE_HAND_OF_FATE_METADATA_CHANGED", nullptr);
        }

        if (sacrificeMetaChanged) {
            CoA::Events::Signal("ASCENSION_DRAFTMODE_HAND_OF_FATE_SACRIFICE_METADATA_CHANGED", nullptr);
        }

        if (selectedMetaChanged) {
            CoA::Events::Signal("ASCENSION_DRAFTMODE_HAND_OF_FATE_SELECTED_METADATA_CHANGED", nullptr);
        }

        const bool any = picksChanged || hofChanged || sacrificeChanged || selectedChanged || pickMetaChanged || hofMetaChanged || sacrificeMetaChanged ||
            selectedMetaChanged;

        if (any || old.Byte(0x49) != d.Byte(0x49)) {
            CoA::Events::Signal("ASCENSION_DRAFT_STATE_UPDATED", nullptr);
        }

        return 1;
    }

    bool DraftModeOn() {   // FUN_1031ef10 bit 3
        return ((CoA::GameMode::Mode() >> 3) & 1) != 0;
    }

    int PushFlag(lua_State* L, uint32_t off) {
        CoA::Script::PushBool(L, s_draft.Byte(off) != 0);
        return 1;
    }

    int GetDraftModePickCount(lua_State* L) {
        CoA::Script::PushInt(L, static_cast<int32_t>(s_draft.picks.size()));
        return 1;
    }

    int GetHandOfFatePickCount(lua_State* L) {
        CoA::Script::PushInt(L, s_draft.Byte(0x30) == 1 ? static_cast<int32_t>(s_draft.hof.size()) : 0);
        return 1;
    }

    int GetHandOfFateSacrificePickCount(lua_State* L) {
        CoA::Script::PushInt(L, s_draft.Byte(0x30) == 2 ? static_cast<int32_t>(s_draft.sacrifice.size()) : 0);
        return 1;
    }

    bool AnyNonZero(const std::vector<uint32_t>& v) {
        return std::any_of(v.begin(), v.end(), [](uint32_t x) { return x != 0; });
    }

    int HasDraftModePick(lua_State* L) {
        CoA::Script::PushBool(L, DraftModeOn() && AnyNonZero(s_draft.picks));
        return 1;
    }

    int HasHandOfFatePick(lua_State* L) {
        CoA::Script::PushBool(L, DraftModeOn() && s_draft.Byte(0x30) == 1 && AnyNonZero(s_draft.hof));
        return 1;
    }

    int HasHandOfFateSacrificePick(lua_State* L) {
        CoA::Script::PushBool(L, DraftModeOn() && s_draft.Byte(0x30) == 2 && AnyNonZero(s_draft.sacrifice));
        return 1;
    }

    uint32_t At(const std::vector<uint32_t>& v, lua_State* L) {
        const uint32_t i = static_cast<uint32_t>(CoA::Script::ToInt(CoA::Script::CheckNumber(L, 1)));
        return i < v.size() ? v[i] : 0;
    }

    int GetDraftModePickSpellAtIndex(lua_State* L) {
        CoA::Script::PushInt(L, static_cast<int32_t>(At(s_draft.picks, L)));
        return 1;
    }

    int GetHandOfFatePickSpellAtIndex(lua_State* L) {
        const uint32_t v = At(s_draft.hof, L);
        CoA::Script::PushInt(L, s_draft.Byte(0x30) == 1 ? static_cast<int32_t>(v) : 0);
        return 1;
    }

    int GetHandOfFateSacrificePickSpellAtIndex(lua_State* L) {
        const uint32_t v = At(s_draft.sacrifice, L);
        CoA::Script::PushInt(L, s_draft.Byte(0x30) == 2 ? static_cast<int32_t>(v) : 0);
        return 1;
    }

    int GetSelectedHandOfFateSpell(lua_State* L) {
        CoA::Script::PushInt(L, static_cast<int32_t>(s_draft.selected));
        return 1;
    }

    int IsMasteryDraft(lua_State* L) {
        return PushFlag(L, 0x58);
    }

    int IsCardSwapPick(lua_State* L) {
        return PushFlag(L, 0x5F);
    }

    int IsCardSwapSacrifice(lua_State* L) {
        return PushFlag(L, 0x66);
    }

    int IsCardSwapPicked(lua_State* L) {
        return PushFlag(L, 0x67);
    }

    int IsSkillCardPicked(lua_State* L) {
        return PushFlag(L, 0x68);
    }

    int IsLuckySkillCardPicked(lua_State* L) {
        return PushFlag(L, 0x69);
    }

    int IsCardSwap(lua_State* L) {
        CoA::Script::PushBool(L, s_draft.Byte(0x5F) || s_draft.Byte(0x66) || s_draft.Byte(0x67));
        return 1;
    }

    // FUN_10223cd0 / bb0 / d30 / c10: (byte)index < 3, else false.
    template <uint32_t Base>
    int IndexedFlag(lua_State* L) {
        const uint8_t i = static_cast<uint8_t>(static_cast<int32_t>(CoA::Script::CheckNumber(L, 1)));
        CoA::Script::PushBool(L, i < 3 && s_draft.Byte(Base + i) != 0);
        return 1;
    }

    // ---- Hand of Fate rewards ----
    // SMSG_HAND_OF_FATE_REWARDS_LIST 0x6AA (FUN_10226ff0) is a CompressedDataStore: u32 n x 0x32-byte records, each also
    // indexed by (flagA, flagB) and tier: maps at +0x78 (A=1,B=0), +0x98 (1,1), +0xB8 (0,0), +0xD8 (0,1). The list and
    // maps are cleared first, even if inflating fails.
#pragma pack(push, 1)
    struct Reward {
        uint32_t tier;
        uint8_t flagA;
        uint8_t flagB;
        uint32_t level;
        uint32_t items[4];
        uint32_t counts[4];
        uint32_t cost;
        uint32_t release;
    };
#pragma pack(pop)

    static_assert(sizeof(Reward) == 0x32, "hand of fate reward");

    struct Claimed {   // 0x14 bytes: v[0] tier, v[1] claimed (flag), v[2] claimed (no flag)
        uint32_t v[5];
    };

    struct Rewards {
        std::vector<Reward> rewards;                                      // +0x6C
        std::unordered_map<uint32_t, std::vector<uint32_t>> maps[2][2];   // [flagA][flagB] -> tier -> reward indices
        std::vector<Claimed> claimed;                                     // +0xF8
        uint32_t lastTier = 0;                                            // +0x104
        uint32_t lastCount = 0;                                           // +0x108
        bool lastFlag = false;                                            // +0x10C
    };

    Rewards s_hof;

    int32_t OnRewardsList(void*, uint32_t, uint32_t, CDataStore* msg) {
        s_hof.rewards.clear();

        for (auto& row : s_hof.maps) {
            for (auto& m : row) {
                m.clear();
            }
        }

        std::vector<uint8_t> data;

        if (!CoA::Read::Inflate(msg, data) || data.size() < 4) {
            return 1;
        }

        uint32_t n;
        memcpy(&n, data.data(), 4);
        size_t at = 4;

        for (uint32_t i = 0; i < n && at + sizeof(Reward) <= data.size(); ++i, at += sizeof(Reward)) {
            Reward r;
            memcpy(&r, data.data() + at, sizeof(r));
            s_hof.rewards.push_back(r);
            s_hof.maps[r.flagA != 0][r.flagB != 0][r.tier].push_back(static_cast<uint32_t>(s_hof.rewards.size() - 1));
        }

        return 1;
    }

    // SMSG_COLLECTED_HAND_OF_FATE_REWARDS_LIST 0x6B7 (FUN_10225780): u32 n x 0x14 raw.
    int32_t OnClaimedList(void*, uint32_t, uint32_t, CDataStore* msg) {
        s_hof.claimed.clear();

        for (uint32_t n = CoA::Read::U32(msg); n && CoA::Read::Remaining(msg) >= sizeof(Claimed); --n) {
            Claimed c;
            CoA::Read::Data(msg, &c, sizeof(c));
            s_hof.claimed.push_back(c);
        }

        return 1;
    }

    const char* const kResults[8] = { "COLLECT_HAND_OF_FATE_REWARDS_OK", "COLLECT_HAND_OF_FATE_REWARDS_UNKNOWN",
        "COLLECT_HAND_OF_FATE_REWARDS_SPECIALIZATION_OUT_OF_RANGE", "COLLECT_HAND_OF_FATE_REWARDS_COUNT_TOO_HIGH", "COLLECT_HAND_OF_FATE_REWARDS_INVALID_COUNT",
        "COLLECT_HAND_OF_FATE_REWARDS_NOT_ENOUGH_MARKS_OF_ASCENSION", "COLLECT_HAND_OF_FATE_REWARDS_TOO_LOW_LEVEL", "COLLECT_HAND_OF_FATE_REWARDS_NOT_RELEASED" };

    // SMSG_COLLECT_HAND_OF_FATE_REWARDS_RESULT 0x6B4 (FUN_102253c0): cstring result. On _OK the last claim
    // (ClaimHandOfFateRewards) is applied to the claimed list; then the event fires with it.
    int32_t OnClaimResult(void*, uint32_t, uint32_t, CDataStore* msg) {
        const std::string result = CoA::Read::Str(msg);

        if (result == kResults[0]) {
            if (s_hof.claimed.size() <= s_hof.lastTier) {
                s_hof.claimed.resize(s_hof.lastTier + 1, Claimed {});
            }

            Claimed& c = s_hof.claimed[s_hof.lastTier];
            c.v[0] = s_hof.lastTier;
            c.v[s_hof.lastFlag ? 1 : 2] += s_hof.lastCount;
        }

        CoA::Events::Signal("COLLECT_HAND_OF_FATE_REWARDS_RESULT", "%s", result.c_str());
        return 1;
    }

    // FUN_10225370
    uint32_t ClaimedCount(uint32_t tier, bool flag) {
        return tier < s_hof.claimed.size() ? s_hof.claimed[tier].v[flag ? 1 : 2] : 0;
    }

    // FUN_102252c0: the 1-based index-th reward of (flag, 0) for the tier, or null.
    const Reward* RewardAt(uint32_t tier, uint32_t index, bool flag) {
        const auto& m = s_hof.maps[flag][0];
        const auto it = m.find(tier);

        if (it == m.end() || index == 0 || index > it->second.size()) {
            return nullptr;
        }

        return &s_hof.rewards[it->second[index - 1]];
    }

    // FUN_10225200: consecutive rewards from `start`, stopping at the first gap.
    std::vector<const Reward*> NextRewards(uint32_t tier, uint32_t start, uint32_t count, bool flag) {
        std::vector<const Reward*> out;

        for (uint32_t i = start; i < start + count; ++i) {
            const Reward* r = RewardAt(tier, i, flag);

            if (!r) {
                break;
            }

            out.push_back(r);
        }

        return out;
    }

    // FUN_10225080: items summed over the next `count` unclaimed rewards, and their total cost.
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

    uint32_t CountItem(uint32_t entry) {   // FUN_10308570
        return s_deps.itemCount ? s_deps.itemCount(entry) : 0;
    }

    int64_t Now() {
        return s_deps.now ? s_deps.now() : 0;
    }

    // FUN_10224c10: 0 = claimable, else the COLLECT_HAND_OF_FATE_REWARDS_* code.
    uint32_t CheckClaim(uint32_t tier, uint32_t count, bool flag) {
        if (tier >= s_hof.claimed.size()) {
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

        if (CountItem(0x5B9D2) < cost) {
            return 5;
        }

        const int64_t now = Now();

        for (const Reward* r : recs) {
            if (s_deps.hasPlayer && s_deps.hasPlayer() && (s_deps.playerLevel ? s_deps.playerLevel() : 0) < r->level) {
                return 6;
            }

            if (now < static_cast<int64_t>(r->release)) {
                return 7;
            }
        }

        return 0;
    }

    // FUN_10221e50: {number, number, boolean} -> tier (1-based), count, flag.
    bool ReadClaimArgs(lua_State* L, uint32_t& tier, uint32_t& count, bool& flag) {
        if (!CoA::Script::ValidateInput(L, { LUA_TNUMBER, LUA_TNUMBER, LUA_TBOOLEAN })) {
            return false;
        }

        tier = static_cast<uint32_t>(static_cast<int64_t>(CoA::Script::CheckNumber(L, 1)));
        count = static_cast<uint32_t>(static_cast<int64_t>(CoA::Script::CheckNumber(L, 2)));
        flag = lua_toboolean(L, 3) != 0;
        return true;
    }

    // FUN_100e92d0: {number, boolean}.
    bool ReadTierFlag(lua_State* L, uint32_t& tier, bool& flag) {
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

    int CanClaimHandOfFateRewards(lua_State* L) {
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

    // CMSG 0x6B3 {u32 tier, u32 count, u8 flag}; the claim is remembered for the 0x6B4 result.
    int ClaimHandOfFateRewards(lua_State* L) {
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

        CoA::Packet(0x6B3).U32(tier - 1).U32(count).U8(flag ? 1 : 0).Send();
        s_hof.lastTier = tier - 1;
        s_hof.lastCount = count;
        s_hof.lastFlag = flag;
        CoA::Script::PushBool(L, true);
        return 1;
    }

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

    int GetNumClaimedHandOfFateRewards(lua_State* L) {
        uint32_t tier;
        bool flag;

        if (!ReadTierFlag(L, tier, flag) || tier == 0) {
            return 0;
        }

        CoA::Script::PushInt(L, static_cast<int32_t>(ClaimedCount(tier - 1, flag)));
        return 1;
    }

    // FUN_102240f0 / FUN_10224010 / FUN_10222aa0: CMSG 0x750 / 0x751 {u32 spell}, CMSG 0x752 {}.
    int SendHandOfFateSpellSelection(lua_State* L) {
        CoA::Packet(0x750).U32(static_cast<uint32_t>(CoA::Script::ToInt(CoA::Script::CheckNumber(L, 1)))).Send();
        return 0;
    }

    int SendHandOfFateSacrificeSpellSelection(lua_State* L) {
        CoA::Packet(0x751).U32(static_cast<uint32_t>(CoA::Script::ToInt(CoA::Script::CheckNumber(L, 1)))).Send();
        return 0;
    }

    int CancelHandOfFateSelection(lua_State*) {
        CoA::Packet(0x752).Send();
        return 0;
    }

    // FUN_102237c0: the largest count (from 1, up to 0x4000) whose claim check passes.
    int GetNumClaimableHandOfFateRewards(lua_State* L) {
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

    const std::vector<uint32_t>* TierRewards(bool flagA, uint32_t tier) {   // FUN_10224f20 / FUN_10224fd0
        const auto& m = s_hof.maps[flagA][0];
        const auto it = m.find(tier);
        return it != m.end() ? &it->second : nullptr;
    }

    // FUN_10223590: (released, total, seconds until the next release) for the tier's (0,0) rewards.
    int GetHandOfFateRewardsReleaseInfo(lua_State* L) {
        uint32_t tier;

        if (!CoA::Script::ReadNumber(L, tier) || tier == 0) {
            return 0;
        }

        const std::vector<uint32_t>* recs = TierRewards(false, tier - 1);

        if (!recs || recs->empty()) {
            return CoA::Script::PushNils(L, 3);
        }

        const uint32_t now = static_cast<uint32_t>(Now());
        uint32_t released = 0;
        uint32_t next = 0;

        for (uint32_t idx : *recs) {
            next = s_hof.rewards[idx].release;

            if (next > now) {
                break;
            }

            ++released;
        }

        CoA::Script::PushInt(L, static_cast<int32_t>(released));
        CoA::Script::PushInt(L, static_cast<int32_t>(recs->size()));

        if (released < recs->size()) {
            CoA::Script::PushInt(L, static_cast<int32_t>(next - now));
        } else {
            lua_pushnil(L);
        }

        return 3;
    }

    // FUN_10222d50: the tier's (1,0) rewards up to the server's max level: {Level, Rewards, IsClaimed}.
    int GetLevelingInfo(lua_State* L) {
        uint32_t tier;

        if (!CoA::Script::ReadNumber(L, tier) || tier == 0) {
            return 0;
        }

        struct Entry {
            uint32_t level;
            std::unordered_map<uint32_t, uint32_t> items;
            bool claimed;
        };

        std::vector<Entry> entries;

        if (const std::vector<uint32_t>* recs = TierRewards(true, tier - 1)) {
            const uint32_t maxLevel = CoA::GameEvents::ServerMaxLevel();

            for (uint32_t i = 0; i < recs->size(); ++i) {
                const Reward& r = s_hof.rewards[(*recs)[i]];

                if (r.level > maxLevel) {
                    continue;
                }

                Entry e { r.level, {}, false };

                for (int k = 0; k < 4; ++k) {
                    if (r.items[k] && r.counts[k]) {
                        e.items[r.items[k]] = r.counts[k];
                    }
                }

                e.claimed = i + 1 <= ClaimedCount(tier - 1, true);
                entries.push_back(std::move(e));
            }
        }

        lua_createtable(L, 0, 0);
        lua_checkstack(L, 2);

        for (uint32_t i = 0; i < entries.size(); ++i) {
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

    const CoA::Binding s_bindings[] = {
        COA_NATIVE("", "GetDraftModePickCount", GetDraftModePickCount),
        COA_NATIVE("", "GetHandOfFatePickCount", GetHandOfFatePickCount),
        COA_NATIVE("", "GetHandOfFateSacrificePickCount", GetHandOfFateSacrificePickCount),
        COA_NATIVE("", "HasDraftModePick", HasDraftModePick),
        COA_NATIVE("", "HasHandOfFatePick", HasHandOfFatePick),
        COA_NATIVE("", "HasHandOfFateSacrificePick", HasHandOfFateSacrificePick),
        COA_NATIVE("", "GetDraftModePickSpellAtIndex", GetDraftModePickSpellAtIndex),
        COA_NATIVE("", "GetHandOfFatePickSpellAtIndex", GetHandOfFatePickSpellAtIndex),
        COA_NATIVE("", "GetHandOfFateSacrificePickSpellAtIndex", GetHandOfFateSacrificePickSpellAtIndex),
        COA_NATIVE("", "GetSelectedHandOfFateSpell", GetSelectedHandOfFateSpell),
        COA_NATIVE("", "IsMasteryDraft", IsMasteryDraft),
        COA_NATIVE("", "IsCardSwap", IsCardSwap),
        COA_NATIVE("", "IsCardSwapPick", IsCardSwapPick),
        COA_NATIVE("", "IsCardSwapSacrifice", IsCardSwapSacrifice),
        COA_NATIVE("", "IsCardSwapPicked", IsCardSwapPicked),
        COA_NATIVE("", "IsSkillCardPicked", IsSkillCardPicked),
        COA_NATIVE("", "IsLuckySkillCardPicked", IsLuckySkillCardPicked),
        COA_NATIVE("", "IsSkillCardDraft", IndexedFlag<0x59>),
        COA_NATIVE("", "IsLuckySkillCardDraft", IndexedFlag<0x5C>),
        COA_NATIVE("", "IsSkillCardPick", IndexedFlag<0x60>),
        COA_NATIVE("", "IsLuckySkillCardPick", IndexedFlag<0x63>),
        COA_NATIVE("", "SendHandOfFateSpellSelection", SendHandOfFateSpellSelection),
        COA_NATIVE("", "SendHandOfFateSacrificeSpellSelection", SendHandOfFateSacrificeSpellSelection),
        COA_NATIVE("", "CancelHandOfFateSelection", CancelHandOfFateSelection),
        COA_NATIVE("C_DraftRewards", "CanClaimHandOfFateRewards", CanClaimHandOfFateRewards),
        COA_NATIVE("C_DraftRewards", "ClaimHandOfFateRewards", ClaimHandOfFateRewards),
        COA_NATIVE("C_DraftRewards", "GetRewards", GetRewards),
        COA_NATIVE("C_DraftRewards", "GetMarkOfAscensionCost", GetMarkOfAscensionCost),
        COA_NATIVE("C_DraftRewards", "GetNumClaimedHandOfFateRewards", GetNumClaimedHandOfFateRewards),
        COA_NATIVE("C_DraftRewards", "GetHandOfFateRewardsReleaseInfo", GetHandOfFateRewardsReleaseInfo),
        COA_NATIVE("C_DraftRewards", "GetLevelingInfo", GetLevelingInfo),
        COA_NATIVE("C_DraftRewards", "GetNumClaimableHandOfFateRewards", GetNumClaimableHandOfFateRewards),
    };
}

CoA::DraftMode::Deps& CoA::DraftMode::Dependencies() {
    return s_deps;
}

void CoA::DraftMode::Init() {
    CoA::LuaReg::AddModule(s_bindings, sizeof(s_bindings) / sizeof(s_bindings[0]));
    CoA::Lifecycle::OnGlueScreen(ResetState);
    COA_PACKET(0x753, OnDraftState);
    COA_PACKET(0x6AA, OnRewardsList);
    COA_PACKET(0x6B4, OnClaimResult);
    COA_PACKET(0x6B7, OnClaimedList);
    COA_EVENT("ASCENSION_DRAFTMODE_DRAFT_PICK_CHANGED");
    COA_EVENT("ASCENSION_DRAFTMODE_HAND_OF_FATE_CHANGED");
    COA_EVENT("ASCENSION_DRAFTMODE_HAND_OF_FATE_SACRIFICE_CHANGED");
    COA_EVENT("ASCENSION_DRAFTMODE_HAND_OF_FATE_SELECTED_CHANGED");
    COA_EVENT("ASCENSION_DRAFTMODE_DRAFT_PICK_METADATA_CHANGED");
    COA_EVENT("ASCENSION_DRAFTMODE_HAND_OF_FATE_METADATA_CHANGED");
    COA_EVENT("ASCENSION_DRAFTMODE_HAND_OF_FATE_SACRIFICE_METADATA_CHANGED");
    COA_EVENT("ASCENSION_DRAFTMODE_HAND_OF_FATE_SELECTED_METADATA_CHANGED");
    COA_EVENT("ASCENSION_DRAFT_STATE_UPDATED");
    COA_EVENT("COLLECT_HAND_OF_FATE_REWARDS_RESULT");
}

void CoA::DraftMode::Reset() {
    ResetState();
    s_hof = Rewards();
}
