// AscDiscord.cpp's SDK half: discord_game_sdk.dll loaded at run time (LoadLibraryW + GetProcAddress("DiscordCreate"))
// in place of the original's static import, its C ABI (discord_game_sdk.h, DISCORD_VERSION 2) and the callback thread.
// The install ships the 32-bit SDK, which this x64 process cannot load: Create then reports Discord::kNoSdkResult with
// no core, and the pure half takes the original's "no core" path (CMSG 0x52E, no thread).
#include "CoA/Source/Modules/Discord.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/Log.h"
#include <windows.h>
#include <cstring>
#include <thread>

namespace {
    // ---- the Game SDK's C ABI ---------------------------------------------------------------------------------------
    struct DiscordUser {
        int64_t id;
        char username[256];
        char discriminator[8];
        char avatar[128];
        bool bot;
    };

    struct DiscordActivity {
        int32_t type;
        int64_t applicationId;
        char name[128], state[128], details[128];
        struct { int64_t start, end; } timestamps;
        struct { char largeImage[128], largeText[128], smallImage[128], smallText[128]; } assets;
        struct { char id[128]; struct { int32_t current, max; } size; } party;
        struct { char match[128], join[128], spectate[128]; } secrets;
        bool instance;
    };

    using ResultCallback = void (*)(void* data, int32_t result);

    struct IDiscordUserManager {
        int32_t (*get_current_user)(IDiscordUserManager*, DiscordUser*);
    };

    struct IDiscordActivityManager {
        void* register_command;
        void* register_steam;
        void (*update_activity)(IDiscordActivityManager*, DiscordActivity*, void*, ResultCallback);
    };

    struct IDiscordOverlayManager {
        void (*is_enabled)(IDiscordOverlayManager*, bool*);
        void* is_locked;
        void* set_locked;
        void* open_activity_invite;
        void (*open_guild_invite)(IDiscordOverlayManager*, const char*, void*, ResultCallback);
    };

    struct IDiscordCore {
        void (*destroy)(IDiscordCore*);
        int32_t (*run_callbacks)(IDiscordCore*);
        void* set_log_hook;
        void* get_application_manager;
        IDiscordUserManager* (*get_user_manager)(IDiscordCore*);
        void* get_image_manager;
        IDiscordActivityManager* (*get_activity_manager)(IDiscordCore*);
        void* get_relationship_manager;
        void* get_lobby_manager;
        void* get_network_manager;
        IDiscordOverlayManager* (*get_overlay_manager)(IDiscordCore*);
    };

    struct IDiscordUserEvents {
        void (*on_current_user_update)(void*);
    };

    struct IDiscordActivityEvents {
        void (*on_activity_join)(void*, const char*);
        void (*on_activity_spectate)(void*, const char*);
        void (*on_activity_join_request)(void*, DiscordUser*);
        void (*on_activity_invite)(void*, int32_t, DiscordUser*, DiscordActivity*);
    };

    struct DiscordCreateParams {
        int64_t clientId;
        uint64_t flags;
        void* events;
        void* eventData;
        struct { void* events; int32_t version; } managers[12];   // application, user, image, activity,
                                                                  // relationship, lobby, network, overlay,
                                                                  // storage, store, voice, achievement
    };

    using DiscordCreate_t = int32_t (*)(int32_t version, DiscordCreateParams* params, IDiscordCore** core);

    IDiscordCore* s_core = nullptr;
    DiscordUser s_user{};   // DiscordMgr +0x10

    void IgnoreResult(void*, int32_t) {}   // the wrapper's empty lambdas (0x10221A90)

    void UserUpdated(void*) {
        CoA::Discord::OnCurrentUserUpdate();
    }

    void ActivityJoin(void*, const char* secret) {
        CoA::Discord::OnActivityJoin(secret);
    }

    void ActivitySpectate(void*, const char*) {}

    void ActivityJoinRequest(void*, DiscordUser* user) {
        CoA::Discord::OnActivityJoinRequest(static_cast<uint64_t>(user->id));
    }

    void ActivityInvite(void*, int32_t, DiscordUser*, DiscordActivity*) {}

    IDiscordUserEvents s_userEvents = { UserUpdated };
    IDiscordActivityEvents s_activityEvents = { ActivityJoin, ActivitySpectate, ActivityJoinRequest, ActivityInvite };

    int32_t Create(bool& core) {   // discord::Core::Create (FUN_10088d30)
        core = false;
        HMODULE sdk = LoadLibraryW(L"discord_game_sdk.dll");
        DiscordCreate_t create = sdk ? reinterpret_cast<DiscordCreate_t>(GetProcAddress(sdk, "DiscordCreate")) : nullptr;

        if (!create) {
            CoA::Log::Printf("Discord: discord_game_sdk.dll / DiscordCreate not loadable (error %lu)", GetLastError());
            return CoA::Discord::kNoSdkResult;
        }

        DiscordCreateParams params{};
        params.clientId = 0x0C7C30A11D421000ll;
        params.flags = 1;   // NoRequireDiscord

        for (auto& m : params.managers) {
            m.version = 1;
        }

        params.managers[1].events = &s_userEvents;
        params.managers[3].events = &s_activityEvents;
        IDiscordCore* c = nullptr;
        const int32_t result = create(2, &params, &c);

        if (result != 0 && c) {
            c->destroy(c);
            c = nullptr;
        }

        s_core = c;
        core = c != nullptr;
        return result;
    }

    void StartCallbacks() {   // FUN_10220a60 -> FUN_10220ae0
        std::thread([] {
            while (CoA::Discord::Enabled()) {
                s_core->run_callbacks(s_core);
                Sleep(0x32);
            }
        }).detach();
    }

    bool CurrentUser(uint64_t& id, std::string& username) {
        IDiscordUserManager* users = s_core->get_user_manager(s_core);

        if (users->get_current_user(users, &s_user) != 0) {
            return false;
        }

        id = static_cast<uint64_t>(s_user.id);
        username = s_user.username;
        return true;
    }

    void Copy(char* dst, const std::string& s) {
        strncpy(dst, s.c_str(), 0x80);
        dst[0x7F] = 0;
    }

    void UpdateActivity(const CoA::Discord::Activity& a) {
        DiscordActivity d{};
        d.type = a.type;
        Copy(d.state, a.state);
        Copy(d.details, a.details);
        d.timestamps.start = a.start;
        d.timestamps.end = a.end;
        Copy(d.assets.largeImage, a.largeImage);
        Copy(d.assets.largeText, a.largeText);
        Copy(d.assets.smallImage, a.smallImage);
        Copy(d.assets.smallText, a.smallText);
        Copy(d.party.id, a.partyId);
        d.party.size.current = a.partySize;
        d.party.size.max = a.partyMax;
        Copy(d.secrets.match, a.match);
        Copy(d.secrets.join, a.join);
        Copy(d.secrets.spectate, a.spectate);
        IDiscordActivityManager* m = s_core->get_activity_manager(s_core);
        m->update_activity(m, &d, nullptr, IgnoreResult);
    }

    void OpenGuildInvite(const char* code) {
        IDiscordOverlayManager* overlay = s_core->get_overlay_manager(s_core);
        overlay->open_guild_invite(overlay, code, nullptr, IgnoreResult);
    }

    bool OverlayEnabled() {
        bool on = false;
        IDiscordOverlayManager* overlay = s_core->get_overlay_manager(s_core);
        overlay->is_enabled(overlay, &on);
        return on;
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
}
