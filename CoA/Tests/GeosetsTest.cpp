#include "CoA/Source/Modules/Geosets.h"
#include "CoA/Source/Modules/PatchData.h"
#include "CoA/Source/Runtime/ClientHooks.h"
#include "CoA/Source/Runtime/Dbc.h"
#include "CoA/Tests/Support.h"
#include "CoA/Tests/TestMain.h"
#include <cstring>
#include <string>
#include <vector>

namespace {
    using V = std::vector<std::string>;
    const char kTable[] = "DBFilesClient\\CreatureDisplayInfoGeosetData.dbc";

    std::vector<std::vector<uint32_t>> s_rows;
    bool s_exists = true;
    V s_calls;
    char s_models[4];

    bool Read(const char* path, std::vector<uint8_t>& out) {
        if (!s_exists || !strstr(path, "CreatureDisplayInfoGeosetData.dbc")) {
            return false;
        }

        out = CoATest::Wdbc(s_rows, "");
        return true;
    }

    // id, group (+4), geoset (+8), display (+0xC)
    std::vector<uint32_t> Row(uint32_t id, uint32_t group, uint32_t geoset, uint32_t display) {
        return { id, group, geoset, display };
    }

    std::string Name(void* model) {
        return model == nullptr ? "null" : "m" + std::to_string(static_cast<char*>(model) - s_models);
    }

    void Start(const std::vector<std::vector<uint32_t>>& rows) {
        CoA::ClientHooks::Reset();
        CoA::Geosets::Reset();
        CoA::PatchData::Dependencies().geosetsRebuild = nullptr;
        s_rows = rows;
        s_exists = true;
        s_calls.clear();
        CoA::Dbc::SetFileReader(Read);
        CoA::Dbc::Get(kTable).Unload();
        CoA::Geosets::Dependencies().setGeometryVisible = [](void* model, uint32_t first, uint32_t last, uint32_t visible) {
            s_calls.push_back(Name(model) + ":" + std::to_string(first) + "-" + std::to_string(last) + "=" + std::to_string(visible));
        };
        CoA::Geosets::Init();
    }

    void Stop() {
        CoA::Dbc::Get(kTable).Unload();
        CoA::Dbc::SetFileReader(nullptr);
        CoA::Geosets::Reset();
        CoA::PatchData::Dependencies().geosetsRebuild = nullptr;
        CoA::ClientHooks::Reset();
    }
}

// File order, the first record of a display / group wins; geosets outside 1..100 are skipped (a later one is taken).
COA_TEST(Geosets_MapAndLookup) {
    Start({
        Row(10, 2, 5, 900),
        Row(11, 2, 7, 900),     // same display / group: ignored
        Row(5, 3, 0, 900),      // invalid -> skipped
        Row(6, 3, 101, 900),    // invalid -> skipped
        Row(7, 3, 100, 900),    // valid, the first for group 3
        Row(8, 19, 1, 901),
    });
    CHECK(CoA::Geosets::Lookup(900, 2) == 5);
    CHECK(CoA::Geosets::Lookup(900, 3) == 100);
    CHECK(CoA::Geosets::Lookup(900, 4) == -1);
    CHECK(CoA::Geosets::Lookup(901, 19) == 1);
    CHECK(CoA::Geosets::Lookup(902, 2) == -1);
    Stop();
}

// Built once on first use; a 0x6FC patch (PatchData's geosetsRebuild) rebuilds from the table as it now is.
COA_TEST(Geosets_RebuildThroughPatchData) {
    Start({ Row(1, 0, 4, 900) });
    CHECK(CoA::PatchData::Dependencies().geosetsRebuild != nullptr);
    CHECK(CoA::Geosets::Lookup(900, 0) == 4);
    s_rows = { Row(1, 0, 9, 900) };
    CoA::Dbc::Get(kTable).Unload();
    CHECK(CoA::Geosets::Lookup(900, 0) == 4);   // the built map
    if (CoA::PatchData::Dependencies().geosetsRebuild) {
        CoA::PatchData::Dependencies().geosetsRebuild(nullptr);
    }

    CHECK(CoA::Geosets::Lookup(900, 0) == 9);
    Stop();
}

// A missing table: no entries, Apply never touches the model.
COA_TEST(Geosets_MissingTable) {
    Start({});
    s_exists = false;
    CoA::Dbc::Get(kTable).Unload();
    CHECK(CoA::Geosets::Lookup(900, 0) == -1);
    CoA::Geosets::SetDisplay(900);
    CoA::Geosets::Apply(s_models + 1);
    CHECK(s_calls.empty());
    Stop();
}

// FUN_101a1510: nothing without a display; per group: hide i*100..i*100+99, show i*100+geoset, groups in order.
COA_TEST(Geosets_Apply) {
    Start({ Row(1, 3, 2, 900), Row(2, 0, 7, 900), Row(3, 19, 100, 900), Row(4, 20, 5, 900) });
    CoA::Geosets::Apply(s_models + 1);
    CHECK(s_calls.empty());
    CoA::Geosets::SetDisplay(900);
    CHECK(CoA::Geosets::Display() == 900);
    CoA::Geosets::Apply(s_models + 1);
    CHECK((s_calls == V{ "m1:0-99=0", "m1:7-7=1", "m1:300-399=0", "m1:302-302=1", "m1:1900-1999=0", "m1:2000-2000=1" }));
    Stop();
}

