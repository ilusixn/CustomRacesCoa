// AscMisc.cpp's client passthroughs: FadeScreen, IsKeyDown, the command line (IsAutoLogin / IsRenderDebug and the
// launcher's automatic login) and -config.
#include "CoA/Source/Modules/Misc.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/Lifecycle.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Script.h"
#include "Engine/Source/ConsoleBase/ConsoleVar.h"
#include "Engine/Source/Event/Event.h"
#include "Engine/Source/Frame/CSimpleTop.h"
#include "WoW/Source/Client.h"
#include "WoW/Source/Glue/CGlueMgr.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Source/DB/Rec/MapRec.h"
#include "WoW/Source/Game/GameClient/PlayerName.h"
#include "WoW/Source/Game/GameClient/WorldText.h"
#include "WoW/Source/Object/ObjectClient/Player_C.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"
#include "WoW/Source/Ui/CGWorldStateInfo.h"
#include "WoW/Source/Ui/GameUI.h"
#include "WoW/Source/Ui/StableInfo.h"
#include "WoW/Source/WorldClient/Map.h"

// WoW/Source/Ui/GameUI.cpp: the interact-range floats at 0xAC8A64 (SetInteractDistance).
extern float s_duelDistSq;
extern float s_playerInteractDistSq;
extern float s_inspectDistSq;
extern float s_followDistSq;
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>

namespace {
    // ---- command line ---------------------------------------------------------------------------------------
    // FUN_10195180 (on the first glue screen only, flag 0x10BDEC0B; installer FUN_101952e0):
    //   -controller v  -> gxWindow "0" when v is "1", else "1"
    //   -realm v       -> realmName v (CVar set with its last flag 1)
    //   -login l -password p -> CGlueMgr::m_scandllOkayToLogIn = 1, CSimpleTop::s_eventTime = time(), then
    //                     CGlueMgr::SetCurrentAccount(l) and LoginServerLogin(l, p): the launcher's automatic login
    //   -autologin n / -renderdebug n -> atoi(n) != 0 into 0x10BDEC08 / 0x10BDEC09
    bool s_commandLineDone = false;   // 0x10BDEC0B
    bool s_autoLogin = false;         // 0x10BDEC08
    bool s_renderDebug = false;       // 0x10BDEC09

    // FUN_10114c40 -> CVar::Set (0x7668C0): refused when name and value together exceed 0xF9 characters.
    void SetCVarValue(CVar* var, const char* value, bool setNeedsSave) {
        if (strlen(value) + strlen(var->m_key.m_str) > 0xF9) {
            return;
        }

        var->Set(value, true, false, false, setNeedsSave);
    }

    void ApplyCommandLine() {
        if (s_commandLineDone) {
            return;
        }

        s_commandLineDone = true;
        uint32_t now = static_cast<uint32_t>(_time64(nullptr));

        if (const char* controller = CoA::Script::CommandLineArg("controller")) {
            if (CVar* gxWindow = CVar::Lookup("gxWindow")) {
                SetCVarValue(gxWindow, strcmp(controller, "1") == 0 ? "0" : "1", false);
            }
        }

        const char* realm = CoA::Script::CommandLineArg("realm");

        if (realm && *realm) {
            if (CVar* realmName = CVar::Lookup("realmName")) {
                SetCVarValue(realmName, realm, true);
            }
        }

        const char* login = CoA::Script::CommandLineArg("login");
        const char* password = CoA::Script::CommandLineArg("password");

        if (login && password) {
            CGlueMgr::m_scandllOkayToLogIn = 1;
            CSimpleTop::s_eventTime = now;
            CGlueMgr::SetCurrentAccount(login);
            std::string passwordCopy(password);
            CGlueMgr::LoginServerLogin(login, &passwordCopy[0]);
        }

        if (const char* value = CoA::Script::CommandLineArg("autologin")) {
            s_autoLogin = atoi(value) != 0;
        }

        if (const char* value = CoA::Script::CommandLineArg("renderdebug")) {
            s_renderDebug = atoi(value) != 0;
        }
    }

    // ApplyConfigFile: -config names the file CVar::Initialize reads (the original rewrites the pointer at
    // 0x406851). Kept for the life of the process.
    std::string s_configFile;

    void ApplyConfigFile() {
        const char* config = CoA::Script::CommandLineArg("config");

        if (!config) {
            return;
        }

        s_configFile = config;
        g_configFileName = s_configFile.c_str();
    }

    int IsAutoLogin(lua_State* L) {   // handler_IsAutoLogin
        CoA::Script::PushBool(L, s_autoLogin);
        return 1;
    }

