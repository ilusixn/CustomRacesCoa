#include "CoA/Source/Modules/UnitSelect.h"
#include "CoA/Source/Runtime/CVars.h"
#include "CoA/Source/Runtime/ClientHooks.h"
#include "CoA/Tests/Support.h"
#include "CoA/Tests/TestMain.h"
#include "Engine/Source/ConsoleBase/ConsoleVar.h"
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

namespace {
    using V = std::vector<std::string>;

    std::vector<std::string> s_loaded;
    std::vector<void*> s_released;
    uint32_t s_now = 0;
    float s_turnRate = 0.0f;
    bool s_haveMap = true;
    uint32_t s_mapType = 0;
    std::vector<float> s_pads;
    int s_indicators = 0;
    int s_questDraws = 0;
    int s_ownDraws = 0;
    char s_handles[16];

    void Set(const char* name, const char* value) {
        CVar* var = CVar::Lookup(name);
        CHECK(var != nullptr);

        if (var) {
            var->Set(value, true, false, false, false);
        }
    }

    void Start() {
        CoA::ClientHooks::Reset();
        CoA::CVars::Reset();
        CoA::UnitSelect::Reset();
        CoA::ClientHooks::enterWorld.Clear();
        CoA::ClientHooks::afterClearTrackedObjects.Clear();
        CoA::ClientHooks::initialSpellsAlternate.Clear();
        s_loaded.clear();
        s_released.clear();
        s_now = 0;
        s_turnRate = 0.0f;
        s_haveMap = true;
        s_mapType = 0;
        s_pads.clear();
        s_indicators = s_questDraws = s_ownDraws = 0;
        CoA::UnitSelect::Deps& d = CoA::UnitSelect::Dependencies();
        d.loadTexture = [](const char* path) -> void* {
            s_loaded.push_back(path);
            return s_handles + s_loaded.size();
        };
        d.releaseTexture = [](void* t) { s_released.push_back(t); };
        d.now = [] { return s_now; };
        d.setTrackingTurnRate = [](float r) { s_turnRate = r; };
        d.mapInstanceType = [](uint32_t& type) { type = s_mapType; return s_haveMap; };
        d.setProjectionZPad = [](float pad) { s_pads.push_back(pad); };
        d.colours = [] {
            CoA::UnitSelect::Colours c;
            c.self = 1;
            c.enemy = 2;
            c.friendly = 3;
            return c;
        };
        d.drawIndicators = [] { ++s_indicators; };
        d.drawQuestCircles = [] { ++s_questDraws; };
        d.drawOwnCircle = [] { ++s_ownDraws; };
        CoA::UnitSelect::Init();
        CoA::CVars::RegisterWorld();
    }

    void Stop() {
        CoATest::StopRecordingPackets();
        CoA::UnitSelect::Reset();
        CoA::ClientHooks::enterWorld.Clear();
        CoA::ClientHooks::afterClearTrackedObjects.Clear();
        CoA::ClientHooks::initialSpellsAlternate.Clear();
    }

    struct Spec {
        const char* name;
        const char* def;
        uint32_t category;
    };

    const Spec kSpecs[] = {
        { "aoeRadiusIndicatorSelf", "0", 3 },
        { "aoeRadiusIndicatorEnemy", "0", 3 },
        { "aoeRadiusIndicatorFriendly", "0", 3 },
        { "showQuestUnitCircles", "1", 4 },
        { "ObjectSelectionCircleSelf", "0", 4 },
        { "AutoInteractTurnSpeed", "3.15", 3 },
        { "AutoInteractCursorTime", "0", 3 },
    };
}

// FUN_102daaf0: seven world CVars, flags 1, no callbacks (checked against the DLL); the hook lists; 10.0 at start-up.
COA_TEST(UnitSelect_CVarsAndInstall) {
    Start();
    size_t count = 0;
    const CoA::CVarEntry* world = CoA::CVars::World(count);
    CHECK(count == sizeof(kSpecs) / sizeof(kSpecs[0]));

    for (size_t i = 0; i < count && i < sizeof(kSpecs) / sizeof(kSpecs[0]); ++i) {
        CHECK(strcmp(world[i].name, kSpecs[i].name) == 0);
        CHECK(strcmp(world[i].defaultValue, kSpecs[i].def) == 0);
        CHECK(world[i].flags == 1);
        CHECK(world[i].category == kSpecs[i].category);
        CHECK(world[i].callback == nullptr && world[i].help == nullptr && !world[i].setCommand && world[i].slot != nullptr);
    }

    CHECK(!CoA::ClientHooks::enterWorld.Empty());
    CHECK(!CoA::ClientHooks::afterClearTrackedObjects.Empty());
    CHECK(!CoA::ClientHooks::initialSpellsAlternate.Empty());
    CHECK((s_pads == std::vector<float>{ 10.0f }));
    CHECK(CoA::UnitSelect::ProjectionZPad() == 10.0f);
    Stop();
}

