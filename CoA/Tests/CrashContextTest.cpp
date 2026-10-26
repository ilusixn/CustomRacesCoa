#include "CoA/Source/Modules/CrashContext.h"
#include "CoA/Source/Runtime/ClientHooks.h"
#include "CoA/Tests/TestMain.h"
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

namespace {
    struct FatalCall {
        std::string message;
        std::string file;
        int32_t line;
    };

    std::vector<FatalCall> s_fatals;
    bool s_memoryOk = true;
    bool s_hd = false;
    int32_t s_opcode = 0;
    std::string s_trace;
    bool s_mapExists = true;

    void Fatal(const char* message, const char* file, int32_t line) {
        s_fatals.push_back({ message ? message : "<null>", file ? file : "<null>", line });
    }

    bool Memory(uint64_t& workingSet, uint64_t& peak) {
        workingSet = 300ull << 20;
        peak = (512ull << 20) + 12345;
        return s_memoryOk;
    }

    bool Hd() { return s_hd; }
    int32_t Opcode() { return s_opcode; }
    const char* Connection() { return "logon.example:3724"; }
    std::string Trace() { return s_trace; }
    bool MapExists(uint32_t map) { return s_mapExists && map != 0xDEAD; }

    void Start() {
        CoA::CrashContext::Reset();
        CoA::ClientHooks::Reset();
        s_fatals.clear();
        s_memoryOk = true;
        s_hd = false;
        s_opcode = 0;
        s_trace.clear();
        s_mapExists = true;
        CoA::CrashContext::Deps& d = CoA::CrashContext::Dependencies();
        d.fatal = Fatal;
        d.memory = Memory;
        d.hdPatch = Hd;
        d.opcode = Opcode;
        d.connection = Connection;
        d.luaTrace = Trace;
        d.mapExists = MapExists;
    }

    bool Has(const std::string& report, const std::string& line) {
        return report.find(line + "\r\n") != std::string::npos;
    }

    std::string Pointer(const void* p) {
        std::stringstream s;
        s << p;
        return s.str();
    }
}

// FUN_1019e5f0: the block the crash-text builder appends, every line at its empty default.
COA_TEST(CrashContext_ReportDefaults) {
    Start();
    const std::string expected = std::string("\r\n----------------------------------------\r\n") +
        "     Ascension Debug\r\n" +
        "----------------------------------------\r\n" +
        "\r\n" +
        "DLL Version: 792084a2572c06bfd865feaf553c001f2bdd60fd\r\n" +
        "Process Memory Usage: 300MB (Peak: 512MB)\r\n" +
        "Assert Info: \r\n" +
        "HD Patch: 0\r\n" +
        "Last DetailDoodad IDX: \r\n" +
        "Last FrameScript_Execute: \r\n" +
        "Last FrameScript_SignalEvent: \r\n" +
        "Last Loaded Map Object Name: \r\n" +
        "Last Loaded M2 Name (CM2Cache): \r\n" +
        "Last Loaded M2 Name (CM2Shared): \r\n" +
        "Last Loaded M2 (Skin) Name: \r\n" +
        "Last M2 Animate Name: \r\n" +
        "Last M2 Animate MT Name: \r\n" +
        "Last M2 ProcessCallbacks Name: \r\n" +
        "Last M2 Animate Particle Name: \r\n" +
        "Last M2 Scene Render Name: \r\n" +
        "Last M2 SetVerticies Name: \r\n" +
        "Last M2 WaitForLoad Name: \r\n" +
        "Last M2 Is Drawable: \r\n" +
        "Last M2 Shadow Map Name: \r\n" +
        "Last M2 GetCollisionFacets: \r\n" +
        "Last M2 GetSequenceInfo: \r\n" +
        "Last Received Opcode: 0\r\n" +
        "Last Spell Tooltip ID: \r\n" +
        "Last Spell Table Lookup ID: \r\n" +
        "Last Loaded Map ID: \r\n" +
        "Last Async Texture: \r\n" +
        "Last Unit::GetModelData ID: 0\r\n" +
        "Last TextureCreate: \r\n" +
        "Last BLP: \r\n" +
        "Last GameObject::CGGameObject_C__ModelLoaded Entry: 0\r\n" +
        "Last GameObject::CGGameObject_C__ModelLoaded Model Name: \r\n" +
        "Last ClientDB::GetRow: DB = " + Pointer(nullptr) + " ID = 0\r\n" +
        "Last LoadGameTable = \r\n" +
        "Connection: logon.example:3724\r\n" +
        "\r\n";
    CHECK(CoA::CrashContext::Report() == expected);

    // No memory counters: the line is left out.
    s_memoryOk = false;
    CHECK(CoA::CrashContext::Report().find("Process Memory Usage") == std::string::npos);
}

