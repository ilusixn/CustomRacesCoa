// AscAttachLua.cpp's client half: the GLOBAL_MOUSE_DOWN / UP signals (Engine hook pointer).
#include "CoA/Source/Modules/AttachLua.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/Events.h"
#include "Engine/Source/Frame/CSimpleFrame.h"

namespace {
    // FUN_10a46660 / FUN_10a466f0 (AscRuntime::Signal: only when the client's table has the name).
    void GlobalMouse(int32_t down, uint32_t button) {
        CoA::Events::Signal(down ? "GLOBAL_MOUSE_DOWN" : "GLOBAL_MOUSE_UP", "%u", button);
    }
}

void CoA::AttachLuaClient::Init() {
    g_globalMouseHook = GlobalMouse;
}
