#include "CoA/Source/Modules/GameMode.h"
#include "CoA/Tests/CAFixture.h"

using namespace CAFixture;

namespace {
    using Packets = std::vector<std::vector<uint8_t>>;

    CoATest::Bytes Entries(const std::vector<std::pair<uint32_t, uint32_t>>& entries) {
        CoATest::Bytes b;
        b.U32(static_cast<uint32_t>(entries.size()));

        for (const auto& e : entries) {
            b.U32(e.first).U32(e.second).U32(e.second).U8(0).U32(0).U32(0);
        }

        return b;
    }

    // The 0x727 payload FUN_10166a50 writes for {id, rank} entries whose +0xC is the rank (as Entries sends it) or 0.
    std::vector<uint8_t> Build727(const std::vector<std::pair<uint32_t, uint32_t>>& entries, bool learnedField) {
        CoATest::Bytes b;
        b.U32(static_cast<uint32_t>(entries.size()));

        for (const auto& e : entries) {
            b.U32(e.first).U32(e.second).U32(learnedField ? e.second : 0).U8(0).U32(0).U32(0);
        }

        return CoATest::Expected(0x727, b);
    }

    bool s_rollActive = false;

    lua_State* Start(const std::vector<std::pair<uint32_t, uint32_t>>& entries, bool enter = true) {
        Setup();
        s_rollActive = false;
        CoA::GameMode::Init();
        CoA::CA::Dependencies().wildcardRollActive = [] { return s_rollActive; };
        CoA::CA::Init();

        if (enter) {
            Feed(0x725, CoATest::Bytes().U32(0).U32(1));
            Feed(0x726, Entries(entries));
        }

        return CoATest::NewState(false);
    }

    void Stop(lua_State* L) {
        Feed(0x90B, CoATest::Bytes().U32(0));   // GameMode outlives the test
        lua_close(L);
        Teardown();
    }

    Packets Sent(lua_State* L, const char* chunk, V* results = nullptr) {
        CoATest::RecordPackets();
        V r = Run(L, chunk);
        CoATest::StopRecordingPackets();

        if (results) {
            *results = r;
        }

        return CoATest::SentPackets();
    }
}

// The plain request natives: their CMSGs carry exactly the original's fields.
COA_TEST(CA_EditRequests) {
    lua_State* L = Start({});
    CHECK((Sent(L, "C_CharacterAdvancement.LockID(100)") == Packets { CoATest::Expected(0x658, CoATest::Bytes().U32(100)) }));
    CHECK((Sent(L, "C_CharacterAdvancement.UnlockID(101)") == Packets { CoATest::Expected(0x65A, CoATest::Bytes().U32(101)) }));
    CHECK((Sent(L, "C_CharacterAdvancement.UnlearnAllSpells()") == Packets { CoATest::Expected(0x528, CoATest::Bytes()) }));
    CHECK((Sent(L, "C_CharacterAdvancement.UnlearnAllTalents()") == Packets { CoATest::Expected(0x213, CoATest::Bytes()) }));
    CHECK((Sent(L, "C_CharacterAdvancement.ActivateLoadout('u1')") == Packets { CoATest::Expected(0x778, CoATest::Bytes().Str("u1")) }));
    CHECK((Sent(L, "C_CharacterAdvancement.SetLoadoutName('u1', 'Tank')") ==
        Packets { CoATest::Expected(0x779, CoATest::Bytes().Str("u1").Str("Tank")) }));
    CHECK((Sent(L, "C_CharacterAdvancement.SetLoadoutSortOrder('u1', 3)") ==
        Packets { CoATest::Expected(0x77A, CoATest::Bytes().Str("u1").U32(3)) }));
    CHECK((Run(L, "return C_CharacterAdvancement.LockID('x')") == V{ "<error> Usage: C_CharacterAdvancement.LockID(entryId)" }));
    Stop(L);
}

