// AscDiscord.cpp (see Discord.h): DiscordMgr's state, packets and natives; the SDK itself is behind Deps
// (DiscordClient.cpp), checked against the genuine DLL (FUN_10220b10, FUN_10221060, 0x10220E80 / 0x10220EC0).
#include "CoA/Source/Modules/Discord.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/ClientHooks.h"
#include "CoA/Source/Runtime/Log.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Packet.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Source/Runtime/Read.h"
#include "CoA/Source/Runtime/Script.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <atomic>
#include <string>

using CoA::Script::PushBool;

namespace {
    struct Mgr {
        std::atomic<bool> enabled{ false };   // +0x04 (the callback thread reads it)
        bool activitySubscribed = false;      // +0x05
        int32_t result = 0;                   // +0x08
        bool userSubscribed = false;          // Initialize's OnCurrentUserUpdate lambda registered
        // CoA64's client-built status: the server took over (0x90C / 0x90D seen), the world entry time, the last
        // activity sent (to skip unchanged ones).
        bool serverStatus = false;
        int64_t worldStart = 0;
        bool sentAny = false;
        CoA::Discord::Activity lastSent;
    };

    CoA::Discord::Deps s_deps;
    Mgr s_mgr;

    // CoA64: the art key in CoA64's own Discord application (DiscordClient's kClientId); the original's
    // "ascension_logo" lives in Ascension's application.
    constexpr const char* kLogo = "descension_logo";
    // The project's website, a button on the client-built status (user request).
    constexpr const char* kWebsite = "https://coa-development.org";

    // The small image (user decision): the faction in world, the application icon otherwise.
    constexpr const char* kAppIcon = "app_icon";

    void SetFaction(CoA::Discord::Activity& a, const std::string& faction) {
        if (faction == "Horde" || faction == "Alliance") {
            a.smallImage = faction == "Horde" ? "si_horde" : "si_alliance";
            a.smallText = faction;
        } else {
            a.smallImage = kAppIcon;
        }
    }

    // The wrapper's setters (strncpy 0x80, last byte 0): cut at the first NUL and at 127 bytes.
    std::string Cut(const std::string& s) {
        return std::string(s.c_str()).substr(0, 0x7F);
    }

    void UpdateActivity(const CoA::Discord::Activity& a) {   // FUN_10088100
        if (s_deps.updateActivity) {
            s_deps.updateActivity(a);
        }
    }

    bool Same(const CoA::Discord::Activity& a, const CoA::Discord::Activity& b) {
        return a.state == b.state && a.details == b.details && a.start == b.start && a.end == b.end &&
            a.largeImage == b.largeImage && a.largeText == b.largeText && a.smallImage == b.smallImage &&
            a.smallText == b.smallText && a.buttons == b.buttons;
    }

    bool ClientStatusOn() {
        return s_deps.clientStatus && s_mgr.enabled && !s_mgr.serverStatus;
    }

    void SendClientStatus(const CoA::Discord::Activity& a) {
        if (s_mgr.sentAny && Same(a, s_mgr.lastSent)) {
            return;
        }

        s_mgr.lastSent = a;
        s_mgr.sentAny = true;
        UpdateActivity(a);
    }

    void ShowCharacterSelect() {
        if (!ClientStatusOn()) {
            return;
        }

        CoA::Discord::Activity a;
        a.state = "Selecting World";
        a.largeImage = kLogo;
        a.smallImage = kAppIcon;
        a.buttons = { { "Website", kWebsite } };
        SendClientStatus(a);
    }

    void OpenGuildInvite(const std::string& code) {   // FUN_10220dd0
        if (!s_mgr.enabled) {
            return;
        }

        if (s_deps.openGuildInvite) {
            s_deps.openGuildInvite(code.c_str());
        }
    }

    int32_t OnInitialize(void*, uint32_t, uint32_t, CDataStore*) {   // 0x90C 0x10220EC0
        CoA::Log::Printf("Discord: SMSG 0x90C (enabled %d)", s_mgr.enabled ? 1 : 0);
        s_mgr.serverStatus = true;

        if (!s_mgr.enabled) {
            return 1;
        }

        CoA::Discord::Activity a;
        a.state = "Selecting World";
        a.smallImage = kLogo;
        UpdateActivity(a);
        return 1;
    }

    int32_t OnUpdate(void*, uint32_t, uint32_t, CDataStore* msg) {   // 0x90D FUN_10221060
        CoA::Log::Printf("Discord: SMSG 0x90D (enabled %d, subscribed %d)", s_mgr.enabled ? 1 : 0, s_mgr.activitySubscribed ? 1 : 0);
        s_mgr.serverStatus = true;

        if (!s_mgr.enabled) {
            return 1;
        }

        CoA::Read::U8(msg);   // skipped
        CoA::Discord::Activity a;
        a.state = Cut(CoA::Read::Name(msg));
        a.details = Cut(CoA::Read::Name(msg));
        a.start = static_cast<int64_t>(CoA::Read::U64(msg));
        a.end = static_cast<int64_t>(CoA::Read::U64(msg));
        a.largeImage = Cut(CoA::Read::Name(msg));
        a.smallImage = Cut(CoA::Read::Name(msg));
        a.largeText = Cut(CoA::Read::Name(msg));
        a.smallText = Cut(CoA::Read::Name(msg));
        a.partyId = Cut(CoA::Read::Name(msg));
        a.partySize = CoA::Read::I32(msg);
        a.partyMax = CoA::Read::I32(msg);
        a.match = Cut(CoA::Read::Name(msg));
        a.join = Cut(CoA::Read::Name(msg));
        a.spectate = Cut(CoA::Read::Name(msg));

        if (!s_mgr.activitySubscribed) {
            UpdateActivity(a);
            s_mgr.activitySubscribed = true;
        }

        return 1;
    }

