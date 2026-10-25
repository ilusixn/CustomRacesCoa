// AscCAMgr.cpp's bindings that change a build or send one: the pending-build edits and their Can* forms, the pending
// summary, apply, direct learn / unlearn / lock (CMSG 0x727 / 0x61D / 0x658 / 0x65A / 0x528 / 0x213), the CA_CONFIRM_*
// lists, specializations and loadouts (CMSG 0x778..0x77A), swap and trade.
#include "CoA/Source/Modules/CANativesInternal.h"
#include "CoA/Source/Modules/GameMode.h"
#include "CoA/Source/Runtime/Log.h"
#include "CoA/Source/Runtime/Packet.h"
#include "CoA/Source/Runtime/Script.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <algorithm>

namespace {
#include "CoA/Source/Modules/CAEnums.generated.inc"
}

using namespace CoA::CA;
using namespace CoA::CA::detail;
using CoA::Script::CheckNumber;
using CoA::Script::CheckString;
using CoA::Script::PushBool;
using CoA::Script::PushInt;
using CoA::Script::PushNils;
using CoA::Script::PushStr;
using CoA::Script::ReadBool;
using CoA::Script::ReadInt2;
using CoA::Script::ReadNumber;
using CoA::Script::ReadString;
using CoA::Script::ToInt;
using CoA::Script::ValidateInput;

namespace {
    int32_t I(uint32_t v) {
        return static_cast<int32_t>(v);
    }

    // FUN_10168f80 / FUN_100999d0 / FUN_10169de0: CA_UPDATE_ENTRIES_*, CA_LEARN_* and CA_UNLEARN_* names.
    std::string UpdateName(uint32_t r) {
        return r < 9 ? kUpdateResults[r] : "UNEXPECTED_ENUM_VALUE_" + std::to_string(r);
    }

    std::string LearnName(uint32_t r) {
        return r < 0x2E ? kLearnResults[r] : "UNEXPECTED_ENUM_VALUE_" + std::to_string(r);
    }

    std::string UnlearnName(uint32_t r) {
        return r < 0x17 ? kUnlearnResults[r] : "UNEXPECTED_ENUM_VALUE_" + std::to_string(r);
    }

    void PushLearnResult(lua_State* L, uint32_t result) {   // (ok, nil) or (false, CA_LEARN_* name)
        PushBool(L, result == 0);

        if (result == 0) {
            lua_pushnil(L);
        } else {
            PushStr(L, LearnName(result).c_str());
        }
    }

    // (ok) then, on failure, (updateResult, learnResult, id, rank); nils when ok.
    void PushApplyResult(lua_State* L, uint32_t result, uint32_t learnResult, uint32_t id, uint32_t rank) {
        PushBool(L, result == 0);

        if (result == 0) {
            PushNils(L, 4);
            return;
        }

        PushStr(L, UpdateName(result).c_str());
        PushStr(L, LearnName(learnResult).c_str());
        PushInt(L, I(id));
        PushInt(L, I(rank));
    }

    uint32_t Apply(const Build& base, const std::vector<Entry>& entries, uint32_t& lr, uint32_t& id, uint32_t& rank, ApplyCosts& costs) {
        lr = id = rank = 0;
        return ValidateApply(ActivePlayerGuid(), base, entries, lr, id, rank, costs);
    }

    uint32_t Apply(const Build& base, const std::vector<Entry>& entries, uint32_t& lr, uint32_t& id, uint32_t& rank) {
        ApplyCosts costs;
        return Apply(base, entries, lr, id, rank, costs);
    }

    // CMSG 0x727 (FUN_10166a50): u32 count, then per entry u32 id, u32 rank, u32 +0xC, u8 +0x10, u32 +0x18, u32 +0x1C.
    void Send0727(const std::vector<Entry>& entries) {
        CoA::Packet pk(0x727);
        pk.U32(static_cast<uint32_t>(entries.size()));

        for (const Entry& e : entries) {
            pk.U32(e.id).U32(e.rank).U32(e.u0c).U8(e.locked).U32(e.u18).U32(e.u1c);
        }

        pk.Send();
    }

    bool HasMasteries(Row r) {
        for (uint32_t i = 0; i < 3; ++i) {
            if (RowU32(r, 0x1B0 + i * 4)) {
                return true;
            }
        }

        return false;
    }

    bool WildcardMode() {   // FUN_1031ef10 bit 6
        return (CoA::GameMode::Mode() >> 6) & 1;
    }

    // ---- pending build edits -----------------------------------------------------------------------------------
    int UnlearnAllSpells(lua_State* L) {
        CoA::Packet(0x528).Send();
        PushBool(L, true);
        return 1;
    }

    int UnlearnAllTalents(lua_State* L) {
        CoA::Packet(0x213).Send();
        PushBool(L, true);
        return 1;
    }

    int CancelPendingBuild(lua_State*) {
        ResetPendingBuild();
        return 0;
    }

    // What Clear (FUN_10175ef0) removes: every entry outside group 1, with forReset the rows flagged 2 kept.
    bool Cleared(const Entry& e, bool forReset) {
        return RowU32(e.row, 0x74) != 1 && (!forReset || !RowHasFlag(e.row, 2));
    }

    int ClearPendingBuild(lua_State* L) {
        bool forReset;

        if (!ReadBool(L, forReset)) {
            return Usage(L, "Usage: C_CharacterAdvancement.ClearPendingBuild(forReset)");
        }

        Build* b = PendingOrError(L, "ClearPendingBuild");

        if (!b) {
            return 0;
        }

        const std::vector<Entry> snapshot = b->entries;

        for (const Entry& e : snapshot) {
            if (Cleared(e, forReset)) {
                b->Remove(e.id);
            }
        }

        BumpPendingVersion();
        return 0;
    }

    int ClearPendingBuildByTab(lua_State* L) {   // FUN_10176030
        if (!ValidateInput(L, { LUA_TSTRING, LUA_TSTRING, LUA_TBOOLEAN })) {
            return Usage(L, "Usage: C_CharacterAdvancement.ClearPendingBuildByTab(classType, tabType, forReset)");
        }

        const uint32_t cls = EnumArg(L, 1, kClassTypes, 0x2F);
        const uint32_t tab = EnumArg(L, 2, kTabTypes, 0x5F);
        const bool forReset = lua_toboolean(L, 3) != 0;
        Build* b = PendingOrError(L, "ClearPendingBuildByTab");

        if (!b) {
            return 0;
        }

        const std::vector<Entry> snapshot = b->entries;

        for (const Entry& e : snapshot) {
            if (RowU32(e.row, 0x80) == cls && RowU32(e.row, 0x84) == tab && (!forReset || !RowHasFlag(e.row, 2))) {
                b->Remove(e.id);
            }
        }

        BumpPendingVersion();
        return 0;
    }

    // FUN_1016d220: add `ranks` ranks, all-or-nothing when more than one.
    void AddRanks(Build& pending, uint32_t id, uint32_t ranks) {
        if (ranks > 1) {
            Build copy(pending, true);

            for (uint32_t i = 0; i < ranks; ++i) {
                if (copy.ValidateLearn(0, id, {}, {}) != 0) {
                    return;
                }

                copy.AddRank(id);
            }

            for (uint32_t i = 0; i < ranks; ++i) {
                if (pending.ValidateLearn(0, id, {}, {}) == 0) {
                    pending.AddRank(id);
                }
            }
        } else if (pending.ValidateLearn(0, id, {}, {}) == 0) {
            pending.AddRank(id);
        }

        pending.Reorder(false);
        BumpPendingVersion();
    }