// The pending-build edits: add one rank, the all-or-nothing multi-rank add, the summary, remove, cancel.
COA_TEST(CA_EditPendingBuild) {
    lua_State* L = Start({});
    Run(L, "C_CharacterAdvancement.AddByEntryID(100, 1)");
    CHECK(CoA::CA::PendingBuild()->RankOf(100) == 1);
    CHECK((Run(L, "return C_CharacterAdvancement.IsPending(), C_CharacterAdvancement.IsPendingEntryID(100)") == V{ "true", "true" }));
    CHECK((Run(L, "local s = C_CharacterAdvancement.GetPendingSummary() return s.Entry, s.CARankDiff, s.Added, s.Removed") ==
        V{ "100", "1", "true", "false" }));
    Run(L, "C_CharacterAdvancement.AddByEntryID(100, 2)");   // 1 + 2 > max rank 2: nothing
    CHECK(CoA::CA::PendingBuild()->RankOf(100) == 1);
    Run(L, "C_CharacterAdvancement.AddByEntryID(100, 1)");
    CHECK(CoA::CA::PendingBuild()->RankOf(100) == 2);
    Run(L, "C_CharacterAdvancement.RemoveByEntryID(100)");
    CHECK(!CoA::CA::PendingBuild()->Has(100));
    CHECK((Run(L, "return C_CharacterAdvancement.GetPendingSummary()") == V{ "nil" }));
    Run(L, "C_CharacterAdvancement.AddByEntryID(100, 1)");
    Run(L, "C_CharacterAdvancement.CancelPendingBuild()");
    CHECK(!CoA::CA::PendingBuild()->Has(100));
    CHECK((Run(L, "return C_CharacterAdvancement.AddByEntryID(999, 1)") == V{ "<error> AddByEntryID: invalid entryId" }));
    Stop(L);
}

// Clear keeps every group-1 entry (the original's test is +0x74 != 1); a choice-group entry goes.
COA_TEST(CA_EditClearKeepsGroupOne) {
    lua_State* L = Start({});
    CoA::CA::PendingBuild()->AddEntry(100, 1);
    CoA::CA::PendingBuild()->AddEntry(135, 1);
    CHECK((Run(L, "return C_CharacterAdvancement.CanClearPendingBuild(false)") == V{ "true", "nil", "nil", "nil", "nil" }));
    Run(L, "C_CharacterAdvancement.ClearPendingBuild(false)");
    CHECK(CoA::CA::PendingBuild()->Has(100));
    CHECK(!CoA::CA::PendingBuild()->Has(135));
    Stop(L);
}

// The Can* verdicts carry the CA_LEARN_* name of the first failing rule.
COA_TEST(CA_EditCanAdd) {
    lua_State* L = Start({});
    CHECK((Run(L, "return C_CharacterAdvancement.CanAddByEntryID(100, 1)") == V{ "true", "nil" }));
    CHECK((Run(L, "return C_CharacterAdvancement.CanAddByEntryID(103, 1)") == V{ "false", "CA_LEARN_LOW_LEVEL" }));
    CHECK((Run(L, "return C_CharacterAdvancement.CanAddByEntryID(100, 3)") == V{ "false", "CA_LEARN_ALREADY_KNOWN" }));
    // Only the investment slots: 103's level does not matter, 139's invested AE does.
    CHECK((Run(L, "return C_CharacterAdvancement.MeetsInvestmentForAddByEntryID(103, 1)") == V{ "true", "nil" }));
    CHECK((Run(L, "return C_CharacterAdvancement.MeetsInvestmentForAddByEntryID(139, 1)") == V{ "false", "CA_LEARN_NOT_ENOUGH_INVESTED_AE" }));
    CHECK((Run(L, "return C_CharacterAdvancement.CanLearnID(103)") == V{ "false", "CA_LEARN_LOW_LEVEL", "103" }));
    CHECK((Run(L, "return C_CharacterAdvancement.CanLearnID({100, 102})") == V{ "true", "CA_LEARN_OK", "0" }));
    Stop(L);
}

// LearnID sends the whole build as 0x727 after the apply check; ApplyPendingBuild sends the pending build.
COA_TEST(CA_EditLearnAndApply) {
    lua_State* L = Start({});
    V r;
    CHECK((Sent(L, "return C_CharacterAdvancement.LearnID(100)", &r) == Packets { Build727({ { 100, 1 } }, false) }));
    CHECK((r == V{ "true", "nil", "nil", "nil", "nil" }));
    CoA::CA::PendingBuild()->AddEntry(100, 2);
    CHECK((Sent(L, "C_CharacterAdvancement.ApplyPendingBuild()") == Packets { Build727({ { 100, 2 } }, false) }));
    CHECK((Run(L, "local ok = C_CharacterAdvancement.CanApplyPendingBuild() return ok") == V{ "true" }));
    Stop(L);
}

