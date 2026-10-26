// AscWildcardRapid.cpp (see Wildcard.h): C_Wildcard, part 3 -- rapid rolling (WildcardMgr +0x1D8..+0x274,
// FUN_10a349d0): the session record, the quick-roll configs (SMSG 0x6F0), the gates, and the unlearn (CMSG 0x61D) ->
// roll (CMSG 0x643) -> learn (SMSG 0x621) -> reveal loop driven by WildcardRolls.cpp's handlers. Also the plain roll,
// unlearn, roll-icon and starting-choice natives.
//
// Session (+0x1E8): u64 id, phase (0 Idle .. 8 Cancelled), target entry, its rank at start, is-ability, roll count,
// reason, learned entry, new rank, pre-roll rank, stop code, is-desired, then the desired ids / tag ids sent with each
// roll; +0x268 the session counter, +0x270 the phase a cancel was requested in (0 none).
#include "CoA/Source/Modules/WildcardInternal.h"
#include "CoA/Source/Modules/Config.h"
#include "CoA/Source/Modules/Token.h"
#include "CoA/Source/Runtime/ClientHooks.h"
#include "CoA/Source/Runtime/Events.h"
#include "CoA/Source/Runtime/Lifecycle.h"
#include "CoA/Source/Runtime/Logger.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Packet.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Source/Runtime/Read.h"
#include "CoA/Source/Runtime/Script.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <algorithm>
#include <random>

using CoA::CA::Row;
using CoA::Script::PushBool;
using CoA::Script::PushStr;
using CoA::Script::ReadNumber;
using CoA::WildcardSets::Next;

const char* const CoA::Wildcard::detail::kStopCodes[7] = { "STOP_RAPID_ROLLING_NONE", "STOP_RAPID_ROLLING_UNKNOWN",
    "STOP_RAPID_ROLLING_UNHANDLED", "STOP_RAPID_ROLLING_DESIRED_ENTRY_LEARNED", "STOP_RAPID_ROLLING_COUNT_DEPLETED",
    "STOP_RAPID_ROLLING_TALENT_UPGRADE_LEARNED", "STOP_RAPID_ROLLING_TALENT_UPGRADE_BONUS_LEARNED" };

namespace {
    using CoA::Wildcard::detail::kStopCodes;
    using CoA::Wildcard::detail::StopCodeIndex;

    const char* const kPhases[9] = { "Idle", "WaitingForUnlearn", "WaitingForRoll", "WaitingForLearn", "Revealing",
        "AwaitingContinue", "Completed", "Failed", "Cancelled" };
    const char* const kCanUse[7] = { "CAN_USE_RAPID_ROLLING_OK", "CAN_USE_RAPID_ROLLING_UNKNOWN",
        "CAN_USE_RAPID_ROLLING_NOT_ENABLED", "CAN_USE_RAPID_ROLLING_NOT_WILDCARD", "CAN_USE_RAPID_ROLLING_NOT_MAX_LEVEL",
        "CAN_USE_RAPID_ROLLING_UNSPENT_ABILITY_ESSENCE", "CAN_USE_RAPID_ROLLING_UNSPENT_TALENT_ESSENCE" };   // 0x10B1FA94
    const char* const kCanStart[22] = { "CAN_START_RAPID_ROLLING_OK", "CAN_START_RAPID_ROLLING_UNKNOWN",
        "CAN_START_RAPID_ROLLING_NOT_ENABLED", "CAN_START_RAPID_ROLLING_NOT_WILDCARD", "CAN_START_RAPID_ROLLING_NOT_MAX_LEVEL",
        "CAN_START_RAPID_ROLLING_CANNOT_USE_RAPID_ROLLING", "CAN_START_RAPID_ROLLING_NO_ROLL",
        "CAN_START_RAPID_ROLLING_TOO_LOW_COUNT", "CAN_START_RAPID_ROLLING_TOO_HIGH_COUNT",
        "CAN_START_RAPID_ROLLING_NO_DESIRED_ENTRIES", "CAN_START_RAPID_ROLLING_NO_UNDESIRED_ENTRIES",
        "CAN_START_RAPID_ROLLING_NEXT_UNDESIRED_ENTRY_NOT_FOUND", "CAN_START_RAPID_ROLLING_NEXT_UNDESIRED_ENTRY_SPELL_TAG",
        "CAN_START_RAPID_ROLLING_NEXT_UNDESIRED_ENTRY_SUGGESTION", "CAN_START_RAPID_ROLLING_NEXT_UNDESIRED_ENTRY_NOT_VALID",
        "CAN_START_RAPID_ROLLING_MISSING_ABILITY_TOKENS", "CAN_START_RAPID_ROLLING_MISSING_TALENT_TOKENS",
        "CAN_START_RAPID_ROLLING_UNSPENT_ABILITY_ESSENCE", "CAN_START_RAPID_ROLLING_UNSPENT_TALENT_ESSENCE",
        "CAN_START_RAPID_ROLLING_NO_DESIRED_ABILITIES", "CAN_START_RAPID_ROLLING_NO_DESIRED_TALENTS",
        "CAN_START_RAPID_ROLLING_UNBLOCKED_SKILL_CARDS" };   // 0x10B20960
    const char* const kUnlearnResults[23] = { "CA_UNLEARN_OK", "CA_UNLEARN_UNKNOWN", "CA_UNLEARN_NOT_IN_WORLD",
        "CA_UNLEARN_BAD_ABILITY", "CA_UNLEARN_NOT_IN_BATTLEGROUNDS", "CA_UNLEARN_NOT_THIS_ABILITY", "CA_UNLEARN_NOT_IN_COMBAT",
        "CA_UNLEARN_NOT_KNOWN", "CA_UNLEARN_NO_UNLEARN_ITEM", "CA_UNLEARN_MISSING_CONNECTED_ENTRIES", "CA_UNLEARN_MASTERY_ID",
        "CA_UNLEARN_MISSING_REQUIRED_ID", "CA_UNLEARN_NO_SCROLL_OF_FORTUNE", "CA_UNLEARN_LOCKED",
        "CA_UNLEARN_WILDCARD_UNSPENT_AE", "CA_UNLEARN_WILDCARD_UNSPENT_TE", "CA_UNLEARN_BUILD_CREATOR",
        "CA_UNLEARN_NOT_ENOUGH_INVESTED_AE", "CA_UNLEARN_NOT_ENOUGH_INVESTED_TE", "CA_UNLEARN_NOT_ENOUGH_INVESTED_POINTS",
        "CA_UNLEARN_SCROLL_OF_FORTUNE_LIMIT", "CA_UNLEARN_NOT_WILDCARD", "CA_UNLEARN_MAX" };   // 0x10B204C8
    const char* const kRollResults[3] = { "ROLL_ABILITIES_OK", "ROLL_ABILITIES_NO_TOKENS", "ROLL_ABILITIES_NO_ROLL" };   // 0x10B60C60

