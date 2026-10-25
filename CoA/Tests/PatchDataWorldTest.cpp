#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Modules/PatchData.h"
#include "CoA/Source/Runtime/Dbc.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Tests/Support.h"
#include "CoA/Tests/TestMain.h"
#include <cstring>
#include <string>
#include <vector>

namespace {
    std::vector<std::string> s_calls;

    void Feed(uint32_t opcode, CoATest::Bytes bytes) {
        CDataStore msg(bytes.data.data(), static_cast<uint32_t>(bytes.data.size()));
        int32_t result = 0;
        CoA::Packets::Dispatch(static_cast<int32_t>(opcode), 0, &msg, result);
    }

    void Setup() {
        CoA::Packets::Clear();
        CoA::PatchData::Init();
        s_calls.clear();
        CoA::PatchData::Deps& deps = CoA::PatchData::Dependencies();
        deps.itemAppearancesPatched = [] { s_calls.push_back("itemAppearances"); };
        deps.itemSetAppearancesPatched = [] { s_calls.push_back("itemSetAppearances"); };
        deps.rebuildOutfits = [] { s_calls.push_back("outfits"); };
        deps.tagTypesPatched = [] { s_calls.push_back("tagTypes"); };
        deps.geosetsRebuild = [](const uint8_t* row) { s_calls.push_back("geosets:" + std::to_string(CoA::Dbc::Table::U32(row, 0))); };
        deps.gameObjectTextures = [] { s_calls.push_back("gameObjects"); };
        deps.rebuildSpellCustomAttr = [] { s_calls.push_back("customAttr"); };
        deps.spellTagRemove = [](uint32_t spell, uint32_t tag) { s_calls.push_back("tag-" + std::to_string(spell) + "/" + std::to_string(tag)); };
        deps.spellTagAdd = [](uint32_t spell, uint32_t tag) { s_calls.push_back("tag+" + std::to_string(spell) + "/" + std::to_string(tag)); };
        deps.rebuildKnownTags = [] { s_calls.push_back("knownTags"); };
    }

    CoATest::Bytes Image(uint32_t id, uint32_t dwords) {
        CoATest::Bytes b;
        b.U32(id);

        for (uint32_t i = 1; i < dwords; ++i) {
            b.U32(i);
        }

        return b;
    }

    using V = std::vector<std::string>;
}

// Every PatchData opcode with a callback stores its row and reaches the callback, as registered by Init.
COA_TEST(PatchData_WorldRegistrationsRebuild) {
    Setup();
    Feed(0x693, Image(9001, 3));
    Feed(0x6EC, Image(9001, 3));
    Feed(0x6C0, Image(9001, 0x1D).Str("a").Str("b").Str("c").Str("d").Str("e").Str("f"));
    Feed(0x6FC, Image(9001, 4));
    Feed(0x6FB, CoATest::Bytes().U32(9001).U32(0).Str("tex.blp"));
    CHECK((s_calls == V{ "itemAppearances", "itemSetAppearances", "tagTypes", "geosets:9001", "gameObjects" }));
    CHECK(CoA::Dbc::Get("DBFilesClient\\ItemAppearances.dbc").Row(9001) != nullptr);
    CHECK(CoA::Dbc::Get("DBFilesClient\\ItemSetAppearances.dbc").Row(9001) != nullptr);
    CHECK(CoA::Dbc::Get("DBFilesClient/SpellTagTypes.dbc").Row(9001) != nullptr);
    CHECK(CoA::Dbc::Get("DBFilesClient\\CreatureDisplayInfoGeosetData.dbc").Row(9001) != nullptr);
    CoA::Dbc::Table& go = CoA::Dbc::Get("DBFilesClient/GameObjectDisplayInfoAddon.dbc");
    CHECK(go.Row(9001) && strcmp(go.Str(go.Row(9001), 4), "tex.blp") == 0);

    // 0x692 runs its callback only for a new row.
    s_calls.clear();
    Feed(0x692, Image(9002, 0x11).Str("look"));
    Feed(0x692, Image(9002, 0x11).Str("look2"));
    CHECK(s_calls == V{ "outfits" });
}

// 0x5F4: an overwrite rebuilds the spell -> row map, a new row does not.
COA_TEST(PatchData_SpellCustomAttr) {
    Setup();
    Feed(0x5F4, Image(9101, 0xB));
    CHECK(s_calls.empty());
    CHECK(CoA::Dbc::Get("DBFilesClient\\SpellCustomAttr.dbc").Row(9101) != nullptr);
    Feed(0x5F4, Image(9101, 0xB));
    CHECK(s_calls == V{ "customAttr" });
}

