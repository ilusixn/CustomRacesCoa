#include "CoA/Source/Modules/SpawnVisibility.h"
#include "CoA/Source/Runtime/ClientHooks.h"
#include "CoA/Source/Runtime/Dbc.h"
#include "CoA/Tests/Support.h"
#include "CoA/Tests/TestMain.h"
#include <cstring>
#include <set>
#include <string>
#include <vector>

namespace {
    const char kTable[] = "DBFilesClient\\ObjectSpawnVisibilitySettings.dbc";

    std::vector<std::vector<uint32_t>> s_rows;
    std::set<uint32_t> s_quests;
    std::set<uint32_t> s_achievements;
    bool s_tableExists = true;

    // Strings: 1 TYPEID_UNIT, 13 TYPEID_GAMEOBJECT, 31 TYPEID_PLAYER, 45 BOGUS, 51 TYPEID_OBJECT.
    const std::string kStrings = std::string("TYPEID_UNIT") + '\0' + "TYPEID_GAMEOBJECT" + '\0' + "TYPEID_PLAYER" + '\0' +
        "BOGUS" + '\0' + "TYPEID_OBJECT" + '\0';
    enum : uint32_t { UNIT = 1, GAMEOBJECT = 13, PLAYER = 31, BOGUS = 45, OBJECT = 51 };

    bool Read(const char* path, std::vector<uint8_t>& out) {
        if (!s_tableExists || !strstr(path, "ObjectSpawnVisibilitySettings.dbc")) {
            return false;
        }

        out = CoATest::Wdbc(s_rows, kStrings);
        return true;
    }

    // id, group, type name, entry, counter, quest, achievement
    std::vector<uint32_t> Row(uint32_t id, uint32_t group, uint32_t type, uint32_t entry, uint32_t counter, uint32_t quest = 0,
        uint32_t achievement = 0) {
        return { id, group, type, entry, counter, quest, achievement };
    }

    struct Object {
        uint32_t type;
        uint64_t guid;
        uint32_t entry;
    };

    std::vector<uint32_t> s_order;

    void Start(const std::vector<std::vector<uint32_t>>& rows) {
        CoA::ClientHooks::Reset();
        CoA::SpawnVisibility::Reset();
        s_rows = rows;
        s_quests.clear();
        s_achievements.clear();
        s_tableExists = true;
        s_order.clear();
        CoA::Dbc::SetFileReader(Read);
        CoA::Dbc::Get(kTable).Unload();
        CoA::SpawnVisibility::Deps& d = CoA::SpawnVisibility::Dependencies();
        d.objectInfo = [](void* object, CoA::SpawnVisibility::ObjectInfo& out) {
            const Object* o = static_cast<const Object*>(object);
            out.type = o->type;
            out.guid = o->guid;
            out.entry = o->entry;
            return true;
        };
        d.questCompleted = [](uint32_t q) { return s_quests.count(q) != 0; };
        d.achievementCompleted = [](uint32_t a) { return s_achievements.count(a) != 0; };
        CoA::SpawnVisibility::Init();
    }

    void Stop() {
        CoA::Dbc::Get(kTable).Unload();
        CoA::Dbc::SetFileReader(nullptr);
        CoA::SpawnVisibility::Reset();
        CoA::ClientHooks::Reset();
    }

    bool Apply(Object o, uint32_t spell) {
        return CoA::ClientHooks::addAuraEffect.AllPass(&o, 0, &spell);
    }

    const uint64_t kCreature = 0xF130000101ABCDEFull;   // counter 0xABCDEF (lo 0x01ABCDEF)
    const uint64_t kPlayer = 0x0000000000000777ull;
}

// FUN_10211aa0: the eight TYPEID names; anything else is the fallback.
COA_TEST(SpawnVisibility_TypeIdByName) {
    const char* names[] = { "TYPEID_OBJECT", "TYPEID_ITEM", "TYPEID_CONTAINER", "TYPEID_UNIT", "TYPEID_PLAYER",
        "TYPEID_GAMEOBJECT", "TYPEID_DYNAMICOBJECT", "TYPEID_CORPSE" };

    for (uint32_t i = 0; i < 8; ++i) {
        CHECK(CoA::SpawnVisibility::TypeIdByName(names[i], 99) == i);
    }

    CHECK(CoA::SpawnVisibility::TypeIdByName("typeid_unit", 99) == 99);
    CHECK(CoA::SpawnVisibility::TypeIdByName("", 7) == 7);
}

// Only spell 0x16E59 is asked about; the hook (0x101A2025) runs last in the list's measured order, after SpellVisuals'
// 0x10323151 and Appearance's 0x100C5B88.
COA_TEST(SpawnVisibility_HookOnlyForTheSpell) {
    Start({ Row(1, 1, UNIT, 500, 0) });
    CHECK(!CoA::ClientHooks::addAuraEffect.Empty());
    // Shown (no requirement) -> 0x16E59 refused; any other spell untouched.
    CHECK(!Apply({ 3, kCreature, 500 }, 0x16E59));
    CHECK(Apply({ 3, kCreature, 500 }, 0x16E58));
    CHECK(!Apply({ 3, kCreature, 501 }, 0x16E59));  // not named: both sets empty -> shown -> refused

    CoA::ClientHooks::addAuraEffect.Add([](void*, uint32_t, const void*) { s_order.push_back(2); return true; }, 0x10323151);
    CoA::ClientHooks::addAuraEffect.Add([](void*, uint32_t, const void*) { s_order.push_back(1); return true; }, 0x100C5B88);
    CHECK(!Apply({ 3, kCreature, 500 }, 0x16E59));
    CHECK((s_order == std::vector<uint32_t>{ 2, 1 }));   // both run before the refusal
    Stop();
}