    template <size_t N>
    int IndexOf(const std::string& s, const char* const (&names)[N]) {
        for (size_t i = 0; i < N; ++i) {
            if (s == names[i]) {
                return static_cast<int>(i);
            }
        }

        return -1;
    }

    std::string EnumName(uint32_t code, const char* const* names, uint32_t count) {   // FUN_10a343d0 & co
        return code < count ? std::string(names[code]) : "UNEXPECTED_ENUM_VALUE_" + std::to_string(code);
    }

    struct Session {
        uint64_t id = 0;                                    // +0x1E8
        uint32_t phase = 0;                                 // +0x1F0
        uint32_t target = 0;                                // +0x1F4
        uint32_t startRank = 0;                             // +0x1F8
        bool isAbility = false;                             // +0x1FC
        uint32_t rollCount = 0;                             // +0x200
        std::string reason;                                 // +0x204
        uint32_t learned = 0;                               // +0x21C
        uint32_t newRank = 0;                               // +0x220
        uint32_t preRollRank = 0;                           // +0x224
        std::string stopCode = "STOP_RAPID_ROLLING_NONE";   // +0x228
        bool isDesired = false;                             // +0x240
        std::vector<uint32_t> ids;                          // +0x244
        std::vector<uint32_t> tags;                         // +0x250
    };

    Session s_session;
    uint32_t s_cancelFrom = 0;        // +0x270
    uint64_t s_sessionCounter = 0;    // +0x268
    bool s_rerollPending = false;     // DAT_10d3d0b0: an unlearn (CMSG 0x61D) is outstanding

    struct QuickRoll {
        uint8_t kind;
        uint32_t threshold;
        uint32_t max;
    };

    std::vector<QuickRoll> s_quick;   // +0x1D8 (SMSG 0x6F0)

    bool Active() {   // FUN_10a34ce0
        return s_session.phase >= 1 && s_session.phase <= 5;
    }

    bool InFlight() {   // FUN_10a34cb0
        return s_cancelFrom != 0 || (s_session.phase >= 1 && s_session.phase <= 3);
    }

    bool Awaiting() {   // FUN_10a34ad0
        return s_cancelFrom == 0 && s_session.phase == 5 && s_session.stopCode == "STOP_RAPID_ROLLING_TALENT_UPGRADE_LEARNED";
    }

    bool QuickRollingEnabled() {   // FUN_10a34ba0
        const bool* v = CoA::Config::Bool("CONFIG_WILDCARD_QUICK_ROLLING_ENABLED");
        return v && *v;
    }

    // FUN_10a346d0(isAbility, next): the configs of that kind in order against the spec's count (ability ModeMgr +0x08,
    // talent +0x58): without `next` the last one reached (threshold <= count), with it the first not yet reached.
    const QuickRoll* Config(bool isAbility, bool next) {
        if (!CoA::Wildcard::detail::HasPlayer()) {
            return nullptr;
        }

        const uint32_t count = CoA::WildcardRolls::SpecCounter(isAbility ? 0 : 1);
        const QuickRoll* last = nullptr;

        for (const QuickRoll& q : s_quick) {
            if ((q.kind != 0) != isAbility) {
                continue;
            }

            if (count < q.threshold && !next) {
                return last;
            }

            last = &q;

            if (next && count < q.threshold) {
                return &q;
            }
        }

        return last;
    }

    uint32_t MaxRolls(bool isAbility) {   // FUN_10a344a0
        const QuickRoll* q = Config(isAbility, false);
        return q ? q->max : 0;
    }

    // FUN_10a347e0: the spec's roll tokens plus its ability (or talent) tokens (FUN_10a2ffc0 / FUN_10a2fd80 /
    // FUN_10a30180; token 0x3D past spec 19).
    uint64_t RollTokens(bool ability) {
        const int spec = CoA::CA::ActiveSpecIndex();
        const bool ok = spec >= 0 && spec < 20;
        const uint32_t roll = ok ? static_cast<uint32_t>(spec) : 0x3D;
        const uint32_t kind = ok ? (ability ? 0x14u : 0x28u) + static_cast<uint32_t>(spec) : 0x3D;
        return CoA::Token::Amount64(roll) + CoA::Token::Amount64(kind);
    }

