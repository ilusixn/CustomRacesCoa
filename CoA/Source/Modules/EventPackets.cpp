// SMSG handlers that turn a packet straight into a UI event (port of AscEventPackets.cpp's installer FUN_100d4b90).
// The handlers that need the client (the bulk query repeats 0x58E / 0x58F / 0x590, the packet broadcast 0x904, group
// connection 0x675, encounter state 0x68F, server CVars 0x668, spell failure 0x742 and the level watch) are in
// EventPacketsClient.cpp.
//   0x588 (FUN_100d3800)  u32 n: n ? BATTLEGROUND_COUNTDOWN_STARTED("%u", n) : BATTLEGROUND_STARTED
//   0x5A5 (FUN_100d49c0)  u32 length + bytes -> TOGGLE_GAME_MODE_RESULT("%s")
//   0x687 (FUN_100d45a0)  s, s, u32, u32, u32, s, s, u32, u8 -> TALKING_HEAD_FRAME_DISPLAY ("%s%s%u%u%u%s%s%u%b")
//   0x736 (FUN_100d4440)  s -> SUBMIT_GAME_FEEDBACK_RESULT("%s")
//   0x73E (FUN_100d4030)  u32 -> OPEN_CUSTOM_STORE("%u")
//   0x76A (FUN_100d42f0)  u8, u32 -> SET_ACTION_BUTTON_SPELL_PAYLOAD("%u%u")
//   0x76B / 0x76C         u32, u8, u8 -> ITEM_USED_PAYLOAD / ITEM_USE_FAILED_PAYLOAD("%u%u%u")
//   0x76D (FUN_100d4150)  u32, u32 -> PROCESSED_STATISTIC_QUERY_PAYLOAD("%u%d")
//   0x76E (FUN_100d40b0)  u32, u32 -> PROCESSED_ASSET_QUERY_PAYLOAD("%u%u")
//   0x754..0x75D (FUN_10225970) C string -> DRAFT_RESULT("%s"), then the Logger (channel 2) line
//                 "Draft result <opcode>: <text>"
//   0x661 (FUN_101102c0)  nothing read -> BUILD_DRAFT_PICK_PROMPT
//   0x73C (FUN_10275310)  C string -> TOGGLE_FEL_COMMUTATION_STATUS_RESULT("%s")
//   0x92B (FUN_100e6de0)  u32 a, b, c -> ASCENSION_BG_QUEUE_ALERT("%u%u%u")
//   0x68E (FUN_10a4fa40)  C string: fires the event of that name, no arguments
//   0x906 (FUN_100d55e0)  u8 window, u8 visible -> <window's name (FUN_100d3620)>("%b", visible != 0)
//   0x72D (FUN_10193a00)  u32 -> DAT_10BDEBB4 (reset to 0 at the glue screen); its accessor has no caller
//   0x9B1 (FUN_10235a90)  u32 spell, u32-length texture, u32 position (low byte), float scale, u32 r, g, b
//                 -> SPELL_ACTIVATION_SHOW("%u%s%s%f%u%u%u", spell, texture, position name, ...)
//   0x9B2 (FUN_10235840)  u32 spell, u32 position (low byte): 0 -> SPELL_ACTIVATION_HIDE("%u", spell), else
//                 ("%u%s", spell, position name)
//   0x75F / 0x760 (FUN_10190700 / FUN_10190a30) C string: its index in the ACTIVATE_CHARACTER_* (8) /
//                 DEACTIVATE_CHARACTER_* (7) names, else the Logger (channel 2) line "Unexpected Value: <s>" and the last
//                 (_FAILED); then CHARACTER_ACTIVATE_RESULT / CHARACTER_DEACTIVATE_RESULT("%b%s", index == 0, s)
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/Events.h"
#include "CoA/Source/Runtime/Lifecycle.h"
#include "CoA/Source/Runtime/Logger.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Source/Runtime/Read.h"
#include <string>

namespace {
    void CharacterResult(CDataStore* msg, const char* const* names, uint32_t count, const char* event) {
        std::string s = CoA::Read::Str(msg);
        uint32_t index = 0;

        while (index < count && s != names[index]) {
            ++index;
        }

        if (index == count) {
            CoA::Logger::Write(CoA::Logger::Debug, "Unexpected Value: " + s);
            index = count - 1;
        }

        CoA::Events::Signal(event, "%b%s", index == 0 ? 1u : 0u, s.c_str());
    }

