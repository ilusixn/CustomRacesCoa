// C_Wildcard, the desired / undesired lists (WildcardSets.cpp): the add checks and their codes, the list changes and
// their events, the bulk removals, the entry browsers, and the CA list callbacks / resets that empty them.
#include "CoA/Tests/WildcardFixture.h"
#include "CoA/Source/Runtime/Lifecycle.h"
#include <algorithm>

using namespace WCFixture;

namespace {
    bool Has(const V& v, const std::string& s) {
        return std::find(v.begin(), v.end(), s) != v.end();
    }

    // The entry at a filtered index as "ID Type Class Tab Name RequiredLevel Icon #Spells Spells[1]".
    V At(lua_State* L, const char* which, int index) {
        return R(L, std::string("local t = C_Wildcard.GetFiltered") + which + "EntryAtIndex(" + std::to_string(index) +
            ") if not t then return 'none' end return t.ID, t.Type, t.Class, t.Tab, t.Name, t.RequiredLevel, t.Icon, #t.Spells, "
            "t.Spells[1]");
    }

    V Ids(lua_State* L, const char* which) {
        return R(L, std::string("local out = {} for i = 1, C_Wildcard.GetNumFiltered") + which + "Entries() do out[#out + 1] = "
            "C_Wildcard.GetFiltered" + which + "EntryAtIndex(i).Type .. ':' .. C_Wildcard.GetFiltered" + which +
            "EntryAtIndex(i).ID end return unpack(out)");
    }
}

// FUN_10a33170's ADD_WILDCARD_DESIRED_ENTRY_* codes.
COA_TEST(WildcardSets_CanAddDesired) {
    lua_State* L = Start({ { 100 } });
    CHECK((R(L, "return C_Wildcard.CanAddDesiredID(100, 'Ability')") == V{ "false", "ADD_WILDCARD_DESIRED_ENTRY_ALREADY_KNOWN" }));
    CHECK((R(L, "return C_Wildcard.CanAddDesiredID(999, 'Ability')") == V{ "false", "ADD_WILDCARD_DESIRED_ENTRY_NOT_FOUND" }));
    CHECK((R(L, "return C_Wildcard.CanAddDesiredID(102, 'Ability')") == V{ "false", "ADD_WILDCARD_DESIRED_ENTRY_UNKNOWN" }));
    CHECK((R(L, "return C_Wildcard.CanAddDesiredID(130, 'Ability')") == V{ "false", "ADD_WILDCARD_DESIRED_ENTRY_UNKNOWN" }));
    CHECK((R(L, "return C_Wildcard.CanAddDesiredID(110, 'Ability')") == V{ "false", "ADD_WILDCARD_DESIRED_ENTRY_UNKNOWN" }));
    CHECK((R(L, "return C_Wildcard.CanAddDesiredID(101, 'Ability')") == V{ "false", "ADD_WILDCARD_DESIRED_ENTRY_UNKNOWN" }));
    CHECK((R(L, "return C_Wildcard.CanAddDesiredID(102, 'Talent')") == V{ "true", "nil" }));
    CHECK((R(L, "return C_Wildcard.CanAddDesiredID(142, 'TalentAbility')") == V{ "true", "nil" }));
    Run(L, "C_Wildcard.AddDesiredID(102, 'Talent')");
    CHECK((R(L, "return C_Wildcard.CanAddDesiredID(102, 'Talent')") == V{ "false", "ADD_WILDCARD_DESIRED_ENTRY_ALREADY_DESIRED" }));
    CHECK((R(L, "return C_Wildcard.CanAddDesiredID(1, 'Tag')") == V{ "true", "nil" }));
    CHECK((R(L, "return C_Wildcard.CanAddDesiredID(99, 'Tag')") == V{ "false", "ADD_WILDCARD_DESIRED_ENTRY_NOT_FOUND" }));
    CHECK((R(L, "return C_Wildcard.CanAddDesiredID(2, 'Suggestion')") == V{ "true", "nil" }));
    CHECK((R(L, "return C_Wildcard.CanAddDesiredID(3, 'Suggestion')") == V{ "false", "ADD_WILDCARD_DESIRED_ENTRY_NOT_FOUND" }));
    CHECK((R(L, "return C_Wildcard.CanAddDesiredID(102, 'Trait')") == V{ "false", "ADD_WILDCARD_DESIRED_ENTRY_NOT_FOUND" }));
    CHECK((R(L, "return C_Wildcard.CanAddDesiredID(102, 'None')") == V{ "false", "ADD_WILDCARD_DESIRED_ENTRY_NOT_FOUND" }));
    CHECK((R(L, "return C_Wildcard.CanAddDesiredID(102, 'Spell')").empty()));   // not a kind name
    CHECK((R(L, "return C_Wildcard.CanAddDesiredID('x', 'Talent')").empty()));
    Stop(L);
}