// A group-1 entry is exclusive: learning one drops every other group-1 entry from what is sent (0x1017D35D).
COA_TEST(CA_EditLearnGroupOneExclusive) {
    lua_State* L = Start({ { 120, 1 } });
    CHECK((Sent(L, "C_CharacterAdvancement.LearnID(100)") == Packets { Build727({ { 100, 1 } }, false) }));
    Stop(L);
}

// UnlearnID outside wildcard: the build minus the ids as 0x727. In wildcard: one 0x61D per id, none while a roll runs.
COA_TEST(CA_EditUnlearn) {
    lua_State* L = Start({ { 100, 1 }, { 102, 1 } });
    V r;
    CHECK((Sent(L, "return C_CharacterAdvancement.UnlearnID(102)", &r) == Packets { Build727({ { 100, 1 } }, true) }));
    CHECK((r == V{ "true" }));
    Feed(0x90B, CoATest::Bytes().U32(0x40));
    CHECK((Sent(L, "return C_CharacterAdvancement.UnlearnID({100, 102})", &r) ==
        Packets { CoATest::Expected(0x61D, CoATest::Bytes().U32(100)), CoATest::Expected(0x61D, CoATest::Bytes().U32(102)) }));
    s_rollActive = true;
    CHECK((Sent(L, "return C_CharacterAdvancement.UnlearnID(100)", &r).empty()));
    CHECK((r == V{ "false" }));
    Stop(L);
}

// A confirmation for learning an entry whose mastery the build lacks; none for one without masteries.
COA_TEST(CA_EditConfirmations) {
    lua_State* L = Start({});
    CHECK((Run(L, "local ok, t = C_CharacterAdvancement.ShouldConfirmLearnID(111) return ok, t[1].Error, t[1].Arg1") ==
        V{ "true", "CA_CONFIRM_LEARN_INCLUDES_MASTERY", "110" }));
    CHECK((Run(L, "return C_CharacterAdvancement.ShouldConfirmLearnID(100)") == V{ "false" }));
    CHECK((Run(L, "return C_CharacterAdvancement.CanUnlearnAllSpells()") == V{ "false", "CA_PURGE_ABILITIES_NO_KNOWN_ABILITIES" }));
    Stop(L);
}

// Swaps: the data must hold the existing entry above the rank it keeps; a valid swap trades one entry for another.
COA_TEST(CA_EditSwap) {
    lua_State* L = Start({});
    CoA::CA::PendingBuild()->AddEntry(100, 1);
    CHECK((Run(L, "return C_CharacterAdvancement.CanSwapEntriesByID({ ExistingEntry = { Entry = 100, NewCARank = 1 }, UpdatedEntries = {} })") ==
        V{ "<error> CanSwapEntriesByID: invalid swap data" }));
    Run(L, "C_CharacterAdvancement.SwapEntriesByID({ ExistingEntry = { Entry = 100, NewCARank = 0 }, "
           "UpdatedEntries = { { Entry = 132, NewCARank = 1 } } })");
    CHECK(!CoA::CA::PendingBuild()->Has(100));
    CHECK(CoA::CA::PendingBuild()->RankOf(132) == 1);
    CHECK((Run(L, "return C_CharacterAdvancement.CanSwitchActiveChrSpec(1)") == V{ "true", "nil" }));
    Stop(L);
}

// Without a pending build the edits raise or answer NO_PENDING_BUILD.
COA_TEST(CA_EditNoPendingBuild) {
    lua_State* L = Start({}, false);
    CHECK((Run(L, "return C_CharacterAdvancement.AddByEntryID(100, 1)") == V{ "<error> AddByEntryID: pending build is not available" }));
    CHECK((Run(L, "return C_CharacterAdvancement.CanSwitchActiveChrSpec(1)") == V{ "false", "NO_PENDING_BUILD" }));
    CHECK((Run(L, "return C_CharacterAdvancement.SwapEntriesByID({})") == V{ "<error> SwapEntriesByID: pending build is not available" }));
    Stop(L);
}
