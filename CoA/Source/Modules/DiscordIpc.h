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
        enum Kind { Other, Ready, Join, JoinRequest, Error } kind = Other;
        uint64_t userId = 0;
        std::string username, secret, message;
    };

    Event Parse(const std::string& json);
}

#endif
