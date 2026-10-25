#include "CoA/Tests/CAFixture.h"
#include <algorithm>

using namespace CAFixture;

namespace {
    std::vector<uint32_t> s_pickedUp;
    bool s_inBook = true;

    CoATest::Bytes Entries(const std::vector<std::pair<uint32_t, uint32_t>>& entries) {
        CoATest::Bytes b;
        b.U32(static_cast<uint32_t>(entries.size()));

        for (const auto& e : entries) {
            b.U32(e.first).U32(e.second).U32(e.second).U8(0).U32(0).U32(0);
        }

        return b;
    }

    // The CA module with a Lua world state; `entries` arrive as the active spec's known entries.
    lua_State* Start(const std::vector<std::pair<uint32_t, uint32_t>>& entries, bool enter = true) {
        Setup();
        s_pickedUp.clear();
        s_inBook = true;
        CoA::CA::Deps& d = CoA::CA::Dependencies();
        d.unitTokenGuid = [](const char* token, uint64_t& guid) {
            guid = strcmp(token, "player") == 0 ? kPlayer : 0;
            return guid != 0;
        };
        d.pickupFromSpellbook = [](uint32_t spell) {
            if (s_inBook) {
                s_pickedUp.push_back(spell);
            }

            return s_inBook;
        };
        CoA::CA::Init();

        if (enter) {
            Feed(0x725, CoATest::Bytes().U32(0).U32(1));
            Feed(0x726, Entries(entries));
        }

        return CoATest::NewState(false);
    }

    void Stop(lua_State* L) {
        lua_close(L);
        Teardown();
    }
}

// The investment and budget natives read the active build; a bad argument raises the original's usage text.
COA_TEST(CA_NativeInvestment) {
    lua_State* L = Start({ { 100, 1 } });
    CHECK((Run(L, "local C = C_CharacterAdvancement return C.GetGlobalAEInvestment(), C.GetRemainingAE(), C.GetExpectedAE(20)") ==
        V{ "2", "18", "200" }));
    CHECK((Run(L, "return C_CharacterAdvancement.GetClassAEInvestment('Warrior', 0), C_CharacterAdvancement.GetTabAEInvestment('Warrior', 'Protection', 0)") ==
        V{ "2", "2" }));
    CHECK((Run(L, "return C_CharacterAdvancement.GetClassAEInvestment(1)") ==
        V{ "<error> Usage: C_CharacterAdvancement.GetClassAEInvestment(classType, minInvestment)" }));
    CHECK((Run(L, "return C_CharacterAdvancement.GetActiveSpecID()") == V{ "1" }));
    Stop(L);
}

// Before the first 0x725 there is no pending build: the totals raise, the per-class sums return nothing.
COA_TEST(CA_NativeNoPendingBuild) {
    lua_State* L = Start({}, false);
    CHECK((Run(L, "return C_CharacterAdvancement.IsPendingBuildAvailable(), C_CharacterAdvancement.IsActiveBuildAvailable()") ==
        V{ "false", "true" }));
    CHECK((Run(L, "return C_CharacterAdvancement.GetPendingGlobalAEInvestment()") ==
        V{ "<error> GetPendingGlobalAEInvestment: pending build is not available" }));
    CHECK((Run(L, "return C_CharacterAdvancement.GetPendingClassAEInvestment('Warrior', 0)") == V{}));
    CHECK((Run(L, "return C_CharacterAdvancement.GetActiveChrSpec(), C_CharacterAdvancement.GetActiveSpecID()") == V{ "nil", "-1" }));
    Stop(L);
}

// The entry table and the entry / spell type queries.
COA_TEST(CA_NativeEntryQueries) {
    lua_State* L = Start({});
    CHECK((Run(L, "local e = C_CharacterAdvancement.GetEntryBySpellID(5000) return e.ID, e.Type, e.Class, e.Tab, #e.Spells, e.AECost, e.Quality") ==
        V{ "100", "Ability", "Warrior", "Protection", "2", "2", "Normal" }));
    CHECK((Run(L, "return C_CharacterAdvancement.GetEntryBySpellID(1), C_CharacterAdvancement.GetInternalID(5100)") == V{ "nil", "102" }));
    CHECK((Run(L, "local C = C_CharacterAdvancement return C.IsTalentSpellID(5100), C.IsAbilitySpellID(5100), C.IsAbilityID(999), C.IsMastery(5400)") ==
        V{ "true", "false", "nil", "true" }));
    CHECK((Run(L, "local r = C_CharacterAdvancement.GetEntryByInternalID(102).RequiredIDs return #r, r[1]") == V{ "1", "100" }));
    CHECK((Run(L, "return C_CharacterAdvancement.GetEntryBySpellID('x')") ==
        V{ "<error> Usage: C_CharacterAdvancement.GetEntryBySpellID(spellId)" }));
    CHECK((Run(L, "local q = C_CharacterAdvancement.GetEntryBySpellID(5600).SpellCastReq[1] return q.ClassType, q.TabType, q.Points, q.IsMet") ==
        V{ "3", "7", "5", "false" }));
    Stop(L);
}

