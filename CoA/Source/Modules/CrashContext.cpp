// AscCrashContext.cpp (see CrashContext.h): the recorders, the assert route and the report.
#include "CoA/Source/Modules/CrashContext.h"
#include "CoA/Source/Runtime/ClientHooks.h"
#include <cstdio>
#include <mutex>
#include <sstream>

namespace {
    CoA::CrashContext::Deps s_deps;
    std::string s_fields[static_cast<int>(CoA::CrashContext::Field::Count)];
    std::string s_assertInfo;          // 0x10BCA210
    std::string s_isDrawableFromScene; // 0x10BCA2A0
    std::string s_isDrawable;          // 0x10BCA288
    const void* s_clientDb = nullptr;  // 0x10BDED2C
    int32_t s_clientDbId = 0;          // 0x10BDED30
    uint32_t s_modelDataEntry = 0;     // 0x10BDED50
    uint32_t s_gameObjectEntry = 0;    // 0x10BDED34
    // The recorders run on the M2 animate threads too (CM2Scene::Animate's AnimateMT); the DLL's std::string assignments
    // race there, ours are serialized.
    std::mutex s_mutex;

    std::string& At(CoA::CrashContext::Field field) {
        return s_fields[static_cast<int>(field)];
    }
}

CoA::CrashContext::Deps& CoA::CrashContext::Dependencies() {
    return s_deps;
}

void CoA::CrashContext::Record(Field field, const char* text) {
    std::lock_guard<std::mutex> lock(s_mutex);
    At(field) = text ? text : "";
}

void CoA::CrashContext::RecordNumber(Field field, uint32_t value) {
    std::lock_guard<std::mutex> lock(s_mutex);
    At(field) = std::to_string(value);
}

void CoA::CrashContext::RecordModelDataEntry(uint32_t entry) {
    std::lock_guard<std::mutex> lock(s_mutex);
    s_modelDataEntry = entry;
}

void CoA::CrashContext::RecordClientDbRow(const void* db, int32_t id) {
    std::lock_guard<std::mutex> lock(s_mutex);
    s_clientDbId = id;
    s_clientDb = db;
}

void CoA::CrashContext::RecordGameObject(uint32_t entry, const char* model) {
    std::lock_guard<std::mutex> lock(s_mutex);
    s_gameObjectEntry = entry;
    At(Field::GameObjectModel) = model ? model : "";
}

// FUN_1019d730.
void CoA::CrashContext::RecordIsDrawable(const char* name, bool fromSceneAnimate) {
    std::lock_guard<std::mutex> lock(s_mutex);
    s_isDrawable = name ? name : "";

    if (fromSceneAnimate) {
        s_isDrawableFromScene = s_isDrawable;
    }
}

void CoA::CrashContext::Assert(const char* message, const char* file, int32_t line) {
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_assertInfo = message ? message : "";
    }

    if (s_deps.fatal) {
        s_deps.fatal(message, file, line);
    }
}

void CoA::CrashContext::BeforeLoadMap(uint32_t map) {
    RecordNumber(Field::MapId, map);

    if (!s_deps.mapExists || !s_deps.mapExists(map)) {
        Assert(("MapID = " + std::to_string(map) + " does not exist in Map.dbc").c_str(), nullptr, 0);
    }
}

bool CoA::CrashContext::DetailDoodadValid(uint32_t index, uint32_t count) {
    if (index < count) {
        RecordNumber(Field::DetailDoodad, index);
        return true;
    }

    return false;
}

void CoA::CrashContext::DetailDoodadInvalid(uint32_t index, int32_t adtX, int32_t adtY, int32_t chunkX, int32_t chunkY) {
    char text[256];
    snprintf(text, sizeof(text), "ADT: [%d,%d] Chunk: [%d,%d] has incorrect GroundEffectDoodad.dbc ID in MCLY.effectId: %u", adtX,
        adtY, chunkX, chunkY, index);
    Assert(text, nullptr, 0);
}

std::string CoA::CrashContext::TraceLine(int32_t level, const char* name, const char* source, int32_t line) {
    if (!name || !*name) {
        name = "<anonymous>";
    }

    std::stringstream out;
    out << "[" << level << "] " << name << " (" << (source ? source : "") << ":" << line << ")\n";
    return out.str();
}

static std::string BuildReport();

std::string CoA::CrashContext::Report() {
    // A crash inside a recorder leaves the lock held on this thread: the report is built without it then.
    std::unique_lock<std::mutex> lock(s_mutex, std::try_to_lock);
    return BuildReport();
}

// The crash path: the last client DB row (0x65C290's globals) is stored under the same try_lock as the report, so a
// lock left held by a crash inside a recorder is never taken again on this thread (review I1).
std::string CoA::CrashContext::ReportWithClientDbRow(const void* db, int32_t id) {
    std::unique_lock<std::mutex> lock(s_mutex, std::try_to_lock);
    s_clientDbId = id;
    s_clientDb = db;
    return BuildReport();
}

