// AscPacketApi.cpp (see PacketApi.h): the Lua packet API.
#include "CoA/Source/Modules/PacketApi.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/ClientState.h"
#include "CoA/Source/Runtime/Log.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Source/Runtime/Script.h"
#include "Engine/Source/Base/CDataStore.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <cstring>
#include <unordered_map>

namespace {
    using CoA::Script::CheckNumber;
    using CoA::Script::ToInt;

    CoA::PacketApi::Deps s_deps;
    std::unordered_map<uint16_t, int> s_handlers;   // 0x10be3e98: opcode -> registry ref

    // The shared tail of CreatePacket and the handler: wrap a CDataStore* and attach the registry metatable
    // "CDataStore", or leave nil in its place. The userdata holds the pointer (8 bytes here, 4 in the original).
    void PushPacket(lua_State* L, CDataStore* store) {
        *static_cast<CDataStore**>(lua_newuserdata(L, sizeof(CDataStore*))) = store;
        lua_pushstring(L, "CDataStore");
        lua_rawget(L, LUA_REGISTRYINDEX);

        if (lua_type(L, -1) == LUA_TTABLE) {
            lua_setmetatable(L, -2);
        } else {
            lua_settop(L, -3);
            lua_pushnil(L);
        }
    }

    // The original dereferences the first argument's userdata unchecked; here only a packet userdata (the registry
    // "CDataStore" metatable) is one (Ruling) -- nil, an emptied packet, a light or any other userdata is nothing.
    CDataStore* Arg(lua_State* L) {
        if (lua_type(L, 1) != LUA_TUSERDATA || !lua_getmetatable(L, 1)) {
            return nullptr;
        }

        lua_pushstring(L, "CDataStore");
        lua_rawget(L, LUA_REGISTRYINDEX);
        const bool packet = lua_rawequal(L, -1, -2) != 0;
        lua_pop(L, 2);

        CDataStore** ud = packet ? static_cast<CDataStore**>(lua_touserdata(L, 1)) : nullptr;
        return ud ? *ud : nullptr;
    }

    // Security (Ruling): a received packet wraps the client's receive buffer (CDataStore(data, size), m_alloc -1),
    // which FetchWrite never grows, so the original's Put* writes past its end. Only CreatePacket's stores are written.
    CDataStore* WritableArg(lua_State* L) {
        CDataStore* s = Arg(L);
        return s && s->m_alloc != 0xFFFFFFFF ? s : nullptr;
    }

    // FUN_1030f620.
    int CreatePacket(lua_State* L) {
        const uint32_t opcode = static_cast<uint32_t>(ToInt(CheckNumber(L, 1)));
        CDataStore* store = new CDataStore();
        store->Put(opcode);
        PushPacket(L, store);
        return 1;
    }

    // handler_Send: Finalize (0x401130), then 0x406F40.
    int Send(lua_State* L) {
        if (CDataStore* s = Arg(L)) {
            s->Finalize();

            if (s_deps.send) {
                s_deps.send(s);
            }
        }

        return 0;
    }

    // PutData (0x47B1C0) of the value's bytes.
    template <class T>
    void Put(lua_State* L, T v) {
        if (CDataStore* s = WritableArg(L)) {
            s->PutData(&v, sizeof(T));
        }
    }

    int PutBool(lua_State* L) {   // handler_PutBool
        Put<uint8_t>(L, lua_toboolean(L, 2) != 0);
        return 0;
    }

    int PutInt8(lua_State* L) {   // FUN_1030fb80 (also PutUInt8)
        Put<uint8_t>(L, static_cast<uint8_t>(static_cast<int>(CheckNumber(L, 2))));
        return 0;
    }

    int PutInt16(lua_State* L) {   // FUN_1030fb00 (also PutUInt16)
        Put<uint16_t>(L, static_cast<uint16_t>(static_cast<int>(CheckNumber(L, 2))));
        return 0;
    }

    int PutInt32(lua_State* L) {   // handler_PutInt32
        Put<int32_t>(L, static_cast<int>(CheckNumber(L, 2)));
        return 0;
    }

    int PutUInt32(lua_State* L) {   // FUN_1030fcc0
        Put<uint32_t>(L, static_cast<uint32_t>(ToInt(CheckNumber(L, 2))));
        return 0;
    }