    int IsRenderDebug(lua_State* L) {   // handler_IsRenderDebug
        CoA::Script::PushBool(L, s_renderDebug);
        return 1;
    }

    // handler_IsKeyDown: EventIsKeyDown (0x47D230), low byte.
    int IsKeyDown(lua_State* L) {
        uint32_t key = static_cast<uint32_t>(CoA::Script::ToInt(CoA::Script::CheckNumber(L, 1)));
        CoA::Script::PushBool(L, (static_cast<uint32_t>(EventIsKeyDown(static_cast<KEY>(key))) & 0xFF) != 0);
        return 1;
    }

    // handler_FadeScreen: EnableFadingScreen(seconds, callback, 0); the callback (0x10a4c0a0) calls
    // DisableFadingScreen(seconds, 0, 0) with the remembered duration. Returns 1 without pushing.
    float s_fadeSeconds = 0.0f;   // DAT_10d3d7e8

    void FadeScreenDone(void*) {
        DisableFadingScreen(s_fadeSeconds, nullptr, nullptr);
    }

    int FadeScreen(lua_State* L) {
        float seconds = static_cast<float>(CoA::Script::CheckNumber(L, 1));
        EnableFadingScreen(seconds, FadeScreenDone, nullptr);
        s_fadeSeconds = seconds;
        return 1;
    }

    // ---- exe state, read directly ---------------------------------------------------------------------------------
    int GetZoneId(lua_State* L) {   // 0xBD080C
        CoA::Script::PushInt(L, CGGameUI::m_areaID);
        return 1;
    }

    int GetAreaId(lua_State* L) {   // 0xBD0810
        CoA::Script::PushInt(L, CGGameUI::m_subzoneID);
        return 1;
    }

    int GetActiveMapID(lua_State* L) {   // 0xBD088C
        CoA::Script::PushInt(L, CGGameUI::m_iCurrentMapID);
        return 1;
    }

    int GetBindPosition(lua_State* L) {   // 0xC9EB2C, 0xC9D538
        C3Vector position;
        uint32_t mapId;
        PlayerGetBindPoint(&position, &mapId);
        CoA::Script::PushNum(L, position.x);
        CoA::Script::PushNum(L, position.y);
        CoA::Script::PushNum(L, position.z);
        CoA::Script::PushInt(L, static_cast<int32_t>(mapId));
        return 4;
    }

    // FUN_10a4df90: stable slot n (1-based) of the client's stable list, its pet number; nil past the slot count.
    int GetStablePetID(lua_State* L) {
        const int index = static_cast<int>(CoA::Script::CheckNumber(L, 1));

        if (CGStableInfo::m_numSlots < index || index < 1 || index > 6) {
            lua_pushnil(L);
            return 1;
        }

        CoA::Script::PushInt(L, static_cast<int32_t>(CGStableInfo::m_pets[index - 1].m_petNumber));
        return 1;
    }

    // handler_SetInteractDistance: the client's interact-range floats at 0xAC8A64 (duel, player interact, inspect,
    // follow -- squared values written as given); other indices are not written.
    int SetInteractDistance(lua_State* L) {
        const int32_t index = CoA::Script::ToInt(CoA::Script::CheckNumber(L, 1));
        const float distance = static_cast<float>(CoA::Script::CheckNumber(L, 2));
        float* const ranges[] = { &s_duelDistSq, &s_playerInteractDistSq, &s_inspectDistSq, &s_followDistSq };

        if (index >= 0 && index < 4) {
            *ranges[index] = distance;
        }

        return 0;
    }

    // FUN_10a4e280: the client's world-state value (GetWorldState, 0x548D10).
    int GetWorldStateNative(lua_State* L) {
        CoA::Script::PushInt(L, GetWorldState(CoA::Script::ToInt(CoA::Script::CheckNumber(L, 1))));
        return 1;
    }

    // handler_ReloadMap: the current map's directory through CMap::Load(directory, 1); returns 1 with nothing pushed.
    void ReloadCurrentMap() {
        if (MapRec* map = g_mapDB.GetRecord(CGGameUI::m_iCurrentMapID)) {
            CMap::Load(map->m_directory, 1);
        }
    }

    int ReloadMap(lua_State*) {
        ReloadCurrentMap();
        return 1;
    }

    // LoadADT (FUN_10112a00): a one-shot override for the tile (x, y), then the map reloaded; returns 1, nothing pushed.
    bool s_adtArmed = false;   // DAT_10bc98a0
    int32_t s_adtX = 0;
    int32_t s_adtY = 0;
    std::string s_adtFile;

