#include "CoA/Source/Modules/GlobalsJ.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/ClientState.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Packet.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Source/Runtime/Read.h"
#include "CoA/Source/Runtime/Script.h"
#include "Engine/Source/ConsoleBase/ConsoleVar.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <Windows.h>
#include <cstdio>
#include <share.h>
#include <unordered_map>
#include <vector>

namespace {
    // ---- script error handler -------------------------------------------------------------------------------
    int32_t s_errorCallstackHeight = 0;   // 0x10BE3088

    int GetErrorCallstackHeight(lua_State* L) {
        CoA::Script::PushInt(L, s_errorCallstackHeight);
        return 1;
    }

    // ---- CanPerformAction -----------------------------------------------------------------------------------
    // Action 0x19 answers whether Script_RunScript is running; every other action is the client's own gate.
    int32_t s_insideProtectedCall = 0;

    int CanPerformAction(lua_State* L) {
        int32_t action = CoA::Script::ToInt(CoA::Script::CheckNumber(L, 1));

        if (action == 0x19) {
            CoA::Script::PushBool(L, s_insideProtectedCall != 0);
            return 1;
        }

        CoA::Script::PushBool(L, CoA::ClientState::CanPerformAction(action));
        return 1;
    }

    // ---- known / secure addons ------------------------------------------------------------------------------
    struct KnownAddon {
        std::string name;
        bool secure;
    };

    std::vector<KnownAddon> s_knownAddons;

    // GetKnownAddons / GetSecureAddons: every (secure) name that does not contain "Blizzard", pushed as separate
    // return values.
    int PushAddonNames(lua_State* L, bool secureOnly) {
        int n = 0;

        for (const KnownAddon& addon : s_knownAddons) {
            if (secureOnly && !addon.secure) {
                continue;
            }

            if (addon.name.size() >= 8 && addon.name.find("Blizzard") != std::string::npos) {
                continue;
            }

            CoA::Script::PushStr(L, addon.name.c_str());
            ++n;
        }

        return n;
    }

    int GetKnownAddons(lua_State* L) {
        return PushAddonNames(L, false);
    }

    int GetSecureAddons(lua_State* L) {
        return PushAddonNames(L, true);
    }

    // An exact name match, Blizzard ones included.
    int IsKnownAddon(lua_State* L) {
        std::string name = CoA::Script::CheckString(L, 1);
        CoA::Script::PushBool(L, CoA::GlobalsJ::IsKnownAddon(name.c_str()));
        return 1;
    }

    // ---- custom WTF files and saved CVars -------------------------------------------------------------------
    // ReadCustomWTF(name) / WriteCustomWTF(name, text) (FUN_10259d10 / FUN_1025a1d0): text mode. Both need the
    // client's action gate 0x11; without it they return 1 value having pushed nothing (the caller gets the top of
    // the stack). Read: "" when the directory is missing or the file cannot be read. Write: false for text over
    // 0x6400000 bytes or when the directory cannot be created, else whether the stream stayed good.
    int ReadCustomWTF(lua_State* L) {
        if (!CoA::ClientState::CanPerformAction(0x11)) {
            return 1;
        }

        std::string name = CoA::Script::CheckString(L, 1);
        std::string text;

        if (GetFileAttributesA("WTF\\Custom") != INVALID_FILE_ATTRIBUTES) {
            if (FILE* f = _fsopen(CoA::GlobalsJ::CustomWtfPath(name).c_str(), "r", _SH_DENYNO)) {
                char buffer[4096];
                size_t n;

                while ((n = fread(buffer, 1, sizeof(buffer), f)) > 0) {
                    text.append(buffer, n);
                }

                fclose(f);
            }
        }

        CoA::Script::PushStr(L, text.c_str());
        return 1;
    }

