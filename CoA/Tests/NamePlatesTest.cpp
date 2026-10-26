#include "CoA/Source/Modules/GlobalsG.h"
#include "CoA/Source/Modules/NamePlates.h"
#include "CoA/Source/Runtime/CVars.h"
#include "CoA/Source/Runtime/ClientHooks.h"
#include "CoA/Source/Runtime/Lifecycle.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Tests/Support.h"
#include "CoA/Tests/TestMain.h"
#include "Engine/Source/ConsoleBase/ConsoleVar.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace {
    using V = std::vector<std::string>;
    using NP = CoA::NamePlates::Show;

    const uint64_t kPlayer = 0x101;
    const uint64_t kMob = 0x202;
    const uint64_t kMob2 = 0x303;

    bool s_havePlayer = true;
    CoA::NamePlates::Body s_player;
    uint64_t s_target = 0;
    std::map<uint64_t, CoA::NamePlates::Body> s_bodies;
    bool s_blocked = false;
    bool s_haveCamera = true;
    std::map<void*, std::vector<uint32_t>> s_alphas;
    std::map<void*, bool> s_shown;
    float s_bearing = 0.0f;
    std::vector<CoA::NamePlates::Plate> s_plateList;
    std::vector<CoA::NamePlates::Unit> s_units;
    std::vector<std::pair<void*, int32_t>> s_levels;
    int s_reruns = 0;
    std::vector<void*> s_pushed;

    // Plate frames: fake addresses; low byte 0 for kZeroLow.
    char s_frames[0x400];
    void* Frame(int i) { return s_frames + 1 + i * 0x10; }
    void* const kZeroLow = reinterpret_cast<void*>(static_cast<uintptr_t>(0x12345600));
    int s_unitA;
    int s_unitB;
    int s_unitC;

    void Set(const char* name, const char* value) {
        CVar* var = CVar::Lookup(name);
        CHECK(var != nullptr);

        if (var) {
            var->Set(value, true, false, false, false);
        }
    }

    void Start() {
        CoA::CVars::Reset();
        CoA::NamePlates::Reset();
        CoA::GlobalsG::Reset();
        CoA::Lifecycle::Reset();
        CoA::LuaReg::Reset();
        CoA::ClientHooks::leaveWorld.Clear();
        s_havePlayer = true;
        s_player = CoA::NamePlates::Body();
        s_player.guid = kPlayer;
        s_player.height = 2.0f;
        s_target = 0;
        s_bodies.clear();
        s_blocked = false;
        s_haveCamera = true;
        s_alphas.clear();
        s_shown.clear();
        s_bearing = 0.0f;
        s_plateList.clear();
        s_units.clear();
        s_levels.clear();
        s_reruns = 0;
        s_pushed.clear();
        CoA::NamePlates::Deps& d = CoA::NamePlates::Dependencies();
        d.player = [](CoA::NamePlates::Body& out) { out = s_player; return s_havePlayer; };
        d.lockedTarget = [] { return s_target; };
        d.cameraXY = [](float& x, float& y) { x = 100.0f; y = 200.0f; return s_haveCamera; };
        d.unitBody = [](uint64_t guid, uint32_t, CoA::NamePlates::Body& out) {
            auto it = s_bodies.find(guid);

            if (it == s_bodies.end()) {
                return false;
            }

            out = it->second;
            return true;
        };
        d.lineBlocked = [](const float*, const float*) { return s_blocked; };
        d.setAlpha = [](void* frame, uint32_t alpha) { s_alphas[frame].push_back(alpha); };
        d.showPlate = [](void* frame, bool shown) { s_shown[frame] = shown; };
        d.bearing = [](const float*, const float*) { return s_bearing; };
        d.plates = [] { return s_plateList; };
        d.units = [] { return s_units; };
        d.setFrameLevel = [](void* frame, int32_t level) { s_levels.push_back({ frame, level }); };
        d.rerunPlatePass = [] { ++s_reruns; };
        d.tokenGuid = [](const char* token) -> uint64_t {
            return !strcmp(token, "target") ? kMob : !strcmp(token, "focus") ? kMob2 : !strcmp(token, "player") ? kPlayer : 0;
        };
        d.pushPlate = [](lua_State* L, void* frame) {
            s_pushed.push_back(frame);
            lua_pushstring(L, "plate");
        };
        CoA::NamePlates::Init();
        CoA::CVars::RegisterWorld();
    }

    void Stop() {
        CoATest::StopRecordingSignals();
        CoA::NamePlates::Reset();
        CoA::GlobalsG::Reset();
        CoA::Lifecycle::Reset();
        CoA::LuaReg::Reset();
        CoA::ClientHooks::leaveWorld.Clear();
    }

    CoA::NamePlates::Body At(uint64_t guid, float x, float y, float z, float height = 2.0f) {
        CoA::NamePlates::Body b;
        b.guid = guid;
        b.pos[0] = x;
        b.pos[1] = y;
        b.pos[2] = z;
        b.height = height;
        return b;
    }

    struct Spec {
        const char* name;
        const char* def;
        bool callback;
    };

    const Spec kSpecs[] = {
        { "nameplateVerticalOffset", "0.0", true },
        { "nameplateFixedVerticalOffset", "0", false },
        { "nameplateDistance", "1681", true },
        { "nameplateAngle", "0.0", true },
        { "nameplateIntersectOpacity", "0.5", false },
        { "nameplateIntersectUseCamera", "0", false },
        { "nameplateFadeIn", "1", false },
        { "nameplateShowNeutral", "1", true },
        { "nameplateShowDead", "0", true },
        { "nameplateShowPersonal", "0", false },
        { "nameplatePersonalPosition", "2", true },
        { "nameplateInCombatOnly", "0", false },
    };
}

