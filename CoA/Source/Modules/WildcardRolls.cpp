// AscWildcardRolls.cpp (see Wildcard.h): C_Wildcard, part 1 -- the WildcardModeMgr (FUN_10a315d0, static 0x10D3D0D8):
// spent-roll repurchasing, the roll / reset / starting-choice gates, the reveal records of SMSG 0x621 and the
// spellbook adds deferred behind them, the roll-ready poll, and the module's packet handlers.
//
// SMSG 0x6EE (FUN_10a30820): u32 n (<= 20) x {u32 x5} -> spec i. SMSG 0x6EF (FUN_10a31130): 5 u32 for the active spec;
// WILDCARD_REPURCHASABLE_ROLLS_CHANGED / _ABILITY_ / _TALENT_ "%u%u" (old, new) on change. SMSG 0x6F4 (FUN_10a3a0d0) /
// 0x646 (FUN_10a30a50) / 0x64C (FUN_10a308f0): str -> WILDCARD_REPURCHASE_ROLLS_RESULT /
// WILDCARD_RESET_ABILITIES_RESULT / WILDCARD_REROLL_UNLOCKED_STARTING_ABILITIES_RESULT. SMSG 0x61E (FUN_10a30dd0),
// 0x644 (FUN_10a30bb0), 0x621 (FUN_10a30260): the unlearn, roll and learn answers (with the rapid-rolling session's
// part, WildcardRapid.cpp).
#include "CoA/Source/Modules/WildcardInternal.h"
#include "CoA/Source/Modules/Config.h"
#include "CoA/Source/Modules/GameEvents.h"
#include "CoA/Source/Modules/GameMode.h"
#include "CoA/Source/Modules/RealmInfo.h"
#include "CoA/Source/Runtime/ClientHooks.h"
#include "CoA/Source/Runtime/Events.h"
#include "CoA/Source/Runtime/Lifecycle.h"
#include "CoA/Source/Runtime/Logger.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Packet.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Source/Runtime/Read.h"
#include "CoA/Source/Runtime/Script.h"
#include "CoA/Source/Runtime/Timer.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <algorithm>
#include <iterator>
#include <map>

using CoA::CA::Build;
using CoA::Script::CheckNumber;
using CoA::Script::PushBool;
using CoA::Script::PushStr;
using CoA::Script::ReadBool;
using CoA::Script::ValidateInput;

namespace {
    CoA::Wildcard::Deps s_deps;

    const char* const kRepurchase[11] = { "REPURCHASE_WILDCARD_ROLL_OK", "REPURCHASE_WILDCARD_ROLL_UNKNOWN",
        "REPURCHASE_WILDCARD_ROLL_NOT_WILDCARD", "REPURCHASE_WILDCARD_ROLL_NO_COUNT", "REPURCHASE_WILDCARD_ROLL_NOT_MAX_LEVEL",
        "REPURCHASE_WILDCARD_ROLL_NOT_ENABLED", "REPURCHASE_WILDCARD_ROLL_TOO_LOW_COUNT",
        "REPURCHASE_WILDCARD_ROLL_TOO_HIGH_COUNT", "REPURCHASE_WILDCARD_ROLL_NOT_ENOUGH_COUNT",
        "REPURCHASE_WILDCARD_ROLL_NO_TOKENS", "REPURCHASE_WILDCARD_ROLL_NO_MONEY" };   // 0x10B1EEE8

    // ---- the per-spec arrays ---------------------------------------------------------------------------------
    // The manager's +0x00..+0x197 as dwords. FUN_1016f580's index is used unchecked, so spec -1 reads / writes the dword
    // before an array (the previous array's spec 19, or +0x04 for +0x08); past +0x197 the manager's vectors begin,
    // which is not modelled (reads 0, writes dropped).
    const uint32_t kCounterA = 0x08;
    const uint32_t kCounterB = 0x58;
    const uint32_t kRolls = 0xA8;
    const uint32_t kAbilityRolls = 0xF8;
    const uint32_t kTalentRolls = 0x148;
    const uint32_t kArrayEnd = 0x198;
    uint32_t s_words[kArrayEnd / 4] = {};

    uint32_t* Word(uint32_t base, int64_t index) {
        const int64_t off = static_cast<int64_t>(base) + index * 4;
        return off >= 0 && off < kArrayEnd ? &s_words[off / 4] : nullptr;
    }

    uint32_t SpecWord(uint32_t base) {
        const uint32_t* p = Word(base, CoA::CA::ActiveSpecIndex());
        return p ? *p : 0;
    }

    void SetSpecWord(uint32_t base, uint32_t v) {
        if (uint32_t* p = Word(base, CoA::CA::ActiveSpecIndex())) {
            *p = v;
        }
    }