    int32_t On75F(void* param, uint32_t opcode, uint32_t time, CDataStore* msg) {
        static const char* const names[8] = {
            "ACTIVATE_CHARACTER_OK",
            "ACTIVATE_CHARACTER_NOT_FOUND",
            "ACTIVATE_CHARACTER_NOT_OWNED",
            "ACTIVATE_CHARACTER_ALREADY_ACTIVE",
            "ACTIVATE_CHARACTER_TRANSFER_LOCKED",
            "ACTIVATE_CHARACTER_ONLINE",
            "ACTIVATE_CHARACTER_MAX_ACTIVE",
            "ACTIVATE_CHARACTER_FAILED",
        };

        CharacterResult(msg, names, 8, "CHARACTER_ACTIVATE_RESULT");
        return 1;
    }

    int32_t On760(void* param, uint32_t opcode, uint32_t time, CDataStore* msg) {
        static const char* const names[7] = {
            "DEACTIVATE_CHARACTER_OK",
            "DEACTIVATE_CHARACTER_NOT_FOUND",
            "DEACTIVATE_CHARACTER_NOT_OWNED",
            "DEACTIVATE_CHARACTER_ALREADY_INACTIVE",
            "DEACTIVATE_CHARACTER_TRANSFER_LOCKED",
            "DEACTIVATE_CHARACTER_ONLINE",
            "DEACTIVATE_CHARACTER_FAILED",
        };

        CharacterResult(msg, names, 7, "CHARACTER_DEACTIVATE_RESULT");
        return 1;
    }
    int32_t On588(void*, uint32_t, uint32_t, CDataStore* msg) {
        const uint32_t n = CoA::Read::U32(msg);

        if (n) {
            CoA::Events::Signal("BATTLEGROUND_COUNTDOWN_STARTED", "%u", n);
        } else {
            CoA::Events::Signal("BATTLEGROUND_STARTED", nullptr);
        }

        return 1;
    }

    int32_t On5A5(void*, uint32_t, uint32_t, CDataStore* msg) {
        const std::string s = CoA::Read::Name(msg);
        CoA::Events::Signal("TOGGLE_GAME_MODE_RESULT", "%s", s.c_str());
        return 1;
    }

    int32_t On687(void*, uint32_t, uint32_t, CDataStore* msg) {
        const std::string s1 = CoA::Read::Str(msg);
        const std::string s2 = CoA::Read::Str(msg);
        const uint32_t a = CoA::Read::U32(msg);
        const uint32_t b = CoA::Read::U32(msg);
        const uint32_t c = CoA::Read::U32(msg);
        const std::string s3 = CoA::Read::Str(msg);
        const std::string s4 = CoA::Read::Str(msg);
        const uint32_t d = CoA::Read::U32(msg);
        const uint32_t e = CoA::Read::U8(msg);
        CoA::Events::Signal("TALKING_HEAD_FRAME_DISPLAY", "%s%s%u%u%u%s%s%u%b", s1.c_str(), s2.c_str(), a, b, c, s3.c_str(), s4.c_str(), d, e);
        return 1;
    }

    // One C string -> event("%s").
    void StringEvent(CDataStore* msg, const char* event) {
        const std::string s = CoA::Read::Str(msg);
        CoA::Events::Signal(event, "%s", s.c_str());
    }

    int32_t On736(void*, uint32_t, uint32_t, CDataStore* msg) {
        StringEvent(msg, "SUBMIT_GAME_FEEDBACK_RESULT");
        return 1;
    }

    int32_t On73C(void*, uint32_t, uint32_t, CDataStore* msg) {
        StringEvent(msg, "TOGGLE_FEL_COMMUTATION_STATUS_RESULT");
        return 1;
    }

    int32_t On73E(void*, uint32_t, uint32_t, CDataStore* msg) {
        CoA::Events::Signal("OPEN_CUSTOM_STORE", "%u", CoA::Read::U32(msg));
        return 1;
    }

    int32_t On76A(void*, uint32_t, uint32_t, CDataStore* msg) {
        const uint32_t a = CoA::Read::U8(msg);
        const uint32_t b = CoA::Read::U32(msg);
        CoA::Events::Signal("SET_ACTION_BUTTON_SPELL_PAYLOAD", "%u%u", a, b);
        return 1;
    }

    // The original pushes the two bytes with stale upper bytes in the registers; here they are zero-extended.
    void ItemPayload(CDataStore* msg, const char* event) {
        const uint32_t item = CoA::Read::U32(msg);
        const uint32_t b = CoA::Read::U8(msg);
        const uint32_t c = CoA::Read::U8(msg);
        CoA::Events::Signal(event, "%u%u%u", item, b, c);
    }