// Lists by class come back in the original's order: required level, then the investment requirements, then id.
COA_TEST(CA_NativeListsByClass) {
    lua_State* L = Start({});
    CHECK((Run(L, "local t = C_CharacterAdvancement.GetSpellsByClass('Warrior', 'Protection', false) local n = #t "
                  "return t[n - 2].ID, t[n - 1].ID, t[n].ID, t[1].ID") == V{ "139", "140", "103", "100" }));
    CHECK((Run(L, "local t = C_CharacterAdvancement.GetTalentsByClass('Warrior', 'Protection', false) return #t, t[1].ID, t[2].ID") == V{ "2", "102", "142" }));
    CHECK((Run(L, "local t = C_CharacterAdvancement.GetMasteriesByClass('Warrior', 'Protection') return #t, t[1].ID") == V{ "1", "110" }));
    CHECK((Run(L, "local t = C_CharacterAdvancement.GetSpellsByClass('Warrior', 'Protection', false) for _, e in ipairs(t) do "
                  "if e.ID == 110 or e.ID == 101 then return 'leak' end end return 'ok'") == V{ "ok" }));
    Stop(L);
}

// Known entries, spells and ranks; a unit's build by token and spec.
COA_TEST(CA_NativeKnown) {
    lua_State* L = Start({ { 100, 2 }, { 102, 1 }, { 142, 1 } });
    CHECK((Run(L, "local C = C_CharacterAdvancement return C.GetTalentRankByID(102)") == V{ "1", "1" }));
    CHECK((Run(L, "local C = C_CharacterAdvancement return C.GetTalentRankByID(100)") == V{ "2", "1" }));
    CHECK((Run(L, "local C = C_CharacterAdvancement return C.GetTalentRankByID(999)") == V{ "nil", "nil" }));
    CHECK((Run(L, "local C = C_CharacterAdvancement return C.IsKnownID(100), C.IsKnownID(103), C.IsKnownSpellID(5001), C.IsKnownSpellID(1)") ==
        V{ "true", "false", "true", "nil" }));
    CHECK((Run(L, "local t = C_CharacterAdvancement.GetKnownSpells() return #t, t[1]") == V{ "3", "5001" }));
    CHECK((Run(L, "local t = C_CharacterAdvancement.GetKnownSpellEntries() return #t, t[1].ID, t[2].ID") == V{ "2", "100", "142" }));
    CHECK((Run(L, "local t = C_CharacterAdvancement.GetKnownTalentEntries() return #t, t[1].ID") == V{ "1", "102" }));
    CHECK((Run(L, "local t = C_CharacterAdvancement.GetKnownSpellEntriesForClass('Warrior', 'Protection') return #t, t[1].ID") == V{ "2", "100" }));
    CHECK((Run(L, "return C_CharacterAdvancement.UnitKnownID('player', 100, 1), C_CharacterAdvancement.UnitKnownID('player', 100, 2)") ==
        V{ "true", "nil" }));
    CHECK((Run(L, "return C_CharacterAdvancement.UnitKnownID('player', 100, 0)") == V{ "<error> UnitKnownID: specialization must be >= 1" }));
    CHECK((Run(L, "return C_CharacterAdvancement.UnitTalentRankByID('nobody', 100, 1)") == V{ "<error> UnitTalentRankByID: invalid unit token" }));
    CHECK((Run(L, "return C_CharacterAdvancement.GetLearnedAE(), C_CharacterAdvancement.GetLearnedAE('Warrior')") == V{ "4", "4" }));
    CHECK((Run(L, "return C_CharacterAdvancement.GetLearnedAE('Bogus')") == V{ "<error> GetLearnedAE(classType) expects a valid class name" }));
    CHECK((Run(L, "return C_CharacterAdvancement.IsConnectionAllowed(100, 134), C_CharacterAdvancement.IsConnectionAllowed(102, 134)") ==
        V{ "true", "false" }));
    Stop(L);
}

// The globals: IsSpellIDKnown / CA_IsSpellKnown read the known-spell set, C_Spell.HasRuneUI the rune set,
// CA_GetCreditAmount the credit map.
COA_TEST(CA_NativeGlobals) {
    lua_State* L = Start({});
    s_runeSpells = { 777 };
    CHECK((Run(L, "return IsSpellIDKnown(5000), C_Spell.HasRuneUI()") == V{ "false", "false" }));
    CoA::CA::OnInitialSpells({ 5000, 777 });
    CHECK((Run(L, "return IsSpellIDKnown(5000), CA_IsSpellKnown(5000), C_Spell.HasRuneUI()") == V{ "true", "true", "true" }));
    Feed(0x926, CoATest::Bytes().U8(2).U32(500));
    CHECK((Run(L, "return CA_GetCreditAmount(2), CA_GetCreditAmount(3)") == V{ "500", "0" }));
    CHECK((Run(L, "return CA_GetCreditAmount('x')") == V{}));
    Stop(L);
}

