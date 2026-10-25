#include "CoA/Source/CoA.h"
#include "CoA/Source/Runtime/Log.h"
#include "CoA/Source/Modules/Account.h"
#include "CoA/Source/Modules/GlobalsJ.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/ClientState.h"
#include "CoA/Source/Runtime/Dbc.h"
#include "CoA/Source/Runtime/Packet.h"
#include "CoA/Source/Runtime/Events.h"
#include "Engine/Source/Storm/H/storm.h"
#include "CoA/Source/Runtime/Logger.h"
#include "CoA/Source/Runtime/LuaReg.h"

extern void (*g_luaBaseOpenedHook)(lua_State* L);
extern const char** (*g_createEventsHook)(const char** names, uint32_t& count);

// The ported modules, in AscBindings::InitModules order. Filled as modules are ported.
static const CoA::Module s_modules[] = {
    { "RealmInfo", CoA::RealmInfo::Init },
    { "Account", CoA::Account::Init },
    { "Config", CoA::Config::Init },
    { "GameEvents", CoA::GameEvents::Init },
    { "Misc", CoA::Misc::Init },
    { "GlobalsB", CoA::GlobalsB::Init },
    { "GlobalsD", CoA::GlobalsD::Init },
    { "GlobalsJ", CoA::GlobalsJ::Init },
    { "Json", CoA::Json::Init },
    { "LoggerApi", CoA::LoggerApi::Init },
    { "SmallApis", CoA::SmallApis::Init },
    { "GlueCVars", CoA::GlueCVars::Init },
};

// Runtime/CrashTrace.cpp (developer diagnostic, -extlog only).
void CoA_InstallCrashTrace();

void CoA::Attach() {
    // DllMain (FUN_10a79300): Storm's log title "Ascension" (0x771890); the client sets its own later.
    SErrSetLogTitleString("Ascension");
    CoA::Log::Init();
    CoA::Logger::Init();
    CoA::Log::Printf("CoA attach");
    if (CoA::Log::Enabled()) {
        CoA_InstallCrashTrace();
    }

    // AscRuntime::InstallHooks: the event table extension (orig hook 0x81B5F0) and the DLL's own event names.
    g_createEventsHook = CoA::Events::ExtendList;
    CoA::Events::RegisterNames();
    CoA::Events::InstallClient();
    // AscDbc: custom tables read through the client's SFile.
    CoA::Dbc::InstallClient();
    CoA::ClientState::InstallClient();
    CoA::Account::InstallClient();
    CoA::GlobalsJ::InstallClient();
    CoA::Packet::InstallClient();

    for (const CoA::Module& module : s_modules) {
        if (!module.init) {
            continue;
        }

        CoA::Log::Printf("init %s", module.name);
        module.init();
    }

    // AscRegistry::OnAttach: natives go into each new Lua state from luaopen_base (orig hook 0x855060).
    g_luaBaseOpenedHook = CoA_OnLuaBaseOpened;
}