    int32_t On76B(void*, uint32_t, uint32_t, CDataStore* msg) {
        ItemPayload(msg, "ITEM_USED_PAYLOAD");
        return 1;
    }

    int32_t On76C(void*, uint32_t, uint32_t, CDataStore* msg) {
        ItemPayload(msg, "ITEM_USE_FAILED_PAYLOAD");
        return 1;
    }

    int32_t On76D(void*, uint32_t, uint32_t, CDataStore* msg) {
        const uint32_t a = CoA::Read::U32(msg);
        const uint32_t b = CoA::Read::U32(msg);
        CoA::Events::Signal("PROCESSED_STATISTIC_QUERY_PAYLOAD", "%u%d", a, b);
        return 1;
    }

    int32_t On76E(void*, uint32_t, uint32_t, CDataStore* msg) {
        const uint32_t a = CoA::Read::U32(msg);
        const uint32_t b = CoA::Read::U32(msg);
        CoA::Events::Signal("PROCESSED_ASSET_QUERY_PAYLOAD", "%u%u", a, b);
        return 1;
    }

    int32_t OnDraftResult(void*, uint32_t opcode, uint32_t, CDataStore* msg) {
        const std::string s = CoA::Read::Str(msg);
        CoA::Events::Signal("DRAFT_RESULT", "%s", s.c_str());
        CoA::Logger::Write(CoA::Logger::Debug, "Draft result " + std::to_string(opcode) + ": " + s);
        return 1;
    }

    int32_t On661(void*, uint32_t, uint32_t, CDataStore*) {
        CoA::Events::Signal("BUILD_DRAFT_PICK_PROMPT", nullptr);
        return 1;
    }

    int32_t On92B(void*, uint32_t, uint32_t, CDataStore* msg) {
        const uint32_t a = CoA::Read::U32(msg);
        const uint32_t b = CoA::Read::U32(msg);
        const uint32_t c = CoA::Read::U32(msg);
        CoA::Events::Signal("ASCENSION_BG_QUEUE_ALERT", "%u%u%u", a, b, c);
        return 1;
    }

    int32_t On68E(void*, uint32_t, uint32_t, CDataStore* msg) {
        const std::string name = CoA::Read::Str(msg);
        CoA::Events::Signal(name.c_str(), nullptr);
        return 1;
    }

    int32_t On906(void*, uint32_t, uint32_t, CDataStore* msg) {
        static const char* const names[7] = {
            "ASCENSION_UNKNOWN_WINDOW_VISIBILITY_CHANGED",
            "ASCENSION_REFORGE_ENCHANTMENT_WINDOW_VISIBILITY_CHANGED",
            "ASCENSION_FEL_COMMUTATION_WINDOW_VISIBILITY_CHANGED",
            "ASCENSION_SKILL_CARDS_WINDOW_VISIBILITY_CHANGED",
            "ASCENSION_LUCKY_SKILL_CARDS_WINDOW_VISIBILITY_CHANGED",
            "ASCENSION_MYTHIC_PLUS_KEYSTONE_ACTIVATION_WINDOW_VISIBILITY_CHANGED",
            "ASCENSION_MAX_WINDOW_VISIBILITY_CHANGED",
        };

        const uint8_t window = CoA::Read::U8(msg);
        const uint8_t visible = CoA::Read::U8(msg);
        CoA::Events::Signal(window < 7 ? names[window] : "ASCENSION_UNHANDLED_WINDOW_VISIBILITY_CHANGED", "%b", visible ? 1u : 0u);
        return 1;
    }

    uint32_t s_value72D = 0;   // DAT_10BDEBB4

    int32_t On72D(void*, uint32_t, uint32_t, CDataStore* msg) {
        s_value72D = CoA::Read::U32(msg);
        return 1;
    }

    void Reset72D() {   // FUN_101939f0
        s_value72D = 0;
    }

    // FUN_10228a60: a position enum value as its name, or UNEXPECTED_ENUM_VALUE_<n>.
    std::string PositionName(uint8_t v) {
        static const char* const names[14] = {
            "NONE", "CENTER", "LEFT", "RIGHT", "TOP", "BOTTOM", "TOPRIGHT", "TOPLEFT", "BOTTOMRIGHT", "BOTTOMLEFT", "LEFT_FLIPPED",
            "RIGHT_FLIPPED", "TOP_FLIPPED", "BOTTOM_FLIPPED",
        };

        return v < 14 ? names[v] : "UNEXPECTED_ENUM_VALUE_" + std::to_string(v);
    }

