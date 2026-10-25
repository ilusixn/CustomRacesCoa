// AscMisc.cpp's client passthroughs: FadeScreen, IsKeyDown, the command line (IsAutoLogin / IsRenderDebug and the
// launcher's automatic login) and -config.
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/Lifecycle.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Script.h"
#include "Engine/Source/ConsoleBase/ConsoleVar.h"
#include "Engine/Source/Event/Event.h"
#include "Engine/Source/Frame/CSimpleTop.h"
#include "WoW/Source/Client.h"
#include "WoW/Source/Glue/CGlueMgr.h"
#include "WoW/Source/Ui/GameUI.h"
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

    const CoA::Binding s_bindings[] = {
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