    int PutFloat(lua_State* L) {   // handler_PutFloat
        Put<float>(L, static_cast<float>(CheckNumber(L, 2)));
        return 0;
    }

    // FUN_1030fbd0: CDataStore::PutString (0x47B300), NUL included. The string is checked before the packet.
    int PutString(lua_State* L) {
        CDataStore* s = WritableArg(L);
        const char* v = luaL_checkstring(L, 2);

        if (s) {
            s->PutString(v);
        }

        return 0;
    }

    // The bytes left at the read cursor; 0 while the store is being written (m_read -1) or past its end.
    uint32_t Left(const CDataStore* s) {
        return s->m_read > s->m_size ? 0 : s->m_size - s->m_read;
    }

    const uint8_t* Cursor(const CDataStore* s) {
        return s->m_data + (s->m_read - s->m_base);
    }

    // The original reads *(T*)(m_data + m_read) and advances, whatever is left (Ruling: too few bytes -> nothing).
    template <class T>
    bool Get(lua_State* L, T& out) {
        CDataStore* s = Arg(L);

        if (!s || Left(s) < sizeof(T)) {
            return false;
        }

        memcpy(&out, Cursor(s), sizeof(T));
        s->m_read += sizeof(T);
        return true;
    }

    int GetBool(lua_State* L) {
        uint8_t v;

        if (!Get(L, v)) {
            return 0;
        }

        lua_pushboolean(L, v != 0);
        return 1;
    }

    template <class T>
    int GetInt(lua_State* L) {
        T v;

        if (!Get(L, v)) {
            return 0;
        }

        lua_pushnumber(L, static_cast<lua_Number>(v));
        return 1;
    }

    int GetFloat(lua_State* L) {
        float v;

        if (!Get(L, v)) {
            return 0;
        }

        lua_pushnumber(L, v);
        return 1;
    }

    // FUN_1030f8f0: a NUL-terminated string at the read cursor (Ruling: without a NUL before the end, nothing).
    int GetString(lua_State* L) {
        CDataStore* s = Arg(L);

        if (!s) {
            return 0;
        }

        const uint32_t left = Left(s);
        const uint8_t* at = Cursor(s);
        const void* nul = left ? memchr(at, 0, left) : nullptr;

        if (!nul) {
            return 0;
        }

        const uint32_t length = static_cast<uint32_t>(static_cast<const uint8_t*>(nul) - at);
        lua_pushlstring(L, reinterpret_cast<const char*>(at), length);
        s->m_read += length + 1;
        return 1;
    }

    // FUN_1030f7c0: a u64, formatted by 0x74D0D0 (GUIDToHexString: "0x" and 16 upper-case hex digits).
    int GetGUID(lua_State* L) {
        uint64_t v;

        if (!Get(L, v)) {
            return 0;
        }

        char buf[19];
        buf[0] = '0';
        buf[1] = 'x';
        buf[18] = '\0';

        for (int32_t i = 17; i >= 2; --i, v >>= 4) {
            const uint8_t nibble = static_cast<uint8_t>(v & 0xF);
            buf[i] = static_cast<char>(nibble >= 10 ? nibble + 'A' - 10 : nibble + '0');
        }

        lua_pushstring(L, buf);
        return 1;
    }

    // FUN_102c4400: above 0x55E the DLL's own table (replacing), otherwise the client's SetMessageHandler (0x6B0B80).
    void RouteOpcode(uint32_t opcode) {
        if (opcode > 0x55E) {
            CoA::Packets::Set(opcode, CoA::PacketApi::OnRegisteredPacket, nullptr);
        } else if (s_deps.setStockHandler) {
            s_deps.setStockHandler(opcode, CoA::PacketApi::OnRegisteredPacket);
        }
    }

    // FUN_1030fd10: luaL_ref takes the top of the stack (the function when called with two arguments).
    int RegisterPacket(lua_State* L) {
        const uint32_t opcode = static_cast<uint32_t>(ToInt(CheckNumber(L, 1)));
        luaL_checktype(L, 2, LUA_TFUNCTION);
        s_handlers[static_cast<uint16_t>(opcode)] = luaL_ref(L, LUA_REGISTRYINDEX);
        RouteOpcode(opcode);
        return 0;
    }

