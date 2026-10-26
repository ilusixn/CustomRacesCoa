// DiscordIpc.cpp (see DiscordIpc.h): the Discord IPC frames and JSON messages, without any transport.
#include "CoA/Source/Modules/DiscordIpc.h"
#include "ThirdParty/nlohmann/json.hpp"
#include <cstring>

using nlohmann::json;

namespace {
    void PutU32(std::vector<uint8_t>& out, uint32_t v) {
        for (int i = 0; i < 4; ++i) {
            out.push_back(static_cast<uint8_t>(v >> (8 * i)));
        }
    }

    uint32_t GetU32(const std::vector<uint8_t>& in, size_t at) {
        uint32_t v = 0;
        memcpy(&v, in.data() + at, 4);
        return v;
    }

    // Discord ids are 64-bit snowflakes sent as strings.
    uint64_t Id(const json& v) {
        if (v.is_string()) {
            return std::strtoull(v.get<std::string>().c_str(), nullptr, 10);
        }

        return v.is_number_unsigned() ? v.get<uint64_t>() : 0;
    }

    std::string Str(const json& v, const char* key) {
        return v.is_object() && v.contains(key) && v[key].is_string() ? v[key].get<std::string>() : std::string();
    }

    void PutIf(json& o, const char* key, const std::string& value) {
        if (!value.empty()) {
            o[key] = value;
        }
    }

    // Invalid UTF-8 in a game string would make the JSON writer throw; such bytes are replaced.
    std::string Dump(const json& j) {
        return j.dump(-1, ' ', false, json::error_handler_t::replace);
    }
}

std::vector<uint8_t> CoA::DiscordIpc::Frame(uint32_t op, const std::string& body) {
    std::vector<uint8_t> out;
    out.reserve(8 + body.size());
    PutU32(out, op);
    PutU32(out, static_cast<uint32_t>(body.size()));
    out.insert(out.end(), body.begin(), body.end());
    return out;
}

std::vector<uint8_t> CoA::DiscordIpc::Handshake(uint64_t clientId) {
    return Frame(Handshake_, Dump({ { "v", 1 }, { "client_id", std::to_string(clientId) } }));
}

std::vector<uint8_t> CoA::DiscordIpc::SetActivity(const CoA::Discord::Activity& a, uint32_t pid, uint32_t nonce) {
    json act = json::object();
    PutIf(act, "state", a.state);
    PutIf(act, "details", a.details);

    if (a.start || a.end) {
        json t = json::object();

        if (a.start) {
            t["start"] = a.start;
        }

        if (a.end) {
            t["end"] = a.end;
        }

        act["timestamps"] = t;
    }

    json assets = json::object();
    PutIf(assets, "large_image", a.largeImage);
    PutIf(assets, "large_text", a.largeText);
    PutIf(assets, "small_image", a.smallImage);
    PutIf(assets, "small_text", a.smallText);

    if (!assets.empty()) {
        act["assets"] = assets;
    }

    if (!a.partyId.empty()) {
        json party = { { "id", a.partyId } };

        if (a.partyMax > 0) {
            party["size"] = json::array({ a.partySize, a.partyMax });
        }

        act["party"] = party;
    }

    json secrets = json::object();
    PutIf(secrets, "match", a.match);
    PutIf(secrets, "join", a.join);
    PutIf(secrets, "spectate", a.spectate);

    if (!secrets.empty()) {
        act["secrets"] = secrets;
    } else if (!a.buttons.empty()) {
        json buttons = json::array();

        for (size_t i = 0; i < a.buttons.size() && i < 2; ++i) {
            buttons.push_back({ { "label", a.buttons[i].first }, { "url", a.buttons[i].second } });
        }

        act["buttons"] = buttons;
    }

    json args = { { "pid", pid } };

    if (!act.empty()) {
        args["activity"] = act;
    }

    return Frame(Frame_, Dump({ { "cmd", "SET_ACTIVITY" }, { "nonce", std::to_string(nonce) }, { "args", args } }));
}

