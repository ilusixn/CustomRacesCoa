// RenderOptionsUi's client half: registers C_CoARender and runs the dropdown builder once the world UI exists
// (entering the world, and after a /reload).
#include "CoA/Source/Modules/RenderOptionsUi.h"
#include "CoA/Source/Runtime/ClientHooks.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "Engine/Source/FrameScript/FrameScript.h"

namespace {
    void RunBuilder() {
        FrameScript_Execute(CoA::RenderOptionsUi::BuilderLua(), "CoARenderOptions", nullptr);
    }
}

void CoA::RenderOptionsUi::Init() {
    size_t count = 0;
    const CoA::Binding* bindings = Bindings(count);
    CoA::LuaReg::AddModule(bindings, count);
    CoA::ClientHooks::afterEnterWorld.Add(RunBuilder);
    CoA::ClientHooks::afterUiReload.Add(RunBuilder);
}