    // FUN_10a344c0's acceptance test: out of tokens relative to this kind's quick-roll limit.
    bool Unlearnable(const Next& n) {
        const QuickRoll* q = Config(n.ability, false);
        const uint32_t limit = q ? q->max : 0;
        return limit <= RollTokens(n.ability);
    }

    bool NextUnlearn(Next& n) {
        return CoA::WildcardSets::NewestUnlearnable(n, Unlearnable);
    }

    bool NextIsAbility() {   // FUN_10a34b50
        Next n;

        if (!NextUnlearn(n) || !(n.kind == 1 || n.kind == 2 || n.kind == 4)) {
            return false;
        }

        Row r = CoA::CA::FindRow(n.id);
        const int t = r ? CoA::CA::RowType(r) : -1;
        return t == 1 || t == 4;
    }

    // FUN_10a336b0: CAN_USE_RAPID_ROLLING_*.
    uint32_t CanUse() {
        if (!QuickRollingEnabled()) {
            return 2;
        }

        if (!CoA::Wildcard::detail::WildcardMode()) {
            return 3;
        }

        if (!CoA::Wildcard::detail::HasPlayer() || !CoA::WildcardRolls::AtMaxLevel()) {
            return 4;
        }

        if (CoA::WildcardRolls::CanAffordRoll(false)) {
            return 5;
        }

        return CoA::WildcardRolls::CanAffordRoll(true) ? 6 : 0;
    }

    // FUN_10a333d0(count, isAbility): CAN_START_RAPID_ROLLING_*.
    uint32_t CanStartCode(uint32_t count, bool isAbility) {
        if (!QuickRollingEnabled()) {
            return 2;
        }

        if (!CoA::Wildcard::detail::WildcardMode()) {
            return 3;
        }

        if (!CoA::Wildcard::detail::HasPlayer() || !CoA::WildcardRolls::AtMaxLevel()) {
            return 4;
        }

        if (CoA::WildcardRolls::CanAffordRoll(false)) {
            return 0x11;
        }

        if (CoA::WildcardRolls::CanAffordRoll(true)) {
            return 0x12;
        }

        const QuickRoll* q = Config(isAbility, false);

        if (q && count < q->max) {
            return 7;
        }

        if (MaxRolls(isAbility) < count) {
            return 8;
        }

        if (CoA::WildcardSets::DesiredCount() == 0) {
            return 9;
        }

        if (CoA::WildcardSets::UndesiredCount() == 0) {
            return 10;
        }

        Next n;

        if (!NextUnlearn(n)) {
            return 0xB;
        }

        if (n.kind == 5) {
            return 0xC;
        }

        if (n.kind == 6) {
            return 0xD;
        }

        Row r = CoA::CA::FindRow(n.id);

        if (!r) {
            return 0xB;
        }

        const bool abilityRow = CoA::CA::RowType(r) == 1 || CoA::CA::RowType(r) == 4;

        if (RollTokens(abilityRow) < count) {
            return abilityRow ? 0xF : 0x10;
        }

        if (n.ability && !CoA::WildcardSets::HasDesiredWith(true)) {
            return 0x13;
        }

        if (n.talent && !CoA::WildcardSets::HasDesiredWith(false)) {
            return 0x14;
        }

        // FUN_102f4f70: a slotted, unblocked skill card in the active spec.
        const int spec = CoA::CA::ActiveSpecIndex();
        const CoA::Wildcard::Deps& d = CoA::Wildcard::Dependencies();
        return d.skillCardUnblocked && d.skillCardUnblocked(spec < 0 ? 0xFFFFFFFF : static_cast<uint32_t>(spec)) ? 0x15 : 0;
    }

    struct Result {
        bool ok;
        std::string reason;
    };

    // FUN_10a2a410: {ok, reason}.
    Result CanStart() {
        if (Active()) {
            return { false, "RAPID_ROLLING_SESSION_ACTIVE" };
        }

        if (InFlight()) {
            return { false, "RAPID_ROLLING_REQUEST_IN_FLIGHT" };
        }

        if (CoA::WildcardRolls::RevealsPending()) {
            return { false, "WILDCARD_REVEAL_PENDING" };
        }

        if (s_rerollPending) {
            return { false, "WILDCARD_REROLL_PENDING" };
        }

        const bool isAbility = NextIsAbility();
        const uint32_t code = CanStartCode(MaxRolls(isAbility), isAbility);

        if (code == 0) {
            return { true, std::string() };
        }

        return { false, EnumName(code, kCanStart, 22) };
    }

    // ---- the session's transitions ---------------------------------------------------------------------------
    // FUN_10a36740: a new phase (or reason) is announced; the same phase with the same reason is not.
    void SetPhase(uint32_t phase, const std::string& reason) {
        if (s_session.phase == phase && s_session.reason == reason) {
            return;
        }

        s_session.phase = phase;
        s_session.reason = reason;
        CoA::Events::Signal("WILDCARD_RAPID_ROLL_STATE_CHANGED", nullptr);
    }

    // FUN_10a36280: a fresh record under the same session id, in `phase` with `reason`.
    void Reset(uint32_t phase, const std::string& reason) {
        const uint64_t id = s_session.id;
        s_session = Session();
        s_session.id = id;
        s_session.phase = phase;
        s_session.reason = reason;
        CoA::Events::Signal("WILDCARD_RAPID_ROLL_STATE_CHANGED", nullptr);
    }

