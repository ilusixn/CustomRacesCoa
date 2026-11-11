#include "WoW/Source/Ui/ClientPlugins.h"

#include "Engine/Source/lua-5.1/src/lua.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"

#include <string>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace {

using OpenLuaFn = void (*)(const CoaPluginLua*, lua_State*);

void PushBoolean(lua_State* L, int b) { lua_pushboolean(L, b); }
void PushCClosure(lua_State* L, int (*fn)(lua_State*), int n) { lua_pushcclosure(L, fn, n); }

std::string g_gameDir;
std::vector<OpenLuaFn> g_plugins;
bool g_loaded = false;

CoaPluginLua g_api;

void LoadPlugins() {
    g_loaded = true;
#if defined(_WIN32)
    wchar_t exe[MAX_PATH];
    DWORD n = GetModuleFileNameW(nullptr, exe, MAX_PATH);
    std::wstring dir(exe, n);
    dir = dir.substr(0, dir.find_last_of(L"\\/") + 1);
    char dirUtf8[MAX_PATH * 3] = {};
    WideCharToMultiByte(CP_UTF8, 0, dir.c_str(), -1, dirUtf8, sizeof(dirUtf8), nullptr, nullptr);
    g_gameDir = dirUtf8;

    WIN32_FIND_DATAW fd;
    HANDLE find = FindFirstFileW((dir + L"Plugins\\*.dll").c_str(), &fd);
    if (find == INVALID_HANDLE_VALUE) {
        return;
    }
    do {
        HMODULE module = LoadLibraryW((dir + L"Plugins\\" + fd.cFileName).c_str());
        if (!module) {
            continue;
        }
        auto open = reinterpret_cast<OpenLuaFn>(GetProcAddress(module, "CoaPlugin_OpenLua"));
        if (open) {
            g_plugins.push_back(open);
        } else {
            FreeLibrary(module);
        }
    } while (FindNextFileW(find, &fd));
    FindClose(find);
#endif

    g_api.version = CoA64_PLUGIN_VERSION;
    g_api.globalsIndex = LUA_GLOBALSINDEX;
    g_api.gameDir = g_gameDir.c_str();
    g_api.gettop = lua_gettop;
    g_api.type = lua_type;
    g_api.pushnil = lua_pushnil;
    g_api.pushnumber = lua_pushnumber;
    g_api.pushstring = lua_pushstring;
    g_api.pushboolean = PushBoolean;
    g_api.pushcclosure = PushCClosure;
    g_api.createtable = lua_createtable;
    g_api.setfield = lua_setfield;
    g_api.checklstring = luaL_checklstring;
    g_api.checknumber = luaL_checknumber;
    g_api.tolstring = lua_tolstring;
    g_api.tonumber = lua_tonumber;
}

}

void ClientPluginsOpenLua(lua_State* L) {
    if (!g_loaded) {
        LoadPlugins();
    }
    for (OpenLuaFn open : g_plugins) {
        open(&g_api, L);
    }
}
