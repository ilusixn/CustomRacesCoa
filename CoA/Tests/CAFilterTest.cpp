#include "CoA/Source/Modules/CAFilter.h"
#include "CoA/Tests/CAFixture.h"
#include <algorithm>

using namespace CAFixture;

namespace {
    std::map<uint32_t, std::string> s_names;

    const std::string& Name(uint32_t spell) {
        static const std::string empty;
        auto it = s_names.find(spell);
        return it == s_names.end() ? empty : it->second;
    }

    CoATest::Bytes Entries(const std::vector<std::pair<uint32_t, uint32_t>>& entries, uint32_t lockedId = 0) {
        CoATest::Bytes b;
        b.U32(static_cast<uint32_t>(entries.size()));

        for (const auto& e : entries) {
            b.U32(e.first).U32(e.second).U32(e.second).U8(e.first == lockedId ? 1 : 0).U32(0).U32(0);
        }

        return b;
    }

    lua_State* Start(const std::vector<std::pair<uint32_t, uint32_t>>& entries, uint32_t lockedId = 0) {
        Setup();
        s_names = { { 5000, "Heroic Strike" }, { 5100, "Shield Wall" } };
        CoA::CA::Deps& d = CoA::CA::Dependencies();
        d.spellName = Name;
        d.creatorHasSpell = [](uint32_t spell) { return spell == 5000; };
        CoA::CA::Init();
        Feed(0x725, CoATest::Bytes().U32(0).U32(1));
        Feed(0x726, Entries(entries, lockedId));
        return CoATest::NewState(false);
    }

    void Stop(lua_State* L) {
        lua_close(L);
        Teardown();
    }

    V Ids(lua_State* L, const char* set) {
        return Run(L, (std::string(set) + " local C = C_CharacterAdvancement local t = {} "
                                          "for i = 1, C.GetNumFilteredEntries() do t[#t + 1] = C.GetFilteredEntryAtIndex(i).ID end "
                                          "return unpack(t)").c_str());
    }
}

// The query matches the chain's spell names (case-insensitive); IsFiltered reports membership once a query is set.
COA_TEST(CA_FilterQuery) {
    lua_State* L = Start({});
    CHECK((Ids(L, "C_CharacterAdvancement.SetFilteredEntries('HEROIC', {})") == V{ "100" }));
    CHECK((Run(L, "return C_CharacterAdvancement.IsFiltered(100), C_CharacterAdvancement.IsFiltered(102)") == V{ "true", "false" }));
    CHECK((Ids(L, "C_CharacterAdvancement.SetFilteredEntries('shield', {})") == V{ "102" }));
    CHECK((Run(L, "return C_CharacterAdvancement.GetFilteredEntryAtIndex(0)") ==
        V{ "<error> GetFilteredEntryAtIndex: index must be >= 1" }));
    CHECK((Run(L, "return C_CharacterAdvancement.SetFilteredEntries(1)") ==
        V{ "<error> Usage: C_CharacterAdvancement.SetFilteredEntries(query, additionalFiltersTable)" }));
    Stop(L);
}

// Filters OR within a group (types) and AND across groups (type and known); a quality word in the query becomes a
// quality filter; a '$' token empties the results.
COA_TEST(CA_FilterGroups) {
    lua_State* L = Start({ { 102, 1 } });
    CHECK((Ids(L, "C_CharacterAdvancement.SetFilteredEntries('', { FILTER_TYPE_TALENT = true })") == V{ "102" }));
    const V both = Ids(L, "C_CharacterAdvancement.SetFilteredEntries('', { FILTER_TYPE_TALENT = true, FILTER_TYPE_ABILITY = true })");
    CHECK(both.size() > 3 && std::find(both.begin(), both.end(), "102") != both.end() && std::find(both.begin(), both.end(), "142") != both.end());
    CHECK((Ids(L, "C_CharacterAdvancement.SetFilteredEntries('', { FILTER_TYPE_TALENT = true, FILTER_UNKNOWN = true })") == V{}));
    CHECK((Ids(L, "C_CharacterAdvancement.SetFilteredEntries('', { FILTER_TYPE_TALENT = true, FILTER_KNOWN = true })") == V{ "102" }));
    CHECK((Ids(L, "C_CharacterAdvancement.SetFilteredEntries('rare', {})") == V{}));
    const V all = Ids(L, "C_CharacterAdvancement.SetFilteredEntries('', {})");
    CHECK(Ids(L, "C_CharacterAdvancement.SetFilteredEntries('common', {})").size() == all.size());   // every row is Normal
    CHECK((Ids(L, "C_CharacterAdvancement.SetFilteredEntries('$a1', {})") == V{}));
    Stop(L);
}

// Results sort by the entry key: locked first, then known, then id.
COA_TEST(CA_FilterOrder) {
    lua_State* L = Start({ { 120, 1 }, { 135, 1 } }, 135);
    const V ids = Ids(L, "C_CharacterAdvancement.SetFilteredEntries('', {})");
    CHECK(ids.size() > 3 && ids[0] == "135" && ids[1] == "120");
    Stop(L);
}

// A category container lists the rows naming the category at +0x1C8, sorted by the category key: learnable (133)
// before not (130 display-only, 134 missing its connection), then by id.
COA_TEST(CA_FilterCategory) {
    lua_State* L = Start({});
    Run(L, "C_CharacterAdvancement.SetFilteredEntriesByCategory(2, {}, '', {})");
    CHECK((Run(L, "return C_CharacterAdvancement.GetNumFilteredEntriesByCategory(2)") == V{ "3" }));
    CHECK((Run(L, "local e, s = C_CharacterAdvancement.GetFilteredEntryAtIndexByCategory(2, 1) return e.ID, s") == V{ "133", "false" }));
    CHECK((Run(L, "return C_CharacterAdvancement.GetFilteredEntryAtIndexByCategory(2, 2).ID, C_CharacterAdvancement.GetFilteredEntryAtIndexByCategory(2, 3).ID") ==
        V{ "130", "134" }));
    CHECK((Run(L, "return C_CharacterAdvancement.GetFilteredEntryAtIndexByCategory(2, 4)") == V{ "nil", "nil" }));
    CHECK((Run(L, "return C_CharacterAdvancement.GetNumFilteredEntriesByCategory(9)") ==
        V{ "<error> GetNumFilteredEntriesByCategory: invalid categoryId" }));
    Stop(L);
}

// C_BuildEditor raises nothing on bad arguments and asks the Build Creator for its known filter.
COA_TEST(CA_FilterEditor) {
    lua_State* L = Start({});
    CHECK((Run(L, "return C_BuildEditor.SetFilteredEntries(1)") == V{}));
    Run(L, "C_BuildEditor.SetFilteredEntries('', { FILTER_KNOWN = true })");
    CHECK((Run(L, "return C_BuildEditor.GetNumFilteredEntries(), C_BuildEditor.GetFilteredEntryAtIndex(1).ID") == V{ "1", "100" }));
    CHECK((Run(L, "return C_BuildEditor.GetFilteredEntryAtIndex(0)") == V{}));
    Stop(L);
}

// The glue screen empties every container.
COA_TEST(CA_FilterGlueReset) {
    lua_State* L = Start({});
    Run(L, "C_CharacterAdvancement.SetFilteredEntries('heroic', {})");
    CoA::Lifecycle::RunGlueScreen();
    CHECK((Run(L, "return C_CharacterAdvancement.GetNumFilteredEntries(), C_CharacterAdvancement.IsFiltered(100)") == V{ "0", "false" }));
    Stop(L);
}