    // Kinds of repurchase: 0 rolls (+0xA8, FUN_10a2fea0), 1 ability rolls (+0xF8, FUN_10a2fe80), 2 talent rolls (+0x148,
    // FUN_10a2fec0).
    uint32_t Available(int kind) {
        return SpecWord(kind == 0 ? kRolls : kind == 1 ? kAbilityRolls : kTalentRolls);
    }

    int32_t ConfigInt(const char* name, int32_t def) {   // FUN_10196ab0
        const int32_t* v = CoA::Config::Int(name);
        return v ? *v : def;
    }

    bool ConfigBool(const char* name) {   // FUN_10196990(name, 0)
        const bool* v = CoA::Config::Bool(name);
        return v && *v;
    }

    // FUN_1008e380: at the server max level (or, on a development realm, level 60 / 70 / 80).
    bool AtMaxLevel() {
        const uint32_t level = s_deps.playerLevel ? s_deps.playerLevel() : 0;

        if (level >= CoA::GameEvents::ServerMaxLevel()) {
            return true;
        }

        return CoA::RealmInfo::Get().Dev() && (level == 0x3C || level == 0x46 || level == 0x50);
    }

    uint32_t CountItem(uint32_t entry) {   // FUN_10308570
        return s_deps.itemCount ? s_deps.itemCount(entry) : 0;
    }

    // FUN_10a39e70 via FUN_10a3a050 / FUN_10a39e30 / FUN_10a3a090: count 1..limit; gold = price x count in copper
    // (+0x1248), else price x count Marks of Ascension (item 0x5B9D2). The products wrap at 32 bits.
    uint32_t Validate(int kind, uint32_t count, bool gold) {
        const uint32_t limit = kind == 2 ? 0x37EC : 0x29F1;
        const uint32_t price = kind == 2 ? (gold ? 150000u : 0x21u) : (gold ? 200000u : 0x32u);

        if (!CoA::Wildcard::detail::WildcardMode()) {
            return 2;
        }

        const uint32_t available = Available(kind);

        if (available == 0) {
            return 3;
        }

        if (!CoA::Wildcard::detail::HasPlayer() || !AtMaxLevel()) {
            return 4;
        }

        if (!ConfigBool("CONFIG_WILDCARD_ROLL_REPURCHASING_ENABLED")) {
            return 5;
        }

        if (count < 1) {
            return 6;
        }

        if (limit < count) {
            return 7;
        }

        if (available < count) {
            return 8;
        }

        if (!gold) {
            return CountItem(0x5B9D2) < count * price ? 9 : 0;
        }

        if (!CoA::Wildcard::detail::HasPlayer()) {
            return 10;
        }

        const uint32_t money = s_deps.playerMoney ? s_deps.playerMoney() : 0;
        return money < count * price ? 10 : 0;
    }

    void PushResult(lua_State* L, uint32_t code) {   // FUN_10a29a30
        if (code < 11) {
            PushStr(L, kRepurchase[code]);
        } else {
            PushStr(L, ("UNEXPECTED_ENUM_VALUE_" + std::to_string(code)).c_str());
        }
    }

    // FUN_100e92d0: (number, boolean).
    bool ReadCountGold(lua_State* L, uint32_t& count, bool& gold) {
        if (!ValidateInput(L, { LUA_TNUMBER, LUA_TBOOLEAN })) {
            return false;
        }

        count = static_cast<uint32_t>(static_cast<int64_t>(CheckNumber(L, 1)));
        gold = lua_toboolean(L, 2) != 0;
        return true;
    }

    int CanRepurchase(lua_State* L, int kind) {   // FUN_10a2b6d0 / FUN_10a2b560 / FUN_10a2b750
        uint32_t count;
        bool gold;

        if (!ReadCountGold(L, count, gold)) {
            return 0;
        }

        const uint32_t code = Validate(kind, count, gold);
        PushBool(L, code == 0);

        if (code) {
            PushResult(L, code);
        } else {
            lua_pushnil(L);
        }

        return 2;
    }

    int CanRepurchaseRolls(lua_State* L) {
        return CanRepurchase(L, 0);
    }

    int CanRepurchaseAbilityRolls(lua_State* L) {
        return CanRepurchase(L, 1);
    }

    int CanRepurchaseTalentRolls(lua_State* L) {
        return CanRepurchase(L, 2);
    }

    // FUN_10a2d7a0 / FUN_10a2d660 / FUN_10a2d8e0: (count, gold) -> CMSG 0x6F1 / 0x6F2 / 0x6F3 {u32 count, u8 gold}.
    int Repurchase(lua_State* L, int kind) {
        uint32_t count;
        bool gold;

        if (!ReadCountGold(L, count, gold)) {
            return 0;
        }

        if (Validate(kind, count, gold) != 0) {
            PushBool(L, false);
            return 1;
        }

        CoA::Packet(kind == 0 ? 0x6F1 : kind == 1 ? 0x6F2 : 0x6F3).U32(count).U8(gold ? 1 : 0).Send();
        PushBool(L, true);
        return 1;
    }