std::mutex& CoA::CrashContext::LockForTests() {
    return s_mutex;
}

static std::string BuildReport() {
    using namespace CoA::CrashContext;
    std::string text;
    auto line = [&text](const std::string& s) {
        text += s;
        text += "\r\n";
    };

    text += "\r\n----------------------------------------\r\n";
    text += "     Ascension Debug\r\n";
    text += "----------------------------------------\r\n";
    text += "\r\n";
    line("DLL Version: 792084a2572c06bfd865feaf553c001f2bdd60fd");
    uint64_t workingSet = 0;
    uint64_t peak = 0;

    if (s_deps.memory && s_deps.memory(workingSet, peak)) {
        line("Process Memory Usage: " + std::to_string(workingSet >> 20) + "MB (Peak: " + std::to_string(peak >> 20) + "MB)");
    }

    const char* signal = CoA::ClientHooks::LastSignalEvent();
    const std::string& animate = At(Field::M2Animate);
    line("Assert Info: " + s_assertInfo);
    line("HD Patch: " + std::to_string(s_deps.hdPatch && s_deps.hdPatch() ? 1u : 0u));
    line("Last DetailDoodad IDX: " + At(Field::DetailDoodad));
    line("Last FrameScript_Execute: " + At(Field::FrameScriptExecute));
    line("Last FrameScript_SignalEvent: " + std::string(signal ? signal : ""));
    line("Last Loaded Map Object Name: " + At(Field::MapObjectName));
    line("Last Loaded M2 Name (CM2Cache): " + At(Field::M2Cache));
    line("Last Loaded M2 Name (CM2Shared): " + At(Field::M2Shared));
    line("Last Loaded M2 (Skin) Name: " + At(Field::M2Skin));
    line("Last M2 Animate Name: " + animate + (s_isDrawableFromScene.empty() ? std::string() : " (Last IsDrawable: " + s_isDrawableFromScene + ")"));
    line("Last M2 Animate MT Name: " + At(Field::M2AnimateMT));
    line("Last M2 ProcessCallbacks Name: " + At(Field::M2ProcessCallbacks));
    line("Last M2 Animate Particle Name: " + At(Field::M2AnimateParticle));
    line("Last M2 Scene Render Name: " + At(Field::M2SceneRender));
    line("Last M2 SetVerticies Name: " + At(Field::M2SetVertices));
    line("Last M2 WaitForLoad Name: " + At(Field::M2WaitForLoad));
    line("Last M2 Is Drawable: " + s_isDrawable);
    line("Last M2 Shadow Map Name: " + At(Field::M2ShadowMap));
    line("Last M2 GetCollisionFacets: " + At(Field::M2CollisionFacets));
    line("Last M2 GetSequenceInfo: " + At(Field::M2SequenceInfo));
    line("Last Received Opcode: " + std::to_string(static_cast<uint32_t>(s_deps.opcode ? s_deps.opcode() : 0)));
    line("Last Spell Tooltip ID: " + At(Field::SpellTooltip));
    line("Last Spell Table Lookup ID: " + At(Field::SpellTableLookup));
    line("Last Loaded Map ID: " + At(Field::MapId));
    line("Last Async Texture: " + At(Field::AsyncTexture));
    line("Last Unit::GetModelData ID: " + std::to_string(s_modelDataEntry));
    line("Last TextureCreate: " + At(Field::TextureCreate));
    line("Last BLP: " + At(Field::Blp));
    line("Last GameObject::CGGameObject_C__ModelLoaded Entry: " + std::to_string(s_gameObjectEntry));
    line("Last GameObject::CGGameObject_C__ModelLoaded Model Name: " + At(Field::GameObjectModel));
    {
        std::stringstream db;
        db << s_clientDb;   // operator<<(const void*)
        line("Last ClientDB::GetRow: DB = " + db.str() + " ID = " + std::to_string(s_clientDbId));
    }
    line("Last LoadGameTable = " + At(Field::GameTable));
    const char* connection = s_deps.connection ? s_deps.connection() : nullptr;
    line(std::string("Connection: ") + (connection ? connection : ""));
    text += "\r\n";
    const std::string trace = s_deps.luaTrace ? s_deps.luaTrace() : std::string();

    if (!trace.empty()) {
        text += "\r\n----------------------------------------\r\n";
        text += "      Stack Trace (lua)\r\n";
        text += "----------------------------------------\r\n";
        text += "\r\n";
        line(trace);
        // FUN_101a06e0 only appends once its stream is non-empty, and it starts empty: the label alone.
        text += "[Stack] ";
    }

    return text;
}

void CoA::CrashContext::Reset() {
    for (std::string& s : s_fields) {
        s.clear();
    }

    s_assertInfo.clear();
    s_isDrawableFromScene.clear();
    s_isDrawable.clear();
    s_clientDb = nullptr;
    s_clientDbId = 0;
    s_modelDataEntry = 0;
    s_gameObjectEntry = 0;
    s_deps = Deps();
}