    int WriteCustomWTF(lua_State* L) {
        if (!CoA::ClientState::CanPerformAction(0x11)) {
            return 1;
        }

        std::string name = CoA::Script::CheckString(L, 1);
        std::string text = CoA::Script::CheckString(L, 2);

        if (text.size() > 0x6400000) {
            CoA::Script::PushBool(L, false);
            return 1;
        }

        if (GetFileAttributesA("WTF\\Custom") == INVALID_FILE_ATTRIBUTES && !CreateDirectoryA("WTF\\Custom", nullptr)) {
            CoA::Script::PushBool(L, false);
            return 1;
        }

        bool good = false;

        if (FILE* f = _fsopen(CoA::GlobalsJ::CustomWtfPath(name).c_str(), "w", _SH_DENYNO)) {
            good = fwrite(text.data(), 1, text.size(), f) == text.size();
            good = fclose(f) == 0 && good;
        }

        CoA::Script::PushBool(L, good);
        return 1;
    }

    // RegisterSavedCVar(name, default [, perCharacter]) (FUN_10312910): refused (false) while an addon runs
    // (lua_tainted) that is not a known secure one; otherwise CVar::Register with no help text, flags 0x80000001
    // (0x80000021 when a third argument is true) -> true. The original passes category 5 to FUN_10114510, which
    // zeroes its trailing arguments before jumping to CVar::Register, so the client registers category 0.
    int RegisterSavedCVar(lua_State* L) {
        if (lua_tainted) {
            bool blocked = true;

            for (const KnownAddon& addon : s_knownAddons) {
                if (addon.name == lua_tainted) {
                    blocked = !addon.secure;
                    break;
                }
            }

            if (blocked) {
                CoA::Script::PushBool(L, false);
                return 1;
            }
        }

        std::string name = CoA::Script::CheckString(L, 1);
        std::string value = CoA::Script::CheckString(L, 2);
        uint32_t flags = 0x80000001;

        if (lua_gettop(L) == 3) {
            flags = lua_toboolean(L, 3) ? 0x80000021 : 0x80000001;
        }

        CVar::Register(name.c_str(), "", flags, value.c_str(), nullptr, 0, false, nullptr, false);
        CoA::Script::PushBool(L, true);
        return 1;
    }

    // ---- character select ---------------------------------------------------------------------------------
    // Maps 0x10BDB5FC / 0x10BDB61C, keyed by the character's list index; never cleared.
    struct SelectionMail {
        bool mail = false;
        bool storeMail = false;
    };

    struct SelectionGameMode {
        uint32_t a = 0;
        uint32_t b = 0;
        uint32_t c = 0;
        bool d = false;
        uint8_t e = 0;
    };

    std::unordered_map<uint32_t, SelectionMail> s_selectionMail;
    std::unordered_map<uint32_t, SelectionGameMode> s_selectionGameMode;

    int32_t OnSelectionMail(void* param, uint32_t opcode, uint32_t time, CDataStore* msg) {   // handler_0x0770
        uint32_t index = CoA::Read::U32(msg);
        SelectionMail& m = s_selectionMail[index];
        m.mail = CoA::Read::U8(msg) != 0;
        m.storeMail = CoA::Read::U8(msg) != 0;
        return 1;
    }

    int32_t OnSelectionGameMode(void* param, uint32_t opcode, uint32_t time, CDataStore* msg) {   // FUN_100d3910
        uint32_t index = CoA::Read::U32(msg);
        SelectionGameMode& g = s_selectionGameMode[index];
        g.a = CoA::Read::U32(msg);
        g.b = CoA::Read::U32(msg);
        g.c = CoA::Read::U32(msg);
        g.d = CoA::Read::U8(msg) != 0;
        g.e = CoA::Read::U8(msg);
        return 1;
    }

    int GetCharacterSelectionMailData(lua_State* L) {
        uint32_t index;

        if (!CoA::Script::ReadNumber(L, index)) {
            return 0;
        }

        auto it = s_selectionMail.find(index);

        if (it == s_selectionMail.end()) {
            lua_pushnil(L);
            return 1;
        }

        CoA::Script::PushBool(L, it->second.mail);
        CoA::Script::PushBool(L, it->second.storeMail);
        return 2;
    }

