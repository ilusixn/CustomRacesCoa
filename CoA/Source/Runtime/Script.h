#ifndef COA_SOURCE_RUNTIME_SCRIPT_H
#define COA_SOURCE_RUNTIME_SCRIPT_H

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <map>
#include <string>

struct lua_State;

// The DLL's own scripting substrate (port of AscScript): every binding reads its arguments through these few
// helpers, so the accepted argument set is the original's, quirks included.
//
//   ValidateInput   FUN_10309e50  logs and returns false on a mismatch; never raises a Lua error
//   ReadNumber      FUN_1008ba00  {number}          -> truncated u32
//   ReadString      FUN_100b8970  {string}          -> luaL_checklstring(L, 1)
//   ReadInt2        FUN_100b88c0  {number, number}  -> two truncated ints
//   ReadBool        FUN_100b8aa0  {boolean}         -> lua_toboolean(L, 1)
namespace CoA::Script {
    // Slot types are the LUA_T* values. A boolean slot accepts nil and numbers; trailing nil slots are not
    // required but a present, non-nil argument there is rejected.
    bool ValidateInput(lua_State* L, std::initializer_list<int> types);
    bool ValidateInput(lua_State* L, const int* types, size_t count);

    double CheckNumber(lua_State* L, int idx);
    const char* CheckString(lua_State* L, int idx);

    inline int32_t ToInt(double d) { return static_cast<int32_t>(static_cast<int64_t>(d)); }

    bool ReadNumber(lua_State* L, uint32_t& out);
    bool ReadString(lua_State* L, std::string& out);
    bool ReadInt2(lua_State* L, int32_t& a, int32_t& b);
    bool ReadBool(lua_State* L, bool& out);

    void PushStr(lua_State* L, const char* s);   // nullptr pushes ""
    void PushInt(lua_State* L, int32_t v);
    void PushNum(lua_State* L, double v);
    void PushBool(lua_State* L, bool v);
    int PushNils(lua_State* L, int n);
    void SetField(lua_State* L, const char* key, int32_t value);   // t[key] = value, t at -1

    // FUN_100bed80 (FrameScript::lua_totable into a string -> bool map): string or number keys (lua_tolstring converts
    // a number key in place, which breaks the walk as in the original), nil / boolean / number values, the last write
    // winning. A non-table, a bad key or a bad value logs and leaves the map empty.
    void ToBoolMap(lua_State* L, int idx, std::map<std::string, bool>& out);

    // The value after "-name", "/name" or "--name" on the command line, or nullptr (ScriptClient.cpp).
    const char* CommandLineArg(const char* name);
}

#endif