    int32_t On9B1(void*, uint32_t, uint32_t, CDataStore* msg) {
        const uint32_t spell = CoA::Read::U32(msg);
        const std::string texture = CoA::Read::Name(msg);   // FUN_100d3680
        const std::string position = PositionName(static_cast<uint8_t>(CoA::Read::U32(msg)));
        const float scale = CoA::Read::F32(msg);
        const uint32_t r = CoA::Read::U32(msg);
        const uint32_t g = CoA::Read::U32(msg);
        const uint32_t b = CoA::Read::U32(msg);
        CoA::Events::Signal("SPELL_ACTIVATION_SHOW", "%u%s%s%f%u%u%u", spell, texture.c_str(), position.c_str(), static_cast<double>(scale), r, g, b);
        return 1;
    }

    int32_t On9B2(void*, uint32_t, uint32_t, CDataStore* msg) {
        const uint32_t spell = CoA::Read::U32(msg);
        const uint8_t position = static_cast<uint8_t>(CoA::Read::U32(msg));

        if (position == 0) {
            CoA::Events::Signal("SPELL_ACTIVATION_HIDE", "%u", spell);
        } else {
            CoA::Events::Signal("SPELL_ACTIVATION_HIDE", "%u%s", spell, PositionName(position).c_str());
        }

        return 1;
    }
}

uint32_t CoA::EventPackets::Value72D() {   // FUN_10193a50
    return s_value72D;
}

void CoA::EventPackets::Init() {
    COA_PACKET(0x75F, On75F);
    COA_PACKET(0x760, On760);
    COA_EVENT("CHARACTER_ACTIVATE_RESULT");
    COA_EVENT("CHARACTER_DEACTIVATE_RESULT");
    COA_PACKET(0x588, On588);
    COA_PACKET(0x5A5, On5A5);
    COA_PACKET(0x687, On687);
    COA_PACKET(0x736, On736);
    COA_PACKET(0x73E, On73E);
    COA_PACKET(0x76A, On76A);
    COA_PACKET(0x76B, On76B);
    COA_PACKET(0x76C, On76C);
    COA_PACKET(0x76D, On76D);
    COA_PACKET(0x76E, On76E);
    COA_PACKET(0x754, OnDraftResult);
    COA_PACKET(0x755, OnDraftResult);
    COA_PACKET(0x756, OnDraftResult);
    COA_PACKET(0x757, OnDraftResult);
    COA_PACKET(0x758, OnDraftResult);
    COA_PACKET(0x759, OnDraftResult);
    COA_PACKET(0x75A, OnDraftResult);
    COA_PACKET(0x75B, OnDraftResult);
    COA_PACKET(0x75C, OnDraftResult);
    COA_PACKET(0x75D, OnDraftResult);
    COA_PACKET(0x68E, On68E);
    CoA::Lifecycle::OnGlueScreen(Reset72D);
    COA_PACKET(0x72D, On72D);
    COA_PACKET(0x906, On906);
    COA_PACKET(0x661, On661);
    COA_PACKET(0x73C, On73C);
    COA_PACKET(0x92B, On92B);
    COA_PACKET(0x9B1, On9B1);
    COA_PACKET(0x9B2, On9B2);
    COA_EVENT("BATTLEGROUND_COUNTDOWN_STARTED");
    COA_EVENT("BATTLEGROUND_STARTED");
    COA_EVENT("TOGGLE_GAME_MODE_RESULT");
    COA_EVENT("TALKING_HEAD_FRAME_DISPLAY");
    COA_EVENT("SUBMIT_GAME_FEEDBACK_RESULT");
    COA_EVENT("OPEN_CUSTOM_STORE");
    COA_EVENT("SET_ACTION_BUTTON_SPELL_PAYLOAD");
    COA_EVENT("ITEM_USED_PAYLOAD");
    COA_EVENT("ITEM_USE_FAILED_PAYLOAD");
    COA_EVENT("PROCESSED_STATISTIC_QUERY_PAYLOAD");
    COA_EVENT("PROCESSED_ASSET_QUERY_PAYLOAD");
    COA_EVENT("DRAFT_RESULT");
    COA_EVENT("BUILD_DRAFT_PICK_PROMPT");
    COA_EVENT("TOGGLE_FEL_COMMUTATION_STATUS_RESULT");
    COA_EVENT("ASCENSION_BG_QUEUE_ALERT");
    COA_EVENT("SPELL_ACTIVATION_SHOW");
    COA_EVENT("SPELL_ACTIVATION_HIDE");
}
