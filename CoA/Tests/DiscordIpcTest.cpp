// DiscordIpc (DiscordIpc.cpp): the Discord desktop app's local IPC protocol -- frames {u32 op, u32 length, JSON},
// the handshake, SET_ACTIVITY / SUBSCRIBE commands, and the READY / ACTIVITY_JOIN / ACTIVITY_JOIN_REQUEST dispatches.
#include "CoA/Source/Modules/Discord.h"
#include "CoA/Source/Modules/DiscordIpc.h"
#include "CoA/Tests/TestMain.h"
#include "ThirdParty/nlohmann/json.hpp"
#include <cstring>
#include <string>
#include <vector>

namespace {
    std::vector<uint8_t> Bytes(const std::string& s) {
        return std::vector<uint8_t>(s.begin(), s.end());
    }

    nlohmann::json Body(const std::vector<uint8_t>& frame) {
        return nlohmann::json::parse(std::string(frame.begin() + 8, frame.end()));
    }

    uint32_t U32(const std::vector<uint8_t>& frame, size_t at) {
        uint32_t v = 0;
        memcpy(&v, frame.data() + at, 4);
        return v;
    }
}

COA_TEST(DiscordIpc_HandshakeFrame) {
    const std::vector<uint8_t> f = CoA::DiscordIpc::Handshake(899647494105993216ull);
    CHECK(U32(f, 0) == 0);                   // op HANDSHAKE
    CHECK(U32(f, 4) == f.size() - 8);
    const nlohmann::json j = Body(f);
    CHECK(j["v"] == 1);
    CHECK(j["client_id"] == "899647494105993216");   // a string: the id does not fit a JSON double
}

COA_TEST(DiscordIpc_SetActivityCarriesOnlyWhatIsSet) {
    CoA::Discord::Activity a;
    a.state = "The Hinterlands";
    a.details = "Level 59 Necromancer";
    a.start = 1793030619;
    a.largeImage = "ascension_logo";
    a.largeText = "Pale Reach";
    const std::vector<uint8_t> f = CoA::DiscordIpc::SetActivity(a, 4242, 7);
    CHECK(U32(f, 0) == 1);                   // op FRAME
    const nlohmann::json j = Body(f);
    CHECK(j["cmd"] == "SET_ACTIVITY");
    CHECK(j["nonce"] == "7");
    CHECK(j["args"]["pid"] == 4242);
    const nlohmann::json& act = j["args"]["activity"];
    CHECK(act["state"] == "The Hinterlands");
    CHECK(act["details"] == "Level 59 Necromancer");
    CHECK(act["timestamps"]["start"] == 1793030619);
    CHECK(!act["timestamps"].contains("end"));
    CHECK(act["assets"]["large_image"] == "ascension_logo");
    CHECK(act["assets"]["large_text"] == "Pale Reach");
    CHECK(!act["assets"].contains("small_image"));
    CHECK(!act.contains("party"));
    CHECK(!act.contains("secrets"));
}

COA_TEST(DiscordIpc_SetActivityPartyAndSecrets) {
    CoA::Discord::Activity a;
    a.state = "s";
    a.partyId = "group-1";
    a.partySize = 2;
    a.partyMax = 5;
    a.join = "join-secret";
    a.smallImage = "ascension_logo";
    a.smallText = "x\"y\\z";              // escaped by the JSON writer
    const nlohmann::json act = Body(CoA::DiscordIpc::SetActivity(a, 1, 1))["args"]["activity"];
    CHECK(act["party"]["id"] == "group-1");
    CHECK(act["party"]["size"] == nlohmann::json::array({ 2, 5 }));
    CHECK(act["secrets"]["join"] == "join-secret");
    CHECK(!act["secrets"].contains("match"));
    CHECK(act["assets"]["small_text"] == "x\"y\\z");
    CHECK(!act.contains("timestamps"));
}

// An empty activity clears the status (SET_ACTIVITY without "activity").
COA_TEST(DiscordIpc_ClearActivity) {
    const nlohmann::json j = Body(CoA::DiscordIpc::SetActivity(CoA::Discord::Activity(), 9, 2));
    CHECK(j["cmd"] == "SET_ACTIVITY");
    CHECK(j["args"]["pid"] == 9);
    CHECK(!j["args"].contains("activity"));
}

COA_TEST(DiscordIpc_Subscribe) {
    const nlohmann::json j = Body(CoA::DiscordIpc::Subscribe("ACTIVITY_JOIN", 3));
    CHECK(j["cmd"] == "SUBSCRIBE");
    CHECK(j["evt"] == "ACTIVITY_JOIN");
    CHECK(j["nonce"] == "3");
}