// FUN_102bf920: twelve world CVars, no help, flags 0x21, category 4, each with its slot (checked against the DLL).
COA_TEST(NamePlates_TwelveWorldCVars) {
    Start();
    size_t count = 0;
    const CoA::CVarEntry* world = CoA::CVars::World(count);
    CHECK(count == sizeof(kSpecs) / sizeof(kSpecs[0]));

    for (size_t i = 0; i < count && i < sizeof(kSpecs) / sizeof(kSpecs[0]); ++i) {
        CHECK(strcmp(world[i].name, kSpecs[i].name) == 0);
        CHECK(strcmp(world[i].defaultValue, kSpecs[i].def) == 0);
        CHECK(world[i].flags == 0x21);
        CHECK(world[i].category == 4);
        CHECK((world[i].callback != nullptr) == kSpecs[i].callback);
        CHECK(world[i].help == nullptr && !world[i].setCommand && world[i].slot != nullptr);
        CHECK(CVar::Lookup(kSpecs[i].name) != nullptr);
    }

    // The installer's patches are applied with the module.
    CHECK(!CoA::ClientHooks::options.namePlateTargetLevel);
    CHECK(CoA::ClientHooks::options.namePlatesAnchored);
    CHECK(CoA::ClientHooks::options.interactIconRaised);
    Stop();
}

// The callbacks: at their defaults the plate offset is 0 (not the client's 0.667) and the range 60 yards (1681 clamps).
COA_TEST(NamePlates_CVarCallbacks) {
    Start();
    Set("nameplateVerticalOffset", "0.0");
    Set("nameplateDistance", "1681");
    CHECK(CoA::NamePlates::VerticalOffset() == 0.0f);
    CHECK(CoA::GlobalsG::NamePlateRangeSq() == 3600.0f);
    Set("nameplateVerticalOffset", "0.25");
    CHECK(CoA::NamePlates::VerticalOffset() == 0.25f);
    Set("nameplateDistance", "2");
    CHECK(CoA::GlobalsG::NamePlateRangeSq() == 25.0f);
    Set("nameplateDistance", "30");
    CHECK(CoA::GlobalsG::NamePlateRangeSq() == 900.0f);
    Set("nameplateDistance", "x");                       // atof 0 -> 5 yards
    CHECK(CoA::GlobalsG::NamePlateRangeSq() == 25.0f);

    const int before = s_reruns;
    Set("nameplateAngle", "0.5");
    CHECK(s_reruns == before + 1);

    Set("nameplateShowNeutral", "0");
    CHECK(!CoA::NamePlates::ShowNeutral());
    Set("nameplateShowNeutral", "7");
    CHECK(CoA::NamePlates::ShowNeutral());

    Set("nameplateShowDead", "1");
    CHECK(CoA::ClientHooks::options.namePlatesShowDead);
    Set("nameplateShowDead", "01");                      // strcmp, not a number
    CHECK(!CoA::ClientHooks::options.namePlatesShowDead);

    Set("nameplatePersonalPosition", "0");
    CHECK(CoA::NamePlates::PersonalPosition() == 1);
    Set("nameplatePersonalPosition", "5");
    CHECK(CoA::NamePlates::PersonalPosition() == 2);
    Set("nameplatePersonalPosition", "1");
    CHECK(CoA::NamePlates::PersonalPosition() == 1);
    Stop();
}

