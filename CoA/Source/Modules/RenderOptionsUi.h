#ifndef COA_SOURCE_MODULES_RENDEROPTIONSUI_H
#define COA_SOURCE_MODULES_RENDEROPTIONSUI_H

#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/RenderOptions.h"
#include <cstddef>

struct lua_State;

// The Graphics page's DLSS / Frame Generation / Reflex dropdowns (stage 5): C_CoARender.GetOptionState and the Lua
// that builds the dropdowns. CoA64 addition.
namespace CoA::RenderOptionsUi {
    // { dlss = {enabled, reason, max, restart}, frameGen = {...}, reflex = {...} }; reason nil when enabled.
    void PushState(lua_State* L, const CoA::RenderOptions::State& state);
    // The Lua that builds the dropdowns on VideoOptionsResolutionPanel (once; later runs do nothing).
    const char* BuilderLua();
    // C_CoARender's natives.
    const CoA::Binding* Bindings(size_t& count);
    // RenderOptionsUiClient.cpp: the native and the builder.
    void Init();
}

#endif