    int LoadADT(lua_State* L) {
        const int32_t x = CoA::Script::ToInt(CoA::Script::CheckNumber(L, 1));
        const int32_t y = CoA::Script::ToInt(CoA::Script::CheckNumber(L, 2));
        const std::string file = CoA::Script::CheckString(L, 3);
        s_adtArmed = true;
        s_adtX = x;
        s_adtY = y;
        s_adtFile = file;
        ReloadCurrentMap();
        return 1;
    }

    // CreateWorldText (FUN_10a4ac90): text, x, y, z, ascend, fade-in, fade-out start, duration [, start height 0.018]
    // [, end height 0.018] [, 1.0]: world text type 5's style and the float at 0xAF4890 (the second icon position's x),
    // then combat text type 5 on the player's name (PlayerNameCreateText). The original dereferences the player
    // unchecked; without one nothing is raised.
    float s_worldTextPosition[3] = {};   // DAT_10d3d7f0

    int CreateWorldText(lua_State* L) {
        const std::string text = CoA::Script::CheckString(L, 1);
        WORLDTEXTTYPE* style = WorldTextTypeStyle(5);
        s_worldTextPosition[0] = static_cast<float>(CoA::Script::CheckNumber(L, 2));
        s_worldTextPosition[1] = static_cast<float>(CoA::Script::CheckNumber(L, 3));
        s_worldTextPosition[2] = static_cast<float>(CoA::Script::CheckNumber(L, 4));
        style->m_ascendDistance = static_cast<float>(CoA::Script::CheckNumber(L, 5));
        style->m_fadeInTime = static_cast<uint32_t>(static_cast<int64_t>(CoA::Script::CheckNumber(L, 6)));
        style->m_fadeOutStart = static_cast<uint32_t>(static_cast<int64_t>(CoA::Script::CheckNumber(L, 7)));
        style->m_duration = static_cast<uint32_t>(static_cast<int64_t>(CoA::Script::CheckNumber(L, 8)));
        style->m_startHeight = lua_type(L, 9) > 0 ? static_cast<float>(CoA::Script::CheckNumber(L, 9)) : 0.018f;
        style->m_endHeight = lua_type(L, 10) > 0 ? static_cast<float>(CoA::Script::CheckNumber(L, 10)) : 0.018f;
        WorldTextIconPositions()[1].x = lua_type(L, 11) > 0 ? static_cast<float>(CoA::Script::CheckNumber(L, 11)) : 1.0f;

        if (CGPlayer_C* player = ClntObjMgrGetActivePlayerObj()) {
            PlayerNameCreateText(player->m_nameDesc, 5, text.c_str(), nullptr, nullptr);
        }

        return 0;
    }

    const CoA::Binding s_bindings[] = {
        COA_NATIVE("", "CreateWorldText", CreateWorldText),
        COA_NATIVE("", "GetActiveMapID", GetActiveMapID),
        COA_NATIVE("", "GetAreaId", GetAreaId),
        COA_NATIVE("", "GetBindPosition", GetBindPosition),
        COA_NATIVE("", "GetStablePetID", GetStablePetID),
        COA_NATIVE("", "GetWorldState", GetWorldStateNative),
        COA_NATIVE("", "GetZoneId", GetZoneId),
        COA_NATIVE("", "LoadADT", LoadADT),
        COA_NATIVE("", "ReloadMap", ReloadMap),
        COA_NATIVE("", "SetInteractDistance", SetInteractDistance),
        COA_NATIVE("", "IsAutoLogin", IsAutoLogin),
        COA_NATIVE("", "IsRenderDebug", IsRenderDebug),
        COA_NATIVE("", "IsKeyDown", IsKeyDown),
        COA_NATIVE("", "FadeScreen", FadeScreen),
    };
}

void CoA::MiscClient::Init() {
    CoA::Lifecycle::OnGlueScreen(ApplyCommandLine);
    ApplyConfigFile();
    CoA::LuaReg::AddModule(s_bindings, sizeof(s_bindings) / sizeof(s_bindings[0]));
}

const char* CoA::MiscClient::TakeAdtOverride(int32_t x, int32_t y) {
    if (!s_adtArmed || s_adtX != x || s_adtY != y) {
        return nullptr;
    }

    s_adtArmed = false;
    return s_adtFile.c_str();
}

bool CoA::MiscClient::TakeWorldTextPosition(float position[3]) {
    const bool set = s_worldTextPosition[0] != 0.0f && s_worldTextPosition[1] != 0.0f && s_worldTextPosition[2] != 0.0f;

    if (set) {
        position[0] = s_worldTextPosition[0];
        position[1] = s_worldTextPosition[1];
        position[2] = s_worldTextPosition[2];
    }

    s_worldTextPosition[0] = s_worldTextPosition[1] = s_worldTextPosition[2] = 0.0f;
    return set;
}