// FUN_10a33280's ADD_WILDCARD_UNDESIRED_ENTRY_* codes: only a held, unlocked CA entry of its own kind may be undesired.
COA_TEST(WildcardSets_CanAddUndesired) {
    lua_State* L = Start({ { 100 }, { 110 }, { 104, 1, 1 } });
    CHECK((R(L, "return C_Wildcard.CanAddUndesiredID(100, 'Ability')") == V{ "true", "nil" }));
    CHECK((R(L, "return C_Wildcard.CanAddUndesiredID(102, 'Talent')") == V{ "false", "ADD_WILDCARD_UNDESIRED_ENTRY_NOT_KNOWN" }));
    CHECK((R(L, "return C_Wildcard.CanAddUndesiredID(999, 'Ability')") == V{ "false", "ADD_WILDCARD_UNDESIRED_ENTRY_UNKNOWN" }));
    CHECK((R(L, "return C_Wildcard.CanAddUndesiredID(104, 'Ability')") == V{ "false", "ADD_WILDCARD_UNDESIRED_ENTRY_LOCKED" }));
    CHECK((R(L, "return C_Wildcard.CanAddUndesiredID(100, 'Talent')") == V{ "false", "ADD_WILDCARD_UNDESIRED_ENTRY_UNKNOWN" }));
    CHECK((R(L, "return C_Wildcard.CanAddUndesiredID(110, 'Ability')") == V{ "false", "ADD_WILDCARD_UNDESIRED_ENTRY_UNKNOWN" }));
    Run(L, "C_Wildcard.AddUndesiredID(100, 'Ability')");
    CHECK((R(L, "return C_Wildcard.CanAddUndesiredID(100, 'Ability')") == V{ "false", "ADD_WILDCARD_UNDESIRED_ENTRY_ALREADY_UNDESIRED" }));
    CHECK((R(L, "return C_Wildcard.CanAddUndesiredID(1, 'Tag')") == V{ "false", "ADD_WILDCARD_UNDESIRED_ENTRY_SPELL_TAG" }));
    CHECK((R(L, "return C_Wildcard.CanAddUndesiredID(99, 'Tag')") == V{ "false", "ADD_WILDCARD_UNDESIRED_ENTRY_UNKNOWN" }));
    CHECK((R(L, "return C_Wildcard.CanAddUndesiredID(1, 'Suggestion')") == V{ "false", "ADD_WILDCARD_UNDESIRED_ENTRY_SUGGESTION" }));
    CHECK((R(L, "return C_Wildcard.CanAddUndesiredID(3, 'Suggestion')") == V{ "false", "ADD_WILDCARD_UNDESIRED_ENTRY_UNKNOWN" }));
    CHECK((R(L, "return C_Wildcard.CanAddUndesiredID(100, 'Trait')") == V{ "false", "ADD_WILDCARD_UNDESIRED_ENTRY_UNKNOWN" }));
    CHECK((R(L, "return C_Wildcard.CanAddUndesiredID(100, 'Bogus')").empty()));
    Stop(L);
}

