// AscWidget.cpp: the registrar hook that adds every "#<widget>" binding to the client's method tables, and
// #Frame.GetAllAttributes. Also AscPOI.cpp's #Minimap.SetPOIMaxRange (the rest of AscPOI is not ported yet).
//
// The original detours the Frame / Minimap / Model / MovieFrame registrars (0x49E540, 0x57C040, 0x9603D0, 0x970610);
// here each registrar calls g_widgetMethodsHook after its own FillScriptMethodTable. Minimap, Model and MovieFrame call
// the Frame registrar first, so Frame's additions reach their tables too, as in the original.
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Script.h"
#include "Engine/Source/Frame/CSimpleFrame.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include "WoW/Source/Ui/MinimapFrame.h"

namespace {
    // AscWidget.cpp:AddMethod: name, closure, settable(-3) into the table the registrar left at -1.
    void AddMethod(const char* name, lua_CFunction fn, void* ctx) {
        lua_State* L = static_cast<lua_State*>(ctx);
        lua_pushstring(L, name ? name : "");
        lua_pushcclosure(L, fn, 0);
        lua_settable(L, -3);
    }

    void AddWidgetMethods(const char* widget, lua_State* L) {
        CoA::LuaReg::ForEachWidgetMethod(widget, AddMethod, L);
    }

    // Frame:GetAllAttributes (FUN_10a6bde0): self[0] is the frame; every attribute in the frame's list order, name =
    // registry value. No type check on self, as in the original; an attribute whose ref is -1 sets nil (no entry).
    int GetAllAttributes(lua_State* L) {
        lua_rawgeti(L, 1, 0);
        CSimpleFrame* frame = static_cast<CSimpleFrame*>(lua_touserdata(L, -1));
        lua_settop(L, -2);
        lua_createtable(L, 0, 0);

        if (!frame) {
            return 1;
        }

        for (FRAMEATTR* attr = frame->m_attributes.Head(); attr; attr = frame->m_attributes.Next(attr)) {
            lua_pushstring(L, attr->m_key.m_str ? attr->m_key.m_str : "");
            lua_rawgeti(L, LUA_REGISTRYINDEX, attr->m_luaRef);
            lua_settable(L, -3);
        }

        return 1;
    }

    // Minimap:SetPOIMaxRange(range) (FUN_10a6bbc0): luaL_checknumber(L, 2) over the client float at 0xA41E00, the
    // edge-POI distance limit (MinimapFrame.cpp g_maxPOIDistance). Never reset, as in the original.
    int SetPOIMaxRange(lua_State* L) {
        g_maxPOIDistance = static_cast<float>(CoA::Script::CheckNumber(L, 2));
        return 0;
    }

    const CoA::Binding s_bindings[] = {
        COA_NATIVE("#Frame", "GetAllAttributes", GetAllAttributes),
        COA_NATIVE("#Minimap", "SetPOIMaxRange", SetPOIMaxRange),
    };
}

void CoA::WidgetClient::Init() {
    g_widgetMethodsHook = AddWidgetMethods;
    CoA::LuaReg::AddModule(s_bindings, sizeof(s_bindings) / sizeof(s_bindings[0]));
}
