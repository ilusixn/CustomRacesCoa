#include <algorithm>
#include <map>
#include <memory>
#include "CoA/Source/Modules/CAInternal.h"
#include "CoA/Tests/CAFixture.h"

using namespace CAFixture;
using CoA::CA::Build;

namespace {
    std::vector<std::string> s_calls;

    CoATest::Bytes Entries(const std::vector<std::pair<uint32_t, uint32_t>>& entries) {
        CoATest::Bytes b;
        b.U32(static_cast<uint32_t>(entries.size()));

        for (const auto& e : entries) {
            b.U32(e.first).U32(e.second).U32(e.second).U8(0).U32(0).U32(0);
        }

        return b;
    }

    void Enter() {
        Setup();
        CoA::CA::Init();
        Feed(0x725, CoATest::Bytes().U32(0).U32(1));
        Feed(0x726, Entries({}));
    }

    bool Has(const V& signals, const std::string& s) {
        return std::find(signals.begin(), signals.end(), s) != signals.end();
    }

    // The 0x64A wire for a row: its fields in order, strings as C strings, the byte fields as single bytes.
    CoATest::Bytes PatchWire(const std::vector<uint8_t>& row, const std::map<uint32_t, std::string>& strings) {
        CoATest::Bytes b;
        auto u32s = [&](uint32_t from, uint32_t to) {
            for (uint32_t off = from; off <= to; off += 4) {
                uint32_t v;
                memcpy(&v, &row[off], 4);
                b.U32(v);
            }
        };
        auto str = [&](uint32_t off) {
            auto it = strings.find(off);
            b.Str(it != strings.end() ? it->second.c_str() : "");
        };
        u32s(0x00, 0x00);
        str(0x04);
        u32s(0x08, 0x3C);
        str(0x40);
        u32s(0x44, 0x4C);
        str(0x50);
        u32s(0x54, 0x5C);
        str(0x60);
        u32s(0x64, 0xB8);

        for (uint32_t off = 0xBC; off <= 0xC8; off += 4) {
            str(off);
        }

        b.U8(row[0xCC]).U8(row[0xCD]);
        u32s(0xD0, 0xDC);
        b.U8(row[0xE0]);
        u32s(0xE4, 0x11C);
        str(0x120);
        u32s(0x124, 0x124);

        for (uint32_t off = 0x128; off <= 0x12C; ++off) {
            b.U8(row[off]);
        }

        u32s(0x130, 0x1A0);

        for (uint32_t off = 0x1A4; off <= 0x1AC; off += 4) {
            str(off);
        }

        u32s(0x1B0, 0x1F8);
        return b;
    }
}

// One learn-rule slot per fixture row: DISPLAY_ENTRY, DISABLED, starting node, connected entries, group, invested AE,
// invested points.
COA_TEST(CA_ValidateLearnRuleSlots) {
    Setup();
    std::unique_ptr<Build> b(NewBuild(10));
    CHECK(b->ValidateLearn(0, 130, {}, {}) == 4);
    CHECK(b->ValidateLearn(0, 131, {}, {}) == 10);
    CHECK(b->ValidateLearn(0, 132, {}, {}) == 0);
    b->AddEntry(132, 1);
    CHECK(b->ValidateLearn(0, 133, {}, {}) == 12);
    CHECK(b->ValidateLearn(0, 134, {}, {}) == 13);
    b->AddEntry(100, 1);
    CHECK(b->ValidateLearn(0, 134, {}, {}) == 0);
    CHECK(b->ValidateLearn(0, 136, {}, {}) == 0);
    b->AddEntry(135, 1);
    CHECK(b->ValidateLearn(0, 136, {}, {}) == 37);
    CHECK(b->ValidateLearn(0, 139, {}, {}) == 38);
    CHECK(b->ValidateLearn(0, 140, {}, {}) == 40);
    Teardown();
}

// The cache is read: a failure remembered under an include filter comes back from an unfiltered call while it still
// fails, even though the unfiltered slot order would stop earlier.
COA_TEST(CA_LearnCacheIsRead) {
    Setup();
    std::unique_ptr<Build> b(NewBuild(10));
    CHECK(b->ValidateLearn(0, 103, { 41 }, {}) == 41);
    CHECK(b->ValidateLearn(0, 103, {}, {}) == 41);
    Teardown();
}

// A truncated 0x726 leaves the active build alone but still resets the pending build, as the original's always does.
COA_TEST(CA_KnownEntriesTruncated) {
    Enter();
    Feed(0x726, Entries({ { 100, 1 } }));
    CoA::CA::PendingBuild()->AddEntry(102, 1);
    CHECK((Feed(0x726, CoATest::Bytes().U32(3).U32(100)) == V{}));
    CHECK(CoA::CA::ActiveBuild()->RankOf(100) == 1);
    CHECK(!CoA::CA::PendingBuild()->Has(102));
    CHECK(CoA::CA::PendingBuild()->Has(100));
    Teardown();
}