// The adds and removals: one event per change (none for a no-op), the Is* checks by id and kind.
COA_TEST(WildcardSets_AddRemove) {
    lua_State* L = Start({ { 100 }, { 142 } });
    CHECK((Fired(L, "C_Wildcard.AddDesiredID(102, 'Talent')") == V{ "WILDCARD_DESIRED_ENTRIES_CHANGED" }));
    CHECK((Fired(L, "C_Wildcard.AddDesiredID(102, 'Talent')").empty()));
    CHECK((Fired(L, "C_Wildcard.AddDesiredID(100, 'Ability')").empty()));   // known: refused
    CHECK((Fired(L, "C_Wildcard.AddDesiredID(1, 'Tag') C_Wildcard.AddDesiredID(1, 'Suggestion')") ==
        V{ "WILDCARD_DESIRED_ENTRIES_CHANGED", "WILDCARD_DESIRED_ENTRIES_CHANGED" }));
    CHECK((R(L, "return C_Wildcard.IsDesiredID(102, 'Talent'), C_Wildcard.IsDesiredID(102, 'Ability'), C_Wildcard.IsDesiredID(1, 'Tag'), "
                "C_Wildcard.IsDesiredID(1, 'Suggestion'), C_Wildcard.IsDesiredID(2, 'Suggestion')") ==
        V{ "true", "false", "true", "true", "false" }));
    CHECK((Fired(L, "C_Wildcard.RemoveDesiredID(102, 'Ability')").empty()));
    CHECK((Fired(L, "C_Wildcard.RemoveDesiredID(102, 'Talent')") == V{ "WILDCARD_DESIRED_ENTRIES_CHANGED" }));
    CHECK((R(L, "return C_Wildcard.IsDesiredID(102, 'Talent')") == V{ "false" }));
    CHECK((Fired(L, "C_Wildcard.AddUndesiredID(100, 'Ability') C_Wildcard.AddUndesiredID(142, 'TalentAbility')") ==
        V{ "WILDCARD_UNDESIRED_ENTRIES_CHANGED", "WILDCARD_UNDESIRED_ENTRIES_CHANGED" }));
    CHECK((Fired(L, "C_Wildcard.AddUndesiredID(1, 'Tag')").empty()));
    CHECK((R(L, "return C_Wildcard.IsUndesiredID(100, 'Ability'), C_Wildcard.IsUndesiredID(142, 'TalentAbility'), C_Wildcard.IsUndesiredID(1, 'Tag')") ==
        V{ "true", "true", "false" }));
    CHECK(CoA::WildcardSets::IsUndesired(142, 4));
    CHECK((Fired(L, "C_Wildcard.RemoveUndesiredID(142, 'TalentAbility')") == V{ "WILDCARD_UNDESIRED_ENTRIES_CHANGED" }));
    CHECK((Fired(L, "C_Wildcard.RemoveUndesiredID(142, 'TalentAbility')").empty()));
    CHECK((R(L, "return C_Wildcard.IsUndesiredID(100, 'Bogus')").empty()));
    Stop(L);
}

// Clear* only act on a non-empty list; RemoveAllUndesired* drop the ability-flagged (Ability, TalentAbility) or
// talent-flagged entries and fire once when anything went.
COA_TEST(WildcardSets_ClearAndRemoveAll) {
    lua_State* L = Start({ { 100 }, { 102 }, { 142 } });
    CHECK((Fired(L, "C_Wildcard.ClearDesiredSpells() C_Wildcard.ClearUndesiredSpells()").empty()));
    Run(L, "C_Wildcard.AddDesiredID(103, 'Ability') C_Wildcard.AddUndesiredID(100, 'Ability') "
           "C_Wildcard.AddUndesiredID(102, 'Talent') C_Wildcard.AddUndesiredID(142, 'TalentAbility')");
    CHECK((Fired(L, "C_Wildcard.ClearDesiredSpells()") == V{ "WILDCARD_DESIRED_ENTRIES_CHANGED" }));
    CHECK((R(L, "return C_Wildcard.IsDesiredID(103, 'Ability')") == V{ "false" }));
    CoA::CA::detail::Sugg().dirty = false;
    CHECK((Fired(L, "C_Wildcard.RemoveAllUndesiredAbilities()") == V{ "WILDCARD_UNDESIRED_ENTRIES_CHANGED" }));
    CHECK(CoA::CA::detail::Sugg().dirty);
    CHECK((R(L, "return C_Wildcard.IsUndesiredID(100, 'Ability'), C_Wildcard.IsUndesiredID(142, 'TalentAbility'), "
                "C_Wildcard.IsUndesiredID(102, 'Talent')") == V{ "false", "false", "true" }));
    CHECK((Fired(L, "C_Wildcard.RemoveAllUndesiredAbilities()").empty()));
    CHECK((Fired(L, "C_Wildcard.RemoveAllUndesiredTalents()") == V{ "WILDCARD_UNDESIRED_ENTRIES_CHANGED" }));
    CHECK((Fired(L, "C_Wildcard.RemoveAllUndesiredTalents()").empty()));
    Run(L, "C_Wildcard.AddUndesiredID(100, 'Ability')");
    CoA::CA::detail::Sugg().dirty = false;
    CHECK((Fired(L, "C_Wildcard.ClearUndesiredSpells()") == V{ "WILDCARD_UNDESIRED_ENTRIES_CHANGED" }));
    CHECK(CoA::CA::detail::Sugg().dirty);
    Stop(L);
}

