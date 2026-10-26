// Discord (Discord.cpp): the glue-initialize hook registering SMSG 0x90C..0x90E, the no-SDK / no-core path (CMSG 0x52E
// once per Initialize, no thread, Disabled), a core (callbacks started once), the activity packets (the first 0x90D
// sends and subscribes, later ones do not), the guild invite, the SDK callbacks' CMSGs and the three natives.
#include "CoA/Source/Modules/Discord.h"
#include "CoA/Source/Runtime/ClientHooks.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Tests/Support.h"
#include "CoA/Tests/TestMain.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <string>
#include <vector>

namespace {
    using V = std::vector<std::string>;
    using Packets = std::vector<std::vector<uint8_t>>;

    int s_creates = 0;
    int s_starts = 0;
    int32_t s_result = 0;
    bool s_core = false;
    bool s_overlay = false;
    bool s_userOk = true;
    std::vector<CoA::Discord::Activity> s_activities;
    std::vector<std::string> s_invites;

    lua_State* Start() {
        CoA::LuaReg::Reset();
        CoA::Packets::Clear();
        CoA::ClientHooks::afterGlueInitialize.Clear();
        CoA::Discord::Reset();
        CoA::Discord::Init();
        s_creates = 0;
        s_starts = 0;
        s_result = 0;
        s_core = false;
        s_overlay = false;
        s_userOk = true;
        s_activities.clear();
        s_invites.clear();
        return CoATest::NewState(false);
    }

    void Sdk() {
        CoA::Discord::Deps& d = CoA::Discord::Dependencies();
        d.create = [](bool& core) {
            ++s_creates;
            core = s_core;
            return s_result;
        };
        d.startCallbacks = [] { ++s_starts; };
        d.currentUser = [](uint64_t& id, std::string& name) {
            id = 0x1122334455667788ull;
            name = "player#1";
            return s_userOk;
        };
        d.updateActivity = [](const CoA::Discord::Activity& a) { s_activities.push_back(a); };
        d.openGuildInvite = [](const char* code) { s_invites.push_back(code); };
        d.overlayEnabled = [] { return s_overlay; };
    }

    void Stop(lua_State* L) {
        lua_close(L);
        CoATest::StopRecordingPackets();
        CoA::Discord::Reset();
        CoA::ClientHooks::afterGlueInitialize.Clear();
        CoA::Packets::Clear();
    }

    Packets Initialize() {
        CoATest::RecordPackets();
        CoA::ClientHooks::afterGlueInitialize.Run();
        CoATest::StopRecordingPackets();
        return CoATest::SentPackets();
    }

    void Dispatch(uint32_t opcode, CoATest::Bytes& b) {
        CDataStore msg(b.data.data(), static_cast<uint32_t>(b.data.size()));
        int32_t result = 0;
        CoA::Packets::Dispatch(static_cast<int32_t>(opcode), 0, &msg, result);
    }

    V Run(lua_State* L, const std::string& chunk) {
        V out;
        const int base = lua_gettop(L);

        if (luaL_loadstring(L, chunk.c_str()) != 0 || lua_pcall(L, 0, LUA_MULTRET, 0) != 0) {
            out.push_back(std::string("<error> ") + lua_tostring(L, -1));
            lua_settop(L, base);
            return out;
        }

        for (int i = base + 1; i <= lua_gettop(L); ++i) {
            out.push_back(lua_isnil(L, i) ? "nil" : lua_isboolean(L, i) ? (lua_toboolean(L, i) ? "true" : "false") : lua_tostring(L, i));
        }

        lua_settop(L, base);
        return out;
    }

    std::string Sized(CoATest::Bytes& b, const std::string& s) {
        b.U32(static_cast<uint32_t>(s.size())).Raw(s.data(), static_cast<uint32_t>(s.size()));
        return s;
    }

    CoATest::Bytes Update(const std::string& state) {
        CoATest::Bytes b;
        b.U8(0xEE);
        Sized(b, state);
        Sized(b, "details");
        b.U64(1000).U64(2000);
        Sized(b, "large");
        Sized(b, "small");
        Sized(b, "large text");
        Sized(b, "small text");
        Sized(b, "party");
        b.U32(3).U32(5);
        Sized(b, "match");
        Sized(b, "join");
        Sized(b, "spectate");
        return b;
    }
}