    // FUN_10a34180: the answer a cancel was waiting for arrived; a cancelled session records why.
    void Drain(const std::string& reason) {
        s_cancelFrom = 0;

        if (s_session.phase == 8) {
            SetPhase(8, reason);
        }
    }

    // FUN_10a33840 (and the first half of FUN_10a35270): mid-request, the session is marked cancelled and the request's
    // answer drained later; otherwise it simply ends.
    void CancelSession() {
        const uint32_t phase = s_session.phase;

        if (phase == 0 || s_cancelFrom != 0) {
            return;
        }

        if (phase >= 1 && phase <= 3) {
            Reset(8, "CANCELLED");
            s_cancelFrom = phase;
        } else {
            Reset(0, std::string());
        }
    }

    // CMSG 0x643: u32 roll count, the desired ids and tag ids (FUN_10a31ef0: u32 count + u32 each), u8 is-ability.
    void SendRoll(uint32_t count, const std::vector<uint32_t>& ids, const std::vector<uint32_t>& tags, bool isAbility) {
        CoA::Packet p(0x643);
        p.U32(count).U32(static_cast<uint32_t>(ids.size()));

        for (uint32_t id : ids) {
            p.U32(id);
        }

        p.U32(static_cast<uint32_t>(tags.size()));

        for (uint32_t t : tags) {
            p.U32(t);
        }

        p.U8(isAbility ? 1 : 0).Send();
    }

    // FUN_10a36d00: start (or, from AwaitingContinue, continue) a session on the newest undesired entry.
    Result Start(bool continuing) {
        const uint32_t phase = s_session.phase;

        if (phase >= 1 && phase <= 5 && !(continuing && phase == 5)) {
            return { false, "RAPID_ROLLING_SESSION_ACTIVE" };
        }

        if (s_cancelFrom != 0 || (phase >= 1 && phase <= 3)) {
            return { false, "RAPID_ROLLING_REQUEST_IN_FLIGHT" };
        }

        if (CoA::WildcardRolls::RevealsPending()) {
            return { false, "WILDCARD_REVEAL_PENDING" };
        }

        if (s_rerollPending) {
            return { false, "WILDCARD_REROLL_PENDING" };
        }

        uint32_t code = 0xB;
        Next n;

        if (NextUnlearn(n)) {
            if (Row r = CoA::CA::FindRow(n.id)) {
                const int t = CoA::CA::RowType(r);
                const bool isAbility = t == 1 || t == 4;   // FUN_101c6770
                const uint32_t count = MaxRolls(isAbility);
                code = CanStartCode(count, isAbility);

                if (code == 0) {
                    const CoA::CA::Build* b = CoA::CA::ActiveBuild();
                    const uint32_t rank = b ? b->RankOf(n.id) : 0;   // FUN_10152920
                    Session s;
                    s.id = ++s_sessionCounter;   // +0x268
                    s.phase = 1;
                    s.target = n.id;
                    s.startRank = rank;
                    s.isAbility = isAbility;
                    s.rollCount = count;
                    s.reason = "WAITING_FOR_UNLEARN";
                    s.preRollRank = rank;
                    CoA::WildcardSets::DesiredForRoll(s.ids, s.tags);
                    s_session = s;   // FUN_10a327e0
                    CoA::Packet(0x61D).U32(s_session.target).Send();
                    CoA::WildcardRolls::ArmPoll();   // FUN_10a31760
                    CoA::Events::Signal("WILDCARD_RAPID_ROLL_STATE_CHANGED", nullptr);
                    return { true, std::string() };
                }
            }
        }

        return { false, EnumName(code, kCanStart, 22) };
    }

    // FUN_10a364f0: a talent-upgrade learn earns one more talent roll, sent at once.
    bool TalentUpgradeRoll() {
        if (s_session.phase != 4 || s_session.stopCode != "STOP_RAPID_ROLLING_TALENT_UPGRADE_LEARNED") {
            return false;
        }

        s_session.rollCount = 0;
        s_session.isAbility = false;
        s_session.learned = 0;
        s_session.newRank = 0;
        s_session.preRollRank = 0;
        s_session.stopCode = "STOP_RAPID_ROLLING_TALENT_UPGRADE_BONUS_PENDING";
        s_session.isDesired = false;
        s_session.ids.clear();
        s_session.tags.clear();
        SendRoll(0, {}, {}, false);
        SetPhase(2, "WAITING_FOR_TALENT_UPGRADE_ROLL");
        return true;
    }

    // ---- natives: start / continue / cancel / selections --------------------------------------------------------
    int PushResult(lua_State* L, const Result& r) {   // FUN_10a2a810
        PushBool(L, r.ok);

        if (r.reason.empty()) {
            lua_pushnil(L);
        } else {
            PushStr(L, r.reason.c_str());
        }

        return 2;
    }

    int StartRapidRolling(lua_State* L) {   // FUN_10a2e710
        return PushResult(L, Start(false));
    }

    int ContinueRapidRolling(lua_State* L) {   // FUN_10a2bc40 -> FUN_10a34250
        if (s_session.phase == 5) {
            return PushResult(L, Start(true));
        }

        return PushResult(L, { false, s_session.reason.empty() ? std::string("RAPID_ROLLING_NOT_AWAITING_CONTINUE") : s_session.reason });
    }

    int CancelRapidRolling(lua_State* L) {   // FUN_10a2bb30 -> FUN_10a33840
        CancelSession();
        return PushResult(L, { true, std::string() });
    }

