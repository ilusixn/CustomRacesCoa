// AscGMTicket.cpp (see GMTicket.h): the GM ticket list and its filtered view, the player's own ticket, the GM player
// records, their packets and the C_GMTicket / C_PlayerTicket / C_GM natives, transcribed as the decompile has them.
#include "CoA/Source/Modules/GMTicket.h"
#include "CoA/Source/Modules/Account.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/ClientState.h"
#include "CoA/Source/Runtime/Events.h"
#include "CoA/Source/Runtime/FilterContainer.h"
#include "CoA/Source/Runtime/Lifecycle.h"
#include "CoA/Source/Runtime/Log.h"
#include "CoA/Source/Runtime/Logger.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Packet.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Source/Runtime/Read.h"
#include "CoA/Source/Runtime/Script.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

using CoA::Script::CheckNumber;
using CoA::Script::CheckString;
using CoA::Script::PushBool;
using CoA::Script::PushNum;
using CoA::Script::PushStr;
using CoA::Script::ToInt;
using CoA::Script::ValidateInput;

namespace {
    struct ReadReceipt {
        std::string ticketId;
        uint32_t messageId = 0;
        std::string readBy;
        uint32_t timestamp = 0;
    };

    struct Message {   // 0x68
        std::string ticketId;
        uint32_t messageId = 0;
        bool isFromGM = false;
        std::string sender;
        bool hasMessage = false;
        std::string message;
        bool gmOnly = false;
        uint32_t timestamp = 0;
        std::vector<ReadReceipt> receipts;
    };

    struct Suggestion {   // 0x30: Command | SQL, Reason
        std::string text;
        std::string reason;
    };

    struct BugReport {   // 0x1C
        uint32_t id = 0;
        std::string reason;
    };

    struct Ticket {   // 0x110
        std::string ticketId;
        std::string title;
        uint32_t creationDate = 0;
        bool hasLastMessageDate = false;
        uint32_t lastMessageDate = 0;
        std::string creator;
        uint8_t status = 0;
        bool hasAISuggestion = false;
        std::string aiSuggestion;
        std::vector<Suggestion> commands;
        std::vector<Suggestion> sql;
        std::vector<BugReport> bugReports;
        bool hasSummary = false;
        std::string summary;
        std::vector<Message> messages;
        std::vector<std::string> assignedTo;
        bool lastMessageUnseen = false;
        bool anyMessageUnseen = false;
        bool lastMessageFromGM = false;
        uint8_t priority = 0;
        uint8_t category = 0;
        bool closedByCreator = false;
        bool hasCloseDate = false;
        uint32_t closeDate = 0;
        std::string creatorLocale;
        bool hasOnlineName = false;
        std::string onlineName;

        bool Closed() const {   // CLOSED (2) or LOCKED (5)
            return this->status == 2 || this->status == 5;
        }
    };

    // GM player record (0x198, reader FUN_102472d0, Lua writer FUN_10247d40).
    struct HistMessage {   // 0x3C
        bool isFromGM = false;
        bool gmOnly = false;
        std::string sender;
        bool hasMessage = false;
        std::string message;
        uint32_t timestamp = 0;
    };

    struct HistTicket {   // 0x48
        std::string ticketId;
        std::string title;
        uint32_t creationDate = 0;
        bool hasCloseDate = false;
        uint32_t closeDate = 0;
        std::vector<HistMessage> messages;
    };

    struct Mute {   // 0x54
        uint32_t date = 0;
        uint32_t time = 0;
        std::string reason;
        std::string by;
        uint8_t mode = 0;
        std::string realm;
    };

    struct Ban {   // 0x40
        uint32_t date = 0;
        uint32_t remaining = 0;
        bool active = false;
        uint32_t unbanDate = 0;
        std::string reason;
        std::string by;
    };

    struct Warning {   // 0x4C
        std::string realm;
        std::string character;
        std::string reason;
        uint32_t timestamp = 0;
    };

    struct Note {   // 0x38
        uint32_t id = 0;
        std::string gm;
        std::string message;
        uint32_t date = 0;
    };

    struct PlayerInfo {
        std::string name;
        uint32_t guid = 0;
        uint8_t level = 0;
        uint8_t race = 0;
        uint8_t classId = 0;
        uint32_t gameModeMask = 0;
        bool hasItemLevel = false;
        float itemLevel = 0;
        bool hasMoney = false;
        uint32_t money = 0;
        std::vector<std::pair<uint32_t, uint32_t>> challenges;
        bool hasMap = false;
        uint32_t map = 0;
        bool hasX = false;
        bool hasY = false;
        bool hasZ = false;
        float x = 0;
        float y = 0;
        float z = 0;
        bool hasZone = false;
        uint32_t zone = 0;
        bool hasArea = false;
        uint32_t area = 0;
        uint32_t guildId = 0;
        bool hasGuildName = false;
        std::string guildName;
        bool hasRank = false;
        uint8_t rank = 0;
        bool hasRankName = false;
        std::string rankName;
        uint32_t accountId = 0;
        std::string accountName;
        uint32_t gmLevel = 0;
        bool hasDiscord = false;
        std::string discord;
        bool hasHwid = false;
        std::string hwid;
        bool hasHwid2 = false;
        std::string hwid2;
        bool hasOnlineName = false;
        std::string onlineName;
        std::vector<HistTicket> tickets;
        uint32_t numTickets = 0;
        std::vector<Mute> mutes;
        uint32_t numMutes = 0;
        std::vector<Ban> bans;
        uint32_t numBans = 0;
        std::vector<Warning> warnings;
        uint32_t numWarnings = 0;
        std::vector<Note> notes;
        uint32_t numNotes = 0;
        bool hasLatency = false;
        uint32_t latency = 0;
    };

    const char* const kStatus[7] = { "NONE", "OPEN", "CLOSED", "WAITING_FOR_PLAYER", "WAITING_FOR_GAME_MASTER", "LOCKED",
        "ESCALATED_SECURITY_LEVEL" };   // 0x10B3E28C
    const char* const kPriority[4] = { "LOW", "MEDIUM", "HIGH", "URGENT" };   // 0x10B3E6C0
    const char* const kCategory[10] = { "GM_TICKET_CATEGORY_OTHER", "GM_TICKET_CATEGORY_TALENTS", "GM_TICKET_CATEGORY_DUNGEONS",
        "GM_TICKET_CATEGORY_ITEMS", "GM_TICKET_CATEGORY_SPELLS_AND_ABILITIES", "GM_TICKET_CATEGORY_WEBSITE_OR_LAUNCHER",
        "GM_TICKET_CATEGORY_MYSTIC_ENCHANTS", "GM_TICKET_CATEGORY_RAIDS", "GM_TICKET_CATEGORY_QUESTS",
        "GM_TICKET_CATEGORY_UI_OR_ADDONS" };   // 0x10B3E4F0
    const char* const kFilters[8] = { "FILTER_NONE", "FILTER_OPEN", "FILTER_CLOSED", "FILTER_ONLINE", "FILTER_OFFLINE",
        "FILTER_ASSIGNED_TO_ME", "FILTER_ASSIGNED_TO_OTHER", "FILTER_ASSIGNED_TO_NONE" };   // 0x10B3E400
    const char* const kSorts[5] = { "SORT_NONE", "SORT_CREATION_TIME_ASCENDING", "SORT_CREATION_TIME_DESCENDING",
        "SORT_LAST_PLAYER_REPLY_ASCENDING", "SORT_LAST_PLAYER_REPLY_DESCENDING" };   // 0x10B3E588
    const char* const kSecurity[8] = { "SEC_PLAYER", "SEC_MODERATOR", "SEC_GAMEMASTER", "SEC_GAMEMASTER2", "SEC_GAMEMASTER3",
        "SEC_GAMEMASTER4", "SEC_ADMINISTRATOR", "SEC_CONSOLE" };   // 0x10B3E540

    CoA::GMTicket::Deps s_deps;
    std::vector<Ticket> s_tickets;          // GMTicketMgr +0x00
    std::vector<PlayerInfo> s_playerInfo;   // GMTicketMgr +0x88 (appended per result, never deduplicated)
    Ticket s_current;                       // PlayerTicketMgr
    bool s_hasCurrent = false;              // PlayerTicketMgr +0x110

    bool Gate() {   // FUN_10111ce0
        return CoA::ClientState::CanPerformAction(0x11);
    }

    const char* PlayerName() {
        return s_deps.activePlayerName ? s_deps.activePlayerName() : nullptr;
    }

    // ---- wire (the original reads past the end unchecked; here every read past it is 0 / "" and each count loop
    // stops when the bytes run out) ----------------------------------------------------------------------------------
    bool OptStr(CDataStore* msg, std::string& out) {   // FUN_1023a8e0
        if (!CoA::Read::U8(msg)) {
            return false;
        }

        out = CoA::Read::Str(msg);
        return true;
    }

    bool OptU32(CDataStore* msg, uint32_t& out) {
        if (!CoA::Read::U8(msg)) {
            return false;
        }

        out = CoA::Read::U32(msg);
        return true;
    }

    bool OptF32(CDataStore* msg, float& out) {
        if (!CoA::Read::U8(msg)) {
            return false;
        }

        out = CoA::Read::F32(msg);
        return true;
    }

    bool OptU8(CDataStore* msg, uint8_t& out) {
        if (!CoA::Read::U8(msg)) {
            return false;
        }

        out = CoA::Read::U8(msg);
        return true;
    }

    bool More(CDataStore* msg) {
        return CoA::Read::Remaining(msg) != 0;
    }