    int RepurchaseRolls(lua_State* L) {
        return Repurchase(L, 0);
    }

    int RepurchaseAbilityRolls(lua_State* L) {
        return Repurchase(L, 1);
    }

    int RepurchaseTalentRolls(lua_State* L) {
        return Repurchase(L, 2);
    }

    int GetNumRepurchasableRolls(lua_State* L) {   // FUN_10a2c030
        lua_pushinteger(L, static_cast<int32_t>(Available(0)));
        return 1;
    }

    int GetNumRepurchasableAbilityRolls(lua_State* L) {   // FUN_10a2c010
        lua_pushinteger(L, static_cast<int32_t>(Available(1)));
        return 1;
    }

    int GetNumRepurchasableTalentRolls(lua_State* L) {   // FUN_10a2c050
        lua_pushinteger(L, static_cast<int32_t>(Available(2)));
        return 1;
    }

    // FUN_10a2be30 / FUN_10a2bdb0 / FUN_10a2beb0: (gold) -> the largest count <= available that validates, else 0.
    int GetMax(lua_State* L, int kind) {
        bool gold;

        if (!ReadBool(L, gold)) {
            return 0;
        }

        uint32_t n = Available(kind);

        while (n != 0 && Validate(kind, n, gold) != 0) {
            --n;
        }

        lua_pushinteger(L, static_cast<int32_t>(n));
        return 1;
    }

    int GetMaxRepurchasableRolls(lua_State* L) {
        return GetMax(L, 0);
    }

    int GetMaxRepurchasableAbilityRolls(lua_State* L) {
        return GetMax(L, 1);
    }

    int GetMaxRepurchasableTalentRolls(lua_State* L) {
        return GetMax(L, 2);
    }

    // FUN_10a2c4a0 (rolls and ability rolls) / FUN_10a2c520 (talent rolls): (count, gold) -> price x count, or -1 past
    // the count limit.
    int RollCost(lua_State* L, bool talent) {
        uint32_t count;
        bool gold;

        if (!ReadCountGold(L, count, gold)) {
            return 0;
        }

        if (count > (talent ? 0x37EBu : 0x29F0u)) {
            lua_pushinteger(L, -1);
            return 1;
        }

        const uint32_t price = talent ? (gold ? 150000u : 0x21u) : (gold ? 200000u : 0x32u);
        lua_pushinteger(L, static_cast<int32_t>(price * count));
        return 1;
    }

    int GetRepurchaseRollCost(lua_State* L) {
        return RollCost(L, false);
    }

    int GetRepurchaseTalentRollCost(lua_State* L) {
        return RollCost(L, true);
    }

    // FUN_10a2b5e0: (gold) -> any kind whose single-roll check fails only for a count / money reason (not NOT_WILDCARD /
    // NO_COUNT / NOT_MAX_LEVEL / NOT_ENABLED).
    int CanRepurchaseAnyRolls(lua_State* L) {
        bool gold;

        if (!ReadBool(L, gold)) {
            return 0;
        }

        bool any = false;

        for (int kind : { 0, 1, 2 }) {
            const uint32_t code = Validate(kind, 1, gold);
            any = !(code >= 2 && code <= 5) || any;
        }

        PushBool(L, any);
        return 1;
    }

    // ---- the reveal records and the deferred spellbook adds ---------------------------------------------------
    // CA's half of each record lives in CoA::CA::WildcardReveals (it reads them); this module's half (+0x10 the stop
    // code, +0x28 the timer) in the same order here.
    struct RevealTail {
        std::string stop;
        uint32_t timer = 0;
    };

    std::vector<RevealTail> s_revealTails;

    struct DeferredAdd {
        uint32_t spell;
        uint32_t notify;
        uint32_t replace;
    };

    std::map<uint32_t, std::vector<DeferredAdd>> s_deferred;   // +0x1A4, by entry
    bool s_inAdd = false;                                      // the trampoline's add is running

    std::vector<CoA::CA::Request>& Reveals() {
        std::vector<CoA::CA::Request>& v = CoA::CA::WildcardReveals();

        if (s_revealTails.size() != v.size()) {   // CA's list reset under this module (tests): keep the halves paired
            s_revealTails.resize(v.size());
        }

        return v;
    }

    bool RevealsPending() {
        return !Reveals().empty();
    }

    uint32_t FirstRank(uint32_t spell) {   // FUN_102163f0
        return s_deps.firstRank ? s_deps.firstRank(spell) : spell;
    }

