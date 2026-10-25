// Addon-file helpers (port of AscLibrary.cpp): LoadLibrary / IsLibraryLoaded -- XML libraries under
// Interface\LibraryXML\, loaded once each per Lua state. And SaveSavedVariables.
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Script.h"
#include "Engine/Source/FrameXML/FrameXML.h"
#include "Engine/Source/Storm/Source/MD5.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <algorithm>
#include <string>
#include <vector>

// WoW/Source/Ui/AddOns.h (not self-contained).
void AddOnsSaveOne(const char* name);

namespace {
    std::vector<std::string> s_loaded;   // DAT_10d3d7dc..e0

    // The list names libraries loaded into the current Lua state: a /reload or a world re-entry builds a new state
    // with none of them, so the list starts empty with each new state (the original empties it at the top of
    // FUN_10a66100; ElvUI survives /reload on the original, so it must be per state).
    void ForgetLibraries() {
        s_loaded.clear();
    }

    bool Loaded(const std::string& name) {
        return std::find(s_loaded.begin(), s_loaded.end(), name) != s_loaded.end();
    }

    // FUN_10a6fcc0: "Interface\LibraryXML\<name>.toc" through FrameXML_CreateFrames (0x814340, with the client's own
    // status). The taint source lua_tainted is cleared for the load when lua_taintexpected is set and the low byte of
    // lua_taintedclosure is not, then restored.
    int LoadLibraryXml(lua_State* L) {   // not "LoadLibrary": that is a Windows.h macro
        std::string name = CoA::Script::CheckString(L, 1);

        if (Loaded(name)) {
            return 0;
        }

        s_loaded.push_back(name);
        std::string path = "Interface\\LibraryXML\\" + name + ".toc";
        const char* saved = lua_tainted;
        auto untaint = []() { return lua_taintexpected != 0 && (lua_taintedclosure & 0xFF) == 0; };

        if (untaint()) {
            lua_tainted = nullptr;
        }

        // The original passes a zeroed 0x100-byte buffer as the digest context.
        MD5_CTX md5 = {};
        FrameXML_CreateFrames(path.c_str(), name.c_str(), &md5, nullptr);

        if (untaint()) {
            lua_tainted = saved;
        }

        return 0;
    }

    int IsLibraryLoaded(lua_State* L) {
        lua_pushboolean(L, Loaded(CoA::Script::CheckString(L, 1)));
        return 1;
    }

    // FUN_10a70bf0: write only the named addon's SavedVariables (AddOnsSaveOne).
    int SaveSavedVariables(lua_State* L) {
        std::string name = CoA::Script::CheckString(L, 1);
        AddOnsSaveOne(name.c_str());
        return 0;
    }

    const CoA::Binding s_bindings[] = {
        COA_NATIVE("", "IsLibraryLoaded", IsLibraryLoaded),
        COA_NATIVE("", "LoadLibrary", LoadLibraryXml),
        COA_NATIVE("", "SaveSavedVariables", SaveSavedVariables),
    };
}

void CoA::LibraryClient::Init() {
    CoA::LuaReg::OnGlueRegistered(ForgetLibraries);
    CoA::LuaReg::OnWorldRegistered(ForgetLibraries);
    CoA::LuaReg::AddModule(s_bindings, sizeof(s_bindings) / sizeof(s_bindings[0]));
}