    int32_t OnGuildInvite(void*, uint32_t, uint32_t, CDataStore* msg) {   // 0x90E 0x10220E80
        if (s_mgr.enabled) {
            OpenGuildInvite(CoA::Read::Name(msg));
        }

        return 1;
    }

    // ---- natives (FUN_10221dc0) ---------------------------------------------------------------------------------
    int DiscordFeaturesEnabled(lua_State* L) {   // 0x102217C0
        PushBool(L, s_mgr.enabled);
        return 1;
    }

    int DiscordFeaturesOverlayEnabled(lua_State* L) {   // 0x102217F0
        PushBool(L, s_mgr.enabled && s_deps.overlayEnabled && s_deps.overlayEnabled());
        return 1;
    }

    // 0x10221860: exactly one argument, a non-empty string.
    int DiscordOpenGuildInvite(lua_State* L) {
        if (lua_gettop(L) != 1) {
            return 0;
        }

        const std::string code = CoA::Script::CheckString(L, 1);

        if (!code.empty()) {
            OpenGuildInvite(code);
        }

        return 0;
    }

    const CoA::Binding s_bindings[] = {
        COA_NATIVE("", "DiscordFeaturesEnabled", DiscordFeaturesEnabled),
        COA_NATIVE("", "DiscordFeaturesOverlayEnabled", DiscordFeaturesOverlayEnabled),
        COA_NATIVE("", "DiscordOpenGuildInvite", DiscordOpenGuildInvite),
    };
}

CoA::Discord::Deps& CoA::Discord::Dependencies() {
    return s_deps;
}

bool CoA::Discord::Enabled() {
    return s_mgr.enabled;
}

void CoA::Discord::Initialize() {   // FUN_10220b10
    COA_PACKET(0x90C, OnInitialize);
    COA_PACKET(0x90D, OnUpdate);
    COA_PACKET(0x90E, OnGuildInvite);

    if (s_mgr.enabled) {
        return;
    }

    bool core = false;
    s_mgr.result = s_deps.create ? s_deps.create(core) : kNoSdkResult;

    if (!core) {
        CoA::Log::Printf("Discord: no core (result %d)", s_mgr.result);
        CoA::Packet(0x52E).U32(static_cast<uint32_t>(s_mgr.result)).Send();
        return;
    }

    s_mgr.userSubscribed = true;
    s_mgr.enabled = true;

    if (s_deps.startCallbacks) {
        s_deps.startCallbacks();
    }

    ShowCharacterSelect();
}

void CoA::Discord::OnEnterWorld() {
    s_mgr.worldStart = s_deps.now ? s_deps.now() : 0;
    CoA::Discord::Refresh();
}

void CoA::Discord::OnLeaveWorld() {
    s_mgr.worldStart = 0;
    ShowCharacterSelect();
}

void CoA::Discord::Refresh() {
    CoA::Discord::WorldInfo info;

    if (!ClientStatusOn() || !s_deps.worldInfo || !s_deps.worldInfo(info)) {
        return;
    }

    CoA::Discord::Activity a;
    a.details = Cut(info.level ? "Level " + std::to_string(info.level) + " " + info.className : info.className);
    a.state = Cut(info.zone);
    a.start = s_mgr.worldStart;
    a.largeImage = kLogo;
    a.largeText = Cut(info.realm);
    SetFaction(a, info.faction);
    a.buttons = { { "Website", kWebsite } };
    SendClientStatus(a);
}

void CoA::Discord::OnCurrentUserUpdate() {   // FUN_10221aa0
    uint64_t id = 0;
    std::string name;

    const bool got = s_mgr.userSubscribed && s_deps.currentUser && s_deps.currentUser(id, name);
    CoA::Log::Printf("Discord: OnCurrentUserUpdate (user %s)", got ? "read" : "not read");

    if (got) {
        CoA::Packet(0x52A).U64(id).Str(name.c_str()).Send();
    }
}

void CoA::Discord::OnActivityJoinRequest(uint64_t userId) {   // FUN_10221bb0
    if (s_mgr.activitySubscribed) {
        CoA::Packet(0x529).U64(userId).Send();
    }
}

void CoA::Discord::OnActivityJoin(const char* secret) {   // FUN_10221c90
    if (s_mgr.activitySubscribed) {
        CoA::Packet(0x52B).Str(secret).Send();
    }
}

void CoA::Discord::Init() {
    CoA::LuaReg::AddModule(s_bindings, sizeof(s_bindings) / sizeof(s_bindings[0]));
    // AscDiscord.cpp:Detour4DB9F0: DiscordMgr::Initialize after CGlueMgr::Initialize (the hook is in CGlueMgr.cpp).
    CoA::ClientHooks::afterGlueInitialize.Add(CoA::Discord::Initialize);
}

void CoA::Discord::Reset() {
    s_mgr.enabled = false;
    s_mgr.activitySubscribed = false;
    s_mgr.result = 0;
    s_mgr.userSubscribed = false;
    s_mgr.serverStatus = false;
    s_mgr.worldStart = 0;
    s_mgr.sentAny = false;
    s_mgr.lastSent = Activity();
    s_deps = Deps();
}
