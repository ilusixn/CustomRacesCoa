#ifndef COA_SOURCE_MODULES_DISCORD_H
#define COA_SOURCE_MODULES_DISCORD_H

#include <cstdint>
#include <string>
#include <vector>

// AscDiscord.cpp: DiscordMgr (FUN_10220d50 -> static 0x10BE21F0: +0x04 enabled, +0x05 activity callbacks registered,
// +0x08 the create result, +0x10 the current DiscordUser, +0x1A8 the discord::Core*) and the three global natives.
//
// DiscordMgr::Initialize (FUN_10220b10) runs after CGlueMgr::Initialize (0x4DB9F0, the original's detour 0x10A46350):
// it registers SMSG 0x90C / 0x90D / 0x90E, then -- unless already enabled -- discord::Core::Create(app
// 0x0C7C30A11D421000, flags 1 = NoRequireDiscord). No core: CMSG 0x52E {u32 result}, nothing else. A core: the
// OnCurrentUserUpdate subscription (CMSG 0x52A {u64 id, cstring username}), enabled = true, and a detached thread
// running RunCallbacks every 50 ms while enabled.
//   SMSG 0x90C  an activity "Selecting World" with small image "ascension_logo" -> UpdateActivity.
//   SMSG 0x90D  u8 skipped; sized state, details; u64 start, end; sized large image, small image, large text, small
//               text, party id; u32 party size, max; sized match, join, spectate secrets. The FIRST 0x90D subscribes
//               OnActivityJoinRequest (CMSG 0x529 {u64 user id}) / OnActivityJoin (CMSG 0x52B {cstring secret}) and
//               sends the activity; later ones build it and never send it (kept).
//   SMSG 0x90E  sized invite code -> OverlayManager::OpenGuildInvite.
// The original links the Discord Game SDK statically; CoA64 talks to the Discord app's IPC pipe instead (DiscordIpc.h,
// DiscordClient.cpp), and builds its own status while the server sends none (Deps::clientStatus).
namespace CoA::Discord {
    // The activity as the SDK wrapper's setters leave it: every string cut at its first NUL and at 127 bytes.
    struct Activity {
        int32_t type = 0;
        std::string state, details;
        int64_t start = 0, end = 0;
        std::string largeImage, largeText, smallImage, smallText;
        std::string partyId;
        int32_t partySize = 0, partyMax = 0;
        std::string match, join, spectate;
    };

    // What the client-built status shows in world (Deps::worldInfo).
    struct WorldInfo {
        uint32_t level = 0;
        std::string className, zone, realm;
    };

    // What DiscordCreate returns when the SDK cannot be loaded: DiscordResult_InternalError.
    constexpr int32_t kNoSdkResult = 4;

    struct Deps {
        // discord::Core::Create: the result, and whether a core exists. Unset: no SDK (kNoSdkResult, no core).
        int32_t (*create)(bool& core) = nullptr;
        // The callback thread (FUN_10220a60 -> FUN_10220ae0): RunCallbacks every 50 ms while Enabled().
        void (*startCallbacks)() = nullptr;
        bool (*currentUser)(uint64_t& id, std::string& username) = nullptr;   // UserManager::GetCurrentUser == Ok
        void (*updateActivity)(const Activity& activity) = nullptr;            // ActivityManager::UpdateActivity
        void (*openGuildInvite)(const char* code) = nullptr;                   // OverlayManager::OpenGuildInvite
        bool (*overlayEnabled)() = nullptr;                                    // OverlayManager::IsEnabled

        // CoA64 addition: the client's own status while the server sends none (0x90C / 0x90D). Off: the original.
        bool clientStatus = false;
        bool (*worldInfo)(WorldInfo& out) = nullptr;   // false outside the world
        int64_t (*now)() = nullptr;                    // Unix seconds
    };

    Deps& Dependencies();

    void Init();
    // Tests only: the manager as constructed (disabled, nothing subscribed), Deps forgotten.
    void Reset();

    // FUN_10220b10 (run from CoA::ClientHooks::afterGlueInitialize).
    void Initialize();
    bool Enabled();

    // CoA64's client-built status (Deps::clientStatus): world entry starts its timer, Refresh re-reads Deps::worldInfo
    // (zone changes, the periodic refresh) and sends only a changed activity, leaving the world shows the character
    // screen again. All of it stops once the server has sent 0x90C or 0x90D.
    void OnEnterWorld();
    void OnLeaveWorld();
    void Refresh();

    // The SDK's callbacks (the client half calls these from the callback thread, as the original does).
    void OnCurrentUserUpdate();                    // FUN_10221aa0
    void OnActivityJoinRequest(uint64_t userId);   // FUN_10221bb0
    void OnActivityJoin(const char* secret);       // FUN_10221c90
}

#endif