// Ids are the lowest free from 1; ADDED fires once per plate on the next tick (with a player), REMOVED once when the
// plate goes; tokens "nameplateN" resolve while registered.
COA_TEST(NamePlates_RegistryEventsAndTokens) {
    Start();
    CoATest::RecordSignals();
    CoA::NamePlates::AfterPlateCreate(&s_unitA, 0);
    CoA::NamePlates::AfterPlateCreate(&s_unitB, 0);
    CHECK(CoA::NamePlates::IdForUnit(&s_unitA) == 1);
    CHECK(CoA::NamePlates::IdForUnit(&s_unitB) == 2);
    CHECK(CoATest::Signals().empty());
    s_havePlayer = false;
    CoA::NamePlates::Tick();                             // no player: nothing, the queue stays
    CHECK(CoATest::Signals().empty());
    s_havePlayer = true;
    CoA::NamePlates::Tick();
    CHECK((CoATest::Signals() == V{ "NAME_PLATE_UNIT_ADDED:nameplate1", "NAME_PLATE_UNIT_ADDED:nameplate2" }));
    CoA::NamePlates::Tick();
    CHECK(CoATest::Signals().size() == 2);

    CHECK(CoA::NamePlates::UnitForToken("nameplate1") == &s_unitA);
    CHECK(CoA::NamePlates::UnitForToken("nameplate2") == &s_unitB);
    CHECK(CoA::NamePlates::UnitForToken("NAMEPLATE1") == nullptr);     // case-sensitive find
    CHECK(CoA::NamePlates::UnitForToken("nameplate3") == nullptr);
    CHECK(CoA::NamePlates::UnitForToken("nameplate") == nullptr);
    CHECK(CoA::NamePlates::UnitForToken("nameplatex") == nullptr);     // stoul would throw: not a plate
    CHECK(CoA::NamePlates::UnitForToken("nameplate0x2") == &s_unitB); // base 0
    CHECK(CoA::NamePlates::UnitForToken("nameplate1target") == &s_unitA);
    CHECK(CoA::NamePlates::UnitForToken("xnameplate1") == nullptr);    // "e1" after the first nine
    CHECK(CoA::NamePlates::UnitForToken(nullptr) == nullptr);

    CoATest::Signals().clear();
    CoA::NamePlates::AfterPlateRelease(&s_unitA, 0);
    CHECK((CoATest::Signals() == V{ "NAME_PLATE_UNIT_REMOVED:nameplate1" }));
    CoA::NamePlates::AfterPlateRelease(&s_unitA, 0);                  // gone: nothing
    CHECK(CoATest::Signals().size() == 1);
    CHECK(CoA::NamePlates::UnitForToken("nameplate1") == nullptr);
    // The freed id is reused.
    CoA::NamePlates::AfterPlateCreate(&s_unitC, 0);
    CHECK(CoA::NamePlates::IdForUnit(&s_unitC) == 1);
    Stop();
}

// Only the low byte of the plate pointer is tested (both detours).
COA_TEST(NamePlates_LowByteTests) {
    Start();
    CoA::NamePlates::AfterPlateCreate(&s_unitA, reinterpret_cast<uintptr_t>(Frame(0)));   // had a plate: nothing
    CHECK(CoA::NamePlates::Count() == 0);
    CoA::NamePlates::AfterPlateCreate(&s_unitA, reinterpret_cast<uintptr_t>(kZeroLow));   // low byte 0: added again
    CHECK(CoA::NamePlates::Count() == 1);
    CoA::NamePlates::AfterPlateCreate(&s_unitA, reinterpret_cast<uintptr_t>(kZeroLow));
    CHECK(CoA::NamePlates::Count() == 2);
    CoA::NamePlates::AfterPlateRelease(&s_unitA, reinterpret_cast<uintptr_t>(Frame(0)));  // still a plate: stays
    CHECK(CoA::NamePlates::Count() == 2);
    CoA::NamePlates::AfterPlateRelease(&s_unitA, reinterpret_cast<uintptr_t>(kZeroLow));  // one entry goes
    CHECK(CoA::NamePlates::Count() == 1);
    Stop();
}