    Message ReadMessage(CDataStore* msg) {   // FUN_102434c0
        Message m;
        m.ticketId = CoA::Read::Str(msg);
        m.messageId = CoA::Read::U32(msg);
        m.isFromGM = CoA::Read::U8(msg) != 0;
        m.sender = CoA::Read::Str(msg);
        m.hasMessage = OptStr(msg, m.message);
        m.gmOnly = CoA::Read::U8(msg) != 0;
        m.timestamp = CoA::Read::U32(msg);

        for (uint32_t n = CoA::Read::U32(msg); n && More(msg); --n) {   // FUN_1023b9d0 / FUN_10242a70
            ReadReceipt r;
            r.ticketId = CoA::Read::Str(msg);
            r.messageId = CoA::Read::U32(msg);
            r.readBy = CoA::Read::Str(msg);
            r.timestamp = CoA::Read::U32(msg);
            m.receipts.push_back(r);
        }

        return m;
    }

    std::vector<Suggestion> ReadSuggestions(CDataStore* msg) {   // FUN_1023c270
        std::vector<Suggestion> v;

        for (uint32_t n = CoA::Read::U32(msg); n && More(msg); --n) {
            Suggestion s;
            s.text = CoA::Read::Str(msg);
            s.reason = CoA::Read::Str(msg);
            v.push_back(s);
        }

        return v;
    }

    Ticket ReadTicket(CDataStore* msg) {   // FUN_10248730
        Ticket t;
        t.ticketId = CoA::Read::Str(msg);
        t.title = CoA::Read::Str(msg);
        t.creationDate = CoA::Read::U32(msg);
        t.hasLastMessageDate = OptU32(msg, t.lastMessageDate);
        t.creator = CoA::Read::Str(msg);
        t.status = CoA::Read::U8(msg);
        t.hasAISuggestion = OptStr(msg, t.aiSuggestion);
        t.commands = ReadSuggestions(msg);
        t.sql = ReadSuggestions(msg);

        for (uint32_t n = CoA::Read::U32(msg); n && More(msg); --n) {   // FUN_1023bdf0
            BugReport b;
            b.id = CoA::Read::U32(msg);
            b.reason = CoA::Read::Str(msg);
            t.bugReports.push_back(b);
        }

        t.hasSummary = OptStr(msg, t.summary);

        for (uint32_t n = CoA::Read::U32(msg); n && More(msg); --n) {   // FUN_1023cfc0
            t.messages.push_back(ReadMessage(msg));
        }

        for (uint32_t n = CoA::Read::U32(msg); n && More(msg); --n) {   // FUN_1023d4d0
            t.assignedTo.push_back(CoA::Read::Str(msg));
        }

        t.lastMessageUnseen = CoA::Read::U8(msg) != 0;
        t.anyMessageUnseen = CoA::Read::U8(msg) != 0;
        t.lastMessageFromGM = CoA::Read::U8(msg) != 0;
        t.priority = CoA::Read::U8(msg);
        t.category = CoA::Read::U8(msg);
        t.closedByCreator = CoA::Read::U8(msg) != 0;
        t.hasCloseDate = OptU32(msg, t.closeDate);
        t.creatorLocale = CoA::Read::Str(msg);
        t.hasOnlineName = OptStr(msg, t.onlineName);
        return t;
    }

    PlayerInfo ReadPlayerInfo(CDataStore* msg) {   // FUN_102472d0
        PlayerInfo r;
        r.name = CoA::Read::Str(msg);
        r.guid = CoA::Read::U32(msg);
        r.level = CoA::Read::U8(msg);
        r.race = CoA::Read::U8(msg);
        r.classId = CoA::Read::U8(msg);
        r.gameModeMask = CoA::Read::U32(msg);
        r.hasItemLevel = OptF32(msg, r.itemLevel);
        r.hasMoney = OptU32(msg, r.money);

        for (uint32_t n = CoA::Read::U32(msg); n && CoA::Read::Remaining(msg) >= 8; --n) {   // FUN_10117120
            const uint32_t id = CoA::Read::U32(msg);
            r.challenges.emplace_back(id, CoA::Read::U32(msg));
        }

        r.hasMap = OptU32(msg, r.map);
        r.hasX = OptF32(msg, r.x);
        r.hasY = OptF32(msg, r.y);
        r.hasZ = OptF32(msg, r.z);
        r.hasZone = OptU32(msg, r.zone);
        r.hasArea = OptU32(msg, r.area);
        r.guildId = CoA::Read::U32(msg);
        r.hasGuildName = OptStr(msg, r.guildName);
        r.hasRank = OptU8(msg, r.rank);
        r.hasRankName = OptStr(msg, r.rankName);
        r.accountId = CoA::Read::U32(msg);
        r.accountName = CoA::Read::Str(msg);
        r.gmLevel = CoA::Read::U32(msg);
        r.hasDiscord = OptStr(msg, r.discord);
        r.hasHwid = OptStr(msg, r.hwid);
        r.hasHwid2 = OptStr(msg, r.hwid2);
        r.hasOnlineName = OptStr(msg, r.onlineName);

        for (uint32_t n = CoA::Read::U32(msg); n && More(msg); --n) {   // FUN_1023c6c0
            HistTicket t;
            t.ticketId = CoA::Read::Str(msg);
            t.title = CoA::Read::Str(msg);
            t.creationDate = CoA::Read::U32(msg);
            t.hasCloseDate = OptU32(msg, t.closeDate);

            for (uint32_t m = CoA::Read::U32(msg); m && More(msg); --m) {   // FUN_1023cc40
                HistMessage h;
                h.isFromGM = CoA::Read::U8(msg) != 0;
                h.gmOnly = CoA::Read::U8(msg) != 0;
                h.sender = CoA::Read::Str(msg);
                h.hasMessage = OptStr(msg, h.message);
                h.timestamp = CoA::Read::U32(msg);
                t.messages.push_back(h);
            }

            r.tickets.push_back(t);
        }

        r.numTickets = CoA::Read::U32(msg);

        for (uint32_t n = CoA::Read::U32(msg); n && More(msg); --n) {   // FUN_1023b1a0
            Mute m;
            m.date = CoA::Read::U32(msg);
            m.time = CoA::Read::U32(msg);
            m.reason = CoA::Read::Str(msg);
            m.by = CoA::Read::Str(msg);
            m.mode = CoA::Read::U8(msg);
            m.realm = CoA::Read::Str(msg);
            r.mutes.push_back(m);
        }

        r.numMutes = CoA::Read::U32(msg);

        for (uint32_t n = CoA::Read::U32(msg); n && More(msg); --n) {   // FUN_1023aaa0
            Ban b;
            b.date = CoA::Read::U32(msg);
            b.remaining = CoA::Read::U32(msg);
            b.active = CoA::Read::U8(msg) != 0;
            b.unbanDate = CoA::Read::U32(msg);
            b.reason = CoA::Read::Str(msg);
            b.by = CoA::Read::Str(msg);
            r.bans.push_back(b);
        }

        r.numBans = CoA::Read::U32(msg);

        for (uint32_t n = CoA::Read::U32(msg); n && More(msg); --n) {   // FUN_1023b5c0
            Warning w;
            w.realm = CoA::Read::Str(msg);
            w.character = CoA::Read::Str(msg);
            w.reason = CoA::Read::Str(msg);
            w.timestamp = CoA::Read::U32(msg);
            r.warnings.push_back(w);
        }

        r.numWarnings = CoA::Read::U32(msg);

        for (uint32_t n = CoA::Read::U32(msg); n && More(msg); --n) {   // FUN_1023ae00
            Note note;
            note.id = CoA::Read::U32(msg);
            note.gm = CoA::Read::Str(msg);
            note.message = CoA::Read::Str(msg);
            note.date = CoA::Read::U32(msg);
            r.notes.push_back(note);
        }

        r.numNotes = CoA::Read::U32(msg);
        r.hasLatency = OptU32(msg, r.latency);   // FUN_1023a890
        return r;
    }

    Ticket* FindTicket(const std::string& id) {   // FUN_1024a630
        for (Ticket& t : s_tickets) {
            if (t.ticketId == id) {
                return &t;
            }
        }

        return nullptr;
    }

    // ---- Lua writers --------------------------------------------------------------------------------------------
    void SetStr(lua_State* L, const char* k, const std::string& v) {
        lua_pushstring(L, k);
        PushStr(L, v.c_str());
        lua_settable(L, -3);
    }

    void SetOptStr(lua_State* L, const char* k, bool has, const std::string& v) {   // FUN_10240c10
        lua_pushstring(L, k);

        if (has) {
            PushStr(L, v.c_str());
        } else {
            lua_pushnil(L);
        }

        lua_settable(L, -3);
    }

    void SetU32(lua_State* L, const char* k, uint32_t v) {   // FUN_1009b2a0 = lua_pushinteger
        lua_pushstring(L, k);
        lua_pushinteger(L, static_cast<int32_t>(v));
        lua_settable(L, -3);
    }

    void SetOptU32(lua_State* L, const char* k, bool has, uint32_t v) {
        lua_pushstring(L, k);

        if (has) {
            lua_pushinteger(L, static_cast<int32_t>(v));
        } else {
            lua_pushnil(L);
        }

        lua_settable(L, -3);
    }

    void SetOptF32(lua_State* L, const char* k, bool has, float v) {   // FUN_1009b2b0 = pushnumber((double)f)
        lua_pushstring(L, k);

        if (has) {
            lua_pushnumber(L, static_cast<double>(v));
        } else {
            lua_pushnil(L);
        }

        lua_settable(L, -3);
    }

    void SetBool(lua_State* L, const char* k, bool v) {
        lua_pushstring(L, k);
        PushBool(L, v);
        lua_settable(L, -3);
    }

    // FUN_10241840 / FUN_102416e0 / FUN_10241580 / FUN_10241430: the enum's name, or UNEXPECTED_ENUM_VALUE_<n>.
    void SetEnum(lua_State* L, const char* k, uint32_t v, const char* const* names, uint32_t count) {
        lua_pushstring(L, k);

        if (v < count) {
            lua_pushstring(L, names[v]);
        } else {
            lua_pushstring(L, ("UNEXPECTED_ENUM_VALUE_" + std::to_string(v)).c_str());
        }

        lua_settable(L, -3);
    }