    // FUN_1016da10 / FUN_1016e400: an entry with masteries may need one of them first; the copy takes the first
    // mastery that validates when the entry itself does not.
    uint32_t TakeMastery(Build& copy, uint32_t id, Row r) {
        if (copy.ValidateLearn(0, id, {}, {}) == 0) {
            return 0;
        }

        for (uint32_t i = 0; i < 3; ++i) {
            const uint32_t m = RowU32(r, 0x1B0 + i * 4);

            if (m && copy.ValidateLearn(0, m, {}, {}) == 0) {
                copy.AddRank(m);
                return m;
            }
        }

        return 0;
    }

    void AddRanksWithMastery(Build& pending, uint32_t id, uint32_t ranks, Row r) {
        Build copy(pending, true);
        const uint32_t mastery = TakeMastery(copy, id, r);

        for (uint32_t i = 0; i < ranks; ++i) {
            if (copy.ValidateLearn(0, id, {}, {}) != 0) {
                return;
            }

            copy.AddRank(id);
        }

        if (mastery) {
            pending.AddRank(mastery);
        }

        for (uint32_t i = 0; i < ranks; ++i) {
            if (pending.ValidateLearn(0, id, {}, {}) == 0) {
                pending.AddRank(id);
            }
        }

        BumpPendingVersion();
    }

    int AddByEntryID(lua_State* L) {
        int32_t id;
        int32_t ranks;

        if (!ReadInt2(L, id, ranks)) {
            return Usage(L, "Usage: C_CharacterAdvancement.AddByEntryID(entryId, numRanks)");
        }

        Build* b = PendingOrError(L, "AddByEntryID");

        if (!b) {
            return 0;
        }

        Row r = FindRow(static_cast<uint32_t>(id));

        if (!r) {
            return Usage(L, "AddByEntryID: invalid entryId");
        }

        if (HasMasteries(r)) {
            AddRanksWithMastery(*b, static_cast<uint32_t>(id), static_cast<uint32_t>(ranks), r);
        } else {
            AddRanks(*b, static_cast<uint32_t>(id), static_cast<uint32_t>(ranks));
        }

        return 0;
    }

    // FUN_1016e050: one rank validates on the pending build itself (MeetsInvestment... passes only the BAD_ABILITY and
    // NOT_ENOUGH_INVESTED_* slots); several ranks validate one by one on a copy.
    int CanAddRanks(lua_State* L, Build& pending, uint32_t id, uint32_t ranks, bool investmentOnly) {
        if (ranks < 2) {
            static const std::vector<uint32_t> investment = { 3, 0x26, 0x27, 0x28 };
            PushLearnResult(L, pending.ValidateLearn(0, id, investmentOnly ? investment : std::vector<uint32_t> {}, {}));
            return 2;
        }

        Build copy(pending, true);

        for (uint32_t i = 0; i < ranks; ++i) {
            if (const uint32_t result = copy.ValidateLearn(0, id, {}, {})) {
                PushLearnResult(L, result);
                return 2;
            }

            copy.AddRank(id);
        }

        PushLearnResult(L, 0);
        return 2;
    }

    int CanAddRanksWithMastery(lua_State* L, Build& pending, uint32_t id, uint32_t ranks, Row r) {
        Build copy(pending, true);
        TakeMastery(copy, id, r);

        for (uint32_t i = 0; i < ranks; ++i) {
            if (const uint32_t result = copy.ValidateLearn(0, id, {}, {})) {
                PushLearnResult(L, result);
                return 2;
            }

            copy.AddRank(id);
        }

        PushLearnResult(L, 0);
        return 2;
    }

    int CanAddByEntryID(lua_State* L) {
        int32_t id;
        int32_t ranks;

        if (!ReadInt2(L, id, ranks)) {
            return Usage(L, "Usage: C_CharacterAdvancement.CanAddByEntryID(entryId, numRanks)");
        }

        Build* b = PendingOrError(L, "CanAddByEntryID");

        if (!b) {
            return 0;
        }

        Row r = FindRow(static_cast<uint32_t>(id));

        if (!r) {
            return Usage(L, "CanAddByEntryID: invalid entryId");
        }

        if (HasMasteries(r)) {
            return CanAddRanksWithMastery(L, *b, static_cast<uint32_t>(id), static_cast<uint32_t>(ranks), r);
        }

        return CanAddRanks(L, *b, static_cast<uint32_t>(id), static_cast<uint32_t>(ranks), false);
    }

    int MeetsInvestmentForAddByEntryID(lua_State* L) {
        int32_t id;
        int32_t ranks;

        if (!ReadInt2(L, id, ranks)) {
            return Usage(L, "Usage: C_CharacterAdvancement.MeetsInvestmentForAddByEntryID(entryId, numRanks)");
        }

        Build* b = PendingOrError(L, "MeetsInvestmentForAddByEntryID");

        if (!b) {
            return 0;
        }

        if (!FindRow(static_cast<uint32_t>(id))) {
            return Usage(L, "MeetsInvestmentForAddByEntryID: invalid entryId");
        }

        return CanAddRanks(L, *b, static_cast<uint32_t>(id), static_cast<uint32_t>(ranks), true);
    }

    // FUN_101523d0: pending vs active -- {id, rank delta, added, removed, modified}.
    struct Diff {
        uint32_t id;
        int32_t delta;
        bool added;
        bool removed;
        bool modified;
    };

    std::vector<Diff> PendingDiff(const Build& pending, const Build& active) {
        std::vector<Diff> out;

        for (const Entry& e : pending.entries) {
            if (!active.Has(e.id)) {
                out.push_back({ e.id, static_cast<int32_t>(e.rank), true, false, false });
            }
        }

        for (const Entry& e : active.entries) {
            if (!pending.Has(e.id)) {
                out.push_back({ e.id, -static_cast<int32_t>(e.rank), false, true, false });
            }
        }

        for (const Entry& e : pending.entries) {
            bool listed = false;

            for (const Diff& d : out) {
                listed = listed || d.id == e.id;
            }

            if (listed) {
                continue;
            }

            const int32_t delta = static_cast<int32_t>(e.rank) - static_cast<int32_t>(active.RankOf(e.id));

            if (delta) {
                out.push_back({ e.id, delta, false, false, true });
            }
        }

        return out;
    }

    int IsPending(lua_State* L) {
        Build* b = PendingOrError(L, "IsPending");

        if (!b) {
            return 0;
        }

        PushBool(L, !PendingDiff(*b, *ActiveBuild()).empty());
        return 1;
    }

    int GetPendingSummary(lua_State* L) {
        Build* b = PendingOrError(L, "GetPendingSummary");

        if (!b) {
            return 0;
        }

        const std::vector<Diff> diff = PendingDiff(*b, *ActiveBuild());

        if (diff.empty()) {
            lua_pushnil(L);
            return 1;
        }

        // The original writes every diff's fields into the one table (no per-entry subtables): the last one wins.
        lua_createtable(L, 0, 0);
        lua_checkstack(L, 2);

        for (const Diff& d : diff) {
            SetInt(L, "Entry", I(d.id));
            SetInt(L, "CARankDiff", d.delta);
            lua_pushstring(L, "Added");
            lua_pushboolean(L, d.added);
            lua_settable(L, -3);
            lua_pushstring(L, "Removed");
            lua_pushboolean(L, d.removed);
            lua_settable(L, -3);
            lua_pushstring(L, "Modified");
            lua_pushboolean(L, d.modified);
            lua_settable(L, -3);
        }

        return 1;
    }

    // ---- remove / apply ----------------------------------------------------------------------------------------
    // The mastery a removal is really about: the entry itself when it is one, else the first of its masteries the
    // build has; 0 when neither.
    uint32_t RemovalMastery(const Build& b, Row r) {
        if (RowHasFlag(r, 0x1000)) {
            return RowU32(r, 0);
        }

        for (uint32_t i = 0; i < 3; ++i) {
            if (const uint32_t m = RowU32(r, 0x1B0 + i * 4)) {
                if (b.Has(m)) {
                    return m;
                }
            }
        }

        return 0;
    }

