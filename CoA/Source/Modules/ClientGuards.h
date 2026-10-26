#ifndef COA_SOURCE_MODULES_CLIENTGUARDS_H
#define COA_SOURCE_MODULES_CLIENTGUARDS_H

struct lua_State;

// AscClientGuards.cpp: two small client guards from the original's init.
//
//   FUN_10a4c0c0 (on the after-0x52A980 list, i.e. after every UI reload): NOPs CGxDeviceD3d::DeviceSetBaseMipLevel's
//     call of CGxDevice::DeviceSetBaseMipLevel (0x68E4AD, 11 bytes): from then on a base mip change only recreates the
//     textures. The memory guard FUN_10a3abd0 (after every idle tick, past 3200 MB working set: the same NOP and a UI
//     reload request) is the 32-bit address-space workaround the x64 client does not carry (ledger ruling).
//   FUN_10296fd0 over 0x8540D0 (luaopen_string): the string library gets string.toboolean (FUN_10297280).
namespace CoA::ClientGuards {
    struct Deps {
        void (*skipBaseMipStore)() = nullptr;   // the 0x68E4AD NOP, as the Engine flag
    };

    Deps& Dependencies();

    // FUN_10297280: a boolean as it is; a number truncated to int32 and compared with 0; else luaL_checklstring's
    // string is "true", "1", "on" or "t".
    int ToBoolean(lua_State* L);

    void Init();
}

#endif