// Leaving the world (list 0x10BE28AC) clears the registry; queued ADDED ids are kept and fire on the next tick.
COA_TEST(NamePlates_LeaveWorldClears) {
    Start();
    CoATest::RecordSignals();
    CoA::NamePlates::AfterPlateCreate(&s_unitA, 0);
    CoA::ClientHooks::leaveWorld.Run();
    CHECK(CoA::NamePlates::Count() == 0);
    CHECK(CoA::NamePlates::UnitForToken("nameplate1") == nullptr);
    CoA::NamePlates::AfterPlateRelease(&s_unitA, 0);                  // no REMOVED for a cleared plate
    CHECK(CoATest::Signals().empty());
    CoA::Lifecycle::RunAfterIdle();
    CHECK((CoATest::Signals() == V{ "NAME_PLATE_UNIT_ADDED:nameplate1" }));
    Stop();
}

// FUN_102bf780: one argument; the first enumerated unit with the token's GUID decides.
COA_TEST(NamePlates_GetNamePlateForUnit) {
    Start();
    lua_State* L = CoATest::NewState(false);
    CoA::NamePlates::Unit a;
    a.unit = &s_unitA;
    a.guid = kMob;
    a.plate = Frame(1);
    CoA::NamePlates::Unit b;
    b.unit = &s_unitB;
    b.guid = kMob2;
    b.plate = nullptr;
    s_units = { a, b };
    CHECK((CoATest::Call(L, "", "GetNamePlateForUnit", { "s:target" }) == V{}));         // not registered
    CoA::NamePlates::AddPlate(&s_unitA);
    CoA::NamePlates::AddPlate(&s_unitB);
    CHECK((CoATest::Call(L, "", "GetNamePlateForUnit", { "s:target" }) == V{ "plate" }));
    CHECK(s_pushed.size() == 1 && s_pushed[0] == Frame(1));
    CHECK((CoATest::Call(L, "", "GetNamePlateForUnit", { "s:focus" }) == V{}));          // no current plate
    CHECK((CoATest::Call(L, "", "GetNamePlateForUnit", { "s:target", "s:x" }) == V{}));  // two arguments: nothing
    CHECK((CoATest::Call(L, "", "GetNamePlateForUnit", {}) == V{}));
    CHECK((CoATest::Call(L, "", "GetNamePlateForUnit", { "s:player" }) == V{}));         // no such unit
    CHECK(s_pushed.size() == 1);
    lua_close(L);
    Stop();
}

// FUN_102c29a0: frame levels 10, 20, ... farthest first, the locked target last; units without a plate skipped.
COA_TEST(NamePlates_TickFrameLevels) {
    Start();
    CoA::NamePlates::Unit near;
    near.guid = kMob;
    near.plate = Frame(1);
    near.pos[0] = 5.0f;
    CoA::NamePlates::Unit far;
    far.guid = kMob2;
    far.plate = Frame(2);
    far.pos[0] = 20.0f;
    CoA::NamePlates::Unit none;
    none.guid = 0x404;
    none.pos[0] = 50.0f;
    CoA::NamePlates::Unit mid;
    mid.guid = 0x505;
    mid.plate = Frame(3);
    mid.pos[1] = 10.0f;
    s_units = { near, none, far, mid };
    CoA::NamePlates::Tick();
    CHECK((s_levels == std::vector<std::pair<void*, int32_t>>{ { Frame(2), 10 }, { Frame(3), 20 }, { Frame(1), 30 } }));
    s_levels.clear();
    s_target = kMob2;                                   // the farthest, but targeted: last
    CoA::NamePlates::Tick();
    CHECK((s_levels == std::vector<std::pair<void*, int32_t>>{ { Frame(3), 10 }, { Frame(1), 20 }, { Frame(2), 30 } }));
    Stop();
}