    void NewTable(lua_State* L, int arr, int rec) {
        lua_createtable(L, arr, rec);
        lua_checkstack(L, 2);
    }

    // Array entries are keyed with lua_pushnumber(i), as the original's vector writers do.
    void Key(lua_State* L, uint32_t i) {
        lua_pushnumber(L, static_cast<double>(i));
    }

    void PushMessage(lua_State* L, const Message& m) {   // FUN_10243760
        NewTable(L, 0, 8);
        SetStr(L, "TicketID", m.ticketId);
        SetU32(L, "MessageID", m.messageId);
        SetBool(L, "IsFromGM", m.isFromGM);
        SetStr(L, "Sender", m.sender);
        SetOptStr(L, "Message", m.hasMessage, m.message);
        SetBool(L, "GMOnly", m.gmOnly);
        SetU32(L, "Timestamp", m.timestamp);
        lua_pushstring(L, "ReadReceipts");
        NewTable(L, static_cast<int>(m.receipts.size()), 0);   // FUN_1023fee0

        for (uint32_t i = 0; i < m.receipts.size(); ++i) {
            const ReadReceipt& r = m.receipts[i];
            Key(L, i + 1);
            NewTable(L, 0, 4);
            SetStr(L, "TicketID", r.ticketId);
            SetU32(L, "MessageID", r.messageId);
            SetStr(L, "ReadBy", r.readBy);
            SetU32(L, "Timestamp", r.timestamp);
            lua_settable(L, -3);
        }

        lua_settable(L, -3);
    }

    void PushMessages(lua_State* L, const std::vector<Message>& v) {   // FUN_10240750
        NewTable(L, static_cast<int>(v.size()), 0);

        for (uint32_t i = 0; i < v.size(); ++i) {
            Key(L, i + 1);
            PushMessage(L, v[i]);
            lua_settable(L, -3);
        }
    }

    void SetSuggestions(lua_State* L, const char* k, const char* textKey, const std::vector<Suggestion>& v) {   // FUN_10240260 / FUN_10240400
        lua_pushstring(L, k);
        NewTable(L, static_cast<int>(v.size()), 0);

        for (uint32_t i = 0; i < v.size(); ++i) {
            Key(L, i + 1);
            NewTable(L, 0, 2);
            SetStr(L, textKey, v[i].text);
            SetStr(L, "Reason", v[i].reason);
            lua_settable(L, -3);
        }

        lua_settable(L, -3);
    }

    void PushTicket(lua_State* L, const Ticket& t) {   // FUN_10248ea0
        NewTable(L, 0, 0x16);
        SetStr(L, "TicketID", t.ticketId);
        SetStr(L, "Title", t.title);
        SetU32(L, "CreationDate", t.creationDate);
        SetOptU32(L, "LastMessageDate", t.hasLastMessageDate, t.lastMessageDate);
        SetStr(L, "Creator", t.creator);
        SetEnum(L, "Status", t.status, kStatus, 7);
        SetOptStr(L, "AISuggestion", t.hasAISuggestion, t.aiSuggestion);
        SetSuggestions(L, "AISuggestedCommands", "Command", t.commands);
        SetSuggestions(L, "AISuggestedSQL", "SQL", t.sql);
        lua_pushstring(L, "AISuggestedBugTrackerReports");   // FUN_102400d0
        NewTable(L, static_cast<int>(t.bugReports.size()), 0);

        for (uint32_t i = 0; i < t.bugReports.size(); ++i) {
            Key(L, i + 1);
            NewTable(L, 0, 2);
            SetU32(L, "BugTrackerReportID", t.bugReports[i].id);
            SetStr(L, "Reason", t.bugReports[i].reason);
            lua_settable(L, -3);
        }

        lua_settable(L, -3);
        SetOptStr(L, "AISuggestionSummary", t.hasSummary, t.summary);
        lua_pushstring(L, "Messages");
        PushMessages(L, t.messages);
        lua_settable(L, -3);
        lua_pushstring(L, "AssignedTo");   // FUN_100beb90
        NewTable(L, static_cast<int>(t.assignedTo.size()), 0);

        for (uint32_t i = 0; i < t.assignedTo.size(); ++i) {
            Key(L, i + 1);
            PushStr(L, t.assignedTo[i].c_str());
            lua_settable(L, -3);
        }

        lua_settable(L, -3);
        SetBool(L, "IsLastMessageUnseen", t.lastMessageUnseen);
        SetBool(L, "IsAnyMessageUnseen", t.anyMessageUnseen);
        SetBool(L, "IsLastMessageFromGM", t.lastMessageFromGM);
        SetEnum(L, "Priority", t.priority, kPriority, 4);
        SetEnum(L, "Category", t.category, kCategory, 10);
        SetBool(L, "IsClosedByCreator", t.closedByCreator);
        SetOptU32(L, "CloseDate", t.hasCloseDate, t.closeDate);
        SetStr(L, "CreatorLocale", t.creatorLocale);
        SetOptStr(L, "OnlineCharacterName", t.hasOnlineName, t.onlineName);
    }

    // ---- GMTicketContainer --------------------------------------------------------------------------------------
    bool Contains(const std::string& field, const std::string& lower) {
        return CoA::Filter::Lower(field).find(lower) != std::string::npos;
    }

    class TicketContainer : public CoA::Filter::Container<Ticket> {
    protected:
        std::vector<Ticket> DoPopulate() override {   // FUN_10250190: snapshot copies of the manager's list
            return s_tickets;
        }

        uint32_t ArgGroup(uint32_t f) override {   // [4] FUN_10251e40
            if (f == 1 || f == 2) {
                return 1;
            }

            if (f == 3 || f == 4) {
                return 2;
            }

            return f >= 5 && f <= 7 ? 3 : 0;
        }

        bool ArgMatch(uint32_t f, const Ticket& t) override {   // [2] FUN_10251c00
            switch (f) {
            case 1:
                return !t.Closed();
            case 2:
                return t.Closed();
            case 3:
                return t.hasOnlineName;
            case 4:
                return !t.hasOnlineName;
            case 5:
            case 6: {
                const char* me = PlayerName();

                if (!me || t.assignedTo.empty()) {
                    return false;
                }

                const bool mine = std::find(t.assignedTo.begin(), t.assignedTo.end(), std::string(me)) != t.assignedTo.end();
                return f == 5 ? mine : !mine;
            }
            case 7:
                return t.assignedTo.empty();
            default:
                return false;
            }
        }

        // [3] FUN_10250940: a case-insensitive substring of any text the ticket carries.
        bool TextMatch(const std::string& s, const Ticket& t) override {
            if (s.empty()) {
                return true;
            }

            if (Contains(t.title, s) || Contains(t.creator, s) || Contains(t.ticketId, s)) {
                return true;
            }

            for (const std::string& a : t.assignedTo) {
                if (Contains(a, s)) {
                    return true;
                }
            }

            for (const Message& m : t.messages) {
                if ((m.hasMessage && Contains(m.message, s)) || Contains(m.sender, s)) {
                    return true;
                }
            }

            if ((t.hasAISuggestion && Contains(t.aiSuggestion, s)) || (t.hasSummary && Contains(t.summary, s))) {
                return true;
            }

            for (const std::vector<Suggestion>* list : { &t.commands, &t.sql }) {
                for (const Suggestion& c : *list) {
                    if (Contains(c.text, s) || Contains(c.reason, s)) {
                        return true;
                    }
                }
            }

            for (const BugReport& b : t.bugReports) {
                if (Contains(b.reason, s)) {
                    return true;
                }
            }

            return false;
        }

        // [7] FUN_10251e90: 1 bit closed, 1 bit "no close date", then the 64-bit 0xFFFFFFFF:~date (CloseDate when closed
        // and set, else CreationDate): open before closed, a close date before none, newest first.
        std::string PopulateKey(const Ticket& t) override {
            std::string k;
            const bool closed = t.Closed();
            CoA::Filter::AppendByte(k, closed ? 1 : 0);
            CoA::Filter::AppendByte(k, t.hasCloseDate ? 0 : 1);
            CoA::Filter::AppendU32(k, 0xFFFFFFFFu);
            CoA::Filter::AppendU32(k, ~((closed && t.hasCloseDate) ? t.closeDate : t.creationDate));
            return k;
        }

        // [9] FUN_10252570, 64-bit: 1 CreationDate, 2 0xFFFFFFFF:~CreationDate, 3 / 4 the last player (not GM) message's
        // Timestamp (4 inverted), all-ones without one.
        std::string SortKey(uint32_t mode, const Ticket& t) override {
            uint64_t v = 0;

            switch (mode) {
            case 1:
                v = t.creationDate;
                break;
            case 2:
                v = (0xFFFFFFFFull << 32) | static_cast<uint32_t>(~t.creationDate);
                break;
            case 3:
            case 4:
                v = ~0ull;

                for (auto it = t.messages.rbegin(); it != t.messages.rend(); ++it) {
                    if (!it->isFromGM) {
                        v = it->timestamp;
                        break;
                    }
                }

                if (mode == 4 && v != ~0ull) {
                    v = ~v;
                }

                break;
            default:
                break;
            }

            std::string k;
            CoA::Filter::AppendU64(k, v);
            return k;
        }
    };

    TicketContainer s_view;   // GMTicketMgr's container

    void ResetView() {   // FUN_1024d1f0
        s_view.Reset();
    }

    // ---- argument readers ---------------------------------------------------------------------------------------
    bool Present(lua_State* L, int idx) {   // DAT_10bc93c4 = lua_type
        return lua_type(L, idx) > 0;
    }

    bool OptStringAt(lua_State* L, int idx, std::string& out) {   // FUN_100d0970
        if (!Present(L, idx)) {
            return false;
        }

        out = CheckString(L, idx);
        return true;
    }