    // FUN_10a2aba0: the table's values (lua_next order, FUN_10a29b80), each {ID = number, Type = kind name}; false when
    // a Type is not a kind. A value that is not a table empties the list (logged by the original) without failing it.
    bool ReadSelections(lua_State* L, int index, std::vector<CoA::WildcardSets::Selection>& out) {
        if (lua_type(L, index) != LUA_TTABLE) {
            return false;
        }

        out.clear();
        bool ok = true;
        lua_pushnil(L);

        while (lua_next(L, index)) {
            if (lua_type(L, -1) != LUA_TTABLE) {
                lua_settop(L, -3);
                out.clear();
                return true;
            }

            lua_getfield(L, -1, "ID");
            const uint32_t id = lua_isnumber(L, -1) ? static_cast<uint32_t>(static_cast<int64_t>(lua_tonumber(L, -1))) : 0;
            lua_getfield(L, -2, "Type");
            const char* type = lua_isstring(L, -1) ? lua_tolstring(L, -1, nullptr) : nullptr;
            uint8_t kind = 0;

            if (!CoA::WildcardSets::KindIndex(type ? type : "", kind)) {
                ok = false;
            } else if (ok) {
                out.push_back({ id, kind });
            }

            lua_settop(L, -4);   // ID, Type, the value; the key stays for lua_next
        }

        if (!ok) {
            out.clear();
        }

        return ok;
    }

    int SetRapidRollingSelections(lua_State* L) {   // FUN_10a2e4b0 -> FUN_10a368b0
        std::vector<CoA::WildcardSets::Selection> desired;
        std::vector<CoA::WildcardSets::Selection> undesired;

        if (lua_gettop(L) < 2 || !ReadSelections(L, 1, desired) || !ReadSelections(L, 2, undesired)) {
            return PushResult(L, { false, "INVALID_SELECTIONS" });
        }

        const uint32_t phase = s_session.phase;

        if (phase >= 1 && phase <= 5) {
            return PushResult(L, { false, "RAPID_ROLLING_SESSION_ACTIVE" });
        }

        if (s_cancelFrom != 0) {
            return PushResult(L, { false, "RAPID_ROLLING_REQUEST_IN_FLIGHT" });
        }

        CoA::WildcardSets::ApplySelections(desired, undesired);
        return PushResult(L, { true, std::string() });
    }

    int UnlearnAbility(lua_State* L) {   // FUN_10a2e800: CMSG 0x61D, u32 entry
        if (Active() || InFlight()) {
            PushBool(L, false);
            return 1;
        }

        uint32_t id;

        if (!ReadNumber(L, id)) {
            return 0;
        }

        CoA::Packet(0x61D).U32(id).Send();
        PushBool(L, true);
        return 1;
    }

    int RollAbilities(lua_State* L) {   // FUN_10a2dbe0: CMSG 0x643 with nothing desired
        if (!InFlight() && (!Active() || Awaiting()) && !CoA::WildcardRolls::RevealsPending()) {
            SendRoll(0, {}, {}, false);
            PushBool(L, true);
            return 1;
        }

        PushBool(L, false);
        return 1;
    }

    // FUN_10a2c5a0: `count` random entry ids with distinct icons -- every indexed, realm-visible, class-allowed row, five
    // times over, shuffled (std::mt19937 from random_device).
    int GetRollIcons(lua_State* L) {
        uint32_t count;

        if (!ReadNumber(L, count)) {
            return 0;
        }

        std::vector<std::pair<uint32_t, const char*>> list;

        for (Row r : CoA::CA::IndexedRows()) {
            const char* icon = CoA::CA::RowText(r, 0xC0);   // compared by pointer

            if (!icon || !*icon) {
                continue;
            }

            if (std::any_of(list.begin(), list.end(), [&](const auto& e) { return e.second == icon; })) {
                continue;
            }

            if (CoA::CA::RowVisible(r) && CoA::CA::ClassAllowedForPlayer(r)) {
                list.emplace_back(CoA::CA::RowU32(r, 0), icon);
            }
        }

        const std::vector<std::pair<uint32_t, const char*>> once = list;

        for (int i = 0; i < 4; ++i) {   // FUN_10a29590 x4
            list.insert(list.end(), once.begin(), once.end());
        }

        std::random_device rd;
        std::mt19937 rng(rd());

        for (size_t i = 1; i < list.size(); ++i) {
            std::uniform_int_distribution<size_t> pick(0, i);
            const size_t j = pick(rng);

            if (j != i) {
                std::swap(list[i], list[j]);
            }
        }

        if (count < list.size()) {
            list.resize(count);
        }

        lua_createtable(L, 0, static_cast<int>(list.size()));
        lua_checkstack(L, 2);

        for (size_t i = 0; i < list.size(); ++i) {
            lua_pushnumber(L, static_cast<double>(i + 1));
            lua_pushnumber(L, list[i].first);
            lua_settable(L, -3);
        }

        return 1;
    }

    void SetNum(lua_State* L, const char* k, uint32_t v) {
        lua_pushstring(L, k);
        lua_pushinteger(L, static_cast<int32_t>(v));
        lua_settable(L, -3);
    }

    void SetBool(lua_State* L, const char* k, bool v) {
        lua_pushstring(L, k);
        PushBool(L, v);
        lua_settable(L, -3);
    }

