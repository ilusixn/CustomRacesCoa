// AscFriendsMetadata.cpp (see FriendsMetadata.h).
#include "CoA/Source/Modules/FriendsMetadata.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/Events.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Packet.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Source/Runtime/Read.h"
#include "CoA/Source/Runtime/Script.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <string>
#include <unordered_map>

namespace {
    std::unordered_map<std::string, std::string> s_meta;   // 0x10BE2520

    std::string Get(const std::string& name) {   // FUN_10238760: "" for an unknown name
        auto it = s_meta.find(name);
        return it == s_meta.end() ? std::string() : it->second;
    }

    int32_t OnFriendAddonList(void*, uint32_t, uint32_t, CDataStore* msg) {   // 0x9C0 FUN_102388c0
        for (uint32_t n = CoA::Read::U32(msg); n && CoA::Read::Remaining(msg) != 0; --n) {
            const std::string name = CoA::Read::Name(msg);
            const std::string value = CoA::Read::Name(msg);

            if (Get(name) == value) {
                continue;
            }

            s_meta[name] = value;   // FUN_10239080
            CoA::Events::Signal("FRIEND_METADATA_CHANGED", "%s%s", name.c_str(), value.c_str());
        }

        return 1;
    }

    // FUN_10238590: nil for an empty or unknown name.
    int GetFriendsMetadata(lua_State* L) {
        const std::string name = CoA::Script::CheckString(L, 1);
        auto it = name.empty() ? s_meta.end() : s_meta.find(name);

        if (it == s_meta.end()) {
            lua_pushnil(L);
        } else {
            CoA::Script::PushStr(L, it->second.c_str());
        }

        return 1;
    }

    // FUN_10238e50: CMSG 0x556 { u32 len + name, u32 len + value }; returns 1 without pushing (the top argument).
    int SetFriendsMetadata(lua_State* L) {
        const std::string name = CoA::Script::CheckString(L, 1);
        const std::string value = CoA::Script::CheckString(L, 2);
        CoA::Packet p(0x556);
        p.U32(static_cast<uint32_t>(name.size()));

        if (!name.empty()) {
            p.Data(name.data(), static_cast<uint32_t>(name.size()));
        }

        p.U32(static_cast<uint32_t>(value.size()));

        if (!value.empty()) {
            p.Data(value.data(), static_cast<uint32_t>(value.size()));
        }

        p.Send();
        return 1;
    }

    const CoA::Binding s_bindings[] = {
        COA_NATIVE("", "GetFriendsMetadata", GetFriendsMetadata),
        COA_NATIVE("", "SetFriendsMetadata", SetFriendsMetadata),
    };
}

void CoA::FriendsMetadata::Init() {
    CoA::LuaReg::AddModule(s_bindings, sizeof(s_bindings) / sizeof(s_bindings[0]));
    COA_EVENT("FRIEND_METADATA_CHANGED");
    COA_PACKET(0x9C0, OnFriendAddonList);
}

void CoA::FriendsMetadata::Reset() {
    s_meta.clear();
}
