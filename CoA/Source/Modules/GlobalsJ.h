#ifndef COA_SOURCE_MODULES_GLOBALSJ_H
#define COA_SOURCE_MODULES_GLOBALSJ_H

#include <cstdint>
#include <string>

class CDataStore;
struct lua_State;

// More GLOBAL bindings with the client hook or patch that consumes the state they set (port of AscGlobalsJ.cpp).
//
//   GetErrorCallstackHeight   counted by FUN_10296df0, the DLL's replacement of the script error function 0x819730:
//        the client's body plus the lua_getstack depth while the error handler runs.
//   CanPerformAction(0x19)    whether Script_RunScript (0x510B30, detoured) is running.
//   Known / secure addons     SecureAddonMgr (FUN_10291ed0 -> 0x10BE3044), replaced by SMSG 0x94E.
//   ReadCustomWTF / WriteCustomWTF / RegisterSavedCVar.
namespace CoA::GlobalsJ {
    // SMSG 0x94E (FUN_10312b90): u32 n x {cstring name, u8 secure}.
    int32_t OnKnownAddonList(void* param, uint32_t opcode, uint32_t time, CDataStore* msg);
    bool IsKnownAddon(const char* name);
    // AscAddons::IsSecure: listed and secure.
    bool IsSecureAddon(const char* name);

    // "WTF\Custom\<name>.json", relative to the working directory.
    std::string CustomWtfPath(const std::string& name);

    // The detour on Script_RunScript (0x510B30): set while it runs.
    void SetInsideRunScript(bool inside);

    // FUN_10296df0's addition to the error handler (FrameScript_HandleError): entering counts the stack depth,
    // leaving resets it.
    void OnErrorHandler(lua_State* L, int32_t entering);

    // GlobalsJClient.cpp: hooks FrameScript_HandleError.
    void InstallClient();
    // Tests only.
    void Reset();
}

#endif