    // FUN_10a2ccd0: the build's ability entries in learn order (+0x18 key ascending, unset last), at most the starting
    // count, each as FUN_10a2a870's table.
    int GetStartingChoiceEntries(lua_State* L) {
        const CoA::CA::Build* b = CoA::CA::ActiveBuild();

        if (!b || !CoA::WildcardRolls::CanShowStartingChoice()) {
            lua_createtable(L, 0, 0);
            return 1;
        }

        std::vector<CoA::CA::Entry> picks;

        for (const CoA::CA::Entry& e : b->entries) {
            Row r = e.row ? e.row : CoA::CA::FindRow(e.id);

            if (!r) {
                continue;
            }

            const int t = CoA::CA::RowType(r);

            if (t == 1 || t == 4) {
                picks.push_back(e);
            }
        }

        auto key = [](const CoA::CA::Entry& e) { return (static_cast<uint64_t>(e.u1c) << 32) | e.u18; };
        std::stable_sort(picks.begin(), picks.end(), [&](const CoA::CA::Entry& x, const CoA::CA::Entry& y) {
            const uint64_t kx = key(x);
            const uint64_t ky = key(y);
            return kx != 0 && (ky == 0 || kx < ky);
        });
        const uint32_t limit = CoA::WildcardRolls::StartingCount();

        if (limit < picks.size()) {
            picks.resize(limit);
        }

        lua_createtable(L, 0, static_cast<int>(picks.size()));
        lua_checkstack(L, 2);

        for (size_t i = 0; i < picks.size(); ++i) {
            lua_pushinteger(L, static_cast<int32_t>(i + 1));
            const CoA::CA::Entry& e = picks[i];
            Row r = e.row ? e.row : CoA::CA::FindRow(e.id);
            uint32_t spell = CoA::CA::EntryCurrentSpell(*b, e.id);   // FUN_101528d0

            if (!spell && r) {
                spell = CoA::CA::SpellAtRankOf(r, e.rank);   // FUN_101c62e0
            }

            lua_createtable(L, 0, 9);
            SetNum(L, "EntryID", e.id);
            SetNum(L, "EntryId", e.id);
            SetNum(L, "internalID", e.id);
            SetNum(L, "SpellID", spell);
            SetNum(L, "Rank", e.rank);
            SetNum(L, "LearnedRank", e.u0c);
            lua_pushstring(L, "Quality");
            PushStr(L, r ? CoA::CA::QualityName(CoA::CA::QualityIndexOf(*b, r)).c_str() : "");
            lua_settable(L, -3);
            SetBool(L, "IsLocked", e.locked != 0);
            SetBool(L, "Locked", e.locked != 0);
            lua_settable(L, -3);
        }

        return 1;
    }

    // ---- natives -----------------------------------------------------------------------------------------------
    int IsRapidRollingEnabled(lua_State* L) {   // FUN_10a2d2c0
        PushBool(L, QuickRollingEnabled());
        return 1;
    }

    int IsAwaitingRapidRollingTalentUpgradeRoll(lua_State* L) {   // FUN_10a2d190
        PushBool(L, Awaiting());
        return 1;
    }

    int IsRollRequestBlocked(lua_State* L) {   // FUN_10a2d2f0
        bool blocked = true;

        if (!InFlight() && (!Active() || Awaiting())) {
            blocked = CoA::WildcardRolls::RevealsPending();
        }

        PushBool(L, blocked);
        return 1;
    }

    int CanUseRapidRolling(lua_State* L) {   // FUN_10a2b9a0
        const uint32_t code = CanUse();
        PushBool(L, code == 0);

        if (code == 0) {
            lua_pushnil(L);
        } else {
            PushStr(L, EnumName(code, kCanUse, 7).c_str());
        }

        return 2;
    }

    int CanStartRapidRolling(lua_State* L) {   // FUN_10a2b8c0
        return PushResult(L, CanStart());
    }

    // FUN_10a34740: count, the next config's threshold and max; nils once the next threshold is reached.
    int Breakpoint(lua_State* L, bool isAbility) {
        const uint32_t count = CoA::WildcardRolls::SpecCounter(isAbility ? 0 : 1);
        const QuickRoll* next = Config(isAbility, true);
        const uint32_t nextThreshold = next ? next->threshold : 0;

        if (nextThreshold <= count) {
            lua_pushnil(L);
            lua_pushnil(L);
            lua_pushnil(L);
            return 3;
        }

        lua_pushinteger(L, static_cast<int32_t>(count));
        lua_pushinteger(L, static_cast<int32_t>(nextThreshold));
        lua_pushinteger(L, static_cast<int32_t>(next ? next->max : 0));
        return 3;
    }

    int GetRapidRollAbilityBreakpointInfo(lua_State* L) {   // FUN_10a2c070
        return Breakpoint(L, true);
    }

    int GetRapidRollTalentBreakpointInfo(lua_State* L) {   // FUN_10a2c100
        return Breakpoint(L, false);
    }

    int GetMaximumRapidRolls(lua_State* L) {   // FUN_10a2bf30
        lua_pushinteger(L, static_cast<int32_t>(MaxRolls(NextIsAbility())));
        return 1;
    }

    int GetNextUnlearnedID(lua_State* L) {   // FUN_10a2bf70
        Next n;

        if (NextUnlearn(n)) {
            lua_pushinteger(L, static_cast<int32_t>(n.id));
        } else {
            lua_pushnil(L);
        }

        return 1;
    }

