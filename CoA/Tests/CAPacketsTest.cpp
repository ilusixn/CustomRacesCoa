#include "CoA/Source/Modules/AttachPatches.h"
#include "CoA/Tests/CAFixture.h"

using namespace CAFixture;

namespace {
    std::vector<std::pair<uint32_t, uint32_t>> s_specChanges;
    std::vector<std::pair<uint32_t, uint32_t>> s_learned;

    void InitWithCallbacks() {
        Setup();
        CoA::CA::Init();
        s_specChanges.clear();
        s_learned.clear();
        CoA::CA::OnSpecChanged([](uint32_t a, uint32_t b) { s_specChanges.push_back({ a, b }); });
        CoA::CA::OnEntryLearned([](uint32_t a, uint32_t b) { s_learned.push_back({ a, b }); });
    }

    CoATest::Bytes Entries(const std::vector<std::pair<uint32_t, uint32_t>>& entries) {
        CoATest::Bytes b;
        b.U32(static_cast<uint32_t>(entries.size()));

        for (const auto& e : entries) {
            b.U32(e.first).U32(e.second).U32(e.second).U8(0).U32(0).U32(0);
        }

        return b;
    }
}

// SMSG 0x725: the active spec and count; a change fires the spec list (old & 0xFF, new) and the event (spec + 1).
COA_TEST(CA_ActiveSpecPacket) {
    InitWithCallbacks();
    CHECK(Feed(0x725, CoATest::Bytes().U32(1).U32(3)) == V{ "ASCENSION_CA_SPECIALIZATION_ACTIVE_ID_CHANGED:2" });
    CHECK(CoA::CA::ActiveSpecIndex() == 1);
    CHECK((s_specChanges == std::vector<std::pair<uint32_t, uint32_t>> { { 0xFF, 1 } }));
    CHECK(CoA::CA::PendingBuild() != nullptr);
    Teardown();
}

// SMSG 0x726 diffs into events in the original's order and always resets the pending build.
COA_TEST(CA_KnownEntriesDiff) {
    InitWithCallbacks();
    Feed(0x725, CoATest::Bytes().U32(0).U32(1));
    CHECK((Feed(0x726, Entries({ { 100, 1 } })) == V{ "ASCENSION_ENTRY_UPDATED:100", "ASCENSION_KNOWN_ENTRIES_UPDATED" }));
    CHECK(CoA::CA::ActiveBuild()->RankOf(100) == 1);
    CHECK((s_learned == std::vector<std::pair<uint32_t, uint32_t>> { { 100, 1 } }));
    CHECK((Feed(0x726, Entries({ { 100, 2 }, { 102, 1 } })) ==
        V{ "ASCENSION_KNOWN_ENTRY_UPDATED:100,2", "ASCENSION_ENTRY_UPDATED:102", "ASCENSION_KNOWN_ENTRIES_UPDATED" }));
    CHECK(CoA::CA::PendingBuild()->RankOf(100) == 2 && CoA::CA::PendingBuild()->Has(102));
    CHECK((Feed(0x726, Entries({})) ==
        V{ "ASCENSION_KNOWN_ENTRY_REMOVED:100", "ASCENSION_KNOWN_ENTRY_REMOVED:102", "ASCENSION_KNOWN_ENTRIES_UPDATED" }));
    CHECK(CoA::CA::PendingBuild()->entries.empty());
    Teardown();
}

// The result packets: the success flag comes from the enum, the event carries the name as sent; 0x72B is always true.
COA_TEST(CA_ResultPackets) {
    InitWithCallbacks();
    CHECK(Feed(0x728, CoATest::Bytes().Str("CA_LEARN_OK").U32(5).U32(6)) == V{ "CHARACTER_ADVANCEMENT_LEARN_RESULT:true,CA_LEARN_OK,5,6" });
    CHECK(Feed(0x728, CoATest::Bytes().Str("CA_LEARN_LOW_LEVEL").U32(5).U32(6)) ==
        V{ "CHARACTER_ADVANCEMENT_LEARN_RESULT:false,CA_LEARN_LOW_LEVEL,5,6" });
    CHECK(Feed(0x729, CoATest::Bytes().Str("CA_UNLEARN_OK").U32(7)) == V{ "CHARACTER_ADVANCEMENT_UNLEARN_RESULT:true,CA_UNLEARN_OK,7" });
    CHECK(Feed(0x72A, CoATest::Bytes().Str("X")) == V{ "CHARACTER_ADVANCEMENT_PURGE_ABILITIES_RESULT:false,X" });
    CHECK(Feed(0x72B, CoATest::Bytes().Str("CA_PURGE_TALENTS_UNKNOWN")) == V{ "CHARACTER_ADVANCEMENT_PURGE_TALENTS_RESULT:true,CA_PURGE_TALENTS_UNKNOWN" });
    CHECK(Feed(0x72C, CoATest::Bytes().Str("CA_UPDATE_ENTRIES_OK").Str("y").U32(1).U32(2)) ==
        V{ "CHARACTER_ADVANCEMENT_UPDATE_ENTRIES_RESULT:true,CA_UPDATE_ENTRIES_OK,y,1,2" });
    CHECK(Feed(0x659, CoATest::Bytes().U32(100).Str("OK")) == V{ "CHARACTER_ADVANCEMENT_LOCK_ENTRY_RESULT:OK,100" });
    CHECK(Feed(0x65B, CoATest::Bytes().U32(100).Str("NO")) == V{ "CHARACTER_ADVANCEMENT_UNLOCK_ENTRY_RESULT:NO,100" });
    Teardown();
}

