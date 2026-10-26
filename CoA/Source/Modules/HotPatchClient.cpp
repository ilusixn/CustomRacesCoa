#include "CoA/Source/Modules/HotPatch.h"
#include "CoA/Source/Modules/Modules.h"
#include "Engine/Source/FrameScript/FrameScript.h"

// HotPatch's client half: the world Lua state and FrameScript_Execute (0x819210), which the reconstruction calls by
// address (AscLua::GetState / AscLua::Execute).
namespace {
    void Execute(const char* source, const char* name) {
        FrameScript_Execute(source, name, nullptr);
    }
}

void CoA::HotPatchClient::Init() {
    CoA::HotPatch::Deps& d = CoA::HotPatch::Dependencies();
    d.state = FrameScript_GetContext;
    d.execute = Execute;
}