    // FUN_10a2c190: the session as a table (13 keys).
    int GetRapidRollingState(lua_State* L) {
        const Result can = CanStart();
        lua_createtable(L, 0, 0xD);
        lua_checkstack(L, 2);
        auto num = [L](const char* k, double v) {
            lua_pushstring(L, k);
            lua_pushnumber(L, v);
            lua_settable(L, -3);
        };
        auto str = [L](const char* k, const std::string& v) {
            lua_pushstring(L, k);
            PushStr(L, v.c_str());
            lua_settable(L, -3);
        };
        num("SessionID", static_cast<double>(s_session.id));
        str("Phase", s_session.phase < 9 ? kPhases[s_session.phase] : "Unknown");
        SetBool(L, "IsActive", Active());
        num("TargetEntryID", s_session.target);
        SetBool(L, "IsAbility", s_session.isAbility);
        num("RollCount", s_session.rollCount);
        str("Reason", s_session.reason);
        num("LearnedEntryID", s_session.learned);
        num("NewRank", s_session.newRank);
        num("PreRollRank", s_session.preRollRank);
        str("StopCode", s_session.stopCode);
        SetBool(L, "IsDesired", s_session.isDesired);
        SetBool(L, "CanStart", can.ok);
        str("CanStartReason", can.reason);
        return 1;
    }

    // SMSG 0x6F0 (FUN_10a34860): u32 n x {u8 kind, u32 threshold, u32 max}; the list is cleared and resized to n first.
    // The 7a convention: only the records whose 9 bytes are left.
    const uint32_t kQuickRollSize = 9;

    int32_t OnQuickRollConfigs(void*, uint32_t, uint32_t, CDataStore* p) {
        s_quick.clear();

        for (uint32_t n = CoA::Read::U32(p); n && CoA::Read::Remaining(p) >= kQuickRollSize; --n) {
            QuickRoll q;
            q.kind = CoA::Read::U8(p);
            q.threshold = CoA::Read::U32(p);
            q.max = CoA::Read::U32(p);
            s_quick.push_back(q);
        }

        return 1;
    }

    // 0x10A33940 (this file's part): the quick-roll configs cleared, the session reset (FUN_10a33b10).
    void OnGlueScreen() {
        s_quick.clear();
        CoA::WildcardRapid::ResetSession();
    }

    // FUN_10a35270 (CA list 0x10be2e18, a spec change): the session is cancelled and both lists empty.
    void OnSpecChanged(uint32_t, uint32_t) {
        CancelSession();
        CoA::WildcardSets::ClearForSpecChange();
    }

    void ResetOnEnterWorld() {   // 0x10a33c90 -> FUN_10a33b10
        CoA::WildcardRapid::ResetSession();
    }

    const CoA::Binding s_bindings[] = {
        COA_NATIVE("C_Wildcard", "IsRapidRollingEnabled", IsRapidRollingEnabled),
        COA_NATIVE("C_Wildcard", "IsAwaitingRapidRollingTalentUpgradeRoll", IsAwaitingRapidRollingTalentUpgradeRoll),
        COA_NATIVE("C_Wildcard", "IsRollRequestBlocked", IsRollRequestBlocked),
        COA_NATIVE("C_Wildcard", "CanUseRapidRolling", CanUseRapidRolling),
        COA_NATIVE("C_Wildcard", "CanStartRapidRolling", CanStartRapidRolling),
        COA_NATIVE("C_Wildcard", "GetRapidRollAbilityBreakpointInfo", GetRapidRollAbilityBreakpointInfo),
        COA_NATIVE("C_Wildcard", "GetRapidRollTalentBreakpointInfo", GetRapidRollTalentBreakpointInfo),
        COA_NATIVE("C_Wildcard", "GetMaximumRapidRolls", GetMaximumRapidRolls),
        COA_NATIVE("C_Wildcard", "GetNextUnlearnedID", GetNextUnlearnedID),
        COA_NATIVE("C_Wildcard", "GetRapidRollingState", GetRapidRollingState),
        COA_NATIVE("C_Wildcard", "StartRapidRolling", StartRapidRolling),
        COA_NATIVE("C_Wildcard", "ContinueRapidRolling", ContinueRapidRolling),
        COA_NATIVE("C_Wildcard", "CancelRapidRolling", CancelRapidRolling),
        COA_NATIVE("C_Wildcard", "SetRapidRollingSelections", SetRapidRollingSelections),
        COA_NATIVE("C_Wildcard", "UnlearnAbility", UnlearnAbility),
        COA_NATIVE("C_Wildcard", "RollAbilities", RollAbilities),
        COA_NATIVE("C_Wildcard", "GetRollIcons", GetRollIcons),
        COA_NATIVE("C_Wildcard", "GetStartingChoiceEntries", GetStartingChoiceEntries),
    };
}

// ---- shared with the other two files ------------------------------------------------------------------------------
uint32_t CoA::Wildcard::detail::StopCodeIndex(const std::string& s) {   // FUN_10a2f5e0(s, 1)
    const int i = IndexOf(s, kStopCodes);

    if (i >= 0) {
        return static_cast<uint32_t>(i);
    }

    CoA::Logger::Write(CoA::Logger::Error, "Unexpected Value: " + s);
    return 1;
}

uint32_t CoA::Wildcard::detail::UnlearnResultIndex(const std::string& s) {
    const int i = IndexOf(s, kUnlearnResults);
    return i >= 0 ? static_cast<uint32_t>(i) : 1;
}

