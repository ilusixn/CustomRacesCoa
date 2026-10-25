#include "CoA/Tests/Support.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/Events.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include "Engine/Source/lua-5.1/src/lualib.h"
#include <cstdarg>
#include <cstdio>
#include <cstdlib>

namespace {
    std::vector<std::string> s_signals;

    void RecordingSink(const char* name, const char* format, va_list args) {
        std::string line(name);
        char buffer[64];
        bool first = true;

        for (const char* f = format; f && *f; ++f) {
            if (*f != '%' || !f[1]) {
                continue;
            }

            ++f;

            switch (*f) {
                case 's': {
                    const char* s = va_arg(args, const char*);
                    snprintf(buffer, sizeof(buffer), "%s", s ? s : "(null)");
                    break;
                }
                case 'u':
                    snprintf(buffer, sizeof(buffer), "%u", va_arg(args, unsigned int));
                    break;
                case 'd':
                    snprintf(buffer, sizeof(buffer), "%d", va_arg(args, int));
                    break;
                case 'b':
                    snprintf(buffer, sizeof(buffer), "%s", va_arg(args, int) ? "true" : "false");
                    break;
                case 'f':
                    snprintf(buffer, sizeof(buffer), "%g", va_arg(args, double));
                    break;
                default:
                    snprintf(buffer, sizeof(buffer), "?%c", *f);
                    break;
            }

            line += first ? ":" : ",";
            line += buffer;
            first = false;
        }

        s_signals.push_back(line);
    }
}

int32_t CoATest::Deliver(int32_t (*handler)(void*, uint32_t, uint32_t, CDataStore*), uint32_t opcode, CoATest::Bytes& bytes, CDataStore** out) {
    static CDataStore* last = nullptr;
    delete last;
    last = new CDataStore(bytes.data.data(), static_cast<uint32_t>(bytes.data.size()));

    if (out) {
        *out = last;
    }

    return handler(nullptr, opcode, 0, last);
}

void CoATest::RecordSignals() {
    s_signals.clear();
    CoA::Events::SetSignalSink(RecordingSink);
}

void CoATest::StopRecordingSignals() {
    CoA::Events::SetSignalSink(nullptr);
}

std::vector<std::string>& CoATest::Signals() {
    return s_signals;
}

void CoATest::InitRealmModules() {
    CoA::RealmInfo::Init();
    CoA::Account::Init();
    CoA::Config::Init();
    CoA::GameEvents::Init();
    CoA::Misc::Init();
    CoA::GlobalsB::Init();
    CoA::GlobalsD::Init();
}

lua_State* CoATest::NewState(bool glue) {
    lua_State* L = luaL_newstate();
    lua_pushcfunction(L, luaopen_base);
    lua_call(L, 0, 0);
    CoA::LuaReg::Register(L, glue);
    return L;
}

// Arguments: "n:<number>", "s:<string>", "b:0"/"b:1", or "nil".
std::vector<std::string> CoATest::Call(lua_State* L, const char* ns, const char* name, const std::vector<std::string>& args) {
    std::vector<std::string> out;
    int base = lua_gettop(L);

    if (*ns) {
        lua_getfield(L, LUA_GLOBALSINDEX, ns);

        if (!lua_istable(L, -1)) {
            lua_settop(L, base);
            out.push_back("<no namespace>");
            return out;
        }

        lua_getfield(L, -1, name);
        lua_remove(L, -2);
    } else {
        lua_getfield(L, LUA_GLOBALSINDEX, name);
    }

    if (!lua_isfunction(L, -1)) {
        lua_settop(L, base);
        out.push_back("<no function>");
        return out;
    }

    for (const std::string& arg : args) {
        if (arg.compare(0, 2, "n:") == 0) {
            lua_pushnumber(L, atof(arg.c_str() + 2));
        } else if (arg.compare(0, 2, "s:") == 0) {
            lua_pushstring(L, arg.c_str() + 2);
        } else if (arg.compare(0, 2, "b:") == 0) {
            lua_pushboolean(L, arg[2] == '1');
        } else {
            lua_pushnil(L);
        }
    }

    if (lua_pcall(L, static_cast<int>(args.size()), LUA_MULTRET, 0) != 0) {
        out.push_back(std::string("<error> ") + (lua_tostring(L, -1) ? lua_tostring(L, -1) : ""));
        lua_settop(L, base);
        return out;
    }

    char buffer[64];

    for (int i = base + 1; i <= lua_gettop(L); ++i) {
        switch (lua_type(L, i)) {
            case LUA_TNIL:
                out.push_back("nil");
                break;
            case LUA_TBOOLEAN:
                out.push_back(lua_toboolean(L, i) ? "true" : "false");
                break;
            case LUA_TNUMBER:
                snprintf(buffer, sizeof(buffer), "%.10g", lua_tonumber(L, i));
                out.push_back(buffer);
                break;
            case LUA_TSTRING:
                out.push_back(lua_tostring(L, i));
                break;
            default:
                out.push_back(lua_typename(L, lua_type(L, i)));
                break;
        }
    }

    lua_settop(L, base);
    return out;
}