// FUN_102da1c0 at world entry (the old cones released first); FUN_102da2b0 after 0x7460C0.
COA_TEST(UnitSelect_Textures) {
    Start();
    CoA::ClientHooks::enterWorld.Run();
    CHECK((s_loaded == V{ "Interface\\SpellShadow\\ConeSplat_Static_30d_30yd.blp", "Interface\\SpellShadow\\ConeSplat_Static_90d_30yd.blp" }));
    CHECK(s_released.empty());
    void* first30 = CoA::UnitSelect::Cone30();
    void* first90 = CoA::UnitSelect::Cone90();
    CoA::ClientHooks::enterWorld.Run();
    CHECK((s_released == std::vector<void*>{ first30, first90 }));
    s_loaded.clear();
    CoA::UnitSelect::AfterObjectInitialize();
    CHECK((s_loaded == V{ "Textures\\UnitSelectTexture.blp", "Textures\\TrackingTexture.blp", "Textures\\UnitSelectTexture-Quest.blp" }));
    CHECK(*CoA::UnitSelect::UnitSelectTexture() == s_handles + 1);
    CHECK(*CoA::UnitSelect::TrackingTexture() == s_handles + 2);
    CHECK(*CoA::UnitSelect::QuestTexture() == s_handles + 3);
    Stop();
}

// FUN_102da6c0 / FUN_102da340: AutoInteractCursorTime 0 (default) always draws; the turn speed after click-to-move.
COA_TEST(UnitSelect_AutoInteract) {
    Start();
    Set("AutoInteractCursorTime", "0");
    Set("AutoInteractTurnSpeed", "3.15");
    s_now = 100000;
    CHECK(!CoA::UnitSelect::SkipAutoTrackCursor());
    Set("AutoInteractCursorTime", "500");
    CHECK(CoA::UnitSelect::SkipAutoTrackCursor());               // never clicked: long ago
    s_now = 200000;
    CoA::UnitSelect::AfterClickToMove();
    CHECK(s_turnRate == 3.15f);
    s_now = 200500;
    CHECK(!CoA::UnitSelect::SkipAutoTrackCursor());              // 500 < 500 is false
    s_now = 200501;
    CHECK(CoA::UnitSelect::SkipAutoTrackCursor());
    Set("AutoInteractTurnSpeed", "7");
    CoA::UnitSelect::AfterClickToMove();
    CHECK(s_turnRate == 7.0f);

    CoATest::RecordPackets();
    CoA::UnitSelect::BeforeClickToMove(false);
    CHECK(CoATest::SentPackets().empty());
    CoA::UnitSelect::BeforeClickToMove(true);
    CHECK(CoATest::SentPackets().size() == 1);
    CHECK(CoATest::SentPackets().size() == 1 && CoATest::SentPackets()[0] ==
        CoATest::Expected(0x51F, CoATest::Bytes().Str("Ascension").Str("CTM").Str("Click-to-Move called by bot.")));
    Stop();
}

// FUN_102db0d0: 10.0 outside an instance; inside one the stock 2.0 only with an indicator on; no Map row: nothing.
COA_TEST(UnitSelect_RadiusPatch) {
    Start();
    s_pads.clear();
    s_mapType = 0;
    CoA::ClientHooks::InitialSpellsReceived();   // every second packet
    CoA::ClientHooks::InitialSpellsReceived();
    CHECK((s_pads == std::vector<float>{ 10.0f }));
    s_pads.clear();
    s_mapType = 1;
    CoA::UnitSelect::Tick();
    CHECK(s_pads.empty());                        // indicators off: the 10.0 stays
    Set("aoeRadiusIndicatorFriendly", "1");
    CoA::UnitSelect::Tick();
    CHECK((s_pads == std::vector<float>{ 2.0f }));
    CHECK(CoA::UnitSelect::ProjectionZPad() == 2.0f);
    s_mapType = 2;
    Set("aoeRadiusIndicatorFriendly", "0");
    Set("aoeRadiusIndicatorEnemy", "2");           // == 1 only
    s_pads.clear();
    CoA::UnitSelect::Tick();
    CHECK(s_pads.empty());
    s_haveMap = false;
    s_mapType = 0;
    CoA::UnitSelect::Tick();
    CHECK(s_pads.empty());
    CoA::ClientHooks::Reset();
    Stop();
}

