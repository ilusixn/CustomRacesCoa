#ifndef COA_SOURCE_MODULES_CRASHCONTEXT_H
#define COA_SOURCE_MODULES_CRASHCONTEXT_H

#include <cstdint>
#include <mutex>
#include <string>

struct CVar;

// AscCrashContext.cpp (installer FUN_1019dc80): the "Ascension Debug" block of the client's crash report. Hooks across
// the client record the last thing each subsystem touched; the crash-text builder hook (0x682200, after
// GxLogCrashInfo) appends them (FUN_1019e5f0). AscLargeAlloc.cpp's 0x4B9760 (TextureCreate) writes "Last
// TextureCreate"; AscItemCollections' 0x81F8F0 the CM2Shared name; AscPatchData's model-loaded hook the game object;
// FrameScript_SignalEvent's hook the signal name (ClientHooks::LastSignalEvent).
//
// FUN_10112f70 (Assert) is the DLL's route to the client's fatal error: it records the message as "Assert Info", then
// calls BattlenetAssert (0x8C51D0).
namespace CoA::CrashContext {
    // The report's string lines, each holding the last value recorded. M2Cache, M2SetVertices, M2ShadowMap and
    // AsyncTexture have no writer in the original either, so they stay empty.
    enum class Field {
        DetailDoodad,         // 0x10BCA3D8  0x7B31E0 CDetailDoodadInst::AddDoodad
        FrameScriptExecute,   // 0x10BCA324  0x819210 FrameScript_Execute (the source)
        MapObjectName,        // 0x10BCA1B0  0x7BD480 CMap::SafeOpen
        M2Cache,              // 0x10BCA120  (no writer)
        M2Shared,             // 0x10BCBCB0  0x81F8F0 CM2Scene::CreateModel (AscItemCollections)
        M2Skin,               // 0x10BCA180  0x838490 CM2Shared::FinishLoadingSkinProfile
        M2Animate,            // 0x10BCA138  0x830DC0 CM2Model::Animate
        M2AnimateMT,          // 0x10BCA150  0x82F0F0 CM2Model::AnimateMT
        M2ProcessCallbacks,   // 0x10BCA168  0x832260 CM2Model::ProcessCallbacks
        M2AnimateParticle,    // 0x10BCA1F8  0x8309C0 CM2Model::AnimateParticleST
        M2SceneRender,        // 0x10BCA360  0x81F700 CM2SceneRender::SetBatchVertices (its current model)
        M2SetVertices,        // 0x10BCA3B4  (no writer)
        M2WaitForLoad,        // 0x10BCA228  0x823ED0 CM2Model::WaitForLoad
        M2ShadowMap,          // 0x10BCA240  (no writer)
        M2CollisionFacets,    // 0x10BCA258  0x82EC30 CM2Model::GetCollisionFacets
        M2SequenceInfo,       // 0x10BCA270  0x82CED0 CM2Model::GetSequenceInfo
        SpellTooltip,         // 0x10BCA198  0x57ABC0 SpellParserParseText (the spell's id)
        SpellTableLookup,     // 0x10BCA384  0x812410 / 0x810ED0 (their third argument)
        MapId,                // 0x10BCA1C8  0x4D7750 ClntObjMgrInitializeStd
        AsyncTexture,         // 0x10BC9018  (no writer)
        TextureCreate,        // 0x10BCB94C  0x4B9760 TextureCreate (AscLargeAlloc)
        Blp,                  // 0x10BCA3FC  0x6AFFD0 CBLPFile::LockChain2
        GameObjectModel,      // 0x10BCA104  the game object's model (AscPatchData)
        GameTable,            // 0x10BCA450  0x7F6A10 ClientGameTableBase::Initialize (its name)
        Count,
    };

    struct Deps {
        void (*fatal)(const char* message, const char* file, int32_t line) = nullptr;   // BattlenetAssert (0x8C51D0)
        bool (*memory)(uint64_t& workingSet, uint64_t& peak) = nullptr;                 // K32GetProcessMemoryInfo
        bool (*hdPatch)() = nullptr;                                                    // AscRealmData::HdPatchLoaded
        int32_t (*opcode)() = nullptr;                                                  // AscProfiler::CurrentOpcode
        const char* (*connection)() = nullptr;                                          // the selected realm's address
        std::string (*luaTrace)() = nullptr;                                            // FUN_101a0f00, TraceLine per level
        bool (*mapExists)(uint32_t map) = nullptr;                                      // Map.dbc has the row
    };

    Deps& Dependencies();

    // The recorders. A null name records "" (the original's std::string assignment would fault).
    void Record(Field field, const char* text);
    void RecordNumber(Field field, uint32_t value);
    // 0x717A20 CGUnit_C::GetModelData: the unit's entry (0x10BDED50).
    void RecordModelDataEntry(uint32_t entry);
    // 0x65C290 WowClientDB::GetRecord: the table and the id (0x10BDED2C / 0x10BDED30).
    void RecordClientDbRow(const void* db, int32_t id);
    // AscPatchData's model-loaded hook (0x10BDED34 / 0x10BCA104).
    void RecordGameObject(uint32_t entry, const char* model);
    // 0x824FC0 CM2Model::IsDrawable; `fromSceneAnimate`: called from CM2Scene::Animate (return address 0x821C77), which
    // also sets the "(Last IsDrawable: ...)" suffix of the Animate line.
    void RecordIsDrawable(const char* name, bool fromSceneAnimate);

    // FUN_10112f70.
    void Assert(const char* message, const char* file, int32_t line);

    // FUN_10114c40, every module's CVar set: name + value over 249 characters is Assert's fatal error, otherwise
    // CVar::Set(value, 1, 0, 0, setNeedsSave). A null CVar or value does nothing.
    void SetCVar(CVar* cvar, const char* value, bool setNeedsSave);

    // FUN_1019daf0 (0x4D7750): records the map; one Map.dbc does not have is a fatal assert, then the original runs.
    void BeforeLoadMap(uint32_t map);
    // FUN_1019d360 (0x7B31E0): an index inside the detail doodad list (s_doodadList) is recorded and the original runs
    // (true); otherwise the caller raises DetailDoodadInvalid and skips the original.
    bool DetailDoodadValid(uint32_t index, uint32_t count);
    void DetailDoodadInvalid(uint32_t index, int32_t adtX, int32_t adtY, int32_t chunkX, int32_t chunkY);

    // FUN_101a0f00's line for one Lua stack level.
    std::string TraceLine(int32_t level, const char* name, const char* source, int32_t line);

    // FUN_1019e5f0: the text appended after the client's crash text.
    std::string Report();
    // The crash handler's entry: records the last client DB row, then Report(), without ever blocking on the lock.
    std::string ReportWithClientDbRow(const void* db, int32_t id);

    // Tests only.
    void Reset();
    std::mutex& LockForTests();
}

#endif