    int RemoveByEntryID(lua_State* L) {
        uint32_t id;

        if (!ReadNumber(L, id)) {
            return Usage(L, "Usage: C_CharacterAdvancement.RemoveByEntryID(entryId)");
        }

        Build* b = PendingOrError(L, "RemoveByEntryID");

        if (!b) {
            return 0;
        }

        Row r = FindRow(id);

        if (!r) {
            return Usage(L, "RemoveByEntryID: invalid entryId");
        }

        if (const uint32_t m = RemovalMastery(*b, r)) {
            // FUN_10173b10: only when the whole transition would apply.
            Build copy(*b, true);
            copy.RemoveWithMastery(m);
            uint32_t lr;
            uint32_t eid;
            uint32_t er;

            if (Apply(*b, copy.entries, lr, eid, er) == 0) {
                b->RemoveWithMastery(m);
                b->Reorder(false);
                BumpPendingVersion();
            }

            return 0;
        }

        if (b->ValidateUnlearn(0, id, {}, {}) != 0) {
            return 0;
        }

        b->Remove(id);
        b->Reorder(false);
        BumpPendingVersion();
        return 0;
    }

    int CanRemoveByEntryID(lua_State* L) {
        uint32_t id;

        if (!ReadNumber(L, id)) {
            return Usage(L, "Usage: C_CharacterAdvancement.CanRemoveByEntryID(entryId)");
        }

        Build* b = PendingOrError(L, "CanRemoveByEntryID");

        if (!b) {
            return 0;
        }

        Row r = FindRow(id);

        if (!r) {
            return Usage(L, "CanRemoveByEntryID: invalid entryId");
        }

        if (const uint32_t m = RemovalMastery(*b, r)) {
            // FUN_1016e9f0: the transition's verdict, 5 values.
            Build copy(*b, true);
            copy.RemoveWithMastery(m);
            uint32_t lr;
            uint32_t eid;
            uint32_t er;
            const uint32_t result = Apply(*b, copy.entries, lr, eid, er);
            PushApplyResult(L, result, lr, eid, er);
            return 5;
        }

        // FUN_1016e820: the unlearn rules, NOT_WILDCARD excluded.
        const uint32_t result = b->ValidateUnlearn(0, id, {}, { 0x15 });
        PushBool(L, result == 0);

        if (result == 0) {
            lua_pushnil(L);
        } else {
            PushStr(L, UnlearnName(result).c_str());
        }

        return 2;
    }

    // Marks, money, then {MoneyCost, MarkOfAscensionCost, TokenEntry, TokenCost} per token cost.
    void PushCosts(lua_State* L, const ApplyCosts& costs) {
        PushInt(L, I(costs.marks));
        PushInt(L, I(costs.money));
        lua_createtable(L, 0, static_cast<int>(costs.tokens.size()));
        lua_checkstack(L, 2);

        for (size_t i = 0; i < costs.tokens.size(); ++i) {
            lua_pushnumber(L, static_cast<double>(i + 1));
            lua_createtable(L, 0, 4);
            lua_checkstack(L, 2);
            SetInt(L, "MoneyCost", I(costs.tokens[i].money));
            SetInt(L, "MarkOfAscensionCost", I(costs.tokens[i].marks));
            SetInt(L, "TokenEntry", I(costs.tokens[i].item));
            SetInt(L, "TokenCost", I(costs.tokens[i].count));
            lua_settable(L, -3);
        }
    }

    int CanApplyPendingBuild(lua_State* L) {
        Build* b = PendingOrError(L, "CanApplyPendingBuild");

        if (!b) {
            return 0;
        }

        uint32_t lr;
        uint32_t id;
        uint32_t rank;
        ApplyCosts costs;
        const uint32_t result = Apply(*ActiveBuild(), b->entries, lr, id, rank, costs);
        PushApplyResult(L, result, lr, id, rank);
        PushCosts(L, costs);
        return 8;
    }

    int CanClearPendingBuild(lua_State* L) {
        bool forReset;

        if (!ReadBool(L, forReset)) {
            return Usage(L, "Usage: C_CharacterAdvancement.CanClearPendingBuild(forReset)");
        }

        Build* b = PendingOrError(L, "CanClearPendingBuild");

        if (!b) {
            return 0;
        }

        Build copy(*b, true);
        const std::vector<Entry> snapshot = copy.entries;

        for (const Entry& e : snapshot) {
            if (Cleared(e, forReset)) {
                copy.Remove(e.id);
            }
        }

        uint32_t lr;
        uint32_t id;
        uint32_t rank;
        const uint32_t result = Apply(*b, copy.entries, lr, id, rank);
        PushApplyResult(L, result, lr, id, rank);
        return 5;
    }

    int ApplyPendingBuild(lua_State* L) {   // FUN_101742c0
        Build* b = PendingOrError(L, "ApplyPendingBuild");

        if (!b) {
            return 0;
        }

        uint32_t lr;
        uint32_t id;
        uint32_t rank;

        if (Apply(*ActiveBuild(), b->entries, lr, id, rank) != 0) {
            return Usage(L, "ApplyPendingBuild: pending build cannot be applied");
        }

        Send0727(b->entries);
        return 0;
    }

    // ---- direct learn / unlearn --------------------------------------------------------------------------------
    // FUN_10166450 (FUN_1011f230 "FrameScript::lua_toarray") or a single number (FUN_1008ba00).
    bool IdsArg(lua_State* L, std::vector<uint32_t>& ids) {
        ids.clear();

        if (ValidateInput(L, { LUA_TTABLE })) {
            lua_pushnil(L);

            while (lua_next(L, 1)) {
                if (lua_isnumber(L, -1)) {
                    ids.push_back(static_cast<uint32_t>(ToInt(lua_tonumber(L, -1))));
                } else {
                    CoA::Log::Printf("FrameScript::lua_toarray Invalid lua value type");
                }

                lua_settop(L, -2);
            }

            return true;
        }

        uint32_t id;

        if (!ReadNumber(L, id)) {
            return false;
        }

        ids.push_back(id);
        return true;
    }

    // The active player's record and world object, which the mastery dependency and every confirmation require.
    bool ActivePlayerRecord() {
        const uint64_t guid = ActivePlayerGuid();
        Player* p = M().player;
        return p && p->guid == guid && UnitExists(guid);
    }

    // FUN_10149a00: an entry with masteries, none of which the build has, brings its last mastery along.
    uint32_t MasteryDependency(const Build& b, uint32_t id) {
        Row r = FindRow(id);

        if (!r || !ActivePlayerRecord()) {
            return 0;
        }

        uint32_t last = 0;

        for (uint32_t i = 0; i < 3; ++i) {
            if (const uint32_t m = RowU32(r, 0x1B0 + i * 4)) {
                if (b.Has(m)) {
                    return 0;
                }

                last = m;
            }
        }

        return last;
    }