    std::string StringAt(lua_State* L, int idx) {
        const char* s = CheckString(L, idx);
        return s ? s : "";
    }

    // lua_tovalue<enum> (FUN_102419a0 / FUN_10241be0 / FUN_10253070): exact name, else logged and read as 0.
    uint8_t EnumArg(lua_State* L, int idx, const char* const* names, uint32_t count, const char* enumName) {
        const std::string s = StringAt(L, idx);

        for (uint32_t i = 0; i < count; ++i) {
            if (s == names[i]) {
                return static_cast<uint8_t>(i);
            }
        }

        CoA::Log::Printf("lua_tovalue<enum %s>: Unknown enum string '%s'", enumName, s.c_str());
        return 0;
    }

    bool ReadString1(lua_State* L, std::string& out) {   // FUN_100b8970
        return CoA::Script::ReadString(L, out);
    }

    bool ReadOptId(lua_State* L, bool& has, std::string& id) {   // FUN_10239400 / FUN_10252c30
        if (!ValidateInput(L, { Present(L, 1) ? LUA_TSTRING : LUA_TNIL })) {
            return false;
        }

        has = OptStringAt(L, 1, id);
        return true;
    }

    bool ReadTwoStrings(lua_State* L, std::string& a, std::string& b) {   // FUN_100d0770 (via FUN_10239670)
        if (!ValidateInput(L, { LUA_TSTRING, LUA_TSTRING })) {
            return false;
        }

        a = StringAt(L, 1);
        b = StringAt(L, 2);
        return true;
    }

    bool ReadStringNumber(lua_State* L, std::string& s, uint32_t& n) {   // FUN_100e96d0
        if (!ValidateInput(L, { LUA_TSTRING, LUA_TNUMBER })) {
            return false;
        }

        s = StringAt(L, 1);
        n = static_cast<uint32_t>(ToInt(CheckNumber(L, 2)));
        return true;
    }

    bool ReadMarkSeen(lua_State* L, bool& has, std::string& id, uint32_t& messageId) {   // FUN_10239810
        if (!ValidateInput(L, { Present(L, 1) ? LUA_TSTRING : LUA_TNIL, LUA_TNUMBER })) {
            return false;
        }

        has = OptStringAt(L, 1, id);
        messageId = static_cast<uint32_t>(ToInt(CheckNumber(L, 2)));
        return true;
    }

    bool ReadSendMessage(lua_State* L, bool& has, std::string& id, std::string& text, bool& gmOnly) {   // FUN_1023a2e0
        if (!ValidateInput(L, { Present(L, 1) ? LUA_TSTRING : LUA_TNIL, LUA_TSTRING, LUA_TBOOLEAN })) {
            return false;
        }

        has = OptStringAt(L, 1, id);
        text = StringAt(L, 2);
        gmOnly = lua_toboolean(L, 3) != 0;
        return true;
    }

    bool ReadIdEnum(lua_State* L, std::string& id, uint8_t& v, const char* const* names, uint32_t count, const char* enumName) {   // FUN_10239c70 / FUN_10239e10
        if (!ValidateInput(L, { LUA_TSTRING, LUA_TSTRING })) {
            return false;
        }

        id = StringAt(L, 1);
        v = EnumArg(L, 2, names, count, enumName);
        return true;
    }

    bool ReadCreate(lua_State* L, uint8_t& priority, uint8_t& category, std::string& a, std::string& b, std::string& c) {   // FUN_10252d80
        if (!ValidateInput(L, { LUA_TSTRING, LUA_TSTRING, LUA_TSTRING, LUA_TSTRING, LUA_TSTRING })) {
            return false;
        }

        priority = EnumArg(L, 1, kPriority, 4, "GMTicketPriority");
        category = EnumArg(L, 2, kCategory, 10, "GMTicketCategory");
        a = StringAt(L, 3);
        b = StringAt(L, 4);
        c = StringAt(L, 5);
        return true;
    }

    int PushTrueNil(lua_State* L) {
        PushBool(L, true);
        lua_pushnil(L);
        return 2;
    }

    void PutOptId(CoA::Packet& p, bool has, const std::string& id) {   // FUN_100d0c90: written only when present
        if (has) {
            p.Str(id.c_str());
        }
    }

    // ---- C_GMTicket ---------------------------------------------------------------------------------------------
    int GetNumTickets(lua_State* L) {
        if (!Gate()) {
            return 0;
        }

        PushNum(L, static_cast<double>(s_view.Size()));
        return 1;
    }

    int GetTicketAtIndex(lua_State* L) {
        if (!Gate() || !ValidateInput(L, { LUA_TNUMBER })) {
            return 0;
        }

        const int32_t index = ToInt(CheckNumber(L, 1));

        if (index == 0) {
            return 0;
        }

        const uint32_t i = static_cast<uint32_t>(index) - 1;

        if (i < s_view.Size()) {
            PushTicket(L, s_view.Filtered()[i]);
        } else {
            lua_pushnil(L);
        }

        return 1;
    }

    int GetTicketByID(lua_State* L) {
        std::string id;

        if (!Gate() || !ReadString1(L, id)) {
            return 0;
        }

        if (const Ticket* t = FindTicket(id)) {
            PushTicket(L, *t);
        } else {
            lua_pushnil(L);
        }

        return 1;
    }

    int GetTicketMessage(lua_State* L) {
        std::string id;
        uint32_t messageId;

        if (!Gate() || !ReadStringNumber(L, id, messageId)) {
            return 0;
        }

        if (const Ticket* t = FindTicket(id)) {
            for (const Message& m : t->messages) {
                if (m.messageId == messageId) {
                    PushMessage(L, m);
                    return 1;
                }
            }
        }

        lua_pushnil(L);
        return 1;
    }

    int GetTicketMessages(lua_State* L) {
        std::string id;

        if (!Gate() || !ReadString1(L, id)) {
            return 0;
        }

        if (const Ticket* t = FindTicket(id)) {
            PushMessages(L, t->messages);
        } else {
            lua_pushnil(L);
        }

        return 1;
    }

    int GetAutomatedMessage(lua_State* L) {
        std::string id;

        if (!Gate() || !ReadString1(L, id)) {
            return 0;
        }

        const Ticket* t = FindTicket(id);

        if (t && t->hasAISuggestion) {
            PushStr(L, t->aiSuggestion.c_str());
        } else {
            lua_pushnil(L);
        }

        return 1;
    }

    int GetNumSuggestions(lua_State* L) {
        std::string id;

        if (!Gate() || !ReadString1(L, id)) {
            return 0;
        }

        const Ticket* t = FindTicket(id);

        if (!t) {
            return 0;
        }

        PushNum(L, static_cast<double>(t->sql.size() + t->commands.size() + t->bugReports.size() + (t->hasSummary ? 1 : 0)));
        return 1;
    }

    void PushSuggestion(lua_State* L, const char* type, const std::string& command, const std::string& reason) {   // FUN_10240ac0
        NewTable(L, 0, 3);
        SetStr(L, "Type", type);
        SetStr(L, "Command", command);
        SetStr(L, "Reason", reason);
    }

    // Index 1 is always the summary; then the commands, the SQL, the bug tracker reports.
    int GetSuggestionAtIndex(lua_State* L) {
        std::string id;
        uint32_t index;

        if (!Gate() || !ReadStringNumber(L, id, index) || index == 0) {
            return 0;
        }

        const Ticket* t = FindTicket(id);

        if (!t) {
            return 0;
        }

        if (index == 1) {
            PushSuggestion(L, "Summary", t->hasSummary ? t->summary : std::string("<Empty Summary>"), "");
            return 1;
        }

        uint32_t i = index - 2;

        if (i < t->commands.size()) {
            PushSuggestion(L, "Command", t->commands[i].text, t->commands[i].reason);
            return 1;
        }

        i -= static_cast<uint32_t>(t->commands.size());

        if (i < t->sql.size()) {
            PushSuggestion(L, "SQL", t->sql[i].text, t->sql[i].reason);
            return 1;
        }

        i -= static_cast<uint32_t>(t->sql.size());

        if (i >= t->bugReports.size()) {
            lua_pushnil(L);
            return 1;
        }

        PushSuggestion(L, "BugTrackerReport", "https://ascension.gg/bugtracker/view/" + std::to_string(t->bugReports[i].id),
            t->bugReports[i].reason);
        return 1;
    }

    bool ReadBy(const std::vector<Message>& messages, uint32_t messageId, const std::string& name) {
        for (const Message& m : messages) {
            if (m.messageId == messageId) {
                for (const ReadReceipt& r : m.receipts) {
                    if (r.readBy == name) {
                        return true;
                    }
                }
            }
        }

        return false;
    }

    int IsResponseSeen(lua_State* L) {
        std::string id;
        uint32_t messageId;

        if (!Gate() || !ReadStringNumber(L, id, messageId)) {
            return 0;
        }

        const Ticket* t = FindTicket(id);
        const char* me = t ? PlayerName() : nullptr;

        if (!me) {
            return 0;
        }

        PushBool(L, ReadBy(t->messages, messageId, me));
        return 1;
    }

    // No active player reads as assigned.
    int IsTicketAssignedToMe(lua_State* L) {
        std::string id;

        if (!Gate() || !ReadString1(L, id)) {
            return 0;
        }

        const Ticket* t = FindTicket(id);

        if (!t) {
            return 0;
        }

        bool mine = true;

        if (const char* me = PlayerName()) {
            mine = std::find(t->assignedTo.begin(), t->assignedTo.end(), std::string(me)) != t->assignedTo.end();
        }

        PushBool(L, mine);
        return 1;
    }

