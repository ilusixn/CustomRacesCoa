#include "CoA/Source/Modules/RenderOptionsUi.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "Engine/Source/lua-5.1/src/lua.h"

namespace {
    void PushOption(lua_State* L, const char* name, const CoA::RenderOptions::Option& option) {
        lua_newtable(L);
        lua_pushboolean(L, option.enabled ? 1 : 0);
        lua_setfield(L, -2, "enabled");

        if (option.reason != CoA::RenderOptions::Reason::None) {
            lua_pushstring(L, CoA::RenderOptions::ReasonText(option.reason));
            lua_setfield(L, -2, "reason");
        }

        lua_pushnumber(L, option.maxValue);
        lua_setfield(L, -2, "max");
        lua_pushboolean(L, option.restart ? 1 : 0);
        lua_setfield(L, -2, "restart");
        lua_setfield(L, -2, name);
    }

    int GetOptionState(lua_State* L) {
        CoA::RenderOptionsUi::PushState(L, CoA::RenderOptions::Decide(CoA::RenderOptionsClient::Facts()));
        return 1;
    }

    const CoA::Binding s_bindings[] = {
        COA_NATIVE("C_CoARender", "GetOptionState", GetOptionState),
    };
}

void CoA::RenderOptionsUi::PushState(lua_State* L, const CoA::RenderOptions::State& state) {
    lua_newtable(L);
    PushOption(L, "dlss", state.dlss);
    PushOption(L, "frameGen", state.frameGen);
    PushOption(L, "reflex", state.reflex);
}

const char* CoA::RenderOptionsUi::BuilderLua() {
    static const char kBuilder[] =
#include "CoA/Source/Modules/RenderOptionsLua.inc"
        ;
    return kBuilder;
}

const CoA::Binding* CoA::RenderOptionsUi::Bindings(size_t& count) {
    count = sizeof(s_bindings) / sizeof(s_bindings[0]);
    return s_bindings;
}