    int LearnID(lua_State* L) {   // 0x1017D140
        std::vector<uint32_t> ids;

        if (!IdsArg(L, ids)) {
            return Usage(L, "Usage: C_CharacterAdvancement.LearnID(entryId | {entryId1, ...})");
        }

        Build copy(*ActiveBuild(), true);
        copy.UpdatePointers(false);

        for (uint32_t id : ids) {
            if (const uint32_t dep = MasteryDependency(copy, id)) {
                copy.AddRank(dep);
            }

            copy.AddRank(id);
        }

        // A group-1 entry is exclusive: every other group-1 row's entry goes (0x1017D35D..0x1017D397).
        for (uint32_t id : ids) {
            if (Row r = FindRow(id)) {
                if (RowU32(r, 0x74) == 1) {
                    ForEachRow([&](Row other) {
                        if (RowU32(other, 0x74) == 1 && RowU32(other, 0) != id) {
                            copy.Remove(RowU32(other, 0));
                        }
                    });
                }
            }
        }

        uint32_t result = 0;
        uint32_t id = 0;
        uint32_t rank = 0;

        if (!copy.Optimize(true, result, id, rank)) {
            PushBool(L, false);
            PushStr(L, "OPTIMIZE_FOR_TRAVERSAL_FAILED");
            PushStr(L, LearnName(result).c_str());
            PushInt(L, I(id));
            PushInt(L, I(rank));
            return 5;
        }

        uint32_t lr;
        const uint32_t apply = Apply(*ActiveBuild(), copy.entries, lr, id, rank);

        if (apply == 0) {
            Send0727(copy.entries);
        }

        PushApplyResult(L, apply, lr, id, rank);
        return 5;
    }

    int CanLearnID(lua_State* L) {   // 0x10174C10
        std::vector<uint32_t> ids;

        if (!IdsArg(L, ids)) {
            return Usage(L, "Usage: C_CharacterAdvancement.CanLearnID(entryId | {entryId1, ...})");
        }

        Build copy(*ActiveBuild(), true);

        for (uint32_t id : ids) {
            if (const uint32_t result = copy.ValidateLearn(0, id, {}, {})) {
                PushBool(L, false);
                PushStr(L, LearnName(result).c_str());
                PushInt(L, I(id));
                return 3;
            }

            copy.AddRank(id);
        }

        PushBool(L, true);
        PushStr(L, LearnName(0).c_str());
        PushInt(L, 0);
        return 3;
    }

    int CanUnlearnID(lua_State* L) {
        std::vector<uint32_t> ids;

        if (!IdsArg(L, ids)) {
            return Usage(L, "Usage: C_CharacterAdvancement.CanUnlearnID(entryId | {entryId1, ...})");
        }

        const Build* active = ActiveBuild();
        Build copy(*active, true);

        for (uint32_t id : ids) {
            const uint32_t mastery = copy.MasteryTarget(id);
            const bool fusion = copy.ClassFusion() && mastery != 0;
            std::vector<uint32_t> exclude;
            std::vector<Entry> saved;

            if (fusion) {
                saved = copy.TakeMasteryBearers();
            } else if (copy.MasteryInvolved(id)) {
                exclude = { 10, 9, 11 };   // MASTERY_ID, MISSING_CONNECTED_ENTRIES, MISSING_REQUIRED_ID
            }

            if (const uint32_t result = copy.ValidateUnlearn(0, fusion ? mastery : id, {}, exclude)) {
                PushBool(L, false);
                PushStr(L, UnlearnName(result).c_str());   // FUN_10169ed0
                PushInt(L, I(id));
                return 3;
            }

            if (fusion) {
                copy.Remove(mastery);
                copy.ReaddWhileValid(saved);
            } else {
                copy.RemoveWithMastery(id);
            }
        }

        uint32_t lr;
        uint32_t eid;
        uint32_t rank;
        const uint32_t apply = Apply(*active, copy.entries, lr, eid, rank);
        PushBool(L, apply == 0);

        if (apply == 0) {
            PushStr(L, UnlearnName(0).c_str());
            PushInt(L, 0);
        } else {
            PushStr(L, UpdateName(apply).c_str());
            PushInt(L, I(eid));
        }

        return 3;
    }

    // LockID / UnlockID: CMSG 0x658 / 0x65A with the entry id.
    int SendIdPacket(lua_State* L, uint32_t opcode, const char* usage) {
        uint32_t id;

        if (!ReadNumber(L, id)) {
            return Usage(L, usage);
        }

        CoA::Packet(opcode).U32(id).Send();
        PushBool(L, true);
        return 1;
    }

    int LockID(lua_State* L) {
        return SendIdPacket(L, 0x658, "Usage: C_CharacterAdvancement.LockID(entryId)");
    }

    int UnlockID(lua_State* L) {
        return SendIdPacket(L, 0x65A, "Usage: C_CharacterAdvancement.UnlockID(entryId)");
    }

    // ---- confirmations (CA_CONFIRM_*) --------------------------------------------------------------------------
    struct Confirm {
        uint32_t type;
        uint32_t a;
        uint32_t b;
    };

    // FUN_10149a00: learning an entry with masteries, none known, brings the last one along.
    bool LearnConfirm(const Build& b, uint32_t id, std::vector<Confirm>& out) {
        Row r = FindRow(id);

        if (!r || !ActivePlayerRecord()) {
            return false;
        }

        uint32_t last = 0;

        for (uint32_t i = 0; i < 3; ++i) {
            if (const uint32_t m = RowU32(r, 0x1B0 + i * 4)) {
                last = m;

                if (b.Has(m)) {
                    return !out.empty();
                }
            }
        }

        if (last) {
            out.push_back({ 2, last, 0 });
        }

        return !out.empty();
    }

    // A cost the unit can pay, as the confirmation to show: the token, else marks, else gold.
    void CostConfirm(uint64_t unit, const TokenCost& c, uint32_t token, uint32_t marks, uint32_t gold, std::vector<Confirm>& out) {
        if (!((c.item && c.count) || c.marks || c.money)) {
            return;
        }

        if (c.item && c.count && c.count <= ItemCount(unit, c.item)) {
            out.push_back({ token, c.item, c.count });
        } else if (c.marks && c.marks <= ItemCount(unit, kMarkOfAscensionItem)) {
            out.push_back({ marks, c.marks, 0 });
        } else if (c.money && c.money <= Money(unit)) {
            out.push_back({ gold, c.money, 0 });
        }
    }

    // Wildcard Scroll of Fortune tokens per spec (FUN_10a30180/10a300a0 talents, FUN_10a2fd80/10a2fca0 abilities,
    // FUN_10a2ffc0/10a2fee0 any); token index 0x3D and item 0 past spec 19.
    const uint32_t kAnyScroll[20] = { 0x10CDBC, 0x10F4CC, 0x111BDC, 0x1142EC, 0x1169FC, 0x11910C, 0x11B81C, 0x11DF2C, 0xB799A,
        0xB799B, 0xB799C, 0xB799D, 0xB79A7, 0xB79A8, 0xB79A9, 0xB79AA, 0xB79AB, 0xB79AC, 0xB79AD, 0xB79AE };

    bool ScrollConfirm(uint32_t tokenBase, uint32_t itemBase, std::vector<Confirm>& out) {
        const uint32_t spec = static_cast<uint32_t>(ActiveSpecIndex());
        const uint32_t token = spec < 20 ? tokenBase + spec : 0x3D;
        const Deps& d = Dependencies();

        if (!d.tokenAtLeast || !d.tokenAtLeast(token, 1)) {
            return false;
        }

        const uint32_t item = spec >= 20 ? 0 : itemBase ? itemBase + spec : kAnyScroll[spec];
        out.push_back({ 3, item, 1 });
        return true;
    }