// No SDK (the install's x86 DLL): the hook registers the handlers and sends CMSG 0x52E {u32 4}; nothing is enabled,
// no thread starts; the natives answer disabled and every SMSG is harmless.
COA_TEST(Discord_NoSdk) {
    lua_State* L = Start();
    CHECK(!CoA::Packets::Has(0x90C));
    CHECK(Run(L, "return DiscordFeaturesEnabled(), DiscordFeaturesOverlayEnabled()") == (V{ "false", "false" }));
    CHECK(Initialize() == Packets{ CoATest::Expected(0x52E, CoATest::Bytes().U32(CoA::Discord::kNoSdkResult)) });
    CHECK(CoA::Packets::Has(0x90C) && CoA::Packets::Has(0x90D) && CoA::Packets::Has(0x90E));
    CHECK(!CoA::Discord::Enabled());
    CHECK(Run(L, "return DiscordFeaturesEnabled(), DiscordFeaturesOverlayEnabled(), DiscordOpenGuildInvite('abc')") == (V{ "false", "false" }));

    CoATest::RecordPackets();
    CoATest::Bytes empty;
    Dispatch(0x90C, empty);
    CoATest::Bytes u = Update("x");
    Dispatch(0x90D, u);
    CoATest::Bytes g;
    Sized(g, "code");
    Dispatch(0x90E, g);
    CoA::Discord::OnCurrentUserUpdate();
    CoA::Discord::OnActivityJoinRequest(5);
    CoA::Discord::OnActivityJoin("s");
    CoATest::StopRecordingPackets();
    CHECK(CoATest::SentPackets().empty());

    // Each Initialize while disabled tries again.
    CHECK(Initialize().size() == 1);
    Stop(L);
}

// DiscordCreate failing (a result, no core): CMSG 0x52E carries the result; still disabled, no thread.
COA_TEST(Discord_CreateFails) {
    lua_State* L = Start();
    Sdk();
    s_result = 27;
    CHECK(Initialize() == Packets{ CoATest::Expected(0x52E, CoATest::Bytes().U32(27)) });
    CHECK(s_creates == 1);
    CHECK(s_starts == 0);
    CHECK(!CoA::Discord::Enabled());
    CoATest::Bytes empty;
    Dispatch(0x90C, empty);
    CHECK(s_activities.empty());
    Stop(L);
}

// A core: enabled, the callbacks started once, nothing sent; a second Initialize does not create again.
COA_TEST(Discord_CoreCreated) {
    lua_State* L = Start();
    Sdk();
    s_core = true;
    CHECK(Initialize().empty());
    CHECK(CoA::Discord::Enabled());
    CHECK(s_creates == 1 && s_starts == 1);
    CHECK(Initialize().empty());
    CHECK(s_creates == 1 && s_starts == 1);
    CHECK(Run(L, "return DiscordFeaturesEnabled(), DiscordFeaturesOverlayEnabled()") == (V{ "true", "false" }));
    s_overlay = true;
    CHECK(Run(L, "return DiscordFeaturesOverlayEnabled()") == V{ "true" });

    // OnCurrentUserUpdate: CMSG 0x52A {u64 id, cstring name} when GetCurrentUser succeeds.
    CoATest::RecordPackets();
    CoA::Discord::OnCurrentUserUpdate();
    s_userOk = false;
    CoA::Discord::OnCurrentUserUpdate();
    CoATest::StopRecordingPackets();
    CHECK(CoATest::SentPackets() == Packets{ CoATest::Expected(0x52A, CoATest::Bytes().U64(0x1122334455667788ull).Str("player#1")) });
    Stop(L);
}