// Frames arrive in pieces; Take returns whole frames only and keeps the rest.
COA_TEST(DiscordIpc_TakeWholeFrames) {
    std::vector<uint8_t> buffer;
    const std::vector<uint8_t> a = CoA::DiscordIpc::Frame(1, "{\"evt\":\"READY\"}");
    const std::vector<uint8_t> b = CoA::DiscordIpc::Frame(3, "{}");
    buffer.insert(buffer.end(), a.begin(), a.begin() + 5);
    CoA::DiscordIpc::Message m;
    CHECK(!CoA::DiscordIpc::Take(buffer, m));
    buffer.insert(buffer.end(), a.begin() + 5, a.end());
    buffer.insert(buffer.end(), b.begin(), b.end());
    CHECK(CoA::DiscordIpc::Take(buffer, m));
    CHECK(m.op == 1 && m.json == "{\"evt\":\"READY\"}");
    CHECK(CoA::DiscordIpc::Take(buffer, m));
    CHECK(m.op == 3 && m.json == "{}");
    CHECK(buffer.empty());
    CHECK(!CoA::DiscordIpc::Take(buffer, m));
}

// A length past the 64 KiB a Discord frame can carry is a broken stream, not a wait for more bytes.
COA_TEST(DiscordIpc_OversizedFrameIsAnError) {
    std::vector<uint8_t> buffer = Bytes(std::string("\x01\x00\x00\x00\xFF\xFF\xFF\x7F", 8));
    CoA::DiscordIpc::Message m;
    CHECK(!CoA::DiscordIpc::Take(buffer, m));
    CHECK(CoA::DiscordIpc::Broken(buffer));
}

COA_TEST(DiscordIpc_ParseDispatches) {
    CoA::DiscordIpc::Event e = CoA::DiscordIpc::Parse(
        "{\"cmd\":\"DISPATCH\",\"evt\":\"READY\",\"data\":{\"v\":1,\"user\":{\"id\":\"1234567890123456789\",\"username\":\"Name\"}}}");
    CHECK(e.kind == CoA::DiscordIpc::Event::Ready);
    CHECK(e.userId == 1234567890123456789ull);
    CHECK(e.username == "Name");

    e = CoA::DiscordIpc::Parse("{\"cmd\":\"DISPATCH\",\"evt\":\"ACTIVITY_JOIN\",\"data\":{\"secret\":\"abc\"}}");
    CHECK(e.kind == CoA::DiscordIpc::Event::Join);
    CHECK(e.secret == "abc");

    e = CoA::DiscordIpc::Parse("{\"cmd\":\"DISPATCH\",\"evt\":\"ACTIVITY_JOIN_REQUEST\",\"data\":{\"user\":{\"id\":\"42\"}}}");
    CHECK(e.kind == CoA::DiscordIpc::Event::JoinRequest);
    CHECK(e.userId == 42);

    e = CoA::DiscordIpc::Parse("{\"cmd\":\"SET_ACTIVITY\",\"evt\":\"ERROR\",\"data\":{\"code\":4000,\"message\":\"bad\"}}");
    CHECK(e.kind == CoA::DiscordIpc::Event::Error);
    CHECK(e.message == "bad");

    CHECK(CoA::DiscordIpc::Parse("not json").kind == CoA::DiscordIpc::Event::Other);
    CHECK(CoA::DiscordIpc::Parse("{\"cmd\":\"SET_ACTIVITY\",\"evt\":null}").kind == CoA::DiscordIpc::Event::Other);
}

// Discord accepts about 5 activity updates per 20 s; the client sends the newest one at most every 4 s (a burst left
// the app showing an old status until it was restarted).
COA_TEST(DiscordIpc_UpdatesAreSpacedFourSecondsApart) {
    CHECK(CoA::DiscordIpc::SendNow(true, false, 0, 1000));             // first ever: at once
    CHECK(!CoA::DiscordIpc::SendNow(false, true, 1000, 9000));         // nothing pending
    CHECK(!CoA::DiscordIpc::SendNow(true, true, 1000, 4999));          // 3999 ms after the last
    CHECK(CoA::DiscordIpc::SendNow(true, true, 1000, 5000));           // 4000 ms
    CHECK(CoA::DiscordIpc::SendNow(true, true, 0xFFFFF000u, 0x00000FA0u));   // tick wrap: 8000 ms later
}