// 0x6C1: {id, spell, tag}; an overwrite drops the old tag first; then the known tags and SPELL_TAGS_CHANGED.
COA_TEST(PatchData_SpellTags) {
    Setup();
    CoATest::RecordSignals();
    Feed(0x6C1, CoATest::Bytes().U32(9201).U32(100).U32(5));
    Feed(0x6C1, CoATest::Bytes().U32(9201).U32(100).U32(6));
    CoATest::StopRecordingSignals();
    CHECK((s_calls == V{ "tag+100/5", "knownTags", "tag-100/5", "tag+100/6", "knownTags" }));
    CHECK((CoATest::Signals() == V{ "SPELL_TAGS_CHANGED", "SPELL_TAGS_CHANGED" }));
    CHECK(CoA::Dbc::Table::U32(CoA::Dbc::Get("DBFilesClient/SpellTags.dbc").Row(9201), 8) == 6);
}

// 0x686: a new row is indexed by its spell; an overwrite changes the row in place.
COA_TEST(PatchData_SpellAddon) {
    Setup();
    Feed(0x686, CoATest::Bytes().U32(9301).U32(777).Raw(std::vector<uint8_t>(0x54, 1).data(), 0x54));
    const uint8_t* row = CoA::PatchData::SpellAddonForSpell(777);
    CHECK(row && CoA::Dbc::Table::U32(row, 0) == 9301);
    Feed(0x686, CoATest::Bytes().U32(9301).U32(777).Raw(std::vector<uint8_t>(0x54, 2).data(), 0x54));
    CHECK(CoA::PatchData::SpellAddonForSpell(777)[8] == 2);
    CHECK(CoA::PatchData::SpellAddonForSpell(778) == nullptr);
}

// 0x71F / 0x723: field by field (U = u32, S = C string).
COA_TEST(PatchData_FieldPatches) {
    Setup();
    Feed(0x71F, CoATest::Bytes().U32(9401).U32(2).Str("a").Str("b").Str("c").Str("d").U32(7).U32(8).U32(9).U32(10));
    CoA::Dbc::Table& t = CoA::Dbc::Get("DBFilesClient/ExtraActionButtons.dbc");
    const uint8_t* row = t.Row(9401);
    CHECK(row && strcmp(t.Str(row, 0xC), "b") == 0 && CoA::Dbc::Table::U32(row, 0x24) == 10);

    Feed(0x723, CoATest::Bytes().U32(9402).Str("x").U32(1).U32(2).U32(3).U32(4).Str("y"));
    CoA::Dbc::Table& c = CoA::Dbc::Get("DBFilesClient/CharacterAdvancementClassTypes.dbc");
    const uint8_t* crow = c.Row(9402);
    CHECK(crow && strcmp(c.Str(crow, 4), "x") == 0 && strcmp(c.Str(crow, 0x18), "y") == 0);
}

// The plain and sized registrations store their rows.
COA_TEST(PatchData_PlainAndSized) {
    Setup();
    Feed(0x568, Image(9501, 4));
    Feed(0x681, Image(9501, 5));
    Feed(0x68C, Image(9501, 5));
    Feed(0x68D, Image(9501, 0xF));
    Feed(0x678, Image(9501, 4).U32(0).Str("story"));
    Feed(0x6ED, Image(9501, 5).Str("detail"));
    Feed(0x579, Image(9501, 7).Name("lfg"));
    CHECK(CoA::Dbc::Get("DBFilesClient\\DungeonEncounterExtra.dbc").Row(9501) != nullptr);
    CHECK(CoA::Dbc::Get("DBFilesClient\\ZoneLightPoint.dbc").Row(9501) != nullptr);
    CHECK(CoA::Dbc::Get("DBFilesClient\\ManastormPlayerGroupModifiers.dbc").Row(9501) != nullptr);
    CHECK(CoA::Dbc::Get("DBFilesClient\\ManastormModifiers.dbc").Row(9501) != nullptr);
    CoA::Dbc::Table& lfg = CoA::Dbc::Get("DBFilesClient\\LFGActivities.dbc");
    CHECK(lfg.Row(9501) && strcmp(lfg.Str(lfg.Row(9501), 0x10), "lfg") == 0);
}