    // SetTicketFilter(search, {FILTER_* = bool}, {SORT_*, ...}) (FUN_10239fb0, FUN_100bed80, FUN_1011f710) -> nothing.
    int SetTicketFilter(lua_State* L) {
        if (!Gate() || !ValidateInput(L, { LUA_TSTRING, LUA_TTABLE, LUA_TTABLE })) {
            return 0;
        }

        const std::string search = StringAt(L, 1);
        std::vector<uint32_t> filters;
        std::vector<uint32_t> sorts;
        lua_pushnil(L);

        while (lua_next(L, 2)) {
            const std::string key = StringAt(L, -2);

            if (lua_toboolean(L, -1)) {
                for (uint32_t i = 0; i < 8; ++i) {
                    if (key == kFilters[i]) {
                        filters.push_back(i);
                        break;
                    }
                }
            }

            lua_settop(L, -2);
        }

        lua_pushnil(L);

        while (lua_next(L, 3)) {
            const std::string name = StringAt(L, -1);

            for (uint32_t i = 0; i < 5; ++i) {
                if (name == kSorts[i]) {
                    sorts.push_back(i);
                    break;
                }
            }

            lua_settop(L, -2);
        }

        s_view.ApplyFilter(search, filters, sorts);   // FUN_102499c0
        return 0;
    }

    int CanAssignGMTicket(lua_State* L) {   // also CanSetTicketTitle (one function, 0x1024D6D0)
        std::string a;
        std::string b;

        if (!Gate() || !ReadTwoStrings(L, a, b)) {
            return 0;
        }

        return PushTrueNil(L);
    }

    int SendTwoStrings(lua_State* L, uint32_t opcode) {
        std::string a;
        std::string b;

        if (!Gate() || !ReadTwoStrings(L, a, b)) {
            return 0;
        }

        CoA::Packet(opcode).Str(a.c_str()).Str(b.c_str()).Send();
        PushBool(L, true);
        return 1;
    }

    int AssignGMTicket(lua_State* L) {
        return SendTwoStrings(L, 0x717);
    }

    int DeclineAutomatedMessage(lua_State* L) {
        return SendTwoStrings(L, 0x714);
    }

    int SetTicketTitle(lua_State* L) {
        return SendTwoStrings(L, 0x710);
    }

    int ApproveAutomatedMessage(lua_State* L) {
        std::string id;

        if (!Gate() || !ReadString1(L, id)) {
            return 0;
        }

        CoA::Packet(0x712).Str(id.c_str()).Send();
        PushBool(L, true);
        return 1;
    }

    // The player-side twins replace the argument with the player's own ticket id whenever there is a ticket.
    void UseOwnTicket(bool own, bool& has, std::string& id) {
        if (own && s_hasCurrent) {
            id = s_current.ticketId;
            has = true;
        }
    }

    int CanClose(lua_State* L, bool own) {
        bool has;
        std::string id;

        if (!Gate() || !ReadOptId(L, has, id)) {
            return 0;
        }

        UseOwnTicket(own, has, id);
        return PushTrueNil(L);
    }

    int Close(lua_State* L, bool own) {
        bool has;
        std::string id;

        if (!Gate() || !ReadOptId(L, has, id)) {
            return 0;
        }

        UseOwnTicket(own, has, id);
        CoA::Packet p(0x705);
        PutOptId(p, has, id);
        p.Send();
        PushBool(L, true);
        return 1;
    }

    int CanMarkSeen(lua_State* L, bool own) {
        bool has;
        std::string id;
        uint32_t messageId;

        if (!Gate() || !ReadMarkSeen(L, has, id, messageId)) {
            return 0;
        }

        UseOwnTicket(own, has, id);
        return PushTrueNil(L);
    }

    int MarkSeen(lua_State* L, bool own) {
        bool has;
        std::string id;
        uint32_t messageId;

        if (!Gate() || !ReadMarkSeen(L, has, id, messageId)) {
            return 0;
        }

        UseOwnTicket(own, has, id);
        CoA::Packet p(0x71D);
        PutOptId(p, has, id);
        p.U32(messageId).Send();
        PushBool(L, true);
        return 1;
    }

    int CanSendMessage(lua_State* L, bool own) {
        bool has;
        bool gmOnly;
        std::string id;
        std::string text;

        if (!Gate() || !ReadSendMessage(L, has, id, text, gmOnly)) {
            return 0;
        }

        UseOwnTicket(own, has, id);
        return PushTrueNil(L);
    }

    int SendMessage(lua_State* L, bool own) {
        bool has;
        bool gmOnly;
        std::string id;
        std::string text;

        if (!Gate() || !ReadSendMessage(L, has, id, text, gmOnly)) {
            return 0;
        }

        UseOwnTicket(own, has, id);
        CoA::Packet p(0x707);
        PutOptId(p, has, id);
        p.Str(text.c_str()).U8(gmOnly ? 1 : 0).Send();
        PushBool(L, true);
        return 1;
    }

    int CanCloseTicket(lua_State* L) {
        return CanClose(L, false);
    }

    int CloseTicket(lua_State* L) {
        return Close(L, false);
    }

    int CanMarkResponseSeen(lua_State* L) {
        return CanMarkSeen(L, false);
    }

    int MarkResponseSeen(lua_State* L) {
        return MarkSeen(L, false);
    }

    int CanSendTicketMessage(lua_State* L) {
        return CanSendMessage(L, false);
    }

    int SendTicketMessage(lua_State* L) {
        return SendMessage(L, false);
    }

    int SetEnumField(lua_State* L, uint32_t opcode, bool send, const char* const* names, uint32_t count, const char* enumName) {
        std::string id;
        uint8_t v;

        if (!Gate() || !ReadIdEnum(L, id, v, names, count, enumName)) {
            return 0;
        }

        if (!send) {
            return PushTrueNil(L);
        }

        CoA::Packet(opcode).Str(id.c_str()).U8(v).Send();
        PushBool(L, true);
        return 1;
    }

    int CanSetTicketPriority(lua_State* L) {
        return SetEnumField(L, 0x70E, false, kPriority, 4, "GMTicketPriority");
    }

    int SetTicketPriority(lua_State* L) {
        return SetEnumField(L, 0x70E, true, kPriority, 4, "GMTicketPriority");
    }

    int CanSetTicketStatus(lua_State* L) {
        return SetEnumField(L, 0x70C, false, kStatus, 7, "GMTicketStatus");
    }

    int SetTicketStatus(lua_State* L) {
        return SetEnumField(L, 0x70C, true, kStatus, 7, "GMTicketStatus");
    }

    // ---- C_PlayerTicket -----------------------------------------------------------------------------------------
    int GetCurrentTicket(lua_State* L) {
        if (!Gate() || !s_hasCurrent) {
            return 0;
        }

        PushTicket(L, s_current);
        return 1;
    }

    int CanCreateTicket(lua_State* L) {
        uint8_t priority;
        uint8_t category;
        std::string a;
        std::string b;
        std::string c;

        if (!Gate() || !ReadCreate(L, priority, category, a, b, c)) {
            return 0;
        }

        return PushTrueNil(L);
    }

    int CreateTicket(lua_State* L) {
        uint8_t priority;
        uint8_t category;
        std::string a;
        std::string b;
        std::string c;

        if (!Gate() || !ReadCreate(L, priority, category, a, b, c)) {
            return 0;
        }

        CoA::Packet(0x703).U8(priority).U8(category).Str(a.c_str()).Str(b.c_str()).Str(c.c_str()).Send();
        PushBool(L, true);
        return 1;
    }

    int CanReopenTicket(lua_State* L) {
        return CanClose(L, true);
    }

    int ReopenTicket(lua_State* L) {
        bool has;
        std::string id;

        if (!Gate() || !ReadOptId(L, has, id)) {
            return 0;
        }

        UseOwnTicket(true, has, id);
        CoA::Packet p(0x719);
        PutOptId(p, has, id);
        p.Send();
        PushBool(L, true);
        return 1;
    }

    int PlayerCanCloseTicket(lua_State* L) {   // 0x10255330
        return CanClose(L, true);
    }

    int PlayerCloseTicket(lua_State* L) {   // 0x10255840
        return Close(L, true);
    }

    int PlayerCanSendTicketMessage(lua_State* L) {   // 0x10255750
        return CanSendMessage(L, true);
    }

    int PlayerSendTicketMessage(lua_State* L) {   // 0x10256640
        return SendMessage(L, true);
    }

    int PlayerCanMarkResponseSeen(lua_State* L) {   // 0x10255530
        return CanMarkSeen(L, true);
    }

    int PlayerMarkResponseSeen(lua_State* L) {   // 0x10256200
        return MarkSeen(L, true);
    }

    // 0x10255e20: the player's ticket messages without the GM-only ones; nothing without a ticket.
    int PlayerGetTicketMessages(lua_State* L) {
        if (!Gate() || !s_hasCurrent) {
            return 0;
        }

        std::vector<Message> v;

        for (const Message& m : s_current.messages) {
            if (!m.gmOnly) {
                v.push_back(m);
            }
        }

        PushMessages(L, v);
        return 1;
    }

    // 0x10255d70 inverts its reader check: a readable message id returns nothing, and an unreadable one searches with
    // whatever the uninitialised local holds (the caller's ecx) -- 0 here.
    int PlayerGetTicketMessage(lua_State* L) {
        if (!Gate()) {
            return 0;
        }

        uint32_t messageId = 0;

        if (CoA::Script::ReadNumber(L, messageId)) {
            return 0;
        }

        messageId = 0;

        if (!s_hasCurrent) {
            return 0;
        }

        for (const Message& m : s_current.messages) {
            if (m.messageId == messageId && !m.gmOnly) {
                PushMessage(L, m);
                return 1;
            }
        }

        lua_pushnil(L);
        return 1;
    }

    // 0x10256030: has the player (by name) a read receipt on the message?
    int PlayerIsResponseSeen(lua_State* L) {
        uint32_t messageId;

        if (!Gate() || !CoA::Script::ReadNumber(L, messageId) || !s_hasCurrent) {
            return 0;
        }

        const char* me = PlayerName();

        if (!me) {
            return 0;
        }

        PushBool(L, ReadBy(s_current.messages, messageId, me));
        return 1;
    }

