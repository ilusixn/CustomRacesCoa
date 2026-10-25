#include "CoA/Tests/CAFixture.h"
#include <memory>

using namespace CAFixture;
using CoA::CA::Build;

// The index: RowBySpell prefers a class-allowed row; the visible list holds only rows the player's class may use.
COA_TEST(CA_IndexAndRowBySpell) {
    Setup();
    CoA::CA::Row r = CoA::CA::RowBySpell(5000);
    CHECK(r && CoA::CA::RowU32(r, 0) == 100);
    CHECK(CoA::CA::IsCASpell(5001));
    CHECK(!CoA::CA::IsCASpell(9999));
    bool has100 = false;
    bool has101 = false;

    for (CoA::CA::Row x : CoA::CA::IndexedRows()) {
        has100 = has100 || CoA::CA::RowU32(x, 0) == 100;
        has101 = has101 || CoA::CA::RowU32(x, 0) == 101;
    }

    CHECK(has100);
    CHECK(!has101);
    CHECK(CoA::CA::RowType(CoA::CA::FindRow(102)) == 2);
    CHECK(CoA::CA::RowMaxRank(CoA::CA::FindRow(100)) == 2);
    Teardown();
}

// Budgets come from Essence.dbc at the build's level; the sums charge a wildcard choice group once.
COA_TEST(CA_BudgetsAndSums) {
    Setup();
    std::unique_ptr<Build> b(NewBuild(10));
    CHECK(b->AEBudget(10) == 20);
    CHECK(b->TEBudget(10) == 5);
    b->AddEntry(100, 2);
    b->AddEntry(102, 1);
    CHECK(b->GlobalAE(0) == 4);
    CHECK(b->GlobalTE(0) == 1);
    CHECK(b->RemainingAE() == 16);
    CHECK(b->ClassAE(3, 0) == 4);
    CHECK(b->TabTE(3, 7, 0) == 1);

    std::unique_ptr<Build> w(NewBuild(10));
    w->wildcard = 1;
    w->AddEntry(104, 1);
    w->AddEntry(105, 1);
    CHECK(w->GlobalAE(0) == 3);
    CHECK(w->AECost(CoA::CA::FindRow(105), 1) == 0);   // the group is held already
    Teardown();
}

// ValidateLearn: the first failing slot in slot order; include / exclude narrow the slots.
COA_TEST(CA_ValidateLearnSlotOrder) {
    Setup();
    std::unique_ptr<Build> b(NewBuild(10));
    CHECK(b->ValidateLearn(0, 100, {}, {}) == 0);
    CHECK(b->ValidateLearn(0, 103, {}, {}) == 5);          // LOW_LEVEL before MISSING_AE (41)
    CHECK(b->ValidateLearn(0, 103, { 41 }, {}) == 41);
    CHECK(b->ValidateLearn(0, 103, {}, { 5 }) == 41);
    CHECK(b->ValidateLearn(0, 102, {}, {}) == 14);         // MISSING_REQUIRED_ID
    CHECK(b->ValidateLearn(0, 101, {}, {}) == 11);         // WRONG_CLASS
    CHECK(b->ValidateLearn(0, 999, {}, {}) == 3);          // BAD_ABILITY
    b->AddEntry(100, 2);
    CHECK(b->ValidateLearn(0, 100, {}, {}) == 6);          // ALREADY_KNOWN at the max rank
    CHECK(b->ValidateLearn(0, 102, {}, {}) == 0);
    Teardown();
}

// The learn cache returns a remembered failure only while it still fails.
COA_TEST(CA_LearnCache) {
    Setup();
    std::unique_ptr<Build> b(NewBuild(10));
    CHECK(b->ValidateLearn(0, 103, {}, {}) == 5);
    CHECK(b->learnCache.size() == 1);
    b->level = 20;
    CHECK(b->ValidateLearn(0, 103, {}, {}) == 0);          // level 20: budget 200 covers 100
    CHECK(b->learnCache.empty());
    Teardown();
}

