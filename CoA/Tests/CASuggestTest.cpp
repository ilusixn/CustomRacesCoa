#include "CoA/Tests/CAFixture.h"
#include <algorithm>
#include <tuple>

using namespace CAFixture;

namespace {
    using Weights = std::vector<std::pair<uint32_t, uint32_t>>;

    // {id, rank, locked} entries in the 0x726 / 0x6E2 list layout.
    CoATest::Bytes& Put(CoATest::Bytes& b, const std::vector<std::tuple<uint32_t, uint32_t, uint8_t>>& entries) {
        b.U32(static_cast<uint32_t>(entries.size()));

        for (const auto& e : entries) {
            b.U32(std::get<0>(e)).U32(std::get<1>(e)).U32(std::get<1>(e)).U8(std::get<2>(e)).U32(0).U32(0);
        }

        return b;
    }

    lua_State* Start(const std::vector<std::tuple<uint32_t, uint32_t, uint8_t>>& entries) {
        Setup();
        CoA::CA::Dependencies().unitTokenGuid = [](const char* token, uint64_t& guid) {
            guid = strcmp(token, "player") == 0 ? kPlayer : strcmp(token, "target") == 0 ? 0x99 : 0;
            return guid != 0;
        };
        CoA::CA::Init();
        Feed(0x725, CoATest::Bytes().U32(0).U32(1));
        CoATest::Bytes b;
        Feed(0x726, Put(b, entries));
        return CoATest::NewState(false);
    }

    void Stop(lua_State* L) {
        lua_close(L);
        Teardown();
    }

    V Tick() {
        CoATest::RecordSignals();
        CoA::Lifecycle::RunAfterIdle();
        CoATest::StopRecordingSignals();
        return CoATest::Signals();
    }
}

// The tick rebuilds the maps from the entries the active build has and weighs the suggestions heaviest first.
COA_TEST(CA_SuggestWeights) {
    lua_State* L = Start({ { 100, 1, 0 }, { 120, 1, 0 } });
    const V signals = Tick();
    CHECK(std::find(signals.begin(), signals.end(), "CHARACTER_ADVANCEMENT_SUGGESTIONS_UPDATED") != signals.end());
    CHECK((CoA::CA::detail::Sugg().entries == Weights { { 103, 30 }, { 102, 20 } }));
    CHECK((CoA::CA::detail::Sugg().stats == Weights { { 3, 7 }, { 5, 2 } }));
    CHECK((Run(L, "local s, t = C_CharacterAdvancement.GetSuggestedStats() return s, #t, t[2], t[3], t[5]") ==
        V{ "STAT_INTELLECT", "5", "MAX_STATS", "STAT_STRENGTH", "STAT_SPIRIT" }));
    CHECK(Tick().empty());   // nothing dirty: no recompute
    Stop(L);
}

// A locked source entry doubles its suggestion's weight.
COA_TEST(CA_SuggestLockedDoubles) {
    lua_State* L = Start({ { 100, 1, 0 }, { 120, 1, 1 } });
    Tick();
    CHECK((CoA::CA::detail::Sugg().entries == Weights { { 102, 40 }, { 103, 30 } }));
    Stop(L);
}

// Context overrides replace the build's entries as the sources; the natives mark the list dirty.
COA_TEST(CA_SuggestOverrides) {
    lua_State* L = Start({ { 100, 1, 0 }, { 120, 1, 0 } });
    Tick();
    Run(L, "C_CharacterAdvancement.AddSuggestionContextOverride(120)");
    CHECK((Run(L, "local C = C_CharacterAdvancement return C.HasAnySuggestionContextOverrides(), C.IsSuggestionContextOverride(120), "
                  "C.IsSuggestionContextOverride(100)") == V{ "true", "true", "false" }));
    Tick();
    CHECK((CoA::CA::detail::Sugg().entries == Weights { { 102, 20 } }));
    Run(L, "C_CharacterAdvancement.ClearSuggestionContextOverrides()");
    Tick();
    CHECK(CoA::CA::detail::Sugg().entries.size() == 2);
    Stop(L);
}

// Before anything is suggested GetSuggestedStats is nil, nil.
COA_TEST(CA_SuggestStatsEmpty) {
    lua_State* L = Start({});
    Tick();
    CHECK((Run(L, "return C_CharacterAdvancement.GetSuggestedStats()") == V{ "nil", "nil" }));
    Stop(L);
}

// InspectUnit sends the unit's GUID; SMSG 0x6E2 fills the unit's record, which the inspect natives read.
COA_TEST(CA_Inspect) {
    lua_State* L = Start({});
    CoATest::RecordPackets();
    Run(L, "C_CharacterAdvancement.InspectUnit('target')");
    CoATest::StopRecordingPackets();
    CHECK((CoATest::SentPackets() == std::vector<std::vector<uint8_t>> { CoATest::Expected(0x6E1, CoATest::Bytes().U64(0x99)) }));
    CHECK((Run(L, "return C_CharacterAdvancement.GetInspectInfo('target')") == V{ "nil", "nil" }));
    CoATest::Bytes b;
    b.Str("CA_INSPECT_OK").U64(0x99).U32(1).U32(2);
    Put(b, { { 102, 1, 0 } });
    Put(b, { { 122, 1, 0 }, { 102, 1, 0 }, { 100, 2, 1 } });
    CHECK((Feed(0x6E2, b) == V{ "INSPECT_CHARACTER_ADVANCEMENT_RESULT:CA_INSPECT_OK" }));
    CHECK((Run(L, "local s, t = C_CharacterAdvancement.GetInspectInfo('target') return s, #t, t[1], t[2]") == V{ "2", "2", "1", "2" }));
    CHECK((Run(L, "local t = C_CharacterAdvancement.GetInspectedBuild('target', 2) return #t, t[1].EntryId, t[1].Rank, t[1].Locked, t[2].EntryId, t[3].EntryId") ==
        V{ "3", "100", "2", "1", "122", "102" }));
    CHECK((Run(L, "return C_CharacterAdvancement.GetInspectedBuild('target', 5)") == V{ "nil" }));
    CHECK((Run(L, "return C_CharacterAdvancement.GetInspectedBuild('target', 0)") == V{ "<error> GetInspectedBuild: specialization must be >= 1" }));
    CHECK((Feed(0x6E2, CoATest::Bytes().Str("CA_INSPECT_TARGET_NOT_FOUND")) == V{ "INSPECT_CHARACTER_ADVANCEMENT_RESULT:CA_INSPECT_TARGET_NOT_FOUND" }));
    CoA::Lifecycle::RunGlueScreen();
    CHECK((Run(L, "return C_CharacterAdvancement.GetInspectInfo('target')") == V{ "nil", "nil" }));
    Stop(L);
}

// SMSG 0x6CE patches a SpellSpellSuggestions row; the next tick weighs it. (Row 1 is rewritten in place: an insert would
// hit Dbc::Upsert's growth bug.)
COA_TEST(CA_SuggestPatchRow) {
    lua_State* L = Start({ { 100, 1, 0 } });
    Tick();
    CHECK((CoA::CA::detail::Sugg().entries == Weights { { 103, 30 } }));
    Feed(0x6CE, CoATest::Bytes().U32(1).U32(5000).U32(5200).U32(50));
    Tick();
    CHECK((CoA::CA::detail::Sugg().entries == Weights { { 103, 50 } }));
    Feed(0x6CE, CoATest::Bytes().U32(1).U32(5000).U32(5200).U32(30));
    Stop(L);
}