    // FUN_10149e00: unlearning drops a mastery nobody else carries, or costs something.
    bool UnlearnConfirm(const Build& b, uint32_t id, std::vector<Confirm>& out) {
        Row r = FindRow(id);

        if (!r || !ActivePlayerRecord()) {
            return false;
        }

        const uint64_t unit = ActivePlayerGuid();
        uint32_t last = 0;
        bool any = false;
        bool known = false;
        bool shared = false;

        for (uint32_t i = 0; i < 3 && !shared; ++i) {
            const uint32_t m = RowU32(r, 0x1B0 + i * 4);

            if (!m) {
                continue;
            }

            any = true;
            last = m;

            if (!b.Has(m)) {
                continue;
            }

            known = true;
            auto it = Idx().byMastery.find(m);

            if (it != Idx().byMastery.end()) {
                for (Row other : it->second) {
                    if (RowU32(other, 0) != id && b.Has(RowU32(other, 0))) {
                        shared = true;   // another known entry keeps the mastery
                        break;
                    }
                }
            }
        }

        if (!shared && any && known) {
            out.push_back({ 6, last, 0 });
        }

        if (!WildcardMode()) {
            if (UnitLevel(unit) > 9) {
                CostConfirm(unit, UnlearnCostOf(b, r), 3, 4, 5, out);
            }
        } else {
            const int type = RowType(r);

            if (!(type == 2 && ScrollConfirm(0x28, 0x17E8D, out)) && !((type == 1 || type == 4) && ScrollConfirm(0x14, 0x17E79, out))) {
                ScrollConfirm(0, 0, out);
            }
        }

        return !out.empty();
    }

    // FUN_10149b00 / FUN_10149c80. The marks case pushes CA_CONFIRM_UNLEARN_MARKS (4) for both, as the original does.
    bool PurgeConfirm(const Build& b, bool talents, std::vector<Confirm>& out) {
        if (!ActivePlayerRecord()) {
            return false;
        }

        CostConfirm(ActivePlayerGuid(), PurgeCost(b, talents), talents ? 11 : 7, 4, talents ? 13 : 9, out);
        const Deps& d = Dependencies();

        if (d.specHasCreatorBuild && d.specHasCreatorBuild(ActiveSpecIndex())) {
            out.push_back({ talents ? 14u : 10u, 0, 0 });
        }

        return !out.empty();
    }

    // FUN_10169740: {{Error = CA_CONFIRM_*, Arg1 = a, Arg2 = b}, ...}.
    int PushConfirmResult(lua_State* L, const std::vector<Confirm>& list) {
        PushBool(L, !list.empty());

        if (list.empty()) {
            return 1;
        }

        lua_createtable(L, 0, static_cast<int>(list.size()));
        lua_checkstack(L, 2);

        for (size_t i = 0; i < list.size(); ++i) {
            lua_pushnumber(L, static_cast<double>(i + 1));
            lua_createtable(L, 0, 3);
            lua_checkstack(L, 2);
            const uint32_t t = list[i].type;
            SetStr(L, "Error", t < 0xF ? kConfirmTypes[t] : "UNEXPECTED_ENUM_VALUE_" + std::to_string(t));   // FUN_10169c90
            SetInt(L, "Arg1", I(list[i].a));
            SetInt(L, "Arg2", I(list[i].b));
            lua_settable(L, -3);
        }

        return 2;
    }

    int ShouldConfirmLearnID(lua_State* L) {
        std::vector<uint32_t> ids;

        if (!IdsArg(L, ids)) {
            return Usage(L, "Usage: C_CharacterAdvancement.ShouldConfirmLearnID(entryId | {entryId1, ...})");
        }

        std::vector<Confirm> list;
        Build copy(*ActiveBuild(), true);
        copy.UpdatePointers(false);

        for (uint32_t id : ids) {
            std::vector<Confirm> one;

            if (LearnConfirm(copy, id, one)) {
                list.insert(list.end(), one.begin(), one.end());
            }

            copy.AddRank(id);
        }

        return PushConfirmResult(L, list);
    }

    int ShouldConfirmUnlearnID(lua_State* L) {
        std::vector<uint32_t> ids;

        if (!IdsArg(L, ids)) {
            return Usage(L, "Usage: C_CharacterAdvancement.ShouldConfirmUnlearnID(entryId | {entryId1, ...})");
        }

        std::vector<Confirm> list;
        Build copy(*ActiveBuild(), true);
        copy.UpdatePointers(false);

        for (uint32_t id : ids) {
            const uint32_t mastery = copy.MasteryTarget(id);
            const bool fusion = copy.ClassFusion() && mastery != 0;
            std::vector<Confirm> one;

            if (UnlearnConfirm(copy, id, one)) {
                for (const Confirm& c : one) {
                    if (!fusion || c.type != 6) {
                        list.push_back(c);
                    }
                }
            }

            if (fusion) {
                list.push_back({ 6, mastery, 0 });
            }

            copy.RemoveWithMastery(id);
        }

        return PushConfirmResult(L, list);
    }

    int ShouldConfirmUnlearnAllSpells(lua_State* L) {
        std::vector<Confirm> list;
        PurgeConfirm(*ActiveBuild(), false, list);
        return PushConfirmResult(L, list);
    }

    int ShouldConfirmUnlearnAllTalents(lua_State* L) {
        std::vector<Confirm> list;
        PurgeConfirm(*ActiveBuild(), true, list);
        return PushConfirmResult(L, list);
    }

    // FUN_10175450 / FUN_101755d0: (ok, CA_PURGE_{ABILITIES,TALENTS}_*).
    int CanUnlearnAll(lua_State* L, bool talents) {
        const uint32_t r = ActiveBuild()->ValidatePurge(talents, 0);
        PushBool(L, r == 0);
        const char* const* names = talents ? kPurgeTalentResults : kPurgeResults;
        PushStr(L, (r < 10 ? std::string(names[r]) : "UNEXPECTED_ENUM_VALUE_" + std::to_string(r)).c_str());
        return 2;
    }

    int CanUnlearnAllSpells(lua_State* L) {
        return CanUnlearnAll(L, false);
    }

    int CanUnlearnAllTalents(lua_State* L) {
        return CanUnlearnAll(L, true);
    }

    // FUN_1017fbe0. Normal modes: remove each (with its mastery bookkeeping), re-validate the order and send the whole
    // build as 0x727. Wildcard: one Scroll of Fortune request (CMSG 0x61D, u32 id) per id, only while no roll is
    // running and no request is outstanding.
    int UnlearnID(lua_State* L) {
        std::vector<uint32_t> ids;

        if (!IdsArg(L, ids)) {
            return Usage(L, "Usage: C_CharacterAdvancement.UnlearnID(entryId | {entryId1, ...})");
        }

        if (!WildcardMode()) {
            Build copy(*ActiveBuild(), true);
            copy.UpdatePointers(false);

            for (uint32_t id : ids) {
                copy.RemoveWithMastery(id);
            }

            if (!copy.Reorder(false)) {
                PushBool(L, false);
                return 1;
            }

            Send0727(copy.entries);
            PushBool(L, true);
            return 1;
        }

        const Deps& d = Dependencies();

        if ((d.wildcardRollActive && d.wildcardRollActive()) || (d.wildcardRollInFlight && d.wildcardRollInFlight()) || !M().requests.empty()) {
            PushBool(L, false);
            return 1;
        }

        for (uint32_t id : ids) {
            CoA::Packet(0x61D).U32(id).Send();
        }

        // FUN_10a31760 re-arms the mode manager's roll-ready poll; FUN_10a2f910 flags the unlearn as outstanding until
        // SMSG 0x61E.
        if (d.wildcardArmPoll) {
            d.wildcardArmPoll();
        }

        if (d.wildcardMarkRerollPending) {
            d.wildcardMarkRerollPending();
        }

        PushBool(L, true);
        return 1;
    }