// FUN_102be570: no target -> distance fade (range - 13.5 .. range) and the intersect opacity; target / own plate 0xFF.
COA_TEST(NamePlates_PlateUpdateAlpha) {
    Start();
    Set("nameplateFadeIn", "1");
    Set("nameplateDistance", "43");
    Set("nameplateIntersectOpacity", "0.5");
    Set("nameplateIntersectUseCamera", "0");
    s_player.height = 0.0f;
    s_bodies[kMob] = At(kMob, 10.0f, 0.0f, 0.0f, 0.0f);
    CoA::NamePlates::AfterPlateUpdate(Frame(1), kMob);
    CHECK((s_alphas[Frame(1)] == std::vector<uint32_t>{ 0xFF }));   // 10 < 29.5
    s_bodies[kMob] = At(kMob, 36.25f, 0.0f, 0.0f, 0.0f);            // halfway through the fade
    CoA::NamePlates::AfterPlateUpdate(Frame(2), kMob);
    CHECK((s_alphas[Frame(2)] == std::vector<uint32_t>{ 127 }));
    s_blocked = true;
    CoA::NamePlates::AfterPlateUpdate(Frame(3), kMob);
    CHECK((s_alphas[Frame(3)] == std::vector<uint32_t>{ 127, 127 }));   // 0.5 * 255
    Set("nameplateFadeIn", "0");
    Set("nameplateIntersectOpacity", "0.2");
    CoA::NamePlates::AfterPlateUpdate(Frame(4), kMob);
    CHECK((s_alphas[Frame(4)] == std::vector<uint32_t>{ 51 }));
    s_blocked = false;
    CoA::NamePlates::AfterPlateUpdate(Frame(5), kMob);
    CHECK(s_alphas[Frame(5)].empty());                                 // stock alpha kept
    CoA::NamePlates::AfterPlateUpdate(Frame(6), 0x999);                // unit gone
    CHECK(s_alphas[Frame(6)].empty());
    CoA::NamePlates::AfterPlateUpdate(Frame(7), kPlayer);              // the player's own plate
    CHECK((s_alphas[Frame(7)] == std::vector<uint32_t>{ 0xFF }));
    s_target = kMob;
    s_blocked = true;
    CoA::NamePlates::AfterPlateUpdate(Frame(8), kMob);                 // a target: no fade, the target's 0xFF
    CHECK((s_alphas[Frame(8)] == std::vector<uint32_t>{ 0xFF }));
    CoA::NamePlates::AfterPlateUpdate(Frame(9), kMob2);                // someone else with a target: stock
    CHECK(s_alphas[Frame(9)].empty());
    Stop();
}

// With nameplateIntersectUseCamera 1 the line starts at the camera's x / y; with fade-in off and no opacity hit the
// stock alpha stays.
COA_TEST(NamePlates_PlateUpdateCamera) {
    Start();
    Set("nameplateFadeIn", "0");
    Set("nameplateIntersectUseCamera", "1");
    s_bodies[kMob] = At(kMob, 10.0f, 0.0f, 0.0f);
    static float s_from[3];
    CoA::NamePlates::Dependencies().lineBlocked = [](const float* from, const float*) {
        memcpy(s_from, from, sizeof(s_from));
        return false;
    };
    s_player.pos[2] = 1.0f;
    CoA::NamePlates::AfterPlateUpdate(Frame(1), kMob);
    CHECK(s_from[0] == 100.0f && s_from[1] == 200.0f && s_from[2] == 2.0f * 0.95f + 1.0f);
    s_haveCamera = false;
    CoA::NamePlates::AfterPlateUpdate(Frame(1), kMob);
    CHECK(s_from[0] == 0.0f && s_from[1] == 0.0f);
    Set("nameplateIntersectUseCamera", "0");
    s_haveCamera = true;
    CoA::NamePlates::AfterPlateUpdate(Frame(1), kMob);
    CHECK(s_from[0] == 0.0f);
    Stop();
}