// Categories by order or by required level, their display info; tag types by parent in sort order.
COA_TEST(CA_NativeCategoriesAndTags) {
    lua_State* L = Start({});
    CHECK((Run(L, "local t = C_CharacterAdvancement.GetCategories() return t[1], t[2], t[3]") == V{ "2", "1", "3" }));
    CHECK((Run(L, "local t = C_CharacterAdvancement.GetCategories(true) return t[1], t[2], t[3]") == V{ "3", "1", "2" }));
    // FUN_10176450 validates {boolean} (FUN_10309e50): a number passes and counts as true; a string does not.
    CHECK((Run(L, "local t = C_CharacterAdvancement.GetCategories(5) return t[1], t[2], t[3]") == V{ "3", "1", "2" }));
    CHECK((Run(L, "return C_CharacterAdvancement.GetCategories('x')") ==
        V{ "<error> Usage: C_CharacterAdvancement.GetCategories([sortByRequiredLevel])" }));
    CHECK((Run(L, "return C_CharacterAdvancement.GetCategoryDisplayInfo(1)") == V{ "30", "One", "c1", "", "false" }));
    CHECK((Run(L, "return C_CharacterAdvancement.GetCategoryDisplayInfo(2)") == V{ "40", "", "", "two", "true" }));
    CHECK((Run(L, "return C_CharacterAdvancement.GetCategoryDisplayInfo(9)") == V{ "nil", "nil", "nil", "nil", "nil" }));
    CHECK((Run(L, "local t = C_CharacterAdvancement.GetRootSpellTagTypes() return #t, t[1], t[2]") == V{ "2", "2", "1" }));
    CHECK((Run(L, "local t = C_CharacterAdvancement.GetSpellTagTypes(1) return #t, t[1], C_CharacterAdvancement.GetSpellTagTypes(9)") ==
        V{ "1", "3", "nil" }));
    CHECK((Run(L, "return C_CharacterAdvancement.GetSpellTagTypeDisplayInfo(1)") == V{ "Root A", "nil", "desc" }));
    CHECK((Run(L, "return C_CharacterAdvancement.GetSpellTagTypeDisplayInfo(2)") == V{ "Root B", "icon", "" }));
    CHECK((Run(L, "return C_CharacterAdvancement.GetSpellTagTypeDisplayInfo(4)") == V{ "nil", "nil", "nil" }));
    Stop(L);
}

// Qualities: the normal-mode limits without realm config, the count needs a pending build and a valid name.
COA_TEST(CA_NativeQualities) {
    lua_State* L = Start({ { 100, 1 }, { 122, 1 } });
    CHECK((Run(L, "local C = C_CharacterAdvancement return C.GetQualityLimit('Poor'), C.GetQualityLimit('Epic'), C.GetQualityLimit('Uncommon')") ==
        V{ "99", "4", "8" }));
    CHECK((Run(L, "return C_CharacterAdvancement.GetQualityLimit('Shiny')") == V{ "<error> GetQualityLimit: invalid quality name" }));
    CHECK((Run(L, "return C_CharacterAdvancement.GetQualityCount('Normal')") == V{ "3" }));
    CHECK((Run(L, "return C_CharacterAdvancement.GetQualityInfo(5000)") == V{ "Normal", "0" }));
    Stop(L);
}

// PickupSpell picks up the spellbook slot of the known rank; nothing for an unknown spell; an error when the spell
// is known but not in the book.
COA_TEST(CA_NativePickupSpell) {
    lua_State* L = Start({ { 100, 1 } });
    CHECK((Run(L, "return C_CharacterAdvancement.PickupSpell(100)") == V{}));
    CHECK(s_pickedUp.empty());
    s_clientSpells = { 5000 };
    Run(L, "C_CharacterAdvancement.PickupSpell(100)");
    CHECK((s_pickedUp == std::vector<uint32_t> { 5000 }));
    s_inBook = false;
    CHECK((Run(L, "return C_CharacterAdvancement.PickupSpell(100)") == V{ "<error> PickupSpell: spell not found in spellbook" }));
    CHECK((Run(L, "return C_CharacterAdvancement.PickupSpell(999)") == V{ "<error> PickupSpell: invalid entryId" }));
    CHECK((Run(L, "return C_CharacterAdvancement.PickupSpell(102)") == V{}));   // not known, no 0x20000 flag
    Stop(L);
}

// FUN_101c62e0 takes the rank as a byte: 257 is rank 1.
COA_TEST(CA_SpellAtRankByte) {
    Setup();
    CHECK(CoA::CA::SpellAtRankOf(CoA::CA::FindRow(100), 2) == 5001);
    CHECK(CoA::CA::SpellAtRankOf(CoA::CA::FindRow(100), 257) == 5000);
    CHECK(CoA::CA::SpellAtRankOf(CoA::CA::FindRow(100), 10) == 0);
    Teardown();
}