COA_TEST(UnitSelect_IndicatorColour) {
    Start();
    Set("aoeRadiusIndicatorSelf", "0");
    Set("aoeRadiusIndicatorEnemy", "0");
    Set("aoeRadiusIndicatorFriendly", "0");
    uint32_t c = 99;
    CHECK(!CoA::UnitSelect::IndicatorColour(true, 4, c));
    CHECK(!CoA::UnitSelect::IndicatorColour(false, 1, c));
    CHECK(!CoA::UnitSelect::IndicatorColour(false, 5, c));
    Set("aoeRadiusIndicatorSelf", "1");
    CHECK(CoA::UnitSelect::IndicatorColour(true, 0, c) && c == 1);
    Set("aoeRadiusIndicatorEnemy", "1");
    CHECK(CoA::UnitSelect::IndicatorColour(false, 2, c) && c == 2);
    CHECK(!CoA::UnitSelect::IndicatorColour(false, 3, c));        // neutral never
    Set("aoeRadiusIndicatorFriendly", "1");
    CHECK(!CoA::UnitSelect::IndicatorColour(false, 3, c));
    CHECK(CoA::UnitSelect::IndicatorColour(false, 4, c) && c == 3);
    Stop();
}

COA_TEST(UnitSelect_Geometry) {
    const float p[3] = { 10.0f, 20.0f, 30.0f };
    float box[6];
    CoA::UnitSelect::Box(p, 2.0f, box);
    CHECK(box[0] == 8.0f && box[1] == 18.0f && box[2] == 26.0f && box[3] == 12.0f && box[4] == 22.0f && box[5] == 34.0f);
    const uint32_t cone[3] = { 1, 0x36, 0 };
    const uint32_t notCone[3] = { 1, 2, 0x17 };
    CHECK(CoA::UnitSelect::IsCone(cone));
    CHECK(!CoA::UnitSelect::IsCone(notCone));
    for (uint32_t t : { 0x18u, 0x68u, 0x3Bu }) {
        const uint32_t only[3] = { 0, 0, t };
        CHECK(CoA::UnitSelect::IsCone(only));
    }
    CHECK(CoA::UnitSelect::Radius(5.0f, 0.5f, 100.0f, 10) == 10.0f);
    CHECK(CoA::UnitSelect::Radius(5.0f, 0.5f, 8.0f, 10) == 8.0f);
    CoA::UnitSelect::ConeBox(p, 4.0f, 0.0f, box);
    const float r1 = 4.0f * 0.725f;
    const float r2 = r1 * 0.725f;
    CHECK(box[0] == (p[0] - r1) + r2 && box[3] == (p[0] + r1) + r2);
    CHECK(box[1] == p[1] - r1 && box[2] == p[2] - (r1 + r1) && box[5] == r1 + r1 + p[2]);
}

// FUN_102dbf60 / FUN_102dbfe0: quest circles only with showQuestUnitCircles (cache cleared first); the own circle at 1.
COA_TEST(UnitSelect_RenderAndQuestCache) {
    Start();
    Set("showQuestUnitCircles", "1");
    Set("ObjectSelectionCircleSelf", "0");
    CoA::ClientHooks::afterClearTrackedObjects.Run();
    CHECK(s_indicators == 1 && s_questDraws == 1 && s_ownDraws == 0);
    Set("showQuestUnitCircles", "0");
    Set("ObjectSelectionCircleSelf", "1");
    CoA::UnitSelect::Render();
    CHECK(s_indicators == 2 && s_questDraws == 1 && s_ownDraws == 1);

    static int s_asks = 0;
    auto relevant = [](uint32_t entry, void*) { ++s_asks; return entry == 7; };
    CHECK(CoA::UnitSelect::QuestCircle(7, relevant, nullptr));
    CHECK(!CoA::UnitSelect::QuestCircle(8, relevant, nullptr));
    CHECK(CoA::UnitSelect::QuestCircle(7, relevant, nullptr));       // cached: not asked again, pushed again
    CHECK(s_asks == 2);
    CHECK((CoA::UnitSelect::QuestEntries() == std::vector<uint32_t>{ 7, 7 }));
    Set("showQuestUnitCircles", "1");
    CoA::UnitSelect::Render();
    CHECK(CoA::UnitSelect::QuestEntries().empty());
    Stop();
}