// The original checks no read on 0x725 / 0x728: a missing field reads 0 and the event still goes out. A truncated 0x64A
// is dropped whole.
COA_TEST(CA_ShortPackets) {
    Enter();
    CHECK((Feed(0x725, CoATest::Bytes().U32(1)) == V{ "ASCENSION_CA_SPECIALIZATION_ACTIVE_ID_CHANGED:2" }));
    CHECK(CoA::CA::ActiveSpecIndex() == 1);
    CHECK((Feed(0x728, CoATest::Bytes().Str("CA_LEARN_OK").U32(5)) == V{ "CHARACTER_ADVANCEMENT_LEARN_RESULT:true,CA_LEARN_OK,5,0" }));
    CHECK((Feed(0x64A, CoATest::Bytes().U32(141).Str("Ability")) == V{}));
    Teardown();
}

// SMSG 0x64A on an existing row rewrites it in place and re-indexes it.
COA_TEST(CA_PatchEntryInPlace) {
    Enter();
    std::unique_ptr<Build> b(NewBuild(10));
    CHECK(b->ValidateLearn(0, 141, {}, {}) == 0);
    CaRow patched(141);
    patched.Spell(1, 5711).U32(0x124, 0x20000);
    const std::map<uint32_t, std::string> strings = { { 0x04, "Ability" }, { 0x40, "Normal" }, { 0x50, "Normal" },
        { 0x60, "Normal" }, { 0x120, "EXPANSION_CLASSIC" } };
    CHECK((Feed(0x64A, PatchWire(patched.bytes, strings)) ==
        V{ "ASCENSION_ENTRY_UPDATED:141", "CHARACTER_ADVANCEMENT_ENTRY_PATCHED:false,Warrior,Protection,141" }));
    CHECK(CoA::CA::RowBySpell(5711) == CoA::CA::FindRow(141));
    std::unique_ptr<Build> after(NewBuild(10));
    CHECK(after->ValidateLearn(0, 141, {}, {}) == 4);
    Teardown();
}

// The callbacks fire learned, then rank changes, then unlearned.
COA_TEST(CA_CallbackOrder) {
    Enter();
    Feed(0x726, Entries({ { 100, 1 }, { 102, 1 } }));
    s_calls.clear();
    CoA::CA::OnEntryLearned([](uint32_t a, uint32_t b) { s_calls.push_back("L" + std::to_string(a) + "," + std::to_string(b)); });
    CoA::CA::OnEntryRankChanged([](uint32_t a, uint32_t b) { s_calls.push_back("R" + std::to_string(a) + "," + std::to_string(b)); });
    CoA::CA::OnEntryUnlearned([](uint32_t a, uint32_t b) { s_calls.push_back("U" + std::to_string(a) + "," + std::to_string(b)); });
    Feed(0x726, Entries({ { 120, 1 }, { 100, 2 } }));
    CHECK((s_calls == std::vector<std::string> { "L120,1", "R100,2", "U102,1" }));
    Teardown();
}

// The once-a-second update syncs the unit's level into both builds and signals the level change.
COA_TEST(CA_UpdateLevelSync) {
    Enter();
    s_level = 20;
    CoA::CA::detail::M().lastUpdate = 0;
    CoATest::RecordSignals();
    CoA::Lifecycle::RunAfterIdle();
    CoATest::StopRecordingSignals();
    CHECK(Has(CoATest::Signals(), "CHARACTER_ADVANCEMENT_BUILD_LEVEL_UPDATED"));
    CHECK(CoA::CA::ActiveBuild()->level == 20);
    CHECK(CoA::CA::PendingBuild()->level == 20);
    CoA::CA::detail::M().lastUpdate = 0;
    CoATest::RecordSignals();
    CoA::Lifecycle::RunAfterIdle();
    CoATest::StopRecordingSignals();
    CHECK(!Has(CoATest::Signals(), "CHARACTER_ADVANCEMENT_BUILD_LEVEL_UPDATED"));
    Teardown();
}

// A cast requirement fails until the active build holds the points.
COA_TEST(CA_CastRequirementPoints) {
    Enter();
    CHECK(!CoA::CA::CastRequirementsMet(5600));
    Feed(0x726, Entries({ { 122, 1 } }));
    CHECK(CoA::CA::CastRequirementsMet(5600));
    Teardown();
}

// The glue screen also drops the inspect records.
COA_TEST(CA_GlueResetInspect) {
    Enter();
    CoA::CA::detail::M().inspect[42] = new CoA::CA::detail::Player(42);
    CoA::Lifecycle::RunGlueScreen();
    CHECK(CoA::CA::detail::M().inspect.empty());
    Teardown();
}