    // ---- C_GM ---------------------------------------------------------------------------------------------------
    // RequestPlayerInfo(name) (0x1024ffe0): CMSG 0x71B with the name; the answer is SMSG 0x71C.
    int RequestPlayerInfo(lua_State* L) {
        std::string name;

        if (!Gate() || !ReadString1(L, name)) {
            return 0;
        }

        CoA::Packet(0x71B).Str(name.c_str()).Send();
        PushBool(L, true);
        return 1;
    }

    template <class T, class F>
    void SetList(lua_State* L, const char* k, const std::vector<T>& v, int fields, F write) {   // createtable(0, n), pushnumber(i) keys
        lua_pushstring(L, k);
        NewTable(L, 0, static_cast<int>(v.size()));

        for (uint32_t i = 0; i < v.size(); ++i) {
            Key(L, i + 1);
            NewTable(L, 0, fields);
            write(v[i]);
            lua_settable(L, -3);
        }

        lua_settable(L, -3);
    }

    void PushPlayerInfo(lua_State* L, const PlayerInfo& r) {   // FUN_10247d40
        NewTable(L, 0, 0x25);
        SetStr(L, "CharacterName", r.name);
        SetU32(L, "CharacterGuid", r.guid);
        SetU32(L, "CharacterLevel", r.level);
        SetU32(L, "RaceID", r.race);
        SetU32(L, "ClassID", r.classId);
        SetU32(L, "GameModeMask", r.gameModeMask);
        SetOptF32(L, "AverageItemLevel", r.hasItemLevel, r.itemLevel);
        SetOptU32(L, "Money", r.hasMoney, r.money);
        SetList(L, "Challenges", r.challenges, 2, [L](const std::pair<uint32_t, uint32_t>& c) {   // FUN_10240e30
            SetU32(L, "ChallengeID", c.first);
            SetU32(L, "ChallengeLevel", c.second);
        });
        SetOptU32(L, "MapID", r.hasMap, r.map);
        SetOptF32(L, "LocX", r.hasX, r.x);
        SetOptF32(L, "LocY", r.hasY, r.y);
        SetOptF32(L, "LocZ", r.hasZ, r.z);
        SetOptU32(L, "ZoneID", r.hasZone, r.zone);
        SetOptU32(L, "AreaID", r.hasArea, r.area);
        SetU32(L, "GuildID", r.guildId);
        SetOptStr(L, "GuildName", r.hasGuildName, r.guildName);
        SetOptU32(L, "GuildRankID", r.hasRank, r.rank);
        SetOptStr(L, "GuildRankName", r.hasRankName, r.rankName);
        SetU32(L, "AccountID", r.accountId);
        SetStr(L, "AccountName", r.accountName);
        SetEnum(L, "GMLevel", r.gmLevel, kSecurity, 8);
        SetOptStr(L, "DiscordName", r.hasDiscord, r.discord);
        SetOptStr(L, "HardwareID", r.hasHwid, r.hwid);
        SetOptStr(L, "HardwareIDv2", r.hasHwid2, r.hwid2);
        SetOptStr(L, "OnlineCharacterName", r.hasOnlineName, r.onlineName);
        SetList(L, "TicketHistory", r.tickets, 5, [L](const HistTicket& t) {   // FUN_10241290 / FUN_10243010
            SetStr(L, "TicketID", t.ticketId);
            SetStr(L, "Title", t.title);
            SetU32(L, "CreationDate", t.creationDate);
            SetOptU32(L, "CloseDate", t.hasCloseDate, t.closeDate);
            SetList(L, "Messages", t.messages, 5, [L](const HistMessage& m) {   // FUN_102405a0 / FUN_10243340
                SetBool(L, "IsFromGM", m.isFromGM);
                SetBool(L, "GMOnly", m.gmOnly);
                SetStr(L, "Sender", m.sender);
                SetOptStr(L, "Message", m.hasMessage, m.message);
                SetU32(L, "Timestamp", m.timestamp);
            });
        });
        SetU32(L, "NumTickets", r.numTickets);
        SetList(L, "MuteHistory", r.mutes, 6, [L](const Mute& m) {   // FUN_10240f60 / FUN_102425c0
            SetU32(L, "MuteDate", m.date);
            SetU32(L, "MuteTime", m.time);
            SetStr(L, "MuteReason", m.reason);
            SetStr(L, "MutedBy", m.by);
            SetU32(L, "MuteMode", m.mode);
            SetStr(L, "RealmName", m.realm);
        });
        SetU32(L, "NumMutes", r.numMutes);
        SetList(L, "BanHistory", r.bans, 6, [L](const Ban& b) {   // FUN_10240cd0 / FUN_10242020
            SetU32(L, "BanDate", b.date);
            SetU32(L, "RemainingTime", b.remaining);
            SetBool(L, "IsActive", b.active);
            SetU32(L, "UnbanDate", b.unbanDate);
            SetStr(L, "BanReason", b.reason);
            SetStr(L, "BannedBy", b.by);
        });
        SetU32(L, "NumBans", r.numBans);
        SetList(L, "WarningHistory", r.warnings, 4, [L](const Warning& w) {   // FUN_10241100 / FUN_10242970
            SetStr(L, "RealmName", w.realm);
            SetStr(L, "CharacterName", w.character);
            SetStr(L, "Reason", w.reason);
            SetU32(L, "Timestamp", w.timestamp);
        });
        SetU32(L, "NumWarnings", r.numWarnings);
        SetList(L, "GMNotes", r.notes, 4, [L](const Note& n) {   // FUN_1023fce0
            SetU32(L, "NoteID", n.id);
            SetStr(L, "GM", n.gm);
            SetStr(L, "Message", n.message);
            SetU32(L, "Date", n.date);
        });
        SetU32(L, "NumNotes", r.numNotes);
        SetOptU32(L, "Latency", r.hasLatency, r.latency);
    }

    // GetPlayerInfo(name) (0x1024fe40): the FIRST stored record whose name matches exactly (FUN_10087260), or nil.
    int GetPlayerInfo(lua_State* L) {
        std::string name;

        if (!Gate() || !ReadString1(L, name)) {
            return 0;
        }

        for (const PlayerInfo& r : s_playerInfo) {
            if (r.name == name) {
                PushPlayerInfo(L, r);
                return 1;
            }
        }

        lua_pushnil(L);
        return 1;
    }

    // ---- packet handlers ----------------------------------------------------------------------------------------
    // The managers' diagnostics go through the timestamped channel writer (FUN_102936d0): 1 Info for changes, 2 Debug
    // for progress and fired events, 4 Error for an unknown ticket.
    void Diag(uint32_t channel, const char* fmt, ...) {
        va_list ap;
        va_start(ap, fmt);
        va_list copy;
        va_copy(copy, ap);
        const int n = vsnprintf(nullptr, 0, fmt, copy);
        va_end(copy);
        std::string line(n > 0 ? static_cast<size_t>(n) : 0, '\0');

        if (n > 0) {
            vsnprintf(&line[0], static_cast<size_t>(n) + 1, fmt, ap);
        }

        va_end(ap);
        CoA::Logger::WriteStamped(channel, line);
    }

    // SMSG 0x709 GM_TICKET_LIST (FUN_1024acc0): u8 clear, u8 reset, u32 n x ticket (appended).
    int32_t OnTicketList(void*, uint32_t, uint32_t, CDataStore* msg) {
        if (CoA::Read::U8(msg)) {
            s_tickets.clear();
        }

        const bool reset = CoA::Read::U8(msg) != 0;

        for (uint32_t n = CoA::Read::U32(msg); n && More(msg); --n) {
            s_tickets.push_back(ReadTicket(msg));
        }

        if (reset) {
            ResetView();
        }

        return 1;
    }

    // SMSG 0x70A GM_TICKET_CREATED (FUN_1024aaa0).
    int32_t OnTicketCreated(void*, uint32_t, uint32_t, CDataStore* msg) {
        s_tickets.push_back(ReadTicket(msg));
        CoA::Events::Signal("GM_TICKET_NEW_TICKET", "%s", s_tickets.back().ticketId.c_str());
        CoA::Events::Signal("GM_TICKETS_UPDATED", nullptr);
        ResetView();
        CoA::Events::Signal("GM_TICKET_FILTER_RESET", nullptr);
        return 1;
    }