// The desired browser: every desirable CA row, the SpellTagTypes rows with +0xC and +0x10 zero, then the two suggestion
// entries; FILTER_* groups AND, filters in a group OR (tags and suggestions match none); the text matches the name.
COA_TEST(WildcardSets_DesiredBrowser) {
    lua_State* L = Start({ { 100 } });
    CHECK((R(L, "return C_Wildcard.GetNumFilteredDesiredEntries()") == V{ "0" }));
    Run(L, "C_Wildcard.SetFilteredDesiredEntries('', { FILTER_TYPE_TALENT = true })");
    CHECK((Ids(L, "Desired") == V{ "Talent:102" }));
    {   // FUN_100bed80: a bad value or key empties the map -- the same view as no filters
        Run(L, "C_Wildcard.SetFilteredDesiredEntries('', {})");
        const V none = Ids(L, "Desired");
        CHECK(none != V{ "Talent:102" });
        Run(L, "C_Wildcard.SetFilteredDesiredEntries('', { FILTER_TYPE_TALENT = 'x' })");
        CHECK(Ids(L, "Desired") == none);
        Run(L, "C_Wildcard.SetFilteredDesiredEntries('', { FILTER_TYPE_TALENT = true, [true] = true })");
        CHECK(Ids(L, "Desired") == none);
        Run(L, "C_Wildcard.SetFilteredDesiredEntries('', { FILTER_TYPE_TALENT = true })");
    }
    CHECK((At(L, "Desired", 1) == V{ "102", "Talent", "Warrior", "Protection", "", "0", "", "1", "5100" }));
    CHECK((At(L, "Desired", 2) == V{ "none" }));
    CHECK((At(L, "Desired", 0) == V{ "none" }));
    Run(L, "C_Wildcard.SetFilteredDesiredEntries('', { FILTER_TYPE_TALENT = true, FILTER_TYPE_ABILITY = true, FILTER_CLASS_WARRIOR = false })");
    const V all = Ids(L, "Desired");
    CHECK(Has(all, "Talent:102"));
    CHECK(Has(all, "Ability:103"));
    CHECK(Has(all, "TalentAbility:142"));
    CHECK(!Has(all, "Ability:100"));   // known
    CHECK(!Has(all, "Ability:101"));   // another class
    CHECK(!Has(all, "Ability:110"));   // a mastery
    CHECK(!Has(all, "Ability:130"));   // a display entry
    CHECK(!Has(all, "Tag:1"));
    // Without filters the tags and the suggestions follow the rows; the suggestions sort first among the unselected.
    Run(L, "C_Wildcard.SetFilteredDesiredEntries('sugg', {})");
    CHECK((Ids(L, "Desired") == V{ "Suggestion:1", "Suggestion:2" }));
    CHECK((At(L, "Desired", 1) == V{ "1", "Suggestion", "INVALID_CLASS_TYPE_0", "INVALID_TAB_TYPE_0", "Suggested Abilities", "0",
        "Interface\\Icons\\Ability_ThunderBolt", "0", "nil" }));
    CHECK((At(L, "Desired", 2) == V{ "2", "Suggestion", "INVALID_CLASS_TYPE_0", "INVALID_TAB_TYPE_0", "Suggested Talents", "0",
        "Interface\\Icons\\Warrior_talent_icon_singlemindedfury", "0", "nil" }));
    Run(L, "C_Wildcard.SetFilteredDesiredEntries('DESC', { FILTER_NONE = false })");
    CHECK((Ids(L, "Desired") == V{ "Tag:1" }));
    // A selected entry sorts first.
    Run(L, "C_Wildcard.AddDesiredID(2, 'Suggestion') C_Wildcard.SetFilteredDesiredEntries('Suggested', {})");
    CHECK((Ids(L, "Desired") == V{ "Suggestion:2", "Suggestion:1" }));
    // A quality filter: the fixture rows are all of the first quality's group or none.
    Run(L, "C_Wildcard.SetFilteredDesiredEntries('', { FILTER_QUALITY_LEGENDARY = true })");
    CHECK((R(L, "return C_Wildcard.GetNumFilteredDesiredEntries()") == V{ "0" }));
    CHECK((R(L, "return C_Wildcard.SetFilteredDesiredEntries(1, {})").empty()));
    Stop(L);
}

