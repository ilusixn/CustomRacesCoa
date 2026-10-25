// AscBindings.cpp's transcribed bindings that need no client objects (the rest are in BindingsClient.cpp): the
// resurrection choices, C_Callboard.RequestView, C_UICamera.GetCameraInfo and the bindings the original registers
// without implementing them.
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/Dbc.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Packet.h"
#include "CoA/Source/Runtime/Script.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <Windows.h>
#include <cstring>

namespace {
    // FUN_100b2710: the shared "return 0;" the original registers for bindings it never implemented.
    int ReturnNothing(lua_State*) {
        return 0;
    }

    // FUN_1008ba00: validates {number}, reads it and returns nothing (unfinished in the original).
    int GetCollectorCacheRarityRatesInfo(lua_State* L) {
        if (CoA::Script::ValidateInput(L, { LUA_TNUMBER })) {
            static_cast<void>(CoA::Script::ToInt(CoA::Script::CheckNumber(L, 1)));
        }

        return 0;
    }

    // FUN_100de3b0: CMSG_CUSTOM_ASCENSION_RESURRECT_REQUEST (0x524) { u8 where }.
    void Resurrect(uint8_t where) {
        CoA::Packet(0x524).U8(where).Send();
    }

    int RessurectInClosestTown(lua_State*) {
        Resurrect(1);
        return 0;
    }

    int RessurectInClosestCapital(lua_State*) {
        Resurrect(2);
        return 0;
    }

    int RessurectAtCheckpoint(lua_State*) {
        Resurrect(3);
        return 0;
    }

    // FUN_10119a50: CMSG 0x55F with no body, returns true.
    int RequestView(lua_State* L) {
        CoA::Packet(0x55F).Send();
        lua_pushboolean(L, 1);
        return 1;
    }

    // The HD camera set applies when Data\Patch-Q.mpq exists and the command line does not say "-hd 0".
    bool HdCameras() {
        const DWORD attr = GetFileAttributesA("Data\\Patch-Q.mpq");
        const bool patchQ = attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY);
        const char* hd = CoA::Script::CommandLineArg("hd");
        return patchQ && !(hd && strcmp(hd, "0") == 0);
    }

    // FUN_100c9000: (fov, x, y, z, flags) -- the HD set at +0x14.., the SD set at +0x4..
    int GetCameraInfo(lua_State* L) {
        uint32_t id;

        if (!CoA::Script::ReadNumber(L, id)) {
            return 0;
        }

        CoA::Dbc::Table& t = CoA::Dbc::Get("DBFilesClient/UICameras.dbc");
        const uint8_t* row = t.Row(id);

        if (!row) {
            return CoA::Script::PushNils(L, 5);
        }

        const bool hd = HdCameras();
        CoA::Script::PushNum(L, CoA::Dbc::Table::F32(row, 4 + (hd ? 0x10 : 0)));
        CoA::Script::PushNum(L, CoA::Dbc::Table::F32(row, hd ? 0x18 : 0x8));
        CoA::Script::PushNum(L, CoA::Dbc::Table::F32(row, hd ? 0x1C : 0xC));
        CoA::Script::PushNum(L, CoA::Dbc::Table::F32(row, hd ? 0x20 : 0x10));
        CoA::Script::PushInt(L, CoA::Dbc::Table::I32(row, 0x24));
        return 5;
    }

    const CoA::Binding s_bindings[] = {
        COA_NATIVE("", "RessurectInClosestTown", RessurectInClosestTown),
        COA_NATIVE("", "RessurectInClosestCapital", RessurectInClosestCapital),
        COA_NATIVE("", "RessurectAtCheckpoint", RessurectAtCheckpoint),
        COA_NATIVE("C_Callboard", "RequestView", RequestView),
        COA_NATIVE("C_UICamera", "GetCameraInfo", GetCameraInfo),
        COA_NATIVE("C_RecoveryService", "SwapFactionChangePotion", ReturnNothing),
        COA_NATIVE("C_RecoveryService", "SwapRaceChangePotion", ReturnNothing),
        COA_NATIVE("C_CharacterAdvancement", "ClearRecentlyLearnedEntries", ReturnNothing),
        COA_NATIVE("C_CollectorCache", "GetCollectorCacheRarityRatesInfo", GetCollectorCacheRarityRatesInfo),
    };
}

void CoA::Bindings::Init() {
    CoA::LuaReg::AddModule(s_bindings, sizeof(s_bindings) / sizeof(s_bindings[0]));
}