void CoA::Wildcard::detail::InitRapid() {   // 0x10a34970 (this file's part)
    COA_PACKET(0x6F0, OnQuickRollConfigs);
    CoA::Lifecycle::OnGlueScreen(OnGlueScreen);
    CoA::ClientHooks::enterWorld.Add(ResetOnEnterWorld);
    CoA::CA::OnSpecChanged(OnSpecChanged);
    COA_EVENT("WILDCARD_RAPID_ROLL_STATE_CHANGED");
    COA_EVENT("WILDCARD_RAPID_ROLL_UNLEARNED");
    CoA::LuaReg::AddModule(s_bindings, sizeof(s_bindings) / sizeof(s_bindings[0]));
}

void CoA::Wildcard::detail::ResetRapid() {
    s_session = Session();
    s_cancelFrom = 0;
    s_sessionCounter = 0;
    s_rerollPending = false;
    s_quick.clear();
}

// ---- exports ------------------------------------------------------------------------------------------------------
bool CoA::WildcardRapid::Active() {
    return ::Active();
}

bool CoA::WildcardRapid::InFlight() {
    return ::InFlight();
}

void CoA::WildcardRapid::MarkRerollPending() {   // FUN_10a2f910
    s_rerollPending = true;
}

void CoA::WildcardRapid::ResetSession() {   // FUN_10a33b10
    s_session = Session();   // FUN_10a327e0 with a default record
    s_cancelFrom = 0;
}

bool CoA::WildcardRapid::TakeRerollPending() {
    const bool was = s_rerollPending;
    s_rerollPending = false;
    return was;
}

// FUN_10a35c20: SMSG 0x61E inside a session. A drained cancel or a WaitingForUnlearn session consumes it; success sends
// the session's roll (CMSG 0x643) and waits for it.
bool CoA::WildcardRapid::OnUnlearnResult(const std::string& result) {
    if (s_cancelFrom == 1) {
        Drain("CANCELLED_UNLEARN_RESULT_DRAINED");
        return true;
    }

    if (s_session.phase != 1) {
        return false;
    }

    const uint32_t code = CoA::Wildcard::detail::UnlearnResultIndex(result);

    if (code != 0) {
        SetPhase(7, EnumName(code, kUnlearnResults, 23));   // FUN_100999d0
        return true;
    }

    CoA::Events::Signal("WILDCARD_RAPID_ROLL_UNLEARNED", "%u%u", s_session.target, s_session.startRank);

    if (s_session.target == 0) {
        SetPhase(7, "RAPID_ROLLING_SEND_FAILED");
        return true;
    }

    SendRoll(s_session.rollCount, s_session.ids, s_session.tags, s_session.isAbility);
    SetPhase(2, kUnlearnResults[0]);
    return true;
}

// FUN_10a359c0: SMSG 0x644 inside a session. Success is ROLL_ABILITIES_OK or CAN_START_RAPID_ROLLING_OK.
bool CoA::WildcardRapid::OnRollResult(const std::string& result) {
    const bool ok = IndexOf(result, kRollResults) == 0 || IndexOf(result, kCanStart) == 0;

    if (s_cancelFrom == 2) {
        if (!ok) {
            Drain("CANCELLED_ROLL_RESULT_DRAINED");
        } else {
            s_cancelFrom = 3;
        }

        return true;
    }

    if (s_session.phase != 2) {
        return false;
    }

    if (!ok) {
        const std::string reason = result.empty() ? std::string("ROLL_ABILITIES_NO_ROLL") : result;
        s_session.reason = reason;   // assigned before the phase change, as the original does
        SetPhase(7, reason);
    } else {
        SetPhase(3, result);
    }

    return true;
}

// FUN_10a355d0: SMSG 0x621 for the session -- a stop code (or the bonus roll's learn) moves it to Revealing.
void CoA::WildcardRapid::OnEntryLearned(uint32_t entry, uint32_t newRank, uint32_t preRoll, const std::string& stop) {
    uint32_t code = StopCodeIndex(stop);
    const bool bonus = (s_session.phase == 2 || s_session.phase == 3) &&
        s_session.stopCode == "STOP_RAPID_ROLLING_TALENT_UPGRADE_BONUS_PENDING" && (code == 0 || code == 5);

    if (code == 0 && !bonus) {
        return;
    }

    const std::string reached = bonus ? std::string("STOP_RAPID_ROLLING_TALENT_UPGRADE_BONUS_LEARNED") : stop;

    if (bonus) {
        code = 6;
    }

    if (s_cancelFrom == 2 || s_cancelFrom == 3) {
        Drain("CANCELLED_ENTRY_LEARNED_DRAINED");
    } else if (s_session.phase == 3 || s_session.phase == 2) {
        s_session.learned = entry;
        s_session.newRank = newRank;
        s_session.preRollRank = preRoll;
        s_session.stopCode = reached;
        s_session.isDesired = code == 3;
        SetPhase(4, reached);
    }
}

// FUN_10a35870: the reveal of the learned entry ended.
void CoA::WildcardRapid::OnRevealDone(uint32_t entry) {
    if (s_session.phase != 4 || s_session.learned != entry) {
        return;
    }

    switch (StopCodeIndex(s_session.stopCode)) {
        case 3:
            SetPhase(6, s_session.stopCode);
            break;
        case 4:
        case 6:
            SetPhase(5, s_session.stopCode);
            break;
        case 5:
            if (!TalentUpgradeRoll()) {
                SetPhase(7, "RAPID_ROLLING_TALENT_UPGRADE_ROLL_SEND_FAILED");
            }

            break;
        default:
            SetPhase(7, s_session.stopCode);
    }
}