    // ---- specializations and loadouts --------------------------------------------------------------------------
    // FUN_10182500: leave `spec` -- drop every entry outside choice group 1 except the spec-neutral ones (+0x84 ==
    // 0x57 with no costs at +0x88 / +0x90 / +0x98), the spec's entry (+0x70), and its spell's entry (+0x60) when that
    // may be unlearned.
    void LeaveSpec(Build& b, uint32_t spec) {
        const std::vector<Entry> snapshot = b.entries;

        for (const Entry& e : snapshot) {
            if (e.row && RowU32(e.row, 0x74) != 1 &&
                (RowU32(e.row, 0x84) != 0x57 || RowU32(e.row, 0x88) || RowU32(e.row, 0x90) || RowU32(e.row, 0x98))) {
                b.Remove(e.id);
            }
        }

        Row sp = ChrSpecs().Row(spec);

        if (!sp) {
            return;
        }

        if (b.Has(RowU32(sp, 0x70))) {
            b.Remove(RowU32(sp, 0x70));
        }

        Row src = RowBySpell(RowU32(sp, 0x60));

        if (src && Visible(src) && b.ValidateUnlearn(0, RowU32(src, 0), {}, {}) == 0) {
            b.Remove(RowU32(src, 0));
        }
    }

    // FUN_1016df10: take `spec` -- add its entry and its spell's entry at rank 1, then TraverseOrFix (called with an
    // empty keep set; the ids added are only reported to the caller).
    void EnterSpec(Build& b, uint32_t spec) {
        if (Row sp = ChrSpecs().Row(spec)) {
            if (!b.Has(RowU32(sp, 0x70))) {
                b.AddEntry(RowU32(sp, 0x70), 1);
            }

            Row src = RowBySpell(RowU32(sp, 0x60));

            if (src && Visible(src)) {
                b.AddEntry(RowU32(src, 0), 1);
            }
        }

        b.TraverseOrFix({});
    }

    // FUN_10175190: (false, NO_PENDING_BUILD) / (false, CHR_SPEC_ALREADY_ACTIVE) / (true, nil). The original also runs
    // the switch on a throwaway copy of the pending build; nothing reads it.
    int CanSwitchActiveChrSpec(lua_State* L) {
        uint32_t spec;

        if (!ReadNumber(L, spec)) {
            return Usage(L, "Usage: C_CharacterAdvancement.CanSwitchActiveChrSpec(specID)");
        }

        Build* p = M().pending;

        if (!p) {
            PushBool(L, false);
            PushStr(L, "NO_PENDING_BUILD");
            return 2;
        }

        const uint32_t current = SpecOf(*p);

        if (current == spec) {
            PushBool(L, false);
            PushStr(L, "CHR_SPEC_ALREADY_ACTIVE");
            return 2;
        }

        Build copy(*p, true);
        LeaveSpec(copy, current);
        EnterSpec(copy, spec);
        copy.Reorder(false);
        PushBool(L, true);
        lua_pushnil(L);
        return 2;
    }

    int SwitchActiveChrSpec(lua_State* L) {   // FUN_1017f700
        uint32_t spec;

        if (!ReadNumber(L, spec)) {
            return Usage(L, "Usage: C_CharacterAdvancement.SwitchActiveChrSpec(specID)");
        }

        Build* b = M().pending;

        if (!b) {
            return Usage(L, "SwitchActiveChrSpec: pending build is not available");
        }

        const uint32_t current = SpecOf(*b);

        if (current != spec) {
            LeaveSpec(*b, current);
            EnterSpec(*b, spec);
            b->Reorder(false);
            BumpPendingVersion();
        }

        return 0;
    }

    // Loadouts live on the server: CMSG 0x778 activate(uuid), 0x779 rename(uuid, name), 0x77A sort(uuid, u32 order).
    int ActivateLoadout(lua_State* L) {   // FUN_10173fd0
        std::string uuid;

        if (!ReadString(L, uuid)) {
            return Usage(L, "Usage: C_CharacterAdvancement.ActivateLoadout(uuid)");
        }

        CoA::Packet(0x778).Str(uuid.c_str()).Send();
        PushBool(L, true);
        return 1;
    }

    int SetLoadoutName(lua_State* L) {   // FUN_1017eb10
        if (!ValidateInput(L, { LUA_TSTRING, LUA_TSTRING })) {
            return Usage(L, "Usage: C_CharacterAdvancement.SetLoadoutName(uuid, name)");
        }

        const std::string uuid = CheckString(L, 1);
        const std::string name = CheckString(L, 2);
        CoA::Packet(0x779).Str(uuid.c_str()).Str(name.c_str()).Send();
        PushBool(L, true);
        return 1;
    }

    int SetLoadoutSortOrder(lua_State* L) {   // FUN_1017ece0
        if (!ValidateInput(L, { LUA_TSTRING, LUA_TNUMBER })) {
            return Usage(L, "Usage: C_CharacterAdvancement.SetLoadoutSortOrder(uuid, sortOrder)");
        }

        const std::string uuid = CheckString(L, 1);
        const uint32_t order = static_cast<uint32_t>(ToInt(CheckNumber(L, 2)));
        CoA::Packet(0x77A).Str(uuid.c_str()).U32(order).Send();
        PushBool(L, true);
        return 1;
    }

    // ---- swap and trade ----------------------------------------------------------------------------------------
    // Swap data: the existing entry and the rank it keeps (0 = removed), and the entries it becomes.
    struct SwapEntry {
        uint32_t entry = 0;
        uint32_t rank = 0;
    };

    struct SwapData {   // 0x14 bytes
        SwapEntry existing;
        std::vector<SwapEntry> updated;
    };

    int AbsIndex(lua_State* L, int idx) {
        return idx < 0 ? lua_gettop(L) + idx + 1 : idx;
    }

    // FUN_1016b890 / FUN_1016b0e0: {Entry = n, NewCARank = n}; a missing member is logged and left 0.
    SwapEntry ReadSwapEntry(lua_State* L, int idx) {
        SwapEntry e;
        idx = AbsIndex(L, idx);

        if (lua_type(L, idx) != LUA_TTABLE) {
            CoA::Log::Printf("lua_tovalue<CharacterAdvancementSwapEntry>: Value at index %d is not a table!", idx);
            return e;
        }

        auto field = [&](const char* name, uint32_t& out) {
            lua_getfield(L, idx, name);

            if (lua_isnumber(L, -1)) {
                out = static_cast<uint32_t>(ToInt(lua_tonumber(L, -1)));
            } else {
                CoA::Log::Printf("lua_tovalue<CharacterAdvancementSwapEntry>: Missing member '%s' in table at index %d", name, idx);
            }

            lua_settop(L, -2);
        };
        field("Entry", e.entry);
        field("NewCARank", e.rank);
        return e;
    }

    // FUN_1016adf0: {ExistingEntry = {...}, UpdatedEntries = {{...}, ...}}.
    SwapData ReadSwapData(lua_State* L, int idx) {
        SwapData d;
        idx = AbsIndex(L, idx);
        lua_getfield(L, idx, "ExistingEntry");

        if (lua_type(L, -1) == LUA_TTABLE) {
            d.existing = ReadSwapEntry(L, -1);
        } else {
            CoA::Log::Printf("lua_tovalue<CharacterAdvancementSwapData>: Missing member 'ExistingEntry' in table at index %d", idx);
        }

        lua_settop(L, -2);
        lua_getfield(L, idx, "UpdatedEntries");   // FUN_101658e0

        if (lua_type(L, -1) == LUA_TTABLE) {
            const int list = lua_gettop(L);
            lua_pushnil(L);

            while (lua_next(L, list)) {
                d.updated.push_back(ReadSwapEntry(L, -1));
                lua_settop(L, -2);
            }
        } else {
            CoA::Log::Printf("lua_tovalue<CharacterAdvancementSwapData>: Missing member 'UpdatedEntries' in table at index %d", idx);
        }

        lua_settop(L, -2);
        return d;
    }

    // FUN_10149950: the first of the row's +0x1B0 ids that is a row, else the id itself.
    uint32_t SwapTarget(uint32_t id) {
        if (Row r = FindRow(id)) {
            for (uint32_t i = 0; i < 3; ++i) {
                if (const uint32_t m = RowU32(r, 0x1B0 + i * 4)) {
                    if (FindRow(m)) {
                        return m;
                    }
                }
            }
        }

        return id;
    }