    // SMSG 0x70B GM_TICKET_UPDATE (FUN_1024af30): the ticket replacing the one with its id (unknown ids are dropped).
    int32_t OnTicketUpdate(void*, uint32_t, uint32_t, CDataStore* msg) {
        static const char kTag[] = "GMTicketMgr::HandleGMTicketUpdateOpcode: ";
        Diag(2, "%sBegin processing ticket update", kTag);
        const Ticket fresh = ReadTicket(msg);
        const std::string idString = fresh.ticketId;
        const char* id = idString.c_str();
        Diag(2, "%sProcessing ticket ID: %s", kTag, id);
        Ticket* slot = FindTicket(fresh.ticketId);

        if (!slot) {
            Diag(4, "%sTicket %s not found", kTag, id);
            return 1;
        }

        const Ticket old = *slot;
        *slot = fresh;
        Diag(2, "%sFound and updated existing ticket: %s", kTag, id);
        bool statusChanged = false;

        if (old.status != fresh.status) {
            Diag(1, "%sTicket %s status changed from %d to %d", kTag, id, old.status, fresh.status);

            if (old.Closed() != fresh.Closed()) {
                Diag(1, "%sTicket %s closure state changed: %s -> %s", kTag, id, old.Closed() ? "Closed" : "Open",
                    fresh.Closed() ? "Closed" : "Open");
                CoA::Events::Signal("GM_TICKETS_UPDATED", nullptr);
                Diag(2, "%sFired GM_TICKETS_UPDATED event", kTag);
            }

            statusChanged = true;
        }

        if (old.assignedTo.size() != fresh.assignedTo.size()) {
            Diag(1, "%sTicket %s assignment count changed: %u -> %u", kTag, id, static_cast<uint32_t>(old.assignedTo.size()),
                static_cast<uint32_t>(fresh.assignedTo.size()));

            if (!fresh.assignedTo.empty()) {
                std::string names;

                for (const std::string& n : fresh.assignedTo) {
                    if (!names.empty()) {
                        names += ", ";
                    }

                    names += n;
                }

                Diag(1, "%sTicket %s assigned to: %s", kTag, id, names.c_str());
            }
        }

        if (old.assignedTo.size() != fresh.assignedTo.size() || statusChanged) {
            CoA::Events::Signal("GM_TICKET_STATUS_CHANGED", "%s", id);
            Diag(2, "%sFired GM_TICKET_STATUS_CHANGED event for ticket %s", kTag, id);
        }

        if (old.messages.size() != fresh.messages.size()) {
            Diag(1, "%sTicket %s message count changed: %u -> %u", kTag, id, static_cast<uint32_t>(old.messages.size()),
                static_cast<uint32_t>(fresh.messages.size()));

            if (old.messages.size() < fresh.messages.size()) {
                Diag(1, "%sNew message (ID: %u) from: %s", kTag, fresh.messages.back().messageId, fresh.messages.back().sender.c_str());
            }

            CoA::Events::Signal("GM_TICKET_MESSAGE", "%s", id);
            Diag(2, "%sFired GM_TICKET_MESSAGE event for ticket %s", kTag, id);
        }

        if (old.hasAISuggestion != fresh.hasAISuggestion || (old.hasAISuggestion && old.aiSuggestion != fresh.aiSuggestion)) {
            Diag(1, "%sTicket %s AI suggestion changed", kTag, id);
            CoA::Events::Signal("GM_TICKET_SUGGESTION_UPDATE", "%s", id);
            Diag(2, "%sFired GM_TICKET_SUGGESTION_UPDATE event for ticket %s", kTag, id);
        }

        Diag(2, "%sChecking read receipts for %u messages in ticket %s", kTag, static_cast<uint32_t>(fresh.messages.size()), id);

        for (size_t i = 0; i < fresh.messages.size() && i < old.messages.size(); ++i) {
            const Message& m = fresh.messages[i];
            const size_t had = old.messages[i].receipts.size();

            if (m.receipts.size() <= had) {
                continue;
            }

            Diag(1, "%sMessage %u in ticket %s has new read receipts: %u -> %u", kTag, m.messageId, id, static_cast<uint32_t>(had),
                static_cast<uint32_t>(m.receipts.size()));

            for (size_t j = had; j < m.receipts.size(); ++j) {
                Diag(1, "%sMessage %u in ticket %s read by: %s", kTag, m.messageId, id, m.receipts[j].readBy.c_str());
            }

            for (size_t j = had; j < m.receipts.size(); ++j) {
                CoA::Events::Signal("GM_TICKET_MESSAGE_READ", "%s%u", id, m.messageId);
                Diag(2, "%sFired GM_TICKET_MESSAGE_READ event for message %u in ticket %s", kTag, m.messageId, id);
            }
        }

        ResetView();
        CoA::Events::Signal("GM_TICKET_FILTER_RESET", nullptr);
        Diag(2, "%sFired GM_TICKET_FILTER_RESET event", kTag);
        Diag(2, "%sFinished processing ticket update", kTag);
        return 1;
    }

    // SMSG 0x71C REQUEST_PLAYER_INFO_RESULT (FUN_1024c790): str name, u8 found [player record].
    int32_t OnPlayerInfoResult(void*, uint32_t, uint32_t, CDataStore* msg) {
        const std::string name = CoA::Read::Str(msg);

        if (!CoA::Read::U8(msg)) {
            CoA::Events::Signal("REQUEST_PLAYER_INFO_RESULT", "%s", name.c_str());
            return 1;
        }

        s_playerInfo.push_back(ReadPlayerInfo(msg));   // FUN_1023a4a0
        CoA::Events::Signal("REQUEST_PLAYER_INFO_RESULT", "%s%s", s_playerInfo.back().name.c_str(), name.c_str());
        return 1;
    }

    // SMSG 0x701 (0x10253dd0): the player's ticket.
    int32_t OnPlayerTicketInfo(void*, uint32_t, uint32_t, CDataStore* msg) {
        s_current = ReadTicket(msg);
        s_hasCurrent = true;
        CoA::Events::Signal("PLAYER_TICKET_UPDATE", nullptr);
        return 1;
    }

    // SMSG 0x702 (FUN_10253ec0): the player's ticket, diffed against the previous one.
    int32_t OnPlayerTicketUpdate(void*, uint32_t, uint32_t, CDataStore* msg) {
        static const char kTag[] = "PlayerTicketMgr::HandlePlayerGMTicketUpdateOpcode: ";
        Diag(2, "%sBegin processing player ticket update", kTag);
        const bool had = s_hasCurrent;
        const Ticket old = s_current;
        s_current = ReadTicket(msg);
        s_hasCurrent = true;
        // The original's "No ticket in update" branch cannot run: the read always leaves a ticket.
        Diag(2, "%sProcessing player ticket ID: %s", kTag, s_current.ticketId.c_str());
        bool fire = false;

        if (had) {
            Diag(2, "%sBoth old and new tickets exist, checking changes", kTag);
            // A hidden last message in the old ticket suppresses the message-count checks.
            const bool hidden = !old.messages.empty() && !old.messages.back().hasMessage;

            if (hidden) {
                Diag(2, "%sLast message in old ticket was hidden", kTag);
            }

            const bool statusChanged = old.status != s_current.status;

            if (statusChanged) {
                Diag(1, "%sTicket status changed from %d to %d", kTag, old.status, s_current.status);
            }

            const bool countChanged = !hidden && old.messages.size() != s_current.messages.size();

            if (countChanged) {
                Diag(1, "%sMessage count changed: %u -> %u (last hidden: no)", kTag, static_cast<uint32_t>(old.messages.size()),
                    static_cast<uint32_t>(s_current.messages.size()));
            }

            fire = statusChanged || countChanged;

            if (fire) {
                Diag(1, "%sWill send update event due to: status changed=%s, messages changed=%s", kTag, statusChanged ? "true" : "false",
                    countChanged ? "true" : "false");
            }

            if (!hidden && old.messages.size() < s_current.messages.size()) {
                const Message& last = s_current.messages.back();
                Diag(1, "%sNew message added (ID: %u) from: %s", kTag, last.messageId, last.sender.c_str());
                CoA::Events::Signal("PLAYER_TICKET_NEW_MESSAGE", "%u", last.messageId);
                Diag(2, "%sFired PLAYER_TICKET_NEW_MESSAGE event for message ID %u", kTag, last.messageId);
            }

            if (s_current.closedByCreator) {
                Diag(1, "%sClearing ticket %s because it's closed by creator", kTag, s_current.ticketId.c_str());
                s_hasCurrent = false;   // the player closed it: forget it
                fire = true;
            }
        } else if (!s_current.closedByCreator) {
            Diag(1, "%sGM reopened a ticket previously closed by player", kTag);
            fire = true;   // a GM reopened a ticket the player had closed
        }

        if (fire) {
            CoA::Events::Signal("PLAYER_TICKET_UPDATE", nullptr);
            Diag(2, "%sFired PLAYER_TICKET_UPDATE event", kTag);
        } else {
            Diag(2, "%sNo PLAYER_TICKET_UPDATE event needed", kTag);
        }

        Diag(2, "%sFinished processing player ticket update", kTag);
        return 1;
    }

    // The result handlers of both managers: a C string, signalled as the event's "%s". 0x706 / 0x71E / 0x708 pick the
    // event by the account's GM level (AccountInfo +8, FUN_1008c870): 0x706 names the PLAYER result for GMs and 0x71E
    // the GM one, as in the original.
    void SignalResult(const char* event, CDataStore* msg) {
        const std::string s = CoA::Read::Str(msg);
        CoA::Events::Signal(event, "%s", s.c_str());
    }

    int32_t On704(void*, uint32_t, uint32_t, CDataStore* msg) {   // FUN_10253a30
        SignalResult("CREATE_PLAYER_TICKET_RESULT", msg);
        return 1;
    }

    int32_t On70D(void*, uint32_t, uint32_t, CDataStore* msg) {   // FUN_1024cca0
        SignalResult("UPDATE_GM_TICKET_STATUS_RESULT", msg);
        return 1;
    }

    int32_t On70F(void*, uint32_t, uint32_t, CDataStore* msg) {   // FUN_1024cb60
        SignalResult("UPDATE_GM_TICKET_PRIORITY_RESULT", msg);
        return 1;
    }

    int32_t On711(void*, uint32_t, uint32_t, CDataStore* msg) {   // FUN_1024ce00
        SignalResult("UPDATE_GM_TICKET_TITLE_RESULT", msg);
        return 1;
    }

    int32_t On713(void*, uint32_t, uint32_t, CDataStore* msg) {   // FUN_1024a690
        SignalResult("APPROVE_GM_TICKET_SUGGESTION_RESULT", msg);
        return 1;
    }

    int32_t On715(void*, uint32_t, uint32_t, CDataStore* msg) {   // FUN_1024a940
        SignalResult("DECLINE_GM_TICKET_SUGGESTION_RESULT", msg);
        return 1;
    }

    int32_t On718(void*, uint32_t, uint32_t, CDataStore* msg) {   // FUN_1024a7f0
        SignalResult("ASSIGN_GM_TICKET_RESULT", msg);
        return 1;
    }

    int32_t On71A(void*, uint32_t, uint32_t, CDataStore* msg) {   // FUN_10254c90
        SignalResult("REOPEN_PLAYER_TICKET_RESULT", msg);
        return 1;
    }