    // FUN_1030f550.
    int ClearPacket(lua_State* L) {
        s_handlers.erase(static_cast<uint16_t>(ToInt(CheckNumber(L, 1))));
        return 0;
    }

    // Ruling (review M4): a new Lua state (UI reload, glue <-> world) gets its natives before any addon runs; the old
    // state's registry refs are dropped here. The original keeps them and calls whatever the new registry holds there.
    void ForgetHandlers() {
        s_handlers.clear();
    }

    const CoA::Binding s_bindings[] = {
        COA_NATIVE("", "CreatePacket", CreatePacket),
        COA_NATIVE("@CDataStore", "Send", Send),
        COA_NATIVE("", "RegisterPacket", RegisterPacket),
        COA_NATIVE("", "ClearPacket", ClearPacket),
        COA_NATIVE("@CDataStore", "PutBool", PutBool),
        COA_NATIVE("@CDataStore", "PutInt8", PutInt8),
        COA_NATIVE("@CDataStore", "PutUInt8", PutInt8),
        COA_NATIVE("@CDataStore", "PutInt16", PutInt16),
        COA_NATIVE("@CDataStore", "PutUInt16", PutInt16),
        COA_NATIVE("@CDataStore", "PutInt32", PutInt32),
        COA_NATIVE("@CDataStore", "PutUInt32", PutUInt32),
        COA_NATIVE("@CDataStore", "PutFloat", PutFloat),
        COA_NATIVE("@CDataStore", "GetBool", GetBool),
        COA_NATIVE("@CDataStore", "GetInt8", GetInt<int8_t>),
        COA_NATIVE("@CDataStore", "GetUInt8", GetInt<uint8_t>),
        COA_NATIVE("@CDataStore", "GetInt16", GetInt<int16_t>),
        COA_NATIVE("@CDataStore", "GetUInt16", GetInt<uint16_t>),
        COA_NATIVE("@CDataStore", "GetInt32", GetInt<int32_t>),
        COA_NATIVE("@CDataStore", "GetUInt32", GetInt<int32_t>),
        COA_NATIVE("@CDataStore", "GetFloat", GetFloat),
        COA_NATIVE("@CDataStore", "GetGUID", GetGUID),
        COA_NATIVE("@CDataStore", "GetString", GetString),
        COA_NATIVE("@CDataStore", "PutString", PutString),
    };
}

CoA::PacketApi::Deps& CoA::PacketApi::Dependencies() {
    return s_deps;
}

// FUN_1030f410.
int32_t CoA::PacketApi::OnRegisteredPacket(void* param, uint32_t opcode, uint32_t time, CDataStore* msg) {
    auto it = s_handlers.find(static_cast<uint16_t>(opcode));

    if (it == s_handlers.end()) {
        return 1;
    }

    lua_State* L = CoA::ClientState::MainLuaState();

    if (!L) {
        return 1;
    }

    const int top = lua_gettop(L);
    lua_rawgeti(L, LUA_REGISTRYINDEX, it->second);
    lua_pushnumber(L, static_cast<lua_Number>(opcode & 0xFFFF));
    PushPacket(L, msg);
    // A copy of the packet below the call, to empty it afterwards (Ruling).
    lua_pushvalue(L, -1);
    lua_insert(L, top + 1);

    if (lua_pcall(L, 2, 0, 0) != 0) {
        const char* error = lua_tostring(L, -1);
        CoA::Log::Printf("RegisterPacket handler 0x%X: %s", opcode, error ? error : "");
        lua_pop(L, 1);
    }

    if (CDataStore** ud = static_cast<CDataStore**>(lua_touserdata(L, top + 1))) {
        *ud = nullptr;
    }

    lua_settop(L, top);
    return 1;
}

bool CoA::PacketApi::HasHandler(uint32_t opcode) {
    return s_handlers.count(static_cast<uint16_t>(opcode)) != 0;
}

void CoA::PacketApi::Reset() {
    s_handlers.clear();
    s_deps = Deps();
}

void CoA::PacketApi::Init() {
    CoA::LuaReg::OnGlueRegistered(ForgetHandlers);
    CoA::LuaReg::OnWorldRegistered(ForgetHandlers);
    CoA::LuaReg::AddModule(s_bindings, sizeof(s_bindings) / sizeof(s_bindings[0]));
}