// The display around a client path is zeroed afterwards, not restored (a nested path leaves the outer one at 0).
COA_TEST(Geosets_DisplayScope) {
    Start({});
    {
        CoA::Geosets::DisplayScope outer(900);
        CHECK(CoA::Geosets::Display() == 900);
        {
            CoA::Geosets::DisplayScope inner(901);
            CHECK(CoA::Geosets::Display() == 901);
        }
        CHECK(CoA::Geosets::Display() == 0);
    }
    CHECK(CoA::Geosets::Display() == 0);
    Stop();
}

// The CreateModel hook point (0x81F8F0, Plan 13 marker): every result, also nullptr, becomes the last model.
COA_TEST(Geosets_CreateModelHookPoint) {
    Start({ Row(1, 1, 3, 900) });
    CHECK(!CoA::ClientHooks::afterCreateModel.Empty());
    CHECK(CoA::Geosets::LastModel() == nullptr);
    CoA::ClientHooks::afterCreateModel.Run(s_models + 2);
    CHECK(CoA::Geosets::LastModel() == s_models + 2);
    // SetUpModel (0x597700) applies its display to the last model, then zeroes it.
    CoA::Geosets::AfterSetUpModel(900);
    CHECK((s_calls == V{ "m2:100-199=0", "m2:103-103=1" }));
    CHECK(CoA::Geosets::Display() == 0);
    CoA::ClientHooks::afterCreateModel.Run(nullptr);
    CHECK(CoA::Geosets::LastModel() == nullptr);
    s_calls.clear();
    CoA::Geosets::AfterSetUpModel(900);   // no last model: nothing
    CHECK(s_calls.empty());
    Stop();
}

// 0x4E7790 after: the model it got, with the display set around the unit path; a null model is skipped.
COA_TEST(Geosets_AfterApplyMonsterGeosets) {
    Start({ Row(1, 4, 1, 900) });
    CoA::Geosets::AfterApplyMonsterGeosets(s_models + 1);
    CHECK(s_calls.empty());   // no display set
    {
        CoA::Geosets::DisplayScope unit(900);
        CoA::Geosets::AfterApplyMonsterGeosets(s_models + 1);
        CoA::Geosets::AfterApplyMonsterGeosets(nullptr);
    }
    CHECK((s_calls == V{ "m1:400-499=0", "m1:401-401=1" }));
    Stop();
}

// 0x4E3CD0 after: the selected entry's pet model with its pet display; nothing without the entry or the model.
COA_TEST(Geosets_AfterShowCharacter) {
    Start({ Row(1, 0, 2, 950) });
    CoA::Geosets::AfterShowCharacter(false, 950, s_models + 3);
    CoA::Geosets::AfterShowCharacter(true, 950, nullptr);
    CHECK(s_calls.empty());
    CoA::Geosets::SetDisplay(77);
    CoA::Geosets::AfterShowCharacter(true, 950, nullptr);
    CHECK(CoA::Geosets::Display() == 77);   // untouched when nothing is applied
    CoA::Geosets::AfterShowCharacter(true, 950, s_models + 3);
    CHECK((s_calls == V{ "m3:0-99=0", "m3:2-2=1" }));
    CHECK(CoA::Geosets::Display() == 0);
    Stop();
}

// 0x101A1440: exact compare with the value (stock compares visible / hidden), only ids in range, true when one changed.
COA_TEST(Geosets_SetRange) {
    struct Section {
        uint16_t id;
        uint8_t pad[0x2E];
    };
    Section sections[4] = {};
    sections[0].id = 100;
    sections[1].id = 105;
    sections[2].id = 199;
    sections[3].id = 200;
    uint32_t flags[4] = { 1, 1, 0, 1 };
    const uint8_t* ids = reinterpret_cast<const uint8_t*>(&sections[0].id);

    CHECK(CoA::Geosets::SetRange(4, ids, sizeof(Section), flags, 100, 199, 0));
    CHECK(flags[0] == 0 && flags[1] == 0 && flags[2] == 0 && flags[3] == 1);
    CHECK(!CoA::Geosets::SetRange(4, ids, sizeof(Section), flags, 100, 199, 0));
    CHECK(CoA::Geosets::SetRange(4, ids, sizeof(Section), flags, 105, 105, 1));
    CHECK(flags[1] == 1);
    CHECK(CoA::Geosets::SetRange(4, ids, sizeof(Section), flags, 105, 105, 2));   // 1 -> 2: the original's != compare
    CHECK(flags[1] == 2);
    CHECK(!CoA::Geosets::SetRange(0, ids, sizeof(Section), flags, 0, 0xFFFFFFFF, 7));
}