    // FUN_102781e0: the add, with the learn messages muted on a wildcard realm (FUN_10308830), then list 0x10be2aec.
    void AddSpell(uint32_t spell, uint32_t notify, uint32_t replace) {
        const bool wildcard = CoA::Wildcard::detail::WildcardMode();

        if (wildcard) {
            ++CoA::ClientHooks::suppressLearnMessages;
        }

        if (s_deps.addKnownSpell) {
            s_inAdd = true;
            s_deps.addKnownSpell(spell, notify, replace);
            s_inAdd = false;
        }

        if (wildcard && CoA::ClientHooks::suppressLearnMessages > 0) {
            --CoA::ClientHooks::suppressLearnMessages;
        }

        CoA::ClientHooks::afterAddKnownSpell.Run(spell);
    }

    // FUN_10a31b00: an add of a spell whose first rank is being revealed waits for the reveal.
    bool Defer(uint32_t spell, uint32_t notify, uint32_t replace) {
        std::vector<CoA::CA::Request>& v = Reveals();

        if (v.empty()) {
            return false;
        }

        const uint32_t root = FirstRank(spell);

        for (const CoA::CA::Request& r : v) {
            if (r.u4 && r.u4 == root) {
                s_deferred[r.id].push_back({ spell, notify, replace });
                return true;
            }
        }

        return false;
    }

    // FUN_10a2fa70 (the 1.5 s timer LAB_10a31ae0, param = the entry): the reveal ends, its deferred adds run, and the
    // rapid-rolling session moves on.
    int32_t RevealDone(const void*, void* param) {
        const uint32_t entry = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(param));
        std::vector<CoA::CA::Request>& v = Reveals();
        size_t i = 0;

        while (i < v.size() && v[i].id != entry) {
            ++i;
        }

        if (i == v.size()) {
            return 0;
        }

        v.erase(v.begin() + static_cast<ptrdiff_t>(i));
        s_revealTails.erase(s_revealTails.begin() + static_cast<ptrdiff_t>(i));
        auto d = s_deferred.find(entry);

        if (d != s_deferred.end()) {
            for (const DeferredAdd& a : d->second) {
                AddSpell(a.spell, a.notify, a.replace);
            }

            s_deferred.erase(entry);
        }