// The undesired browser: every held, undesirable row, the most recently learned (+0x18) first among the unselected.
COA_TEST(WildcardSets_UndesiredBrowser) {
    lua_State* L = Start({ { 100, 1, 0, 5 }, { 102, 1, 0, 9 }, { 142, 1, 0, 7 }, { 104, 1, 1, 1 } });
    Run(L, "C_Wildcard.SetFilteredUndesiredEntries('', {})");
    CHECK((Ids(L, "Undesired") == V{ "Talent:102", "TalentAbility:142", "Ability:100" }));
    Run(L, "C_Wildcard.AddUndesiredID(100, 'Ability') C_Wildcard.SetFilteredUndesiredEntries('', {})");
    CHECK((Ids(L, "Undesired") == V{ "Ability:100", "Talent:102", "TalentAbility:142" }));
    Run(L, "C_Wildcard.SetFilteredUndesiredEntries('', { FILTER_CLASS_WARRIOR = true, FILTER_TYPE_ABILITY = true })");
    CHECK((Ids(L, "Undesired") == V{ "Ability:100", "TalentAbility:142" }));
    Run(L, "C_Wildcard.SetFilteredUndesiredEntries('', { FILTER_CLASS_MAGE = true })");
    CHECK((R(L, "return C_Wildcard.GetNumFilteredUndesiredEntries()") == V{ "0" }));
    Stop(L);
}

// AddAll*Filter: every filtered entry added quietly, one event when the list grew.
COA_TEST(WildcardSets_AddAllFilters) {
    lua_State* L = Start({ { 100 }, { 102 } });
    CHECK((Fired(L, "C_Wildcard.AddAllDesiredFilter()").empty()));
    Run(L, "C_Wildcard.SetFilteredDesiredEntries('sugg', {})");
    CHECK((Fired(L, "C_Wildcard.AddAllDesiredFilter()") == V{ "WILDCARD_DESIRED_ENTRIES_CHANGED" }));
    CHECK((R(L, "return C_Wildcard.IsDesiredID(1, 'Suggestion'), C_Wildcard.IsDesiredID(2, 'Suggestion')") == V{ "true", "true" }));
    CHECK((Fired(L, "C_Wildcard.AddAllDesiredFilter()").empty()));
    Run(L, "C_Wildcard.SetFilteredUndesiredEntries('', {})");
    CoA::CA::detail::Sugg().dirty = false;
    CHECK((Fired(L, "C_Wildcard.AddAllUndesiredFilter()") == V{ "WILDCARD_UNDESIRED_ENTRIES_CHANGED" }));
    CHECK(CoA::CA::detail::Sugg().dirty);
    CHECK((R(L, "return C_Wildcard.IsUndesiredID(100, 'Ability'), C_Wildcard.IsUndesiredID(102, 'Talent')") == V{ "true", "true" }));
    Stop(L);
}

