// AscDiscord.cpp's client half. The original links the 2019 Discord Game SDK; with current Discord apps that SDK still
// "creates" a core but never completes its session (no current user, no visible activity), so CoA64 speaks the
// desktop app's local IPC protocol itself (DiscordIpc.h) over \\.\pipe\discord-ipc-N -- no discord_game_sdk.dll.
// Ruling (CoA64, Discord status 2026-10-26): Create always reports a core, as the SDK did with NoRequireDiscord; the
// callback thread keeps (re)connecting every 5 s while Discord is closed. The guild invite opens the discord.gg link
// (IPC has no overlay); the overlay reads as disabled. The client-built status (Discord::Deps::clientStatus) reads
// the player from the main Lua state.
#include "CoA/Source/Modules/Discord.h"
#include "CoA/Source/Modules/DiscordIpc.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/ClientHooks.h"
#include "CoA/Source/Runtime/ClientState.h"
#include "CoA/Source/Runtime/Log.h"
#include "CoA/Source/Runtime/Timer.h"
#include "Engine/Source/Os/System.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <windows.h>
#include <ctime>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace {
    constexpr uint64_t kClientId = 0x0C7C30A11D421000ull;   // FUN_10220b10's application id
    constexpr DWORD kReconnectMs = 5000;
    constexpr uint32_t kRefreshMs = 15000;                  // Discord accepts 5 activity updates per 20 s

    std::mutex s_lock;                    // the pipe, the user and the pending activity
    HANDLE s_pipe = INVALID_HANDLE_VALUE;
    std::vector<uint8_t> s_inbox;
    uint32_t s_nonce = 0;
    bool s_haveUser = false;
    uint64_t s_userId = 0;
    std::string s_username;
    bool s_haveActivity = false;
    CoA::Discord::Activity s_activity;    // re-sent after every (re)connect
    bool s_refreshTimer = false;

    bool WriteLocked(const std::vector<uint8_t>& frame) {
        DWORD written = 0;

        if (s_pipe == INVALID_HANDLE_VALUE) {
            return false;
        }

        if (!WriteFile(s_pipe, frame.data(), static_cast<DWORD>(frame.size()), &written, nullptr) || written != frame.size()) {
            CloseHandle(s_pipe);
            s_pipe = INVALID_HANDLE_VALUE;
            return false;
        }

        return true;
    }

    void DisconnectLocked() {
        if (s_pipe != INVALID_HANDLE_VALUE) {
            CloseHandle(s_pipe);
            s_pipe = INVALID_HANDLE_VALUE;
        }

        s_inbox.clear();
    }

    // Reads whatever the pipe holds without blocking; false when it broke.
    bool PumpLocked() {
        DWORD available = 0;

        if (!PeekNamedPipe(s_pipe, nullptr, 0, nullptr, &available, nullptr)) {
            return false;
        }

        while (available) {
            uint8_t chunk[4096];
            DWORD got = 0;

            if (!ReadFile(s_pipe, chunk, available < sizeof(chunk) ? available : sizeof(chunk), &got, nullptr) || !got) {
                return false;
            }

            s_inbox.insert(s_inbox.end(), chunk, chunk + got);
            available -= got;
        }

        return !CoA::DiscordIpc::Broken(s_inbox);
    }

    // Opens discord-ipc-0..9 and sends the handshake; READY arrives through the pump.
    bool ConnectLocked() {
        for (int i = 0; i < 10; ++i) {
            const std::wstring name = L"\\\\.\\pipe\\discord-ipc-" + std::to_wstring(i);
            HANDLE pipe = CreateFileW(name.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);

            if (pipe != INVALID_HANDLE_VALUE) {
                s_pipe = pipe;
                s_inbox.clear();

                if (WriteLocked(CoA::DiscordIpc::Handshake(kClientId))) {
                    return true;
                }
            }
        }

        return false;
    }

    void OnReady(const CoA::DiscordIpc::Event& e) {
        {
            std::lock_guard<std::mutex> lock(s_lock);
            s_haveUser = e.userId != 0;
            s_userId = e.userId;
            s_username = e.username;
            WriteLocked(CoA::DiscordIpc::Subscribe("ACTIVITY_JOIN", ++s_nonce));
            WriteLocked(CoA::DiscordIpc::Subscribe("ACTIVITY_JOIN_REQUEST", ++s_nonce));

            if (s_haveActivity) {
                WriteLocked(CoA::DiscordIpc::SetActivity(s_activity, GetCurrentProcessId(), ++s_nonce));
            }
        }

        CoA::Log::Printf("Discord: connected (user %s)", s_haveUser ? "known" : "unknown");
        CoA::Discord::OnCurrentUserUpdate();   // the SDK's OnCurrentUserUpdate (CMSG 0x52A)
    }

    void Handle(const CoA::DiscordIpc::Message& m) {
        if (m.op == CoA::DiscordIpc::Ping) {
            std::lock_guard<std::mutex> lock(s_lock);
            WriteLocked(CoA::DiscordIpc::Frame(CoA::DiscordIpc::Pong, m.json));
            return;
        }

        if (m.op == CoA::DiscordIpc::Close) {
            CoA::Log::Printf("Discord: closed by the app: %s", m.json.substr(0, 200).c_str());
            std::lock_guard<std::mutex> lock(s_lock);
            DisconnectLocked();
            return;
        }

        const CoA::DiscordIpc::Event e = CoA::DiscordIpc::Parse(m.json);

        switch (e.kind) {
            case CoA::DiscordIpc::Event::Ready: OnReady(e); break;
            case CoA::DiscordIpc::Event::Join: CoA::Discord::OnActivityJoin(e.secret.c_str()); break;
            case CoA::DiscordIpc::Event::JoinRequest: CoA::Discord::OnActivityJoinRequest(e.userId); break;
            case CoA::DiscordIpc::Event::Error: CoA::Log::Printf("Discord: error %s", e.message.c_str()); break;
            default: break;
        }
    }

    int32_t Create(bool& core) {   // discord::Core::Create (FUN_10088d30) with NoRequireDiscord: always a core
        core = true;
        return 0;
    }

    void StartCallbacks() {   // FUN_10220a60 -> FUN_10220ae0: the 50 ms loop, here the pipe's pump
        std::thread([] {
            DWORD lastAttempt = 0;

            while (CoA::Discord::Enabled()) {
                std::vector<CoA::DiscordIpc::Message> messages;
                {
                    std::lock_guard<std::mutex> lock(s_lock);

                    if (s_pipe == INVALID_HANDLE_VALUE) {
                        const DWORD now = GetTickCount();

                        if (!lastAttempt || now - lastAttempt >= kReconnectMs) {
                            lastAttempt = now;
                            ConnectLocked();
                        }
                    }

                    if (s_pipe != INVALID_HANDLE_VALUE) {
                        if (!PumpLocked()) {
                            DisconnectLocked();
                        } else {
                            CoA::DiscordIpc::Message m;

                            while (CoA::DiscordIpc::Take(s_inbox, m)) {
                                messages.push_back(m);
                            }
                        }
                    }
                }

                for (const CoA::DiscordIpc::Message& m : messages) {
                    Handle(m);
                }

                Sleep(0x32);
            }

            std::lock_guard<std::mutex> lock(s_lock);
            DisconnectLocked();
        }).detach();
    }

    bool CurrentUser(uint64_t& id, std::string& username) {
        std::lock_guard<std::mutex> lock(s_lock);
        id = s_userId;
        username = s_username;
        return s_haveUser;
    }

    void UpdateActivity(const CoA::Discord::Activity& a) {
        std::lock_guard<std::mutex> lock(s_lock);
        s_activity = a;
        s_haveActivity = true;
        WriteLocked(CoA::DiscordIpc::SetActivity(a, GetCurrentProcessId(), ++s_nonce));
    }

    void OpenGuildInvite(const char* code) {
        const std::string url = std::string("https://discord.gg/") + code;
        OsLaunchURL(url.c_str());
    }

    bool OverlayEnabled() {
        return false;
    }

    // ---- the client-built status ---------------------------------------------------------------------------------
    bool WorldInfo(CoA::Discord::WorldInfo& out) {
        lua_State* L = CoA::ClientState::MainLuaState();

        if (!L) {
            return false;
        }

        const int top = lua_gettop(L);
        bool ok = false;
        static const char* kQuery =
            "if not UnitExists or not UnitExists('player') then return end "
            "return UnitLevel('player'), (UnitClass('player')), GetRealZoneText(), GetRealmName()";

        if (luaL_loadstring(L, kQuery) == 0 && lua_pcall(L, 0, 4, 0) == 0 && lua_type(L, top + 1) == LUA_TNUMBER) {
            out.level = static_cast<uint32_t>(static_cast<int64_t>(lua_tonumber(L, top + 1)));
            out.className = lua_isstring(L, top + 2) ? lua_tostring(L, top + 2) : "";
            out.zone = lua_isstring(L, top + 3) ? lua_tostring(L, top + 3) : "";
            out.realm = lua_isstring(L, top + 4) ? lua_tostring(L, top + 4) : "";
            ok = true;
        }

        lua_settop(L, top);
        return ok;
    }

    int64_t Now() {
        return static_cast<int64_t>(time(nullptr));
    }

    int32_t OnRefreshTimer(const void*, void*) {
        CoA::Discord::Refresh();
        CoA::Timer::Schedule(kRefreshMs, OnRefreshTimer, nullptr);
        return 0;
    }

    void EnterWorld() {
        CoA::Discord::OnEnterWorld();

        if (!s_refreshTimer) {   // one self-re-arming timer for the process
            s_refreshTimer = CoA::Timer::Schedule(kRefreshMs, OnRefreshTimer, nullptr) != 0;
        }
    }

    void ZoneChanged(int32_t) {
        CoA::Discord::Refresh();
    }
}

void CoA::DiscordClient::Init() {
    CoA::Discord::Deps& deps = CoA::Discord::Dependencies();
    deps.create = Create;
    deps.startCallbacks = StartCallbacks;
    deps.currentUser = CurrentUser;
    deps.updateActivity = UpdateActivity;
    deps.openGuildInvite = OpenGuildInvite;
    deps.overlayEnabled = OverlayEnabled;
    deps.clientStatus = true;
    deps.worldInfo = WorldInfo;
    deps.now = Now;
    CoA::ClientHooks::afterEnterWorld.Add(EnterWorld);
    CoA::ClientHooks::leaveWorld.Add(CoA::Discord::OnLeaveWorld);
    CoA::ClientHooks::afterZoneFeedback.Add(ZoneChanged);
}
