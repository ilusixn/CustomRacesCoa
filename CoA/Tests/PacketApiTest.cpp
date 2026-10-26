#include "CoA/Source/Modules/PacketApi.h"
#include "CoA/Source/Runtime/ClientState.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Tests/Support.h"
#include "CoA/Tests/TestMain.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <cstring>
#include <string>
#include <vector>

namespace {
    lua_State* s_L = nullptr;
    std::vector<std::vector<uint8_t>> s_sent;
    std::vector<uint32_t> s_stock;

    lua_State* MainState() { return s_L; }

    void Send(CDataStore* msg) {
        s_sent.emplace_back(msg->m_data, msg->m_data + msg->m_size);
        CHECK(msg->m_read == 0);   // finalized
    }

    void Stock(uint32_t opcode, CoA::PacketApi::Handler handler) {
        CHECK(handler == CoA::PacketApi::OnRegisteredPacket);
        s_stock.push_back(opcode);
    }

    void Start() {
        CoA::LuaReg::Reset();
        CoA::Packets::Clear();
        CoA::PacketApi::Reset();
        CoA::PacketApi::Init();
        CoA::PacketApi::Deps& d = CoA::PacketApi::Dependencies();
        d.send = Send;
        d.setStockHandler = Stock;
        s_sent.clear();
        s_stock.clear();

        if (s_L) {
            lua_close(s_L);
        }

        s_L = CoATest::NewState(false);
        CoA::ClientState::SetMainLuaStateProbe(MainState);
    }

    bool Run(const char* code) {
        if (luaL_loadstring(s_L, code) || lua_pcall(s_L, 0, 0, 0)) {
            printf("lua: %s\n", lua_tostring(s_L, -1));
            lua_settop(s_L, 0);
            return false;
        }

        return true;
    }

    std::string Global(const char* name) {
        lua_getfield(s_L, LUA_GLOBALSINDEX, name);
        std::string out;

        switch (lua_type(s_L, -1)) {
            case LUA_TNIL: out = "nil"; break;
            case LUA_TBOOLEAN: out = lua_toboolean(s_L, -1) ? "true" : "false"; break;
            case LUA_TNUMBER: {
                char buf[64];
                snprintf(buf, sizeof(buf), "%.9g", lua_tonumber(s_L, -1));
                out = buf;
                break;
            }
            default: out = lua_tostring(s_L, -1) ? lua_tostring(s_L, -1) : "?"; break;
        }

        lua_pop(s_L, 1);
        return out;
    }

    int32_t Deliver(uint32_t opcode, CoATest::Bytes& bytes) {
        return CoATest::Deliver(CoA::PacketApi::OnRegisteredPacket, opcode, bytes);
    }
}

COA_TEST(PacketApi_RegistersGlobalsAndTheCDataStoreMethods) {
    Start();
    CHECK(Run("assert(type(CreatePacket) == 'function' and type(ClearPacket) == 'function' and type(RegisterPacket) == 'function')"));
    const char* methods[] = { "GetBool", "GetFloat", "GetGUID", "GetInt16", "GetInt32", "GetInt8", "GetString", "GetUInt16",
        "GetUInt32", "GetUInt8", "PutBool", "PutFloat", "PutInt16", "PutInt32", "PutInt8", "PutString", "PutUInt16",
        "PutUInt32", "PutUInt8", "Send" };

    for (const char* m : methods) {
        std::string code = std::string("assert(type(CDataStore.") + m + ") == 'function')";
        CHECK(Run(code.c_str()));
    }

    // The metatable is the registry's "CDataStore" table, its own __index.
    CHECK(Run("local p = CreatePacket(1); assert(getmetatable(p) == CDataStore and CDataStore.__index == CDataStore)"));
}

// The 32-bit wire sizes: u32 opcode, then each Put at its own width, PutString with its NUL; Send finalizes and sends.
COA_TEST(PacketApi_CreatePutSend) {
    Start();
    CHECK(Run("local p = CreatePacket(0x1234)\n"
              "p:PutUInt8(0x1FF) p:PutInt8(-1) p:PutBool(true) p:PutBool(nil)\n"
              "p:PutInt16(-2) p:PutUInt16(0x10001) p:PutInt32(-3) p:PutUInt32(4294967295)\n"
              "p:PutFloat(1.5) p:PutString('ab')\n"
              "p:Send()"));
    CHECK(s_sent.size() == 1);
    CoATest::Bytes want;
    want.U8(0xFF).U8(0xFF).U8(1).U8(0).U16(0xFFFE).U16(1).I32(-3).U32(0xFFFFFFFF).F32(1.5f).Str("ab");

    if (s_sent.size() == 1) {
        CHECK(s_sent[0] == CoATest::Expected(0x1234, want));
    }

    // Send again: the same bytes, finalized again.
    CHECK(Run("local p = CreatePacket(7) p:Send() p:Send()"));
    CHECK(s_sent.size() == 3);
}