COA_TEST(CrashContext_RecordersFillTheirLines) {
    Start();
    using F = CoA::CrashContext::Field;
    CoA::CrashContext::Record(F::FrameScriptExecute, "print(1)");
    CoA::CrashContext::Record(F::MapObjectName, "World\\wmo\\a.wmo");
    CoA::CrashContext::Record(F::M2Shared, "Creature\\Bear\\Bear.m2");
    CoA::CrashContext::Record(F::M2Skin, "Creature\\Bear\\Bear.m2");
    CoA::CrashContext::Record(F::M2Animate, "a.m2");
    CoA::CrashContext::Record(F::M2AnimateMT, "b.m2");
    CoA::CrashContext::Record(F::M2ProcessCallbacks, "c.m2");
    CoA::CrashContext::Record(F::M2AnimateParticle, "d.m2");
    CoA::CrashContext::Record(F::M2SceneRender, "e.m2");
    CoA::CrashContext::Record(F::M2WaitForLoad, "f.m2");
    CoA::CrashContext::Record(F::M2CollisionFacets, "g.m2");
    CoA::CrashContext::Record(F::M2SequenceInfo, "h.m2");
    CoA::CrashContext::Record(F::TextureCreate, "Interface\\x.blp");
    CoA::CrashContext::Record(F::Blp, "Interface\\y.blp");
    CoA::CrashContext::Record(F::GameTable, "gtCombatRatings");
    CoA::CrashContext::RecordNumber(F::SpellTooltip, 133);
    CoA::CrashContext::RecordNumber(F::SpellTableLookup, 4294967295u);
    CoA::CrashContext::RecordModelDataEntry(1234);
    CoA::CrashContext::RecordGameObject(181068, "GameObject\\Door.m2");
    int table = 0;
    CoA::CrashContext::RecordClientDbRow(&table, -7);
    s_hd = true;
    s_opcode = 0x1F6;
    CoA::ClientHooks::AllowSignalEvent(5, reinterpret_cast<lua_State*>(1), 0, true, true, "PLAYER_LOGIN");

    const std::string r = CoA::CrashContext::Report();
    CHECK(Has(r, "Last FrameScript_Execute: print(1)"));
    CHECK(Has(r, "Last FrameScript_SignalEvent: PLAYER_LOGIN"));
    CHECK(Has(r, "Last Loaded Map Object Name: World\\wmo\\a.wmo"));
    CHECK(Has(r, "Last Loaded M2 Name (CM2Shared): Creature\\Bear\\Bear.m2"));
    CHECK(Has(r, "Last Loaded M2 (Skin) Name: Creature\\Bear\\Bear.m2"));
    CHECK(Has(r, "Last M2 Animate Name: a.m2"));
    CHECK(Has(r, "Last M2 Animate MT Name: b.m2"));
    CHECK(Has(r, "Last M2 ProcessCallbacks Name: c.m2"));
    CHECK(Has(r, "Last M2 Animate Particle Name: d.m2"));
    CHECK(Has(r, "Last M2 Scene Render Name: e.m2"));
    CHECK(Has(r, "Last M2 WaitForLoad Name: f.m2"));
    CHECK(Has(r, "Last M2 GetCollisionFacets: g.m2"));
    CHECK(Has(r, "Last M2 GetSequenceInfo: h.m2"));
    CHECK(Has(r, "Last TextureCreate: Interface\\x.blp"));
    CHECK(Has(r, "Last BLP: Interface\\y.blp"));
    CHECK(Has(r, "Last LoadGameTable = gtCombatRatings"));
    CHECK(Has(r, "Last Spell Tooltip ID: 133"));
    CHECK(Has(r, "Last Spell Table Lookup ID: 4294967295"));
    CHECK(Has(r, "Last Unit::GetModelData ID: 1234"));
    CHECK(Has(r, "Last GameObject::CGGameObject_C__ModelLoaded Entry: 181068"));
    CHECK(Has(r, "Last GameObject::CGGameObject_C__ModelLoaded Model Name: GameObject\\Door.m2"));
    CHECK(Has(r, "Last ClientDB::GetRow: DB = " + Pointer(&table) + " ID = -7"));
    CHECK(Has(r, "HD Patch: 1"));
    CHECK(Has(r, "Last Received Opcode: 502"));

    // A null name records "" (the original's std::string assignment would fault).
    CoA::CrashContext::Record(F::Blp, nullptr);
    CHECK(Has(CoA::CrashContext::Report(), "Last BLP: "));
}