// FUN_102be880: nameplateAngle 0 (default) leaves the client's pass alone; otherwise distance (the CVar's raw value)
// and angle culling, 3 yards always shown, a plate without a unit shown.
COA_TEST(NamePlates_PlatePassCulling) {
    Start();
    Set("nameplateAngle", "0.0");
    CoA::NamePlates::Plate p1;
    p1.frame = Frame(1);
    p1.guid = kMob;
    CoA::NamePlates::Plate p2;
    p2.frame = Frame(2);
    p2.guid = 0x999;
    s_plateList = { p1, p2 };
    s_bodies[kMob] = At(kMob, 10.0f, 0.0f, 0.0f);
    CoA::NamePlates::AfterPlatePass();
    CHECK(s_shown.empty());

    Set("nameplateAngle", "0.5");
    Set("nameplateDistance", "20");
    s_bearing = 0.4f;
    CoA::NamePlates::AfterPlatePass();
    CHECK(s_shown[Frame(1)] && s_shown[Frame(2)]);
    s_bearing = 0.6f;
    CoA::NamePlates::AfterPlatePass();
    CHECK(!s_shown[Frame(1)]);
    s_bodies[kMob] = At(kMob, 2.0f, 0.0f, 0.0f);                     // within 3 yards
    CoA::NamePlates::AfterPlatePass();
    CHECK(s_shown[Frame(1)]);
    s_bodies[kMob] = At(kMob, 20.0f, 0.0f, 0.0f);                    // at the distance: hidden
    s_bearing = 0.0f;
    CoA::NamePlates::AfterPlatePass();
    CHECK(!s_shown[Frame(1)]);
    // A facing past 3.1415 is wrapped by 6.283.
    s_bodies[kMob] = At(kMob, 10.0f, 0.0f, 0.0f);
    s_player.facing = 6.0f;
    s_bearing = -0.2f;
    CoA::NamePlates::AfterPlatePass();
    CHECK(s_shown[Frame(1)]);
    Stop();
}

// FUN_102c2630.
COA_TEST(NamePlates_BeforeShow) {
    Start();
    Set("nameplateShowPersonal", "0");
    Set("nameplateShowNeutral", "1");
    Set("nameplateInCombatOnly", "0");
    Set("nameplateFixedVerticalOffset", "0");
    Set("nameplateVerticalOffset", "0.5");
    CoA::NamePlates::ShowQuery q;
    CHECK(CoA::NamePlates::BeforeShow(q) == NP::None);                  // plates off
    q.platesShown = true;
    CHECK(CoA::NamePlates::BeforeShow(q) == NP::Original);              // no player
    q.havePlayer = true;
    q.reaction = 3;
    CHECK(CoA::NamePlates::BeforeShow(q) == NP::Original);              // defaults: stock
    q.isActivePlayer = true;
    CHECK(CoA::NamePlates::BeforeShow(q) == NP::Original);
    Set("nameplateShowPersonal", "1");
    Set("nameplatePersonalPosition", "2");
    CHECK(CoA::NamePlates::BeforeShow(q) == NP::PersonalFeet);
    Set("nameplatePersonalPosition", "1");
    CHECK(CoA::NamePlates::BeforeShow(q) == NP::PersonalName);
    q.isActivePlayer = false;
    Set("nameplateShowNeutral", "0");
    CHECK(CoA::NamePlates::BeforeShow(q) == NP::None);                  // neutral, out of combat
    q.isLockedTarget = true;
    CHECK(CoA::NamePlates::BeforeShow(q) == NP::Original);
    q.isLockedTarget = false;
    q.inCombat = true;
    CHECK(CoA::NamePlates::BeforeShow(q) == NP::Original);
    q.inCombat = false;
    q.reaction = 2;
    CHECK(CoA::NamePlates::BeforeShow(q) == NP::Original);
    Set("nameplateInCombatOnly", "1");
    CHECK(CoA::NamePlates::BeforeShow(q) == NP::None);
    q.inCombat = true;
    CHECK(CoA::NamePlates::BeforeShow(q) == NP::Original);
    // Fixed vertical offset: the CVar minus 0.025 per yard.
    CHECK(CoA::NamePlates::VerticalOffset() == 0.5f);
    Set("nameplateFixedVerticalOffset", "1");
    q.unitPos[0] = 10.0f;
    CHECK(CoA::NamePlates::BeforeShow(q) == NP::Original);
    CHECK(CoA::NamePlates::VerticalOffset() == 0.5f - 10.0f * 0.025f);
    Stop();
}

COA_TEST(NamePlates_OwnPlate) {
    CHECK(CoA::NamePlates::IsOwnPlate(kPlayer, kPlayer));
    CHECK(!CoA::NamePlates::IsOwnPlate(kMob, kPlayer));
}
