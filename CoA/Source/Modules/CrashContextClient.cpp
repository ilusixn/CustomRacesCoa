// AscCrashContext.cpp's client half (and AscLargeAlloc.cpp's 0x4B9760): the dependencies, the Engine recorder pointers
// (Engine does not link CoA) and the crash-text writer. The WoW sites call CoA::CrashContext directly.
#include "CoA/Source/Modules/CrashContextClient.h"
#include "CoA/Source/Modules/CrashContext.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Modules/Profiler.h"
#include "CoA/Source/Modules/RealmData.h"
#include "Engine/Source/BLPFile/blp.h"
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/Model2/M2Model.h"
#include "Engine/Source/Services/Texture.h"
#include "Engine/Source/Storm/H/storm.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Source/DB/WowClientDB.h"
#include "WoW/Source/WowServices/BattlenetLogin.h"
#include "WoW/Source/WowSvcs/WowSvcsClient/ClientServices.h"
#include <Windows.h>
#include <psapi.h>
#include <string>

namespace {
    using Field = CoA::CrashContext::Field;

    void Fatal(const char* message, const char* file, int32_t line) {   // 0x8C51D0
        BattlenetAssert(message, file, line);
    }

    bool Memory(uint64_t& workingSet, uint64_t& peak) {
        PROCESS_MEMORY_COUNTERS pmc = {};

        if (!K32GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
            return false;
        }

        workingSet = pmc.WorkingSetSize;
        peak = pmc.PeakWorkingSetSize;
        return true;
    }

    const char* Connection() {   // 0xC79C9E: the selected realm's address
        return ClientServices::m_selectRealmInfo.m_realmAddress;
    }

    // FUN_101a0f00 on the FrameScript state: lua_getstack / lua_getinfo("nSl") per level.
    std::string LuaTrace() {
        lua_State* L = FrameScript_GetContext();

        if (!L) {
            return std::string();
        }

        std::string out;
        lua_Debug ar = {};

        for (int level = 0; lua_getstack(L, level, &ar); ++level) {
            if (lua_getinfo(L, "nSl", &ar)) {
                out += CoA::CrashContext::TraceLine(level, ar.name, ar.short_src, ar.currentline);
            }
        }

        return out;
    }

    // FUN_100b1c60 on Map.dbc's index, without GetRecord (which would record the lookup).
    bool MapExists(uint32_t map) {
        const uint32_t minId = static_cast<uint32_t>(g_mapDB.m_minID);
        const uint32_t maxId = static_cast<uint32_t>(g_mapDB.m_maxID);
        return g_mapDB.m_recordsById && map >= minId && map <= maxId && g_mapDB.m_recordsById[map - minId];
    }

    // The Engine sites: the hooked function's address and the name.
    void M2Record(uint32_t origHook, const char* name) {
        switch (origHook) {
            case 0x81F700: CoA::CrashContext::Record(Field::M2SceneRender, name); break;
            case 0x823ED0: CoA::CrashContext::Record(Field::M2WaitForLoad, name); break;
            case 0x824FC0: CoA::CrashContext::RecordIsDrawable(name, false); break;
            case 0x821C77: CoA::CrashContext::RecordIsDrawable(name, true); break;
            case 0x82CED0: CoA::CrashContext::Record(Field::M2SequenceInfo, name); break;
            case 0x82EC30: CoA::CrashContext::Record(Field::M2CollisionFacets, name); break;
            case 0x82F0F0: CoA::CrashContext::Record(Field::M2AnimateMT, name); break;
            case 0x8309C0: CoA::CrashContext::Record(Field::M2AnimateParticle, name); break;
            case 0x830DC0: CoA::CrashContext::Record(Field::M2Animate, name); break;
            case 0x832260: CoA::CrashContext::Record(Field::M2ProcessCallbacks, name); break;
            case 0x838490: CoA::CrashContext::Record(Field::M2Skin, name); break;
            case 0x81F8F0: CoA::CrashContext::Record(Field::M2Shared, name); break;
            default: break;
        }
    }

    void FrameScriptExecute(const char* source) {   // sub_1019dc40: a null source records ""
        CoA::CrashContext::Record(Field::FrameScriptExecute, source);
    }

    void LoadBlp(const char* fileName) {   // sub_1019d310
        CoA::CrashContext::Record(Field::Blp, fileName);
    }

    void TextureCreated(const char* fileName) {   // AscLargeAlloc.cpp FUN_102a8760
        CoA::CrashContext::Record(Field::TextureCreate, fileName);
    }
}

void CoA::CrashContextClient::AppendReport(char* buffer, uint32_t size) {
    CoA::CrashContext::RecordClientDbRow(g_coaLastGetRowDb, g_coaLastGetRowId);
    SStrPack(buffer, CoA::CrashContext::Report().c_str(), size);
}

void CoA::CrashContextClient::Init() {
    CoA::CrashContext::Deps& d = CoA::CrashContext::Dependencies();
    d.fatal = Fatal;
    d.memory = Memory;
    d.hdPatch = CoA::RealmData::HdPatchLoaded;
    d.opcode = CoA::Profiler::CurrentOpcode;
    d.connection = Connection;
    d.luaTrace = LuaTrace;
    d.mapExists = MapExists;

    g_m2CrashRecordHook = M2Record;
    g_frameScriptExecuteHook = FrameScriptExecute;
    g_blpLockChainHook = LoadBlp;
    g_textureCreateHook = TextureCreated;
}