COA_TEST(PacketApi_GettersReadTheReceivedPacket) {
    Start();
    CHECK(Run("RegisterPacket(0x600, function(op, p)\n"
              "  OP = op; U8 = p:GetUInt8(); I8 = p:GetInt8(); B = p:GetBool(); I16 = p:GetInt16(); U16 = p:GetUInt16()\n"
              "  I32 = p:GetInt32(); U32 = p:GetUInt32(); F = p:GetFloat(); G = p:GetGUID(); S = p:GetString(); S2 = p:GetString()\n"
              "end)"));
    CoATest::Bytes b;
    b.U8(200).U8(0xFE).U8(2).U16(0xFFFF).U16(0xFFFF).I32(-5).U32(0xFFFFFFFF).F32(2.25f).U64(0xABCDEF0012345678ull).Str("hi").Str("");
    CHECK(Deliver(0x10600, b) == 1);
    CHECK(Global("OP") == "1536");   // opcode & 0xFFFF
    CHECK(Global("U8") == "200");
    CHECK(Global("I8") == "-2");
    CHECK(Global("B") == "true");
    CHECK(Global("I16") == "-1");
    CHECK(Global("U16") == "65535");
    CHECK(Global("I32") == "-5");
    CHECK(Global("U32") == "-1");    // GetUInt32 is the original's GetInt32
    CHECK(Global("F") == "2.25");
    CHECK(Global("G") == "0xABCDEF0012345678");
    CHECK(Global("S") == "hi");
    CHECK(Global("S2") == "");
}

// Review focus: a getter never reads past the packet end; it returns nothing and leaves the cursor.
COA_TEST(PacketApi_GettersStopAtThePacketEnd) {
    Start();
    CHECK(Run("RegisterPacket(0x601, function(op, p)\n"
              "  A = p:GetUInt32(); B = p:GetUInt8(); C = p:GetUInt8(); D = p:GetGUID(); E = p:GetString(); F = p:GetBool()\n"
              "end)"));
    CoATest::Bytes b;
    b.U8(9);
    Deliver(0x601, b);
    CHECK(Global("A") == "nil");
    CHECK(Global("B") == "9");
    CHECK(Global("C") == "nil");
    CHECK(Global("D") == "nil");
    CHECK(Global("E") == "nil");
    CHECK(Global("F") == "nil");

    // A string without its terminator is not read.
    CHECK(Run("RegisterPacket(0x602, function(op, p) S = p:GetString(); N = p:GetUInt8() end)"));
    CoATest::Bytes s;
    s.Raw("abc", 3);
    Deliver(0x602, s);
    CHECK(Global("S") == "nil");
    CHECK(Global("N") == "97");

    // A packet still being written has nothing to read.
    CHECK(Run("local p = CreatePacket(5) p:PutUInt32(1) W = p:GetUInt8()"));
    CHECK(Global("W") == "nil");
}

// A received packet kept past its handler reads nothing (the store is the client's and gone).
COA_TEST(PacketApi_KeptPacketIsEmptiedAfterTheHandler) {
    Start();
    CHECK(Run("RegisterPacket(0x603, function(op, p) KEPT = p end)"));
    CoATest::Bytes b;
    b.U32(77);
    Deliver(0x603, b);
    CHECK(Run("X = KEPT:GetUInt32() KEPT:PutUInt8(1) KEPT:Send()"));
    CHECK(Global("X") == "nil");
    CHECK(s_sent.empty());
}

COA_TEST(PacketApi_RoutingAndClear) {
    Start();
    // Above 0x55E: the DLL's table; at or below: the client's SetMessageHandler.
    CHECK(Run("RegisterPacket(0x55F, function() HIT = (HIT or 0) + 1 end) RegisterPacket(0x55E, function() LOW = 1 end)"));
    CHECK(CoA::Packets::Has(0x55F));
    CHECK(!CoA::Packets::Has(0x55E));
    CHECK(s_stock.size() == 1 && s_stock[0] == 0x55E);
    CHECK(CoA::PacketApi::HasHandler(0x55F) && CoA::PacketApi::HasHandler(0x55E));

    CoATest::Bytes b;
    Deliver(0x55F, b);
    CHECK(Global("HIT") == "1");

    // ClearPacket forgets the function; the opcode stays routed and the handler then finds nothing.
    CHECK(Run("ClearPacket(0x55F)"));
    CHECK(!CoA::PacketApi::HasHandler(0x55F));
    CHECK(CoA::Packets::Has(0x55F));
    CHECK(Deliver(0x55F, b) == 1);
    CHECK(Global("HIT") == "1");

    // A handler that raises is contained; the packet still counts as handled.
    CHECK(Run("RegisterPacket(0x700, function() error('boom') end)"));
    CHECK(Deliver(0x700, b) == 1);
    CHECK(lua_gettop(s_L) == 0);

    // A non-function second argument is a Lua error.
    CHECK(!Run("RegisterPacket(0x701, 5)"));
    CHECK(!CoA::PacketApi::HasHandler(0x701));
}

// Without the registry metatable (a state that never registered the methods) CreatePacket returns nil.
COA_TEST(PacketApi_NilWithoutTheMetatable) {
    Start();
    lua_pushnil(s_L);
    lua_setfield(s_L, LUA_REGISTRYINDEX, "CDataStore");
    CHECK(Run("P = CreatePacket(3)"));
    CHECK(Global("P") == "nil");
}
