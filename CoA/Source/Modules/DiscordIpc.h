#ifndef COA_SOURCE_MODULES_DISCORDIPC_H
#define COA_SOURCE_MODULES_DISCORDIPC_H

#include "CoA/Source/Modules/Discord.h"
#include <cstdint>
#include <string>
#include <vector>

// CoA64 addition (not in the original): the Discord desktop app's local IPC protocol, spoken directly over the
// \\.\pipe\discord-ipc-N pipe in place of the 2019 Game SDK -- the SDK still loads and "creates" a core, but no longer
// completes its session with current Discord apps (no current user, no visible activity), while a plain IPC client
// does. Frames are {u32 op, u32 length, UTF-8 JSON}; op 0 handshake, 1 frame, 2 close, 3 ping, 4 pong.
namespace CoA::DiscordIpc {
    enum Op : uint32_t { Handshake_ = 0, Frame_ = 1, Close = 2, Ping = 3, Pong = 4 };

    // The largest frame accepted from the pipe; anything longer is a broken stream.
    constexpr uint32_t kMaxFrame = 0x10000;

    std::vector<uint8_t> Frame(uint32_t op, const std::string& json);
    std::vector<uint8_t> Handshake(uint64_t clientId);
    // SET_ACTIVITY for the given process; an activity with nothing set clears the status.
    std::vector<uint8_t> SetActivity(const CoA::Discord::Activity& activity, uint32_t pid, uint32_t nonce);
    std::vector<uint8_t> Subscribe(const char* event, uint32_t nonce);

    struct Message {
        uint32_t op = 0;
        std::string json;
    };

    // Removes the first whole frame from the buffer; false while it is incomplete (or broken, see Broken).
    bool Take(std::vector<uint8_t>& buffer, Message& out);
    // The buffer starts with a frame header whose length exceeds kMaxFrame.
    bool Broken(const std::vector<uint8_t>& buffer);

    struct Event {
        // Answer: a command's success reply (evt null); Error replies carry the command's nonce too.
        enum Kind { Other, Ready, Join, JoinRequest, Error, Answer } kind = Other;
        uint64_t userId = 0;
        uint32_t nonce = 0;
        std::string username, secret, message;
    };

    Event Parse(const std::string& json);

    // Discord accepts about 5 activity updates per 20 s; a burst left the app showing an old status until restarted.
    // The newest pending activity goes out at most every kMinSpacingMs (GetTickCount values, wrap-safe).
    constexpr uint32_t kMinSpacingMs = 4000;
    bool SendNow(bool pending, bool sentBefore, uint32_t lastSentMs, uint32_t nowMs);

    // How long a sent status waits for Discord's answer before it is sent again.
    constexpr uint32_t kConfirmMs = 5000;

    // Delivery of the status: only the newest one matters (Queue marks it changed). It goes out once the handshake is
    // answered (Ready), at most every kMinSpacingMs, and is resent until Discord confirms it -- an error answer or no
    // answer within kConfirmMs queues it again; a lost connection resends it after the next Ready.
    class Outbox {
    public:
        void Queue();
        void Ready(uint32_t nowMs);
        void Lost();
        bool Due(uint32_t nowMs);
        void Sent(uint32_t nonce, uint32_t nowMs);
        void Answered(uint32_t nonce, bool ok, uint32_t nowMs);

    private:
        bool m_has = false;        // a status was ever queued
        bool m_ready = false;
        bool m_pending = false;
        bool m_inflight = false;
        uint32_t m_nonce = 0;
        bool m_sentBefore = false;
        uint32_t m_lastSent = 0;
    };
}

#endif