    int32_t On706(void*, uint32_t, uint32_t, CDataStore* msg) {   // FUN_10253800
        SignalResult(CoA::Account::GmLevel() >= 1 ? "CLOSE_PLAYER_TICKET_RESULT" : "CLOSE_GM_TICKET_RESULT", msg);
        return 1;
    }

    int32_t On71E(void*, uint32_t, uint32_t, CDataStore* msg) {   // FUN_10253b90
        SignalResult(CoA::Account::GmLevel() >= 1 ? "MARK_GM_TICKET_MESSAGE_AS_READ_RESULT" : "MARK_PLAYER_TICKET_MESSAGE_AS_READ_RESULT", msg);
        return 1;
    }

    // 0x708 (FUN_10254df0): result, u8 has-message [message]. A GM account gets SEND_PLAYER_TICKET_MESSAGE_RESULT("%s%u",
    // result, message id); otherwise SEND_GM_TICKET_MESSAGE_RESULT("%s%s%u", result, ticket id, message id) -- "" / 0
    // without a message.
    int32_t On708(void*, uint32_t, uint32_t, CDataStore* msg) {
        const std::string result = CoA::Read::Str(msg);
        Message m;
        const bool has = CoA::Read::U8(msg) != 0;

        if (has) {
            m = ReadMessage(msg);
        }

        if (CoA::Account::GmLevel() >= 1) {
            CoA::Events::Signal("SEND_PLAYER_TICKET_MESSAGE_RESULT", "%s%u", result.c_str(), has ? m.messageId : 0u);
        } else {
            CoA::Events::Signal("SEND_GM_TICKET_MESSAGE_RESULT", "%s%s%u", result.c_str(), has ? m.ticketId.c_str() : "", has ? m.messageId : 0u);
        }

        return 1;
    }

    // FUN_10249940 (every glue screen): the ticket list emptied, the view reset and the player records dropped.
    void ResetTickets() {
        s_tickets.clear();
        ResetView();
        s_playerInfo.clear();
    }

    // FUN_102537d0 (every glue screen): the player's own ticket released when there is one.
    void ResetCurrent() {
        if (!s_hasCurrent) {
            return;
        }

        s_current = Ticket();
        s_hasCurrent = false;
    }

    const CoA::Binding s_bindings[] = {
        COA_NATIVE("C_GMTicket", "ApproveAutomatedMessage", ApproveAutomatedMessage),
        COA_NATIVE("C_GMTicket", "AssignGMTicket", AssignGMTicket),
        COA_NATIVE("C_GMTicket", "CanAssignGMTicket", CanAssignGMTicket),
        COA_NATIVE("C_GMTicket", "CanCloseTicket", CanCloseTicket),
        COA_NATIVE("C_GMTicket", "CanMarkResponseSeen", CanMarkResponseSeen),
        COA_NATIVE("C_GMTicket", "CanSendTicketMessage", CanSendTicketMessage),
        COA_NATIVE("C_GMTicket", "CanSetTicketPriority", CanSetTicketPriority),
        COA_NATIVE("C_GMTicket", "CanSetTicketStatus", CanSetTicketStatus),
        COA_NATIVE("C_GMTicket", "CanSetTicketTitle", CanAssignGMTicket),
        COA_NATIVE("C_GMTicket", "CloseTicket", CloseTicket),
        COA_NATIVE("C_GMTicket", "DeclineAutomatedMessage", DeclineAutomatedMessage),
        COA_NATIVE("C_GMTicket", "GetAutomatedMessage", GetAutomatedMessage),
        COA_NATIVE("C_GMTicket", "GetNumSuggestions", GetNumSuggestions),
        COA_NATIVE("C_GMTicket", "GetNumTickets", GetNumTickets),
        COA_NATIVE("C_GMTicket", "GetSuggestionAtIndex", GetSuggestionAtIndex),
        COA_NATIVE("C_GMTicket", "GetTicketAtIndex", GetTicketAtIndex),
        COA_NATIVE("C_GMTicket", "GetTicketByID", GetTicketByID),
        COA_NATIVE("C_GMTicket", "GetTicketMessage", GetTicketMessage),
        COA_NATIVE("C_GMTicket", "GetTicketMessages", GetTicketMessages),
        COA_NATIVE("C_GMTicket", "IsResponseSeen", IsResponseSeen),
        COA_NATIVE("C_GMTicket", "IsTicketAssignedToMe", IsTicketAssignedToMe),
        COA_NATIVE("C_GMTicket", "MarkResponseSeen", MarkResponseSeen),
        COA_NATIVE("C_GMTicket", "SendTicketMessage", SendTicketMessage),
        COA_NATIVE("C_GMTicket", "SetTicketFilter", SetTicketFilter),
        COA_NATIVE("C_GMTicket", "SetTicketPriority", SetTicketPriority),
        COA_NATIVE("C_GMTicket", "SetTicketStatus", SetTicketStatus),
        COA_NATIVE("C_GMTicket", "SetTicketTitle", SetTicketTitle),
        COA_NATIVE("C_PlayerTicket", "CanCreateTicket", CanCreateTicket),
        COA_NATIVE("C_PlayerTicket", "CanReopenTicket", CanReopenTicket),
        COA_NATIVE("C_PlayerTicket", "CreateTicket", CreateTicket),
        COA_NATIVE("C_PlayerTicket", "GetCurrentTicket", GetCurrentTicket),
        COA_NATIVE("C_PlayerTicket", "ReopenTicket", ReopenTicket),
        COA_NATIVE("C_PlayerTicket", "CanCloseTicket", PlayerCanCloseTicket),
        COA_NATIVE("C_PlayerTicket", "CloseTicket", PlayerCloseTicket),
        COA_NATIVE("C_PlayerTicket", "CanSendTicketMessage", PlayerCanSendTicketMessage),
        COA_NATIVE("C_PlayerTicket", "SendTicketMessage", PlayerSendTicketMessage),
        COA_NATIVE("C_PlayerTicket", "CanMarkResponseSeen", PlayerCanMarkResponseSeen),
        COA_NATIVE("C_PlayerTicket", "MarkResponseSeen", PlayerMarkResponseSeen),
        COA_NATIVE("C_PlayerTicket", "GetTicketMessages", PlayerGetTicketMessages),
        COA_NATIVE("C_PlayerTicket", "GetTicketMessage", PlayerGetTicketMessage),
        COA_NATIVE("C_PlayerTicket", "IsResponseSeen", PlayerIsResponseSeen),
        COA_NATIVE("C_GM", "GetPlayerInfo", GetPlayerInfo),
        COA_NATIVE("C_GM", "RequestPlayerInfo", RequestPlayerInfo),
    };
}

CoA::GMTicket::Deps& CoA::GMTicket::Dependencies() {
    return s_deps;
}

void CoA::GMTicket::Init() {   // FUN_1024cf40 / FUN_102551c0
    CoA::LuaReg::AddModule(s_bindings, sizeof(s_bindings) / sizeof(s_bindings[0]));
    CoA::Lifecycle::OnGlueScreen(ResetTickets);
    CoA::Lifecycle::OnGlueScreen(ResetCurrent);
    COA_PACKET(0x701, OnPlayerTicketInfo);
    COA_PACKET(0x702, OnPlayerTicketUpdate);
    COA_PACKET(0x704, On704);
    COA_PACKET(0x706, On706);
    COA_PACKET(0x708, On708);
    COA_PACKET(0x709, OnTicketList);
    COA_PACKET(0x70A, OnTicketCreated);
    COA_PACKET(0x70B, OnTicketUpdate);
    COA_PACKET(0x70D, On70D);
    COA_PACKET(0x70F, On70F);
    COA_PACKET(0x711, On711);
    COA_PACKET(0x713, On713);
    COA_PACKET(0x715, On715);
    COA_PACKET(0x718, On718);
    COA_PACKET(0x71A, On71A);
    COA_PACKET(0x71C, OnPlayerInfoResult);
    COA_PACKET(0x71E, On71E);
    COA_EVENT("GM_TICKET_NEW_TICKET");
    COA_EVENT("GM_TICKETS_UPDATED");
    COA_EVENT("GM_TICKET_FILTER_RESET");
    COA_EVENT("GM_TICKET_STATUS_CHANGED");
    COA_EVENT("GM_TICKET_MESSAGE");
    COA_EVENT("GM_TICKET_SUGGESTION_UPDATE");
    COA_EVENT("GM_TICKET_MESSAGE_READ");
    COA_EVENT("REQUEST_PLAYER_INFO_RESULT");
    COA_EVENT("PLAYER_TICKET_UPDATE");
    COA_EVENT("PLAYER_TICKET_NEW_MESSAGE");
    COA_EVENT("CREATE_PLAYER_TICKET_RESULT");
    COA_EVENT("UPDATE_GM_TICKET_STATUS_RESULT");
    COA_EVENT("UPDATE_GM_TICKET_PRIORITY_RESULT");
    COA_EVENT("UPDATE_GM_TICKET_TITLE_RESULT");
    COA_EVENT("APPROVE_GM_TICKET_SUGGESTION_RESULT");
    COA_EVENT("DECLINE_GM_TICKET_SUGGESTION_RESULT");
    COA_EVENT("ASSIGN_GM_TICKET_RESULT");
    COA_EVENT("REOPEN_PLAYER_TICKET_RESULT");
    COA_EVENT("CLOSE_PLAYER_TICKET_RESULT");
    COA_EVENT("CLOSE_GM_TICKET_RESULT");
    COA_EVENT("MARK_GM_TICKET_MESSAGE_AS_READ_RESULT");
    COA_EVENT("MARK_PLAYER_TICKET_MESSAGE_AS_READ_RESULT");
    COA_EVENT("SEND_PLAYER_TICKET_MESSAGE_RESULT");
    COA_EVENT("SEND_GM_TICKET_MESSAGE_RESULT");
}

void CoA::GMTicket::Reset() {
    ResetTickets();
    ResetCurrent();
    s_deps = Deps();
}
