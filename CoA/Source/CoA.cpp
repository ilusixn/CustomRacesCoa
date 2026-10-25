#include "CoA/Source/CoA.h"
#include "CoA/Source/Runtime/Log.h"
#include "CoA/Source/Runtime/Events.h"
#include "Engine/Source/Storm/H/storm.h"
#include "CoA/Source/Runtime/Logger.h"
#include "CoA/Source/Runtime/LuaReg.h"

extern void (*g_luaBaseOpenedHook)(lua_State* L);
extern const char** (*g_createEventsHook)(const char** names, uint32_t& count);

// The ported modules, in AscBindings::InitModules order. Filled as modules are ported.
static const CoA::Module s_modules[] = {
    { nullptr, nullptr },
};

void CoA::Attach() {
    // DllMain (FUN_10a79300): Storm's log title "Ascension" (0x771890); the client sets its own later.
    SErrSetLogTitleString("Ascension");
    CoA::Log::Init();
    CoA::Logger::Init();
    CoA::Log::Printf("CoA attach");

    // AscRuntime::InstallHooks: the event table extension (orig hook 0x81B5F0) and the DLL's own event names.
    g_createEventsHook = CoA::Events::ExtendList;
    CoA::Events::RegisterNames();

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
