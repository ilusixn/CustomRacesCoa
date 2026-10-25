// C_Logger (0x10291F50..0x102933A0) and ReportMetric; port of AscLogger.cpp's bindings (the channel files are
// CoA::Logger).
//
// Each binding runs string.format (0x853C50) over its arguments, reads the result (luaL_checklstring(L, -1)), and
// when it is not empty writes "HH:MM:SS [<short_src>:<currentline>] <text>" with the caller's frame from
// lua_getstack(L, 1) / lua_getinfo("nSl"). Info and Debug only log while the CVars logInfo / logDebug (glue CVars,
// default "1") are non-zero. ClearLUA reopens LUA.txt.
#include "CoA/Source/Modules/GlobalsJ.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/Logger.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Packet.h"
#include "CoA/Source/Runtime/Script.h"
#include "Engine/Source/ConsoleBase/ConsoleVar.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include "Engine/Source/lua-5.1/src/lualib.h"
#include <ctime>
#include <string>

namespace {
    // The shared body of every level binding; text is the formatted message.
    void Log(lua_State* L, uint32_t channel, const std::string& text) {
        if (text.empty()) {
            return;
        }

        lua_Debug ar = {};
        lua_getstack(L, 1, &ar);
        lua_getinfo(L, "nSl", &ar);
        char stamp[16];
        time_t now = time(nullptr);
        tm local;
        localtime_s(&local, &now);
        strftime(stamp, sizeof(stamp), "%H:%M:%S", &local);
        CoA::Logger::Write(channel, std::string(stamp) + " [" + ar.short_src + ":" + std::to_string(ar.currentline) + "] " + text);
    }

    std::string Formatted(lua_State* L) {
        lua_strformat(L);
        const char* s = CoA::Script::CheckString(L, -1);
        return s ? s : "";
    }

    // FUN_10114480 -> CVar::Lookup; the integer value.
    bool CVarOn(const char* name) {
        CVar* var = CVar::Lookup(name);
        return var && var->m_intValue != 0;
    }

    int Trace(lua_State* L) {   // FUN_10293100
        Log(L, CoA::Logger::Trace, Formatted(L));
        return 0;
    }

    int Warning(lua_State* L) {   // 0x102933A0
        Log(L, CoA::Logger::Warning, Formatted(L));
        return 0;
    }

    int Error(lua_State* L) {   // FUN_10292170
        Log(L, CoA::Logger::Error, Formatted(L));
        return 0;
    }

    int Fatal(lua_State* L) {   // FUN_10292410
        Log(L, CoA::Logger::Fatal, Formatted(L));
        return 0;
    }

    int LFG(lua_State* L) {   // FUN_102928b0
        Log(L, CoA::Logger::LFG, Formatted(L));
        return 0;
    }

    int LUA(lua_State* L) {   // FUN_10292b50
        Log(L, CoA::Logger::LUA, Formatted(L));
        return 0;
    }

    int Debug(lua_State* L) {   // handler_Debug
        if (CVarOn("logDebug")) {
            Log(L, CoA::Logger::Debug, Formatted(L));
        }

        return 0;
    }

    int Info(lua_State* L) {   // 0x102926B0
        if (CVarOn("logInfo")) {
            Log(L, CoA::Logger::Info, Formatted(L));
        }

        return 0;
    }

    int ClearLUA(lua_State*) {   // handler_ClearLUA: FUN_102912c0(7)
        CoA::Logger::Reopen(CoA::Logger::LUA);
        return 0;
    }

    // ReportMetric(name, value) (FUN_10292df0): not while a known secure addon is running (lua_tainted). Otherwise,
    // with both strings non-empty, CMSG 0x55A {u32 length, name, u32 length, value} (no terminators) and the binding
    // returns 1 with nothing pushed -- the caller gets the top of the stack, as in the original. Else nothing.
    int ReportMetric(lua_State* L) {
        if (lua_tainted && CoA::GlobalsJ::IsSecureAddon(lua_tainted)) {
            return 0;
        }

        std::string name = CoA::Script::CheckString(L, 1);
        std::string value = CoA::Script::CheckString(L, 2);

        if (name.empty() || value.empty()) {
            return 0;
        }

        CoA::Packet packet(0x55A);
        packet.U32(static_cast<uint32_t>(name.size())).Data(name.data(), static_cast<uint32_t>(name.size()));
        packet.U32(static_cast<uint32_t>(value.size())).Data(value.data(), static_cast<uint32_t>(value.size()));
        packet.Send();
        return 1;
    }

    const CoA::Binding s_bindings[] = {
        COA_NATIVE("", "ReportMetric", ReportMetric),   // a bare global: FUN_10293d30 registers it after C_Logger
        COA_NATIVE("C_Logger", "Trace", Trace),
        COA_NATIVE("C_Logger", "Error", Error),
        COA_NATIVE("C_Logger", "Debug", Debug),
        COA_NATIVE("C_Logger", "Info", Info),
        COA_NATIVE("C_Logger", "Warning", Warning),
        COA_NATIVE("C_Logger", "Fatal", Fatal),
        COA_NATIVE("C_Logger", "LUA", LUA),
        COA_NATIVE("C_Logger", "ClearLUA", ClearLUA),
        COA_NATIVE("C_Logger", "LFG", LFG),
    };
}

void CoA::LoggerApi::Init() {
    CoA::LuaReg::AddModule(s_bindings, sizeof(s_bindings) / sizeof(s_bindings[0]));
}