std::vector<uint8_t> CoA::DiscordIpc::Subscribe(const char* event, uint32_t nonce) {
    return Frame(Frame_, Dump({ { "cmd", "SUBSCRIBE" }, { "evt", event }, { "nonce", std::to_string(nonce) }, { "args", json::object() } }));
}

bool CoA::DiscordIpc::Broken(const std::vector<uint8_t>& buffer) {
    return buffer.size() >= 8 && GetU32(buffer, 4) > kMaxFrame;
}

bool CoA::DiscordIpc::Take(std::vector<uint8_t>& buffer, Message& out) {
    if (buffer.size() < 8 || Broken(buffer)) {
        return false;
    }

    const uint32_t length = GetU32(buffer, 4);

    if (buffer.size() < 8 + static_cast<size_t>(length)) {
        return false;
    }

    out.op = GetU32(buffer, 0);
    out.json.assign(buffer.begin() + 8, buffer.begin() + 8 + length);
    buffer.erase(buffer.begin(), buffer.begin() + 8 + length);
    return true;
}

CoA::DiscordIpc::Event CoA::DiscordIpc::Parse(const std::string& text) {
    Event e;
    const json j = json::parse(text, nullptr, false);

    if (!j.is_object()) {
        return e;
    }

    const std::string evt = Str(j, "evt");
    const json data = j.contains("data") ? j["data"] : json();

    if (evt == "READY") {
        e.kind = Event::Ready;

        if (data.is_object() && data.contains("user") && data["user"].is_object()) {
            e.userId = Id(data["user"].value("id", json()));
            e.username = Str(data["user"], "username");
        }
    } else if (evt == "ACTIVITY_JOIN") {
        e.kind = Event::Join;
        e.secret = Str(data, "secret");
    } else if (evt == "ACTIVITY_JOIN_REQUEST") {
        e.kind = Event::JoinRequest;

        if (data.is_object() && data.contains("user") && data["user"].is_object()) {
            e.userId = Id(data["user"].value("id", json()));
        }
    } else if (evt == "ERROR") {
        e.kind = Event::Error;
        e.message = Str(data, "message");
    } else if (j.contains("evt") && j["evt"].is_null() && j.contains("cmd")) {
        e.kind = Event::Answer;
    }

    e.nonce = static_cast<uint32_t>(std::strtoul(Str(j, "nonce").c_str(), nullptr, 10));
    return e;
}

bool CoA::DiscordIpc::SendNow(bool pending, bool sentBefore, uint32_t lastSentMs, uint32_t nowMs) {
    return pending && (!sentBefore || nowMs - lastSentMs >= kMinSpacingMs);
}

void CoA::DiscordIpc::Outbox::Queue() {
    m_has = true;
    m_pending = true;
}

void CoA::DiscordIpc::Outbox::Ready(uint32_t) {
    m_ready = true;
    m_inflight = false;
    m_pending = m_has;
    m_sentBefore = false;
}

void CoA::DiscordIpc::Outbox::Lost() {
    m_ready = false;
    m_inflight = false;
}

bool CoA::DiscordIpc::Outbox::Due(uint32_t nowMs) {
    if (m_inflight && nowMs - m_lastSent >= kConfirmMs) {   // no answer: send the newest again
        m_inflight = false;
        m_pending = true;
    }

    return m_ready && !m_inflight && SendNow(m_pending, m_sentBefore, m_lastSent, nowMs);
}

void CoA::DiscordIpc::Outbox::Sent(uint32_t nonce, uint32_t nowMs) {
    m_inflight = true;
    m_nonce = nonce;
    m_pending = false;
    m_sentBefore = true;
    m_lastSent = nowMs;
}

void CoA::DiscordIpc::Outbox::Answered(uint32_t nonce, bool ok, uint32_t) {
    if (!m_inflight || nonce != m_nonce) {
        return;
    }

    m_inflight = false;

    if (!ok) {
        m_pending = true;
    }
}