// 0x90C: "Selecting World" / the logo (CoA64's "descension_logo"; the original's "ascension_logo"); 0x90D: the parsed
// activity, sent and subscribed only the first time;
// the join callbacks send only after that.
COA_TEST(Discord_Activities) {
    lua_State* L = Start();
    Sdk();
    s_core = true;
    Initialize();
    CoATest::Bytes empty;
    Dispatch(0x90C, empty);
    CHECK(s_activities.size() == 1);
    CHECK(s_activities[0].state == "Selecting World" && s_activities[0].smallImage == "descension_logo");
    CHECK(s_activities[0].details.empty() && s_activities[0].largeImage.empty() && s_activities[0].partyMax == 0);

    CoATest::RecordPackets();
    CoA::Discord::OnActivityJoinRequest(9);
    CoA::Discord::OnActivityJoin("s");
    CoATest::StopRecordingPackets();
    CHECK(CoATest::SentPackets().empty());

    CoATest::Bytes u = Update(std::string(200, 'a'));
    Dispatch(0x90D, u);
    CHECK(s_activities.size() == 2);
    const CoA::Discord::Activity& a = s_activities[1];
    CHECK(a.state == std::string(127, 'a'));
    CHECK(a.details == "details" && a.start == 1000 && a.end == 2000);
    CHECK(a.largeImage == "large" && a.smallImage == "small" && a.largeText == "large text" && a.smallText == "small text");
    CHECK(a.partyId == "party" && a.partySize == 3 && a.partyMax == 5);
    CHECK(a.match == "match" && a.join == "join" && a.spectate == "spectate");

    // Later 0x90D: built, never sent.
    CoATest::Bytes u2 = Update("second");
    Dispatch(0x90D, u2);
    CHECK(s_activities.size() == 2);
    Dispatch(0x90C, empty);   // 0x90C still sends
    CHECK(s_activities.size() == 3);

    CoATest::RecordPackets();
    CoA::Discord::OnActivityJoinRequest(0xAABBCCDDEEFF0011ull);
    CoA::Discord::OnActivityJoin("secret!");
    CoATest::StopRecordingPackets();
    CHECK(CoATest::SentPackets() == (Packets{ CoATest::Expected(0x529, CoATest::Bytes().U64(0xAABBCCDDEEFF0011ull)),
        CoATest::Expected(0x52B, CoATest::Bytes().Str("secret!")) }));
    Stop(L);
}

// A string with an embedded NUL is cut there; a short 0x90D reads zeros / empty strings.
COA_TEST(Discord_UpdateEdges) {
    lua_State* L = Start();
    Sdk();
    s_core = true;
    Initialize();
    CoATest::Bytes u = Update(std::string("ab\0cd", 5));
    Dispatch(0x90D, u);
    CHECK(s_activities.size() == 1 && s_activities[0].state == "ab");
    CoA::Discord::Reset();
    Sdk();
    s_core = true;
    CoA::Discord::Initialize();
    CoATest::Bytes cut;
    cut.U8(1).U32(3).Raw("xyz", 3);
    Dispatch(0x90D, cut);
    CHECK(s_activities.size() == 2);
    CHECK(s_activities[1].state == "xyz" && s_activities[1].details.empty() && s_activities[1].start == 0 && s_activities[1].partySize == 0);
    Stop(L);
}

// 0x90E and DiscordOpenGuildInvite: the overlay's guild invite while enabled; the native needs exactly one non-empty
// string.
COA_TEST(Discord_GuildInvite) {
    lua_State* L = Start();
    Sdk();
    s_core = true;
    Initialize();
    CoATest::Bytes g;
    Sized(g, "ascension");
    Dispatch(0x90E, g);
    CHECK(s_invites == V{ "ascension" });
    CHECK(Run(L, "return select('#', DiscordOpenGuildInvite('abc'))") == V{ "0" });
    Run(L, "DiscordOpenGuildInvite('')");
    Run(L, "DiscordOpenGuildInvite('x', 'y')");
    Run(L, "DiscordOpenGuildInvite()");
    CHECK((s_invites == V{ "ascension", "abc" }));
    CHECK(Run(L, "DiscordOpenGuildInvite({})")[0].compare(0, 7, "<error>") == 0);   // luaL_checklstring raises
    Stop(L);
}

namespace {
    CoA::Discord::WorldInfo s_world;
    bool s_inWorld = true;

    void ClientStatus() {
        CoA::Discord::Deps& d = CoA::Discord::Dependencies();
        d.clientStatus = true;
        d.worldInfo = [](CoA::Discord::WorldInfo& out) {
            out = s_world;
            return s_inWorld;
        };
        d.now = [] { return static_cast<int64_t>(1793030619); };
        s_world = CoA::Discord::WorldInfo();
        s_world.level = 59;
        s_world.className = "Necromancer";
        s_world.zone = "The Hinterlands";
        s_world.realm = "Pale Reach";
        s_inWorld = true;
    }
}