// A non-full copy has no rules: everything validates.
COA_TEST(CA_NonFullCopyValidatesEverything) {
    Setup();
    std::unique_ptr<Build> b(NewBuild(10));
    Build copy(*b, false);
    CHECK(copy.ValidateLearn(0, 103, {}, {}) == 0);
    CHECK(copy.ValidateLearn(0, 999, {}, {}) == 0);
    Build full(*b, true);
    CHECK(full.ValidateLearn(0, 103, {}, {}) == 5);
    Teardown();
}

// Reorder places a requirement before what requires it; a step that never validates fails the pass.
COA_TEST(CA_Reorder) {
    Setup();
    std::unique_ptr<Build> b(NewBuild(10));
    b->entries.clear();
    CoA::CA::Entry talent;
    talent.id = 102;
    talent.rank = 1;
    CoA::CA::Entry ability;
    ability.id = 100;
    ability.rank = 2;
    b->entries = { talent, ability };
    b->UpdatePointers(false);
    CHECK(b->Reorder(false));
    CHECK(b->entries.size() == 2 && b->entries[0].id == 100 && b->entries[0].rank == 2 && b->entries[1].id == 102);
    uint32_t id;
    uint32_t rank;
    uint32_t result;
    CHECK(b->ValidateAll(true, {}, {}, false, id, rank, result));
    CoA::CA::Entry high;
    high.id = 103;
    high.rank = 1;
    b->entries.push_back(high);
    b->UpdatePointers(false);
    CHECK(!b->Optimize(false, result, id, rank));
    CHECK(id == 103 && result == 5);
    Teardown();
}

// ValidateApply: an unknown entry is 3 with the entry; no change is 4.
COA_TEST(CA_ValidateApply) {
    Setup();
    std::unique_ptr<Build> b(NewBuild(10));
    b->AddEntry(100, 1);
    uint32_t learn;
    uint32_t id;
    uint32_t rank;
    CoA::CA::ApplyCosts costs;
    CHECK(CoA::CA::ValidateApply(0, *b, b->entries, learn, id, rank, costs) == 4);
    std::vector<CoA::CA::Entry> bad = b->entries;
    CoA::CA::Entry e;
    e.id = 999;
    e.rank = 2;
    bad.push_back(e);
    CHECK(CoA::CA::ValidateApply(0, *b, bad, learn, id, rank, costs) == 3);
    CHECK(id == 999 && rank == 2);
    std::vector<CoA::CA::Entry> more = b->entries;
    e.id = 102;
    e.rank = 1;
    more.push_back(e);
    CHECK(CoA::CA::ValidateApply(0, *b, more, learn, id, rank, costs) == 0);
    Teardown();
}

// The unlearn and purge prices.
COA_TEST(CA_Costs) {
    Setup();
    std::unique_ptr<Build> b(NewBuild(30));
    const CoA::CA::TokenCost u = CoA::CA::UnlearnCostOf(*b, CoA::CA::FindRow(100));
    CHECK(u.money == 521 * 30);
    CHECK(u.marks == 150);
    const CoA::CA::TokenCost p = CoA::CA::PurgeCost(*b, false);
    CHECK(p.money == 149769);
    CHECK(p.marks == 750);
    CHECK(p.item == 0x5D86A && p.count == 1);
    std::unique_ptr<Build> low(NewBuild(10));
    CHECK(CoA::CA::UnlearnCostOf(*low, CoA::CA::FindRow(100)).money == 0);
    Teardown();
}

// The mastery removal drops the mastery with every entry listing it.
COA_TEST(CA_RemoveWithMastery) {
    Setup();
    std::unique_ptr<Build> b(NewBuild(10));
    b->AddEntry(110, 1);
    b->AddEntry(111, 1);
    b->AddEntry(100, 1);
    CHECK(b->MasteryInvolved(111));
    CHECK(b->MasteryTarget(111) == 110);
    b->RemoveWithMastery(111);
    CHECK(!b->Has(110));
    CHECK(!b->Has(111));
    CHECK(b->Has(100));
    Teardown();
}