        CoA::WildcardRapid::OnRevealDone(entry);
        return 0;
    }

    // The reveal timers are cancelled with the name "WildcardReveal" where the entry was the param (only the id and the
    // handler match, so they go either way).
    void DropReveals() {
        char name[] = "WildcardReveal";   // 0x10b608dc

        for (const RevealTail& t : s_revealTails) {
            CoA::Timer::Cancel(t.timer, RevealDone, name);
        }

        Reveals().clear();
        s_revealTails.clear();
        s_deferred.clear();   // FUN_10a31e10
    }

    // ---- roll / reset / starting-choice gates -----------------------------------------------------------------
    uint32_t StartingCount() {   // FUN_10154f50
        return static_cast<uint32_t>(ConfigInt("CONFIG_WILDCARD_STARTING_ABILITY_COUNT", 4));
    }

    // FUN_10155680: a wildcard build holding at most the starting count of abilities (+0x44, the count the CA update
    // caches).
    bool AtStartingCount(const Build& b) {
        return b.wildcard && b.u44 <= StartingCount();
    }

    int CanResetAbilities(lua_State* L) {   // FUN_10a2b7d0
        const Build* b = CoA::CA::ActiveBuild();
        PushBool(L, b && AtStartingCount(*b));
        return 1;
    }

    int ResetAbilities(lua_State* L) {   // FUN_10a2db10: CMSG 0x645, true
        CoA::Packet(0x645).Send();
        PushBool(L, true);
        return 1;
    }

    bool CanShowStartingChoice() {   // FUN_10a2f920
        if (RevealsPending()) {
            return false;
        }

        const Build* b = CoA::CA::ActiveBuild();
        return b && AtStartingCount(*b);
    }

    int CanShowStartingChoice(lua_State* L) {   // FUN_10a2b890
        PushBool(L, CanShowStartingChoice());
        return 1;
    }

    int RerollUnlockedStartingAbilities(lua_State* L) {   // FUN_10a2da20: CMSG 0x64B when nothing is revealing
        if (RevealsPending()) {
            PushBool(L, false);
            return 1;
        }

        CoA::Packet(0x64B).Send();
        PushBool(L, true);
        return 1;
    }

    int WillRollStartingAbilities(lua_State* L) {   // FUN_10a2e960 -> FUN_10159210
        const Build* b = CoA::CA::ActiveBuild();
        PushBool(L, b && b->wildcard && b->u44 < StartingCount());
        return 1;
    }

    int WillRollFirstNonStartingAbility(lua_State* L) {   // FUN_10a2e910 -> FUN_101591f0
        const Build* b = CoA::CA::ActiveBuild();
        PushBool(L, b && b->wildcard && b->u44 == StartingCount());
        return 1;
    }

    // FUN_10155120 / FUN_10155160: the unspent essence (budget above spent, FUN_10153190 / FUN_10153340 at the build's
    // level) covers one roll (FUN_10153410 / FUN_10153660).
    bool CanAffordRoll(const Build& b, bool talent) {
        const uint32_t cost = static_cast<uint32_t>(talent ? ConfigInt("CONFIG_WILDCARD_TALENT_ROLL_COST", 1)
                                                          : ConfigInt("CONFIG_WILDCARD_ABILITY_ROLL_COST", 2));
        const uint32_t spent = talent ? b.GlobalTE(0) : b.GlobalAE(0);
        const uint32_t budget = talent ? b.TEBudget(b.level) : b.AEBudget(b.level);
        return (spent < budget ? budget - spent : 0) >= cost;
    }

    bool CanAffordAnyRoll() {
        const Build* b = CoA::CA::ActiveBuild();
        return b && (CanAffordRoll(*b, false) || CanAffordRoll(*b, true));
    }

    int CanRollAbilities(lua_State* L) {   // FUN_10a2b820
        PushBool(L, !RevealsPending() && CanAffordAnyRoll());
        return 1;
    }

    // ---- the roll-ready poll (FUN_10a31760 / LAB_10a31a00) ----------------------------------------------------
    uint32_t s_pollTimer = 0;    // DAT_10d3d0a4
    bool s_rollReady = false;    // DAT_10d3d0a8
    uint32_t s_pollUntil = 0;    // DAT_10d3d0ac: 50 ms polls until this tick, then 1 s

    int32_t Poll(const void*, void*) {
        const bool ready = CanAffordAnyRoll();

        if (ready && !s_rollReady) {
            CoA::Events::Signal("WILDCARD_ROLL_READY", nullptr);
        }

        s_rollReady = ready;
        const uint32_t delay = static_cast<int32_t>(CoA::Wildcard::detail::TickCount() - s_pollUntil) < 0 ? 50 : 1000;
        s_pollTimer = CoA::Timer::Schedule(delay, Poll, nullptr);
        return 0;
    }

    void CancelPoll() {
        if (s_pollTimer) {
            CoA::Timer::Cancel(s_pollTimer, Poll, nullptr);
            s_pollTimer = 0;
        }
    }

    void ArmPoll() {
        s_pollUntil = CoA::Wildcard::detail::TickCount() + 1000;
        CancelPoll();
        s_pollTimer = CoA::Timer::Schedule(0x32, Poll, nullptr);
    }

    // ---- SMSG -------------------------------------------------------------------------------------------------
    // The 7a convention: a record is read only while its 20 bytes are left.
    const uint32_t kSpecRecordSize = 20;

    int32_t OnMiscList(void*, uint32_t, uint32_t, CDataStore* p) {   // 0x6EE
        uint32_t n = CoA::Read::U32(p);

        if (n > 20) {
            n = 20;
        }

        for (uint32_t i = 0; i < n && CoA::Read::Remaining(p) >= kSpecRecordSize; ++i) {
            s_words[(kCounterA >> 2) + i] = CoA::Read::U32(p);
            s_words[(kCounterB >> 2) + i] = CoA::Read::U32(p);
            s_words[(kRolls >> 2) + i] = CoA::Read::U32(p);
            s_words[(kAbilityRolls >> 2) + i] = CoA::Read::U32(p);
            s_words[(kTalentRolls >> 2) + i] = CoA::Read::U32(p);
        }

        return 1;
    }

    int32_t OnMiscUpdate(void*, uint32_t, uint32_t, CDataStore* p) {   // 0x6EF
        SetSpecWord(kCounterA, CoA::Read::U32(p));
        SetSpecWord(kCounterB, CoA::Read::U32(p));
        const uint32_t oldRolls = SpecWord(kRolls);
        const uint32_t oldAbility = SpecWord(kAbilityRolls);
        const uint32_t oldTalent = SpecWord(kTalentRolls);
        SetSpecWord(kRolls, CoA::Read::U32(p));
        SetSpecWord(kAbilityRolls, CoA::Read::U32(p));
        SetSpecWord(kTalentRolls, CoA::Read::U32(p));

        if (oldRolls != SpecWord(kRolls)) {
            CoA::Events::Signal("WILDCARD_REPURCHASABLE_ROLLS_CHANGED", "%u%u", oldRolls, SpecWord(kRolls));
        }

        if (oldAbility != SpecWord(kAbilityRolls)) {
            CoA::Events::Signal("WILDCARD_REPURCHASABLE_ABILITY_ROLLS_CHANGED", "%u%u", oldAbility, SpecWord(kAbilityRolls));
        }

        if (oldTalent != SpecWord(kTalentRolls)) {
            CoA::Events::Signal("WILDCARD_REPURCHASABLE_TALENT_ROLLS_CHANGED", "%u%u", oldTalent, SpecWord(kTalentRolls));
        }

        return 1;
    }

    int32_t OnRepurchaseResult(void*, uint32_t, uint32_t, CDataStore* p) {   // 0x6F4
        const std::string r = CoA::Read::Str(p);
        CoA::Events::Signal("WILDCARD_REPURCHASE_ROLLS_RESULT", "%s", r.c_str());
        return 1;
    }

    int32_t OnResetResult(void*, uint32_t, uint32_t, CDataStore* p) {   // 0x646
        const std::string r = CoA::Read::Str(p);
        CoA::Events::Signal("WILDCARD_RESET_ABILITIES_RESULT", "%s", r.c_str());
        return 1;
    }

    int32_t OnRerollResult(void*, uint32_t, uint32_t, CDataStore* p) {   // 0x64C
        const std::string r = CoA::Read::Str(p);
        CoA::Events::Signal("WILDCARD_REROLL_UNLOCKED_STARTING_ABILITIES_RESULT", "%s", r.c_str());
        return 1;
    }

    // SMSG 0x621 (FUN_10a30260): u32 entry, u32 ranks gained, str stop code.
    int32_t OnEntryLearned(void*, uint32_t, uint32_t, CDataStore* p) {
        const uint32_t entry = CoA::Read::U32(p);
        const uint32_t numRanks = CoA::Read::U32(p);
        const std::string stop = CoA::Read::Str(p);
        uint32_t spell = 0;

        if (CoA::CA::Row r = CoA::CA::FindRow(entry)) {
            spell = FirstRank(CoA::CA::SpellAtRankOf(r, 1));   // FUN_101c62e0(1) -> FUN_102163f0
        }

        const Build* b = CoA::CA::ActiveBuild();
        const uint32_t preRoll = b ? b->RankOf(entry) : 0;   // FUN_10152920
        CoA::Logger::Write(CoA::Logger::Debug, "WildcardModeMgr::HandleWildcardEntryLearnedOpcode: entry: " + std::to_string(entry) +
            ", numRanks: " + std::to_string(numRanks) + ", preRollRank: " + std::to_string(preRoll) + ", newRank: " +
            std::to_string(preRoll + numRanks) + ", stopRapidRollingCode: " + stop);
        CoA::CA::Request rec;
        rec.id = entry;
        rec.u4 = spell;
        rec.u8 = numRanks;
        rec.rank = preRoll;
        RevealTail tail;
        tail.stop = stop;
        tail.timer = CoA::Timer::Schedule(0x5DC, RevealDone, reinterpret_cast<void*>(static_cast<uintptr_t>(entry)));
        Reveals().push_back(rec);
        s_revealTails.push_back(tail);
        CoA::WildcardRapid::OnEntryLearned(entry, preRoll + numRanks, preRoll, stop);

        if (CoA::Wildcard::detail::StopCodeIndex(stop) == 0) {
            CoA::Events::Signal("WILDCARD_ENTRY_LEARNED", "%u%u%u", entry, preRoll + numRanks, preRoll);
        } else {
            CoA::Events::Signal("WILDCARD_RAPID_ROLL_LEARNED", "%u%u%u%s", entry, preRoll + numRanks, preRoll, stop.c_str());
        }

        return 1;
    }

    // SMSG 0x61E (FUN_10a30dd0): str result. Outside a rapid-rolling session a successful unlearn this client asked for
    // (C_CharacterAdvancement.UnlearnID) is followed by the roll itself (CMSG 0x643 with nothing desired).
    int32_t OnUnlearnResult(void*, uint32_t, uint32_t, CDataStore* p) {
        const std::string result = CoA::Read::Str(p);
        CoA::Events::Signal("WILDCARD_UNLEARN_ABILITY_RESULT", "%s", result.c_str());
        const bool consumed = CoA::WildcardRapid::OnUnlearnResult(result);
        const bool pending = CoA::WildcardRapid::TakeRerollPending();

        if (!consumed && pending && CoA::Wildcard::detail::UnlearnResultIndex(result) == 0) {
            CoA::Packet(0x643).U32(0).U32(0).U32(0).U8(0).Send();
        }

        return 1;
    }

    // SMSG 0x644 (FUN_10a30bb0): str result; a CAN_START_RAPID_ROLLING_* answer is a rapid-roll result.
    int32_t OnRollResult(void*, uint32_t, uint32_t, CDataStore* p) {
        const std::string result = CoA::Read::Str(p);
        CoA::WildcardRapid::OnRollResult(result);

        if (result.find("CAN_START_RAPID_ROLLING_") != std::string::npos) {
            CoA::Events::Signal("WILDCARD_RAPID_ROLL_RESULT", "%s", result.c_str());
        } else {
            CoA::Events::Signal("WILDCARD_ROLL_ABILITIES_RESULT", "%s", result.c_str());
        }

        return 1;
    }

    // FUN_10a2f950: the five per-spec arrays zeroed (+0x04 is not), the poll stopped and its state cleared, the
    // outstanding-unlearn flag dropped, the reveals cancelled and dropped.
    void OnGlueScreen() {
        for (uint32_t base : { kCounterA, kCounterB, kRolls, kAbilityRolls, kTalentRolls }) {
            std::fill(&s_words[base / 4], &s_words[base / 4] + 20, 0u);
        }

        CancelPoll();
        s_rollReady = false;
        s_pollUntil = 0;
        CoA::WildcardRapid::TakeRerollPending();   // DAT_10d3d0b0 = 0
        DropReveals();
    }

    // FUN_10a318b0 (every world entry): the reveals cancelled and dropped, the rapid-rolling session reset
    // (FUN_10a33b10), the roll-ready flag cleared, and the roll poll restarted at 1000 ms.
    void OnEnterWorld() {
        DropReveals();
        CoA::WildcardRapid::ResetSession();
        s_rollReady = false;
        CancelPoll();
        s_pollTimer = CoA::Timer::Schedule(1000, Poll, nullptr);
    }

    const CoA::Binding s_bindings[] = {
        COA_NATIVE("C_Wildcard", "CanRepurchaseRolls", CanRepurchaseRolls),
        COA_NATIVE("C_Wildcard", "CanRepurchaseAbilityRolls", CanRepurchaseAbilityRolls),
        COA_NATIVE("C_Wildcard", "CanRepurchaseTalentRolls", CanRepurchaseTalentRolls),
        COA_NATIVE("C_Wildcard", "RepurchaseRolls", RepurchaseRolls),
        COA_NATIVE("C_Wildcard", "RepurchaseAbilityRolls", RepurchaseAbilityRolls),
        COA_NATIVE("C_Wildcard", "RepurchaseTalentRolls", RepurchaseTalentRolls),
        COA_NATIVE("C_Wildcard", "GetNumRepurchasableRolls", GetNumRepurchasableRolls),
        COA_NATIVE("C_Wildcard", "GetNumRepurchasableAbilityRolls", GetNumRepurchasableAbilityRolls),
        COA_NATIVE("C_Wildcard", "GetNumRepurchasableTalentRolls", GetNumRepurchasableTalentRolls),
        COA_NATIVE("C_Wildcard", "GetMaxRepurchasableRolls", GetMaxRepurchasableRolls),
        COA_NATIVE("C_Wildcard", "GetMaxRepurchasableAbilityRolls", GetMaxRepurchasableAbilityRolls),
        COA_NATIVE("C_Wildcard", "GetMaxRepurchasableTalentRolls", GetMaxRepurchasableTalentRolls),
        COA_NATIVE("C_Wildcard", "GetRepurchaseRollCost", GetRepurchaseRollCost),
        COA_NATIVE("C_Wildcard", "GetRepurchaseAbilityRollCost", GetRepurchaseRollCost),
        COA_NATIVE("C_Wildcard", "GetRepurchaseTalentRollCost", GetRepurchaseTalentRollCost),
        COA_NATIVE("C_Wildcard", "CanRepurchaseAnyRolls", CanRepurchaseAnyRolls),
        COA_NATIVE("C_Wildcard", "CanResetAbilities", CanResetAbilities),
        COA_NATIVE("C_Wildcard", "ResetAbilities", ResetAbilities),
        COA_NATIVE("C_Wildcard", "CanShowStartingChoice", CanShowStartingChoice),
        COA_NATIVE("C_Wildcard", "RerollUnlockedStartingAbilities", RerollUnlockedStartingAbilities),
        COA_NATIVE("C_Wildcard", "WillRollStartingAbilities", WillRollStartingAbilities),
        COA_NATIVE("C_Wildcard", "WillRollFirstNonStartingAbility", WillRollFirstNonStartingAbility),
        COA_NATIVE("C_Wildcard", "CanRollAbilities", CanRollAbilities),
    };
}