    // FUN_10149820: the existing entry is held above the rank it keeps, and every update raises its target's rank.
    bool SwapValid(const SwapData& d, const Build& b) {
        if (!FindRow(d.existing.entry) || b.RankOf(d.existing.entry) <= d.existing.rank) {
            return false;
        }

        for (const SwapEntry& u : d.updated) {
            if (!FindRow(u.entry) || b.RankOf(SwapTarget(u.entry)) >= u.rank) {
                return false;
            }
        }

        return true;
    }

    // FUN_101488f0: drop the existing entry to the rank it keeps, or remove it together with the mastery it belongs to
    // (itself when it is one, else its first known) and everything listing that mastery.
    void RemoveSwapped(Build& b, const SwapEntry& existing) {
        if (existing.rank) {
            b.SetRank(existing.entry, existing.rank);
            return;
        }

        Row r = FindRow(existing.entry);

        if (!r) {
            return;
        }

        uint32_t mastery = RowHasFlag(r, 0x1000) ? existing.entry : 0;

        for (uint32_t i = 0; i < 3 && !mastery; ++i) {
            if (const uint32_t m = RowU32(r, 0x1B0 + i * 4)) {
                if (b.Has(m)) {
                    mastery = m;
                }
            }
        }

        if (!mastery) {
            b.Remove(existing.entry);
            return;
        }

        std::vector<uint32_t> doomed = { mastery };

        for (const Entry& e : b.entries) {
            if (!e.row) {
                continue;
            }

            for (uint32_t i = 0; i < 3; ++i) {
                if (RowU32(e.row, 0x1B0 + i * 4) == mastery) {   // FUN_101c64f0
                    doomed.push_back(e.id);
                    break;
                }
            }
        }

        for (uint32_t id : doomed) {
            b.Remove(id);
        }
    }

    void ApplySwap(Build& b, const SwapData& d) {
        RemoveSwapped(b, d.existing);

        for (const SwapEntry& u : d.updated) {
            const uint32_t target = SwapTarget(u.entry);

            if (b.Has(target)) {
                b.SetRank(target, u.rank);    // FUN_10158940
            } else {
                b.AddEntry(target, u.rank);   // FUN_10151190
            }
        }
    }

    // FUN_10148dd0: the apply check (FUN_10151fd0) on a copy with the swap applied and reordered.
    uint32_t ValidateSwap(const SwapData& d, const Build& b, uint32_t& learnResult, uint32_t& id, uint32_t& rank) {
        Build copy(b, false);
        ApplySwap(copy, d);
        copy.Reorder(false);
        return Apply(b, copy.entries, learnResult, id, rank);
    }

    // FUN_10172f50: {{ExistingEntry = {Entry, NewCARank}, UpdatedEntries = {{Entry, NewCARank}, ...}}, ...}.
    void PushSwapList(lua_State* L, const std::vector<SwapData>& list) {
        auto pushEntry = [&](const SwapEntry& e) {   // FUN_101730e0
            lua_createtable(L, 0, 2);
            SetInt(L, "Entry", I(e.entry));
            SetInt(L, "NewCARank", I(e.rank));
        };
        lua_createtable(L, static_cast<int>(list.size()), 0);

        for (size_t i = 0; i < list.size(); ++i) {
            lua_pushinteger(L, static_cast<int32_t>(i + 1));
            lua_createtable(L, 0, 2);
            lua_pushstring(L, "ExistingEntry");
            pushEntry(list[i].existing);
            lua_settable(L, -3);
            lua_pushstring(L, "UpdatedEntries");
            lua_createtable(L, static_cast<int>(list[i].updated.size()), 0);

            for (size_t j = 0; j < list[i].updated.size(); ++j) {
                lua_pushinteger(L, static_cast<int32_t>(j + 1));
                pushEntry(list[i].updated[j]);
                lua_settable(L, -3);
            }

            lua_settable(L, -3);
            lua_settable(L, -3);
        }
    }

    int CanSwapEntriesByID(lua_State* L) {   // FUN_10174f80
        if (lua_type(L, 1) != LUA_TTABLE) {
            return Usage(L, "Usage: C_CharacterAdvancement.CanSwapEntriesByID(data)");
        }

        const SwapData d = ReadSwapData(L, 1);
        Build* p = M().pending;

        if (!p) {
            return Usage(L, "CanSwapEntriesByID: pending build is not available");
        }

        if (!SwapValid(d, *p)) {
            return Usage(L, "CanSwapEntriesByID: invalid swap data");
        }

        uint32_t lr;
        uint32_t id;
        uint32_t rank;
        const uint32_t result = ValidateSwap(d, *p, lr, id, rank);
        PushApplyResult(L, result, lr, id, rank);
        return 5;
    }

    int SwapEntriesByID(lua_State* L) {   // FUN_1017f590
        if (lua_type(L, 1) != LUA_TTABLE) {
            return Usage(L, "Usage: C_CharacterAdvancement.SwapEntriesByID(data)");
        }

        const SwapData d = ReadSwapData(L, 1);
        Build* b = M().pending;

        if (!b) {
            return Usage(L, "SwapEntriesByID: pending build is not available");
        }

        if (!SwapValid(d, *b)) {
            return Usage(L, "SwapEntriesByID: invalid swap data");
        }

        uint32_t lr;
        uint32_t id;
        uint32_t rank;

        if (ValidateSwap(d, *b, lr, id, rank) != 0) {
            return Usage(L, "SwapEntriesByID: swap not allowed");
        }

        ApplySwap(*b, d);
        b->Reorder(false);
        BumpPendingVersion();
        return 0;
    }

    // FUN_10183a20: every entry of the existing one's kind in the class list that could replace it, each at the
    // highest rank that passes the apply check.
    int AvailableForSwap(lua_State* L, const SwapEntry& existing, uint32_t cls) {
        Build* b = M().pending;

        if (!b) {
            return Usage(L, "GetEntriesAvailableForSwap: pending build is not available");
        }

        SwapData d;
        d.existing = existing;

        if (!SwapValid(d, *b)) {
            return Usage(L, "GetEntriesAvailableForSwap: invalid swap data");
        }

        d.updated.push_back({});
        std::vector<SwapData> results;
        results.reserve(100);
        Row existingRow = FindRow(existing.entry);
        Build copy(*b, false);
        RemoveSwapped(copy, existing);
        uint32_t lastId = 0;
        uint32_t lastRank = 0;
        static const std::vector<Row> none;   // DAT_10bdd49c
        const std::vector<Row>* candidates = &none;
        auto it = Idx().byClass.find(cls);   // FUN_101496a0

        if (existingRow && it != Idx().byClass.end()) {
            candidates = &ClassListFor(it->second, existingRow);
        }

        for (Row c : *candidates) {
            if (lastId) {
                if (lastRank == 0) {
                    copy.Remove(lastId);
                } else {
                    copy.SetRank(lastId, lastRank);
                }
            }

            d.updated.back().entry = RowU32(c, 0);
            const uint32_t target = SwapTarget(RowU32(c, 0));

            if (!FindRow(target)) {
                continue;
            }

            lastId = target;
            lastRank = target == existing.entry ? existing.rank : copy.RankOf(target);

            if (lastRank == 0) {
                copy.AddEntry(target, MaxRank(c));
            } else {
                copy.SetRank(target, MaxRank(c));
            }

            for (uint32_t r = MaxRank(c); r != 0; --r) {
                d.updated.back().rank = r;

                if (!SwapValid(d, *b)) {
                    continue;
                }

                copy.SetRank(target, r);
                copy.Reorder(false);
                uint32_t lr;
                uint32_t id;
                uint32_t rank;

                if (Apply(*b, copy.entries, lr, id, rank) == 0) {
                    results.push_back(d);
                    break;
                }
            }
        }

        PushSwapList(L, results);
        return 1;
    }