// CoA64 (user decision 2026-10-26): when the server sends no status (0x90C / 0x90D), the client builds one --
// "Selecting World" on the character screen, then level / class / zone / realm in world, sent only when it changes.
COA_TEST(Discord_ClientStatusWithoutServerPackets) {
    lua_State* L = Start();
    Sdk();
    ClientStatus();
    s_core = true;
    Initialize();
    CHECK(s_activities.size() == 1);
    CHECK(s_activities.size() == 1 && s_activities[0].state == "Selecting World" && s_activities[0].largeImage == "descension_logo");

    CoA::Discord::OnEnterWorld();
    CHECK(s_activities.size() == 2);
    if (s_activities.size() == 2) {
        const CoA::Discord::Activity& a = s_activities[1];
        CHECK(a.details == "Level 59 Necromancer");
        CHECK(a.state == "The Hinterlands");
        CHECK(a.largeImage == "descension_logo" && a.largeText == "Pale Reach");
        CHECK(a.start == 1793030619);
    }

    CoA::Discord::Refresh();   // nothing changed: not resent
    CHECK(s_activities.size() == 2);
    s_world.zone = "Hillsbrad Foothills";
    CoA::Discord::Refresh();
    CHECK(s_activities.size() == 3 && s_activities.back().state == "Hillsbrad Foothills" && s_activities.back().start == 1793030619);

    CoA::Discord::OnLeaveWorld();
    CHECK(s_activities.size() == 4 && s_activities.back().state == "Selecting World");
    Stop(L);
}

// Server packets win: after 0x90C / 0x90D the client stops building its own status.
COA_TEST(Discord_ServerStatusWins) {
    lua_State* L = Start();
    Sdk();
    ClientStatus();
    s_core = true;
    Initialize();
    CoATest::Bytes u = Update("from server");
    Dispatch(0x90D, u);
    const size_t n = s_activities.size();
    CHECK(s_activities.back().state == "from server");
    CoA::Discord::OnEnterWorld();
    CoA::Discord::Refresh();
    CoA::Discord::OnLeaveWorld();
    CHECK(s_activities.size() == n);
    Stop(L);
}

// Without a Discord connection (no core) or outside the world nothing is sent.
COA_TEST(Discord_ClientStatusNeedsACore) {
    lua_State* L = Start();
    Sdk();
    ClientStatus();
    s_core = false;
    Initialize();
    CoA::Discord::OnEnterWorld();
    CoA::Discord::Refresh();
    CHECK(s_activities.empty());
    Stop(L);

    L = Start();
    Sdk();
    ClientStatus();
    s_core = true;
    Initialize();
    s_inWorld = false;
    CoA::Discord::Refresh();
    CHECK(s_activities.size() == 1);   // only "Selecting World"
    Stop(L);
}

// The small image (user decision 2026-10-26): the character's faction in world ("si_horde" / "si_alliance", its name as
// the hover text), the application icon ("app_icon") on the character screen or for any other faction.
COA_TEST(Discord_ClientStatusSmallImageIsTheFaction) {
    lua_State* L = Start();
    Sdk();
    ClientStatus();
    s_world.faction = "Horde";
    s_core = true;
    Initialize();
    CHECK(s_activities.size() == 1 && s_activities[0].smallImage == "app_icon" && s_activities[0].smallText.empty());

    CoA::Discord::OnEnterWorld();
    CHECK(s_activities.size() == 2 && s_activities.back().smallImage == "si_horde" && s_activities.back().smallText == "Horde");
    s_world.faction = "Alliance";
    CoA::Discord::Refresh();
    CHECK(s_activities.size() == 3 && s_activities.back().smallImage == "si_alliance" && s_activities.back().smallText == "Alliance");
    s_world.faction = "Neutral";
    CoA::Discord::Refresh();
    CHECK(s_activities.size() == 4 && s_activities.back().smallImage == "app_icon" && s_activities.back().smallText.empty());

    CoA::Discord::OnLeaveWorld();
    CHECK(s_activities.size() == 5 && s_activities.back().smallImage == "app_icon");
    Stop(L);
}

// The project's website as a button on the client-built status (user request 2026-10-26).
COA_TEST(Discord_ClientStatusLinksTheWebsite) {
    lua_State* L = Start();
    Sdk();
    ClientStatus();
    s_core = true;
    Initialize();
    CoA::Discord::OnEnterWorld();
    CHECK(s_activities.size() == 2);

    for (const CoA::Discord::Activity& a : s_activities) {
        CHECK(a.buttons.size() == 1);
        CHECK(a.buttons.size() == 1 && a.buttons[0].first == "Website" && a.buttons[0].second == "https://coa-development.org");
    }

    Stop(L);
}