// SMSG 0x926 fills the credit map; the active build picks categories 1..4 up; the glue screen empties the map.
COA_TEST(CA_Credits) {
    InitWithCallbacks();
    Feed(0x926, CoATest::Bytes().U8(2).U32(500));
    CHECK(CoA::CA::Credit(2) == 500);
    Feed(0x725, CoATest::Bytes().U32(0).U32(1));
    Feed(0x726, Entries({}));
    CHECK(CoA::CA::ActiveBuild()->credit[1] == 500);
    CoA::Lifecycle::RunGlueScreen();
    CHECK(CoA::CA::Credit(2) == 0);
    Teardown();
}

// The spell-packet hooks: the known set, the rune compare (10 while a rune spell is known, 6 when none), the events.
COA_TEST(CA_SpellHooks) {
    InitWithCallbacks();
    s_runeSpells = { 777 };
    CoATest::RecordSignals();
    CoA::CA::OnInitialSpells({ 5000, 777 });
    CoATest::StopRecordingSignals();
    CHECK(CoATest::Signals() == V{ "ASCENSION_SPELLS_UPDATED" });
    CHECK(CoA::CA::KnownSpells().count(5000) && CoA::CA::KnownSpells().count(777));
    CHECK(CoA::AttachPatches::RuneUiClass() == 10);
    CHECK(CoA::CA::LearnedSpellIsQuiet(5000));
    CHECK(!CoA::CA::LearnedSpellIsQuiet(888));
    CHECK(!CoA::CA::RemovedSpellLeavesKnownSet(5000));
    CHECK(CoA::CA::RemovedSpellLeavesKnownSet(888));
    CoA::CA::OnRemovedSpells({ 777 });
    CHECK(!CoA::CA::KnownSpells().count(777));
    CHECK(CoA::AttachPatches::RuneUiClass() == 6);
    CoA::CA::OnSupersededSpell(5000, 5001);
    CHECK(!CoA::CA::KnownSpells().count(5000) && CoA::CA::KnownSpells().count(5001));
    CHECK(CoA::CA::SupersedeIsQuiet(5000, 5001));
    CHECK(!CoA::CA::SupersedeIsQuiet(1, 2));
    Teardown();
}

// The glue screen drops the player record, the known sets and the rune compare.
COA_TEST(CA_GlueReset) {
    InitWithCallbacks();
    s_runeSpells = { 777 };
    Feed(0x725, CoATest::Bytes().U32(0).U32(1));
    Feed(0x726, Entries({ { 100, 1 } }));
    CoA::CA::OnInitialSpells({ 777 });
    CoA::Lifecycle::RunGlueScreen();
    CHECK(CoA::CA::ActiveSpecIndex() == -1);
    CHECK(CoA::CA::KnownSpells().empty());
    CHECK(CoA::AttachPatches::RuneUiClass() == 6);
    CHECK(!CoA::CA::ActiveBuild()->Has(100));
    Teardown();
}

// The once-a-second update: the auto-learn pass takes every free 0x100000 row into the pending build, then the pending
// build signal goes out.
COA_TEST(CA_UpdateAutoLearn) {
    InitWithCallbacks();
    Feed(0x725, CoATest::Bytes().U32(0).U32(1));
    Feed(0x726, Entries({}));
    CoA::CA::RequestAutoLearn();
    CoATest::RecordSignals();
    CoA::Lifecycle::RunAfterIdle();
    CoATest::StopRecordingSignals();
    CHECK(CoA::CA::PendingBuild()->RankOf(120) == 1);
    CoATest::RecordSignals();
    CoA::Lifecycle::RunAfterIdle();
    CoATest::StopRecordingSignals();
    CHECK(CoATest::Signals() == V{ "CHARACTER_ADVANCEMENT_PENDING_BUILD_UPDATED" });
    Teardown();
}

// The cast requirement: a spell whose entry lists point requirements needs them met by the active build.
COA_TEST(CA_CastRequirements) {
    InitWithCallbacks();
    CHECK(CoA::CA::CastRequirementsMet(5000));   // no requirements
    CHECK(CoA::CA::CastRequirementsMet(9999));   // no entry
    Teardown();
}