    int GetEntriesAvailableForSwap(lua_State* L) {   // handler_GetEntriesAvailableForSwap
        if (lua_type(L, 1) != LUA_TTABLE) {
            return Usage(L, "Usage: C_CharacterAdvancement.GetEntriesAvailableForSwap(existingEntry)");
        }

        return AvailableForSwap(L, ReadSwapEntry(L, 1), 0);
    }

    int GetEntriesAvailableForSwapInClass(lua_State* L) {   // FUN_10176cb0
        if (!ValidateInput(L, { LUA_TTABLE, LUA_TSTRING })) {
            return Usage(L, "Usage: C_CharacterAdvancement.GetEntriesAvailableForSwapInClass(existingEntry, classType)");
        }

        const SwapEntry existing = ReadSwapEntry(L, 1);
        const uint32_t cls = EnumArg(L, 2, kClassTypes, 0x2F);   // FUN_1016b250
        return AvailableForSwap(L, existing, cls);
    }

    // FUN_10176da0: every pending entry of the desired one's kind that could be traded for it, at the highest workable
    // rank (the requested one, else from its max down to 1).
    int GetEntriesAvailableForTrade(lua_State* L) {
        if (lua_type(L, 1) != LUA_TTABLE) {
            return Usage(L, "Usage: C_CharacterAdvancement.GetEntriesAvailableForTrade(desiredEntry)");
        }

        const SwapEntry desired = ReadSwapEntry(L, 1);
        Build* b = M().pending;

        if (!b) {
            return Usage(L, "GetEntriesAvailableForTrade: pending build is not available");
        }

        Row want = FindRow(desired.entry);

        if (!want) {
            return Usage(L, "GetEntriesAvailableForTrade: invalid desired entryId");
        }

        SwapData d;
        d.updated.push_back({ desired.entry, 0 });
        std::vector<SwapData> results;
        results.reserve(100);
        auto kind = [](Row r, int a, int b2) {
            const int t = RowType(r);
            return t == a || t == b2;
        };

        for (const Entry& e : b->entries) {
            if (!e.row || RowU32(e.row, 0) == desired.entry) {
                continue;
            }

            if (kind(e.row, 1, 4) != kind(want, 1, 4) || kind(e.row, 2, 2) != kind(want, 2, 2) || kind(e.row, 3, 3) != kind(want, 3, 3)) {
                continue;
            }

            d.existing = { RowU32(e.row, 0), 0 };
            uint32_t top = desired.rank ? desired.rank : MaxRank(want);
            const uint32_t low = desired.rank ? desired.rank : 1;

            if (e.rank < top) {
                top = e.rank;
            }

            for (uint32_t r = top; low <= r; --r) {
                d.updated.back().rank = r;

                if (!SwapValid(d, *b)) {
                    continue;
                }

                uint32_t lr;
                uint32_t id;
                uint32_t rank;

                if (ValidateSwap(d, *b, lr, id, rank) == 0) {
                    results.push_back(d);
                    break;
                }

                if (r == 1) {
                    break;
                }
            }
        }

        PushSwapList(L, results);
        return 1;
    }

    const CoA::Binding s_bindings[] = {
        COA_NATIVE("C_CharacterAdvancement", "UnlearnAllSpells", UnlearnAllSpells),
        COA_NATIVE("C_CharacterAdvancement", "UnlearnAllTalents", UnlearnAllTalents),
        COA_NATIVE("C_CharacterAdvancement", "CancelPendingBuild", CancelPendingBuild),
        COA_NATIVE("C_CharacterAdvancement", "ClearPendingBuild", ClearPendingBuild),
        COA_NATIVE("C_CharacterAdvancement", "ClearPendingBuildByTab", ClearPendingBuildByTab),
        COA_NATIVE("C_CharacterAdvancement", "AddByEntryID", AddByEntryID),
        COA_NATIVE("C_CharacterAdvancement", "CanAddByEntryID", CanAddByEntryID),
        COA_NATIVE("C_CharacterAdvancement", "MeetsInvestmentForAddByEntryID", MeetsInvestmentForAddByEntryID),
        COA_NATIVE("C_CharacterAdvancement", "IsPending", IsPending),
        COA_NATIVE("C_CharacterAdvancement", "GetPendingSummary", GetPendingSummary),
        COA_NATIVE("C_CharacterAdvancement", "RemoveByEntryID", RemoveByEntryID),
        COA_NATIVE("C_CharacterAdvancement", "CanRemoveByEntryID", CanRemoveByEntryID),
        COA_NATIVE("C_CharacterAdvancement", "CanApplyPendingBuild", CanApplyPendingBuild),
        COA_NATIVE("C_CharacterAdvancement", "CanClearPendingBuild", CanClearPendingBuild),
        COA_NATIVE("C_CharacterAdvancement", "ApplyPendingBuild", ApplyPendingBuild),
        COA_NATIVE("C_CharacterAdvancement", "LearnID", LearnID),
        COA_NATIVE("C_CharacterAdvancement", "CanLearnID", CanLearnID),
        COA_NATIVE("C_CharacterAdvancement", "CanUnlearnID", CanUnlearnID),
        COA_NATIVE("C_CharacterAdvancement", "LockID", LockID),
        COA_NATIVE("C_CharacterAdvancement", "UnlockID", UnlockID),
        COA_NATIVE("C_CharacterAdvancement", "ShouldConfirmLearnID", ShouldConfirmLearnID),
        COA_NATIVE("C_CharacterAdvancement", "ShouldConfirmUnlearnID", ShouldConfirmUnlearnID),
        COA_NATIVE("C_CharacterAdvancement", "ShouldConfirmUnlearnAllSpells", ShouldConfirmUnlearnAllSpells),
        COA_NATIVE("C_CharacterAdvancement", "ShouldConfirmUnlearnAllTalents", ShouldConfirmUnlearnAllTalents),
        COA_NATIVE("C_CharacterAdvancement", "CanUnlearnAllSpells", CanUnlearnAllSpells),
        COA_NATIVE("C_CharacterAdvancement", "CanUnlearnAllTalents", CanUnlearnAllTalents),
        COA_NATIVE("C_CharacterAdvancement", "UnlearnID", UnlearnID),
        COA_NATIVE("C_CharacterAdvancement", "CanSwitchActiveChrSpec", CanSwitchActiveChrSpec),
        COA_NATIVE("C_CharacterAdvancement", "SwitchActiveChrSpec", SwitchActiveChrSpec),
        COA_NATIVE("C_CharacterAdvancement", "ActivateLoadout", ActivateLoadout),
        COA_NATIVE("C_CharacterAdvancement", "SetLoadoutName", SetLoadoutName),
        COA_NATIVE("C_CharacterAdvancement", "SetLoadoutSortOrder", SetLoadoutSortOrder),
        COA_NATIVE("C_CharacterAdvancement", "CanSwapEntriesByID", CanSwapEntriesByID),
        COA_NATIVE("C_CharacterAdvancement", "SwapEntriesByID", SwapEntriesByID),
        COA_NATIVE("C_CharacterAdvancement", "GetEntriesAvailableForSwap", GetEntriesAvailableForSwap),
        COA_NATIVE("C_CharacterAdvancement", "GetEntriesAvailableForSwapInClass", GetEntriesAvailableForSwapInClass),
        COA_NATIVE("C_CharacterAdvancement", "GetEntriesAvailableForTrade", GetEntriesAvailableForTrade),
    };
}

void CoA::CA::detail::InitEdit() {
    CoA::LuaReg::AddModule(s_bindings, sizeof(s_bindings) / sizeof(s_bindings[0]));
}