    int GetCharacterSelectionGameModeData(lua_State* L) {
        uint32_t index;

        if (!CoA::Script::ReadNumber(L, index)) {
            return 0;
        }

        auto it = s_selectionGameMode.find(index);

        if (it == s_selectionGameMode.end()) {
            lua_pushnil(L);
            return 1;
        }

        const SelectionGameMode& g = it->second;
        CoA::Script::PushInt(L, static_cast<int32_t>(g.a));
        CoA::Script::PushInt(L, static_cast<int32_t>(g.b));
        CoA::Script::PushInt(L, static_cast<int32_t>(g.c));
        CoA::Script::PushBool(L, g.d);
        CoA::Script::PushInt(L, g.e);
        return 5;
    }

    // FUN_100dd280(order): CMSG 0x772 {string order, NUL included}.
    int SetCharacterSelectionSortOrder(lua_State* L) {
        std::string order;

        if (CoA::Script::ReadString(L, order)) {
            CoA::Packet(0x772).Data(order.c_str(), static_cast<uint32_t>(order.size() + 1)).Send();
        }

        return 0;
    }

    const CoA::Binding s_bindings[] = {
        COA_NATIVE("", "GetCharacterSelectionMailData", GetCharacterSelectionMailData),
        COA_NATIVE("", "GetCharacterSelectionGameModeData", GetCharacterSelectionGameModeData),
        COA_NATIVE("", "SetCharacterSelectionSortOrder", SetCharacterSelectionSortOrder),
        COA_NATIVE("", "GetErrorCallstackHeight", GetErrorCallstackHeight),
        COA_NATIVE("", "CanPerformAction", CanPerformAction),
        COA_NATIVE("", "GetKnownAddons", GetKnownAddons),
        COA_NATIVE("", "GetSecureAddons", GetSecureAddons),
        COA_NATIVE("", "IsKnownAddon", IsKnownAddon),
        COA_NATIVE("", "ReadCustomWTF", ReadCustomWTF),
        COA_NATIVE("", "WriteCustomWTF", WriteCustomWTF),
        COA_NATIVE("", "RegisterSavedCVar", RegisterSavedCVar),
    };
}

int32_t CoA::GlobalsJ::OnKnownAddonList(void* param, uint32_t opcode, uint32_t time, CDataStore* msg) {
    s_knownAddons.clear();

    for (uint32_t n = CoA::Read::U32(msg); n && !CoA::Read::Failed(msg); --n) {
        KnownAddon addon;
        addon.name = CoA::Read::Str(msg);
        addon.secure = CoA::Read::U8(msg) != 0;

        if (CoA::Read::Failed(msg)) {
            break;
        }

        s_knownAddons.push_back(addon);
    }

    return 1;
}

bool CoA::GlobalsJ::IsKnownAddon(const char* name) {
    for (const KnownAddon& addon : s_knownAddons) {
        if (addon.name == name) {
            return true;
        }
    }

    return false;
}

bool CoA::GlobalsJ::IsSecureAddon(const char* name) {
    for (const KnownAddon& addon : s_knownAddons) {
        if (addon.name == name) {
            return addon.secure;
        }
    }

    return false;
}

std::string CoA::GlobalsJ::CustomWtfPath(const std::string& name) {
    return "WTF\\Custom\\" + name + ".json";
}

void CoA::GlobalsJ::SetInsideRunScript(bool inside) {
    s_insideProtectedCall = inside ? 1 : 0;
}

void CoA::GlobalsJ::OnErrorHandler(lua_State* L, int32_t entering) {
    if (!entering) {
        s_errorCallstackHeight = 0;
        return;
    }

    lua_Debug ar;

    while (lua_getstack(L, s_errorCallstackHeight, &ar)) {
        ++s_errorCallstackHeight;
    }
}

void CoA::GlobalsJ::Reset() {
    s_knownAddons.clear();
    s_errorCallstackHeight = 0;
    s_insideProtectedCall = 0;
}

void CoA::GlobalsJ::Init() {
    COA_PACKET(0x94E, CoA::GlobalsJ::OnKnownAddonList);
    COA_PACKET(0x770, OnSelectionMail);
    COA_PACKET(0x771, OnSelectionGameMode);
    CoA::LuaReg::AddModule(s_bindings, sizeof(s_bindings) / sizeof(s_bindings[0]));
}