// ---- shared with the other two files ------------------------------------------------------------------------------
CoA::Wildcard::Deps& CoA::Wildcard::Dependencies() {
    return s_deps;
}

bool CoA::Wildcard::detail::WildcardMode() {
    return ((CoA::GameMode::Mode() >> 6) & 1) != 0;
}

bool CoA::Wildcard::detail::HasPlayer() {
    return s_deps.hasPlayer && s_deps.hasPlayer();
}

uint32_t CoA::Wildcard::detail::TickCount() {
    return s_deps.tickCount ? s_deps.tickCount() : 0;
}

void CoA::Wildcard::detail::InitRolls() {   // 0x10a31460 (+ 0x10a3a270 for SMSG 0x6F4)
    CoA::Lifecycle::OnGlueScreen(OnGlueScreen);
    CoA::ClientHooks::enterWorld.Add(OnEnterWorld);
    COA_EVENT("WILDCARD_ROLL_READY");
    COA_EVENT("WILDCARD_UNLEARN_ABILITY_RESULT");
    COA_EVENT("WILDCARD_ROLL_ABILITIES_RESULT");
    COA_EVENT("WILDCARD_RESET_ABILITIES_RESULT");
    COA_EVENT("WILDCARD_REROLL_UNLOCKED_STARTING_ABILITIES_RESULT");
    COA_EVENT("WILDCARD_ENTRY_LEARNED");
    COA_EVENT("WILDCARD_RAPID_ROLL_RESULT");
    COA_EVENT("WILDCARD_RAPID_ROLL_LEARNED");
    COA_EVENT("WILDCARD_REPURCHASE_ROLLS_RESULT");
    COA_EVENT("WILDCARD_REPURCHASABLE_ROLLS_CHANGED");
    COA_EVENT("WILDCARD_REPURCHASABLE_ABILITY_ROLLS_CHANGED");
    COA_EVENT("WILDCARD_REPURCHASABLE_TALENT_ROLLS_CHANGED");
    COA_PACKET(0x61E, OnUnlearnResult);
    COA_PACKET(0x644, OnRollResult);
    COA_PACKET(0x646, OnResetResult);
    COA_PACKET(0x64C, OnRerollResult);
    COA_PACKET(0x621, OnEntryLearned);
    COA_PACKET(0x6EE, OnMiscList);
    COA_PACKET(0x6EF, OnMiscUpdate);
    COA_PACKET(0x6F4, OnRepurchaseResult);
    CoA::LuaReg::AddModule(s_bindings, sizeof(s_bindings) / sizeof(s_bindings[0]));
}