// 0x824FC0: every IsDrawable records the model; the one from CM2Scene::Animate (return 0x821C77) also feeds the
// "(Last IsDrawable: ...)" suffix of the Animate line.
COA_TEST(CrashContext_IsDrawableFromSceneAnimate) {
    Start();
    CoA::CrashContext::RecordIsDrawable("x.m2", false);
    std::string r = CoA::CrashContext::Report();
    CHECK(Has(r, "Last M2 Is Drawable: x.m2"));
    CHECK(Has(r, "Last M2 Animate Name: "));

    CoA::CrashContext::Record(CoA::CrashContext::Field::M2Animate, "anim.m2");
    CoA::CrashContext::RecordIsDrawable("scene.m2", true);
    r = CoA::CrashContext::Report();
    CHECK(Has(r, "Last M2 Is Drawable: scene.m2"));
    CHECK(Has(r, "Last M2 Animate Name: anim.m2 (Last IsDrawable: scene.m2)"));

    CoA::CrashContext::RecordIsDrawable("other.m2", false);
    CHECK(Has(CoA::CrashContext::Report(), "Last M2 Animate Name: anim.m2 (Last IsDrawable: scene.m2)"));
}

// FUN_10112f70: the message becomes "Assert Info", then the client's fatal error 0x8C51D0(message, file, line).
COA_TEST(CrashContext_AssertRecordsThenRaises) {
    Start();
    CoA::CrashContext::Assert("WorldRender::ToDrawPosition requires an active camera", nullptr, 0);
    CHECK(s_fatals.size() == 1);

    if (s_fatals.size() == 1) {
        CHECK(s_fatals[0].message == "WorldRender::ToDrawPosition requires an active camera");
        CHECK(s_fatals[0].file == "<null>");
        CHECK(s_fatals[0].line == 0);
    }

    CHECK(Has(CoA::CrashContext::Report(), "Assert Info: WorldRender::ToDrawPosition requires an active camera"));
}

// 0x4D7750: the map is recorded, and one Map.dbc does not have is a fatal assert (before the original runs).
COA_TEST(CrashContext_LoadMap) {
    Start();
    CoA::CrashContext::BeforeLoadMap(571);
    CHECK(s_fatals.empty());
    CHECK(Has(CoA::CrashContext::Report(), "Last Loaded Map ID: 571"));

    CoA::CrashContext::BeforeLoadMap(0xDEAD);
    CHECK(s_fatals.size() == 1);

    if (!s_fatals.empty()) {
        CHECK(s_fatals[0].message == "MapID = 57005 does not exist in Map.dbc");
    }

    const std::string r = CoA::CrashContext::Report();
    CHECK(Has(r, "Last Loaded Map ID: 57005"));
    CHECK(Has(r, "Assert Info: MapID = 57005 does not exist in Map.dbc"));
}

// 0x7B31E0: an index inside the doodad list is recorded and the original runs; one past it is a fatal assert naming the
// ADT and chunk, the index is not recorded and the original is not called.
COA_TEST(CrashContext_DetailDoodad) {
    Start();
    CHECK(CoA::CrashContext::DetailDoodadValid(3, 4));
    CHECK(Has(CoA::CrashContext::Report(), "Last DetailDoodad IDX: 3"));
    CHECK(!CoA::CrashContext::DetailDoodadValid(4, 4));
    CHECK(s_fatals.empty());
    CHECK(Has(CoA::CrashContext::Report(), "Last DetailDoodad IDX: 3"));

    CoA::CrashContext::DetailDoodadInvalid(4, 32, 48, 7, 15);
    CHECK(s_fatals.size() == 1);

    if (!s_fatals.empty()) {
        CHECK(s_fatals[0].message == "ADT: [32,48] Chunk: [7,15] has incorrect GroundEffectDoodad.dbc ID in MCLY.effectId: 4");
    }
}

// The Lua section follows only when the trace has lines; its value list only ever prints the label.
COA_TEST(CrashContext_LuaTrace) {
    Start();
    s_trace = "[0] <anonymous> ([string \"x\"]:1)\n[1] OnClick (Interface\\FrameXML\\A.lua:20)\n";
    const std::string r = CoA::CrashContext::Report();
    const std::string tail = std::string("Connection: logon.example:3724\r\n") +
        "\r\n" +
        "\r\n----------------------------------------\r\n" +
        "      Stack Trace (lua)\r\n" +
        "----------------------------------------\r\n" +
        "\r\n" +
        s_trace + "\r\n" +
        "[Stack] ";
    CHECK(r.size() >= tail.size() && r.compare(r.size() - tail.size(), tail.size(), tail) == 0);
}

// FUN_101a0f00's line: "[level] name (source:line)\n", "<anonymous>" without a name.
COA_TEST(CrashContext_TraceLine) {
    CHECK(CoA::CrashContext::TraceLine(0, nullptr, "[string \"x\"]", 1) == "[0] <anonymous> ([string \"x\"]:1)\n");
    CHECK(CoA::CrashContext::TraceLine(2, "", "a.lua", -1) == "[2] <anonymous> (a.lua:-1)\n");
    CHECK(CoA::CrashContext::TraceLine(3, "OnClick", "b.lua", 20) == "[3] OnClick (b.lua:20)\n");
}