// The CA lists: a learned entry leaves the desired list, an unlearned one the undesired list (each with its event),
// and both browsers re-run their last query.
COA_TEST(WildcardSets_CALearnedAndUnlearned) {
    lua_State* L = Start({ { 100 } });
    Run(L, "C_Wildcard.AddDesiredID(102, 'Talent') C_Wildcard.AddUndesiredID(100, 'Ability') "
           "C_Wildcard.SetFilteredDesiredEntries('', { FILTER_TYPE_TALENT = true }) C_Wildcard.SetFilteredUndesiredEntries('', {})");
    CHECK((Ids(L, "Desired") == V{ "Talent:102" }));
    CHECK((Ids(L, "Undesired") == V{ "Ability:100" }));
    V s = Feed(0x726, Entries({ { 100 }, { 102 } }));
    CHECK(Has(s, "WILDCARD_DESIRED_ENTRIES_CHANGED"));
    CHECK(!Has(s, "WILDCARD_UNDESIRED_ENTRIES_CHANGED"));
    CHECK((R(L, "return C_Wildcard.IsDesiredID(102, 'Talent'), C_Wildcard.GetNumFilteredDesiredEntries()") == V{ "false", "0" }));
    CHECK((Ids(L, "Undesired") == V{ "Ability:100", "Talent:102" }));   // the undesired one first
    s = Feed(0x726, Entries({ { 102 } }));
    CHECK(Has(s, "WILDCARD_UNDESIRED_ENTRIES_CHANGED"));
    CHECK((R(L, "return C_Wildcard.IsUndesiredID(100, 'Ability')") == V{ "false" }));
    CHECK((Ids(L, "Undesired") == V{ "Talent:102" }));
    Stop(L);
}

// A spec change empties both lists (one event each); the glue screen empties them and the browsers silently.
COA_TEST(WildcardSets_SpecChangeAndGlue) {
    lua_State* L = Start({ { 100 } });
    Run(L, "C_Wildcard.AddDesiredID(102, 'Talent') C_Wildcard.AddUndesiredID(100, 'Ability')");
    V s = Feed(0x725, CoATest::Bytes().U32(1).U32(2));
    CHECK(Has(s, "WILDCARD_DESIRED_ENTRIES_CHANGED"));
    CHECK(Has(s, "WILDCARD_UNDESIRED_ENTRIES_CHANGED"));
    CHECK((R(L, "return C_Wildcard.IsDesiredID(102, 'Talent'), C_Wildcard.IsUndesiredID(100, 'Ability')") == V{ "false", "false" }));
    s = Feed(0x725, CoATest::Bytes().U32(0).U32(2));
    CHECK(!Has(s, "WILDCARD_DESIRED_ENTRIES_CHANGED"));
    Feed(0x726, Entries({ { 100 } }));
    Run(L, "C_Wildcard.AddDesiredID(1, 'Tag') C_Wildcard.AddUndesiredID(100, 'Ability') C_Wildcard.SetFilteredDesiredEntries('sugg', {})");
    CHECK((R(L, "return C_Wildcard.GetNumFilteredDesiredEntries()") == V{ "2" }));
    CoATest::RecordSignals();
    CoA::Lifecycle::RunGlueScreen();
    CoATest::StopRecordingSignals();
    CHECK(!Has(CoATest::Signals(), "WILDCARD_DESIRED_ENTRIES_CHANGED"));
    CHECK(!Has(CoATest::Signals(), "WILDCARD_UNDESIRED_ENTRIES_CHANGED"));
    CHECK((R(L, "return C_Wildcard.IsDesiredID(1, 'Tag'), C_Wildcard.IsUndesiredID(100, 'Ability'), "
                "C_Wildcard.GetNumFilteredDesiredEntries()") == V{ "false", "false", "0" }));
    // The browser forgot its query: a learned entry does not re-run it.
    Feed(0x725, CoATest::Bytes().U32(0).U32(1));
    Feed(0x726, Entries({ { 103 } }));
    CHECK((R(L, "return C_Wildcard.GetNumFilteredDesiredEntries()") == V{ "0" }));
    Stop(L);
}