void CoA::Wildcard::detail::ResetRolls() {
    std::fill(std::begin(s_words), std::end(s_words), 0u);
    CoA::CA::WildcardReveals().clear();
    s_revealTails.clear();
    s_deferred.clear();
    s_inAdd = false;
    s_pollTimer = 0;
    s_rollReady = false;
    s_pollUntil = 0;
    s_deps = CoA::Wildcard::Deps();
}

// The mode manager first, then the WildcardMgr (0x10a31460 + 0x10a3a270, 0x10a34970); then what CharacterAdvancementMgr
// and the suggestion manager ask of them (the CA side's Deps, Plan 7).
void CoA::Wildcard::Init() {
    detail::InitRolls();
    detail::InitSets();
    detail::InitRapid();
    CoA::CA::Deps& d = CoA::CA::Dependencies();
    d.wildcardUndesired = CoA::WildcardSets::IsUndesired;
    d.wildcardRollActive = CoA::WildcardRapid::Active;
    d.wildcardRollInFlight = CoA::WildcardRapid::InFlight;
    d.wildcardMarkRerollPending = CoA::WildcardRapid::MarkRerollPending;
    d.wildcardArmPoll = CoA::WildcardRolls::ArmPoll;
}

void CoA::Wildcard::Reset() {
    detail::ResetRolls();
    detail::ResetSets();
    detail::ResetRapid();
}

// ---- exports ------------------------------------------------------------------------------------------------------
uint32_t CoA::WildcardRolls::SpecCounter(int which) {
    return SpecWord(which == 0 ? kCounterA : kCounterB);
}

bool CoA::WildcardRolls::RevealsPending() {
    return ::RevealsPending();
}

bool CoA::WildcardRolls::AtMaxLevel() {
    return ::AtMaxLevel();
}

void CoA::WildcardRolls::ArmPoll() {
    ::ArmPoll();
}

uint32_t CoA::WildcardRolls::StartingCount() {
    return ::StartingCount();
}

bool CoA::WildcardRolls::CanShowStartingChoice() {
    return ::CanShowStartingChoice();
}

bool CoA::WildcardRolls::CanAffordRoll(bool talent) {
    const Build* b = CoA::CA::ActiveBuild();
    return b && ::CanAffordRoll(*b, talent);
}

bool CoA::WildcardRolls::AddKnownSpellHook(uint32_t spell, uint32_t notify, uint32_t replace) {   // 0x10275ED0
    if (s_inAdd) {
        return false;
    }

    if (!Defer(spell, notify, replace)) {
        AddSpell(spell, notify, replace);
    }

    return true;
}