// No table / an empty table: nothing names the object, both sets are empty -> shown -> the spell is refused.
COA_TEST(SpawnVisibility_MissingTable) {
    Start({});
    s_tableExists = false;
    CoA::Dbc::Get(kTable).Unload();
    CHECK(CoA::SpawnVisibility::Visible(3, 500, 1));
    CHECK(!Apply({ 3, kCreature, 500 }, 0x16E59));
    Stop();

    Start({});
    CHECK(CoA::SpawnVisibility::Visible(5, 0, 0));
    Stop();
}

// Groups: a set passes when one group has every row passing; quest (+0x14) and achievement (+0x18) requirements.
COA_TEST(SpawnVisibility_Groups) {
    Start({
        Row(1, 7, UNIT, 500, 0, 11, 0),
        Row(2, 7, UNIT, 500, 0, 0, 21),
        Row(3, 8, UNIT, 500, 0, 12, 0),
        Row(4, 9, GAMEOBJECT, 500, 0, 0, 0),   // another type: ignored for units
    });
    CHECK(!CoA::SpawnVisibility::Visible(3, 500, 0));
    s_quests.insert(11);
    CHECK(!CoA::SpawnVisibility::Visible(3, 500, 0));   // group 7 still needs achievement 21
    s_achievements.insert(21);
    CHECK(CoA::SpawnVisibility::Visible(3, 500, 0));
    s_quests.clear();
    s_quests.insert(12);
    s_achievements.clear();
    CHECK(CoA::SpawnVisibility::Visible(3, 500, 0));    // group 8 alone
    CHECK(CoA::SpawnVisibility::Visible(5, 500, 0));    // the game object row has no requirement
    CHECK(CoA::SpawnVisibility::Visible(3, 501, 0));    // nothing names entry 501
    Stop();
}

// Both sets must pass: entry rows and counter rows separately; entry / counter 0 never match.
COA_TEST(SpawnVisibility_EntryAndCounterSets) {
    Start({
        Row(1, 1, UNIT, 500, 0, 0, 0),
        Row(2, 2, UNIT, 0, 0xABCDEF, 31, 0),
        Row(3, 3, UNIT, 0, 0, 32, 0),         // names neither: never consulted
    });
    CHECK(!CoA::SpawnVisibility::Visible(3, 500, 0xABCDEF));
    CHECK(CoA::SpawnVisibility::Visible(3, 500, 0xABCDE0));
    CHECK(CoA::SpawnVisibility::Visible(3, 0, 0));
    s_quests.insert(31);
    CHECK(CoA::SpawnVisibility::Visible(3, 500, 0xABCDEF));
    Stop();
}

// The counter: creature-like GUIDs keep their low 24 bits; players and the kinds 0xF101 / 0xF100 / 0x4000 ... keep lo.
COA_TEST(SpawnVisibility_GuidCounter) {
    Start({ Row(1, 1, UNIT, 0, 0xABCDEF, 41, 0), Row(2, 1, PLAYER, 0, 0x777, 42, 0), Row(3, 1, OBJECT, 0, 0x01ABCDEF, 43, 0) });
    // Unit 0xF130... counter 0xABCDEF: row 1 needs quest 41 -> hidden -> applied.
    CHECK(Apply({ 3, kCreature, 0 }, 0x16E59));
    s_quests.insert(41);
    CHECK(!Apply({ 3, kCreature, 0 }, 0x16E59));
    // A player's counter is its lo.
    CHECK(Apply({ 4, kPlayer, 0 }, 0x16E59));
    s_quests.insert(42);
    CHECK(!Apply({ 4, kPlayer, 0 }, 0x16E59));
    // 0xF100 (dynamic object) keeps the whole lo, 0x01ABCDEF.
    CHECK(Apply({ 0, 0xF100000001ABCDEFull, 0 }, 0x16E59));
    s_quests.insert(43);
    CHECK(!Apply({ 0, 0xF100000001ABCDEFull, 0 }, 0x16E59));
    Stop();
}

// An unknown type name reads as TYPEID_OBJECT (fallback 0).
COA_TEST(SpawnVisibility_UnknownTypeName) {
    Start({ Row(1, 1, BOGUS, 500, 0, 51, 0) });
    CHECK(CoA::SpawnVisibility::Visible(3, 500, 0));
    CHECK(!CoA::SpawnVisibility::Visible(0, 500, 0));
    Stop();
}

// An entry / counter of 0 never names the object: rows with 0 there are not consulted even for an object whose entry or
// counter is 0.
COA_TEST(SpawnVisibility_ZeroNeverMatches) {
    Start({ Row(1, 1, UNIT, 0, 0, 61, 0) });
    CHECK(CoA::SpawnVisibility::Visible(3, 0, 0));
    CHECK(CoA::SpawnVisibility::Visible(3, 0, 5));
    CHECK(CoA::SpawnVisibility::Visible(3, 5, 0));
    Stop();
}
