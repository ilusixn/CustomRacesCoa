// C_GMTicket / C_PlayerTicket / C_GM (GMTicket.cpp): the 0x110-byte ticket record round trip, SMSG 0x709 append / clear /
// reset, the container's filters and sorts, the CanPerformAction(0x11) gate, every CMSG, the update diffs, the player
// ticket, the result events, the GM player records and the glue reset.
#include "CoA/Source/Modules/Account.h"
#include "CoA/Source/Modules/GMTicket.h"
#include "CoA/Source/Runtime/ClientState.h"
#include "CoA/Source/Runtime/Lifecycle.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Tests/Support.h"
#include "CoA/Tests/TestMain.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <string>
#include <utility>
#include <vector>

namespace {
    using V = std::vector<std::string>;
    using Packets = std::vector<std::vector<uint8_t>>;

    struct R {
        std::string tid;
        uint32_t mid;
        std::string by;
        uint32_t ts;
    };

    struct M {
        std::string tid = "T";
        uint32_t mid = 1;
        bool fromGM = false;
        std::string sender = "Player";
        bool hasText = true;
        std::string text = "hello";
        bool gmOnly = false;
        uint32_t ts = 100;
        std::vector<R> receipts;
    };

    struct T {
        std::string id = "T";
        std::string title = "Title";
        uint32_t created = 1000;
        bool hasLast = false;
        uint32_t last = 0;
        std::string creator = "Creator";
        uint8_t status = 1;
        bool hasAI = false;
        std::string ai;
        std::vector<std::pair<std::string, std::string>> commands;
        std::vector<std::pair<std::string, std::string>> sql;
        std::vector<std::pair<uint32_t, std::string>> bugs;
        bool hasSummary = false;
        std::string summary;
        std::vector<M> messages;
        std::vector<std::string> assigned;
        uint8_t flags[3] = { 0, 0, 0 };
        uint8_t priority = 0;
        uint8_t category = 0;
        bool closedByCreator = false;
        bool hasClose = false;
        uint32_t close = 0;
        std::string locale = "enUS";
        bool hasOnline = false;
        std::string online;
    };

    void Opt(CoATest::Bytes& b, bool has, const std::string& s) {
        b.U8(has ? 1 : 0);

        if (has) {
            b.Str(s.c_str());
        }
    }

    void OptU(CoATest::Bytes& b, bool has, uint32_t v) {
        b.U8(has ? 1 : 0);

        if (has) {
            b.U32(v);
        }
    }

    void PutMessage(CoATest::Bytes& b, const M& m) {
        b.Str(m.tid.c_str()).U32(m.mid).U8(m.fromGM ? 1 : 0).Str(m.sender.c_str());
        Opt(b, m.hasText, m.text);
        b.U8(m.gmOnly ? 1 : 0).U32(m.ts).U32(static_cast<uint32_t>(m.receipts.size()));

        for (const R& r : m.receipts) {
            b.Str(r.tid.c_str()).U32(r.mid).Str(r.by.c_str()).U32(r.ts);
        }
    }

    void PutTicket(CoATest::Bytes& b, const T& t) {
        b.Str(t.id.c_str()).Str(t.title.c_str()).U32(t.created);
        OptU(b, t.hasLast, t.last);
        b.Str(t.creator.c_str()).U8(t.status);
        Opt(b, t.hasAI, t.ai);

        for (const auto* list : { &t.commands, &t.sql }) {
            b.U32(static_cast<uint32_t>(list->size()));

            for (const auto& c : *list) {
                b.Str(c.first.c_str()).Str(c.second.c_str());
            }
        }

        b.U32(static_cast<uint32_t>(t.bugs.size()));

        for (const auto& c : t.bugs) {
            b.U32(c.first).Str(c.second.c_str());
        }

        Opt(b, t.hasSummary, t.summary);
        b.U32(static_cast<uint32_t>(t.messages.size()));

        for (const M& m : t.messages) {
            PutMessage(b, m);
        }

        b.U32(static_cast<uint32_t>(t.assigned.size()));

        for (const std::string& a : t.assigned) {
            b.Str(a.c_str());
        }

        b.U8(t.flags[0]).U8(t.flags[1]).U8(t.flags[2]).U8(t.priority).U8(t.category).U8(t.closedByCreator ? 1 : 0);
        OptU(b, t.hasClose, t.close);
        b.Str(t.locale.c_str());
        Opt(b, t.hasOnline, t.online);
    }

    T Simple(const char* id, uint32_t created, uint8_t status = 1) {
        T t;
        t.id = id;
        t.created = created;
        t.status = status;
        return t;
    }

    const char* s_player = "Gm";

    lua_State* Start() {
        CoA::LuaReg::Reset();
        CoA::Packets::Clear();
        CoA::Lifecycle::Reset();
        CoA::Account::Reset();
        CoA::GMTicket::Reset();
        CoA::GMTicket::Init();
        s_player = "Gm";
        CoA::GMTicket::Dependencies().activePlayerName = [] { return s_player; };
        CoA::ClientState::SetActionProbe(nullptr);
        return CoATest::NewState(false);
    }

    void Stop(lua_State* L) {
        lua_close(L);
        CoATest::StopRecordingPackets();
        CoATest::StopRecordingSignals();
        CoA::ClientState::SetActionProbe(nullptr);
        CoA::GMTicket::Reset();
        CoA::Account::Reset();
    }

    V Dispatch(uint32_t opcode, CoATest::Bytes& b) {
        CoATest::RecordSignals();
        CDataStore msg(b.data.data(), static_cast<uint32_t>(b.data.size()));
        int32_t result = 0;
        CoA::Packets::Dispatch(static_cast<int32_t>(opcode), 0, &msg, result);
        CoATest::StopRecordingSignals();
        return CoATest::Signals();
    }

    V List(bool clear, bool reset, const std::vector<T>& tickets) {
        CoATest::Bytes b;
        b.U8(clear ? 1 : 0).U8(reset ? 1 : 0).U32(static_cast<uint32_t>(tickets.size()));

        for (const T& t : tickets) {
            PutTicket(b, t);
        }

        return Dispatch(0x709, b);
    }

    V One(uint32_t opcode, const T& t) {
        CoATest::Bytes b;
        PutTicket(b, t);
        return Dispatch(opcode, b);
    }

    void GmLevel(uint32_t level) {
        CoATest::Bytes b;
        b.U32(1).U32(level).U8(0).U8(0).U32(0);
        CoATest::Deliver(CoA::Account::OnAccountInfo, 0x9BB, b);
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
            out.push_back(lua_isnil(L, i) ? "nil" : lua_isboolean(L, i) ? (lua_toboolean(L, i) ? "true" : "false")
                : lua_istable(L, i) ? "table" : lua_tostring(L, i));
        }

        lua_settop(L, base);
        return out;
    }

    Packets Sent(lua_State* L, const std::string& chunk, V* results = nullptr) {
        CoATest::RecordPackets();
        V r = Run(L, chunk);
        CoATest::StopRecordingPackets();

        if (results) {
            *results = r;
        }

        return CoATest::SentPackets();
    }

    std::vector<uint8_t> Expect(uint32_t opcode, const CoATest::Bytes& b) {
        return CoATest::Expected(opcode, b);
    }

    // A table as "k=v" pairs in key order, nested tables as "k={...}" (no table library in the test state).
    const std::string kDump =
        "local function dump(t) local keys = {} for k in pairs(t) do local i = #keys + 1 keys[i] = k "
        "while i > 1 and tostring(keys[i - 1]) > tostring(keys[i]) do keys[i - 1], keys[i] = keys[i], keys[i - 1] i = i - 1 end end "
        "local out = '' for i, k in ipairs(keys) do local v = t[k] if i > 1 then out = out .. ' ' end "
        "out = out .. tostring(k) .. '=' .. (type(v) == 'table' and ('{' .. dump(v) .. '}') or tostring(v)) end "
        "return out end ";

    // The ids of the filtered view, in order.
    const std::string kIds =
        "local function ids() local s = '' for i = 1, C_GMTicket.GetNumTickets() do s = s .. C_GMTicket.GetTicketAtIndex(i).TicketID .. ' ' end "
        "return s end ";

    std::string Ids(lua_State* L, const std::string& search, const std::string& filters, const std::string& sorts) {
        const V r = Run(L, kIds + "C_GMTicket.SetTicketFilter('" + search + "', {" + filters + "}, {" + sorts + "}) return ids()");
        return r.empty() ? "<none>" : r[0];
    }

    T Full() {
        T t;
        t.id = "T1";
        t.title = "Broken quest";
        t.created = 5000;
        t.hasLast = true;
        t.last = 6000;
        t.creator = "Bob";
        t.status = 3;
        t.hasAI = true;
        t.ai = "Try again";
        t.commands = { { ".quest complete", "stuck" } };
        t.sql = { { "SELECT 1", "check" }, { "SELECT 2", "again" } };
        t.bugs = { { 77, "known" } };
        t.hasSummary = true;
        t.summary = "Sum";
        M m;
        m.tid = "T1";
        m.mid = 9;
        m.fromGM = true;
        m.sender = "Gm";
        m.text = "On it";
        m.gmOnly = true;
        m.ts = 6000;
        m.receipts = { { "T1", 9, "Bob", 6100 } };
        M hidden;
        hidden.tid = "T1";
        hidden.mid = 10;
        hidden.hasText = false;
        hidden.ts = 6200;
        t.messages = { m, hidden };
        t.assigned = { "Gm", "Other" };
        t.flags[0] = 1;
        t.flags[1] = 0;
        t.flags[2] = 1;
        t.priority = 2;
        t.category = 6;
        t.closedByCreator = true;
        t.hasClose = true;
        t.close = 7000;
        t.locale = "deDE";
        t.hasOnline = true;
        t.online = "Bobby";
        return t;
    }
}

// The record read in wire order and written back with every key; optional fields absent read as nil; enum values past
// their table are UNEXPECTED_ENUM_VALUE_<n>.
COA_TEST(GMTicket_RecordRoundTrip) {
    lua_State* L = Start();
    T bare = Simple("T2", 10);
    bare.status = 9;
    bare.priority = 4;
    bare.category = 10;
    CHECK(List(true, false, { Full(), bare }).empty());
    CHECK((Run(L, kDump + "return dump(C_GMTicket.GetTicketByID('T1'))") ==
        V{ "AISuggestedBugTrackerReports={1={BugTrackerReportID=77 Reason=known}} "
           "AISuggestedCommands={1={Command=.quest complete Reason=stuck}} "
           "AISuggestedSQL={1={Reason=check SQL=SELECT 1} 2={Reason=again SQL=SELECT 2}} AISuggestion=Try again AISuggestionSummary=Sum "
           "AssignedTo={1=Gm 2=Other} Category=GM_TICKET_CATEGORY_MYSTIC_ENCHANTS CloseDate=7000 CreationDate=5000 Creator=Bob "
           "CreatorLocale=deDE IsAnyMessageUnseen=false IsClosedByCreator=true IsLastMessageFromGM=true IsLastMessageUnseen=true "
           "LastMessageDate=6000 Messages={1={GMOnly=true IsFromGM=true Message=On it MessageID=9 "
           "ReadReceipts={1={MessageID=9 ReadBy=Bob TicketID=T1 Timestamp=6100}} Sender=Gm TicketID=T1 Timestamp=6000} "
           "2={GMOnly=false IsFromGM=false MessageID=10 ReadReceipts={} Sender=Player TicketID=T1 Timestamp=6200}} "
           "OnlineCharacterName=Bobby Priority=HIGH Status=WAITING_FOR_PLAYER TicketID=T1 Title=Broken quest" }));
    CHECK((Run(L, kDump + "return dump(C_GMTicket.GetTicketByID('T2'))") ==
        V{ "AISuggestedBugTrackerReports={} AISuggestedCommands={} AISuggestedSQL={} AssignedTo={} "
           "Category=UNEXPECTED_ENUM_VALUE_10 CreationDate=10 Creator=Creator CreatorLocale=enUS IsAnyMessageUnseen=false "
           "IsClosedByCreator=false IsLastMessageFromGM=false IsLastMessageUnseen=false Messages={} "
           "Priority=UNEXPECTED_ENUM_VALUE_4 Status=UNEXPECTED_ENUM_VALUE_9 TicketID=T2 Title=Title" }));
    CHECK((Run(L, "return C_GMTicket.GetTicketByID('nope'), select('#', C_GMTicket.GetTicketByID())") == V{ "nil", "0" }));
    Stop(L);
}

// A count past the bytes left stops at the end; reads past it are 0 / "".
COA_TEST(GMTicket_ShortPacket) {
    lua_State* L = Start();
    CoATest::Bytes b;
    b.U8(0).U8(0).U32(0xFFFFFFFF).Str("X");
    Dispatch(0x709, b);
    CHECK((Run(L, "local t = C_GMTicket.GetTicketByID('X') return t.Title, t.CreationDate, #t.Messages, t.Status") ==
        V{ "", "0", "0", "NONE" }));
    CHECK((Run(L, "C_GMTicket.SetTicketFilter('', {}, {}) return C_GMTicket.GetNumTickets()") == V{ "1" }));
    Stop(L);
}

// SMSG 0x709: u8 clear, u8 reset; the view is a snapshot until a reset (or nothing was populated yet).
COA_TEST(GMTicket_ListAppendClearReset) {
    lua_State* L = Start();
    CHECK(Run(L, "return C_GMTicket.GetNumTickets()") == V{ "0" });
    List(false, false, { Simple("A", 1) });
    List(false, false, { Simple("B", 2) });
    CHECK(Run(L, "return C_GMTicket.GetNumTickets()") == V{ "0" });   // nothing populated yet
    CHECK(Ids(L, "", "", "") == "B A ");
    List(false, false, { Simple("C", 3) });
    CHECK(Ids(L, "", "", "") == "B A ");   // the snapshot is kept
    CHECK(Run(L, "return C_GMTicket.GetTicketByID('C').TicketID") == V{ "C" });
    List(false, true, {});
    CHECK(Run(L, "return C_GMTicket.GetNumTickets()") == V{ "0" });
    CHECK(Ids(L, "", "", "") == "C B A ");
    List(true, false, { Simple("D", 4) });
    CHECK(Ids(L, "", "", "") == "C B A ");
    CHECK((Run(L, "return C_GMTicket.GetTicketByID('A'), C_GMTicket.GetTicketByID('D').TicketID") == V{ "nil", "D" }));
    List(true, true, {});
    CHECK(Ids(L, "", "", "") == "");
    CHECK((Run(L, "return C_GMTicket.GetTicketAtIndex(0) == nil, select('#', C_GMTicket.GetTicketAtIndex(0)), C_GMTicket.GetTicketAtIndex(1), "
                  "C_GMTicket.GetTicketAtIndex(-1), select('#', C_GMTicket.GetTicketAtIndex('x'))") == V{ "true", "0", "nil", "nil", "0" }));
    Stop(L);
}

// The populate key orders open before closed, a close date before none, then newest first (CloseDate when closed and
// set); the sort selections only break its ties.
COA_TEST(GMTicket_DefaultOrderAndSorts) {
    lua_State* L = Start();
    T closedDated = Simple("CD", 10, 2);
    closedDated.hasClose = true;
    closedDated.close = 50;
    T closedDated2 = Simple("CD2", 40, 5);
    closedDated2.hasClose = true;
    closedDated2.close = 20;
    T openDated = Simple("OD", 5, 1);
    openDated.hasClose = true;
    openDated.close = 999;   // ignored while open: CreationDate
    List(true, true, { Simple("O1", 100), Simple("O2", 300), Simple("C1", 200, 2), closedDated, closedDated2, openDated });
    CHECK(Ids(L, "", "", "") == "OD O2 O1 CD CD2 C1 ");
    // Ascending creation cannot reorder distinct populate keys.
    CHECK(Ids(L, "", "", "'SORT_CREATION_TIME_ASCENDING'") == "OD O2 O1 CD CD2 C1 ");
    // Ties (same state and date): the sorts decide, in the order given.
    T a = Simple("A", 100);
    T b = Simple("B", 100);
    T c = Simple("C", 100);
    M pa;
    pa.ts = 30;
    M pb;
    pb.ts = 10;
    M gm;
    gm.fromGM = true;
    gm.ts = 99;
    a.messages = { pa, gm };   // the last PLAYER message counts
    b.messages = { pb };
    c.messages = { gm };       // no player reply: all ones
    List(true, true, { a, b, c });
    CHECK(Ids(L, "", "", "'SORT_LAST_PLAYER_REPLY_ASCENDING'") == "B A C ");
    CHECK(Ids(L, "", "", "'SORT_LAST_PLAYER_REPLY_DESCENDING'") == "A B C ");
    CHECK(Ids(L, "", "", "'SORT_NONE', 'SORT_LAST_PLAYER_REPLY_DESCENDING'") == "A B C ");
    T d = Simple("D", 100);
    d.messages = { pb };
    T e = Simple("E", 100);
    e.messages = { pb };
    List(true, true, { d, e, b });
    // Creation descending is a 64-bit key: equal here, so the next selection decides.
    CHECK(Ids(L, "", "", "'SORT_CREATION_TIME_DESCENDING', 'SORT_LAST_PLAYER_REPLY_ASCENDING', 'BOGUS'").size() == 6);
    Stop(L);
}

// Filters: groups AND (open/closed, online/offline, assignment), filters inside a group OR; FILTER_NONE and any
// "$<c><digits>" token reject everything; the search is a lowercase substring of any text.
COA_TEST(GMTicket_Filters) {
    lua_State* L = Start();
    T a = Simple("A", 6);
    a.hasOnline = true;
    a.assigned = { "Gm" };
    T b = Simple("B", 5, 2);
    b.assigned = { "Other" };
    T c = Simple("C", 4);
    c.hasOnline = true;
    T d = Simple("D", 3, 5);
    d.hasOnline = true;
    d.assigned = { "Other", "Gm" };
    List(true, true, { a, b, c, d });
    CHECK(Ids(L, "", "FILTER_OPEN = true", "") == "A C ");
    CHECK(Ids(L, "", "FILTER_OPEN = false", "") == "A C B D ");   // a false value selects nothing
    CHECK(Ids(L, "", "FILTER_CLOSED = true", "") == "B D ");
    CHECK(Ids(L, "", "FILTER_OPEN = true, FILTER_CLOSED = true", "") == "A C B D ");
    CHECK(Ids(L, "", "FILTER_ONLINE = true", "") == "A C D ");
    CHECK(Ids(L, "", "FILTER_OFFLINE = true", "") == "B ");
    CHECK(Ids(L, "", "FILTER_OPEN = true, FILTER_ONLINE = true", "") == "A C ");
    CHECK(Ids(L, "", "FILTER_CLOSED = true, FILTER_OFFLINE = true", "") == "B ");
    CHECK(Ids(L, "", "FILTER_ASSIGNED_TO_ME = true", "") == "A D ");
    CHECK(Ids(L, "", "FILTER_ASSIGNED_TO_OTHER = true", "") == "B ");
    CHECK(Ids(L, "", "FILTER_ASSIGNED_TO_NONE = true", "") == "C ");
    CHECK(Ids(L, "", "FILTER_ASSIGNED_TO_NONE = true, FILTER_ASSIGNED_TO_OTHER = true", "") == "C B ");
    CHECK(Ids(L, "", "FILTER_ONLINE = true, FILTER_ASSIGNED_TO_NONE = true", "") == "C ");   // separate groups AND
    CHECK(Ids(L, "", "FILTER_OFFLINE = true, FILTER_ASSIGNED_TO_ME = true", "") == "");
    CHECK(Ids(L, "", "FILTER_NONE = true", "") == "");
    CHECK(Ids(L, "", "FILTER_BOGUS = true", "") == "A C B D ");
    s_player = nullptr;   // no active player: neither "me" nor "other"
    CHECK(Ids(L, "", "FILTER_ASSIGNED_TO_ME = true", "") == "");
    CHECK(Ids(L, "", "FILTER_ASSIGNED_TO_OTHER = true", "") == "");
    s_player = "Gm";
    CHECK(Ids(L, "$c1", "", "") == "");
    CHECK(Ids(L, "  ", "", "") == "A C B D ");
    Stop(L);
}

COA_TEST(GMTicket_Search) {
    lua_State* L = Start();
    T full = Full();
    full.status = 1;
    List(true, true, { full, Simple("Plain", 1) });
    for (const char* s : { "BROKEN", "bob", "t1", "OTHER", "on it", "try again", "sum", ".quest", "stuck", "select 2", "again", "known" }) {
        CHECK(Ids(L, s, "", "") == "T1 ");
    }
    CHECK(Ids(L, "player", "", "") == "T1 ");   // a message sender
    CHECK(Ids(L, "creator", "", "") == "Plain ");
    CHECK(Ids(L, "deDE", "", "") == "");         // the locale is not searched
    CHECK(Ids(L, "bobby", "", "") == "");        // nor the online name
    CHECK(Ids(L, "77", "", "") == "");           // nor the bug report id
    CHECK(Ids(L, "", "", "") == "T1 Plain ");
    Stop(L);
}

// Every binding answers nothing while CanPerformAction(0x11) fails.
COA_TEST(GMTicket_Gate) {
    lua_State* L = Start();
    List(true, true, { Full() });
    CoA::ClientState::SetActionProbe([](int32_t action) { return action != 0x11; });
    CoATest::RecordPackets();
    CHECK((Run(L, "local n, bad = 0, '' for _, ns in ipairs({ 'C_GMTicket', 'C_PlayerTicket', 'C_GM' }) do "
                  "for k, f in pairs(_G[ns]) do n = n + 1 if select('#', f('T1', 'x', 'y', 'z', 'w')) ~= 0 or select('#', f()) ~= 0 then bad = bad .. k end end end "
                  "return n, bad") == V{ "43", "" }));
    CoATest::StopRecordingPackets();
    CHECK(CoATest::SentPackets().empty());
    CoA::ClientState::SetActionProbe([](int32_t) { return true; });
    CHECK(Run(L, "return C_GMTicket.GetNumTickets()") == V{ "0" });
    Stop(L);
}

// The GM CMSGs, byte-exact; an optional ticket id is written only when given.
COA_TEST(GMTicket_GmCmsgs) {
    lua_State* L = Start();
    V r;
    CHECK(Sent(L, "return C_GMTicket.AssignGMTicket('T1', 'Gm')", &r) == Packets{ Expect(0x717, CoATest::Bytes().Str("T1").Str("Gm")) });
    CHECK(r == V{ "true" });
    CHECK(Sent(L, "return C_GMTicket.DeclineAutomatedMessage('T1', 'no')") == Packets{ Expect(0x714, CoATest::Bytes().Str("T1").Str("no")) });
    CHECK(Sent(L, "return C_GMTicket.SetTicketTitle('T1', 'New')") == Packets{ Expect(0x710, CoATest::Bytes().Str("T1").Str("New")) });
    CHECK(Sent(L, "return C_GMTicket.ApproveAutomatedMessage('T1')") == Packets{ Expect(0x712, CoATest::Bytes().Str("T1")) });
    CHECK(Sent(L, "return C_GMTicket.CloseTicket('T1')") == Packets{ Expect(0x705, CoATest::Bytes().Str("T1")) });
    CHECK(Sent(L, "return C_GMTicket.CloseTicket()") == Packets{ Expect(0x705, CoATest::Bytes()) });
    CHECK(Sent(L, "return C_GMTicket.MarkResponseSeen('T1', 7)") == Packets{ Expect(0x71D, CoATest::Bytes().Str("T1").U32(7)) });
    CHECK(Sent(L, "return C_GMTicket.MarkResponseSeen(nil, 7)") == Packets{ Expect(0x71D, CoATest::Bytes().U32(7)) });
    CHECK(Sent(L, "return C_GMTicket.SendTicketMessage('T1', 'hi', true)") == Packets{ Expect(0x707, CoATest::Bytes().Str("T1").Str("hi").U8(1)) });
    CHECK(Sent(L, "return C_GMTicket.SendTicketMessage(nil, 'hi', 0)") == Packets{ Expect(0x707, CoATest::Bytes().Str("hi").U8(1)) });
    CHECK(Sent(L, "return C_GMTicket.SendTicketMessage(nil, 'hi', false)") == Packets{ Expect(0x707, CoATest::Bytes().Str("hi").U8(0)) });
    // A missing boolean is a missing argument (only trailing nil slots are optional).
    CHECK(Sent(L, "return C_GMTicket.SendTicketMessage(nil, 'hi')").empty());
    CHECK(Sent(L, "return C_GMTicket.SetTicketPriority('T1', 'URGENT')") == Packets{ Expect(0x70E, CoATest::Bytes().Str("T1").U8(3)) });
    CHECK(Sent(L, "return C_GMTicket.SetTicketPriority('T1', 'urgent')") == Packets{ Expect(0x70E, CoATest::Bytes().Str("T1").U8(0)) });
    CHECK(Sent(L, "return C_GMTicket.SetTicketStatus('T1', 'ESCALATED_SECURITY_LEVEL')") == Packets{ Expect(0x70C, CoATest::Bytes().Str("T1").U8(6)) });
    CHECK(Sent(L, "return C_GMTicket.SetTicketStatus('T1', 'LOCKED')") == Packets{ Expect(0x70C, CoATest::Bytes().Str("T1").U8(5)) });
    CHECK(Sent(L, "return C_GM.RequestPlayerInfo('Bob')", &r) == Packets{ Expect(0x71B, CoATest::Bytes().Str("Bob")) });
    CHECK(r == V{ "true" });
    // Bad arguments: nothing sent, nothing returned.
    CHECK(Sent(L, "return C_GMTicket.AssignGMTicket('T1'), C_GMTicket.CloseTicket({}), C_GMTicket.SendTicketMessage('T1', 'hi', 'x'), "
                  "C_GMTicket.MarkResponseSeen('T1'), C_GMTicket.SetTicketStatus('T1'), C_GMTicket.ApproveAutomatedMessage()", &r).empty());
    CHECK((Run(L, "return select('#', C_GMTicket.AssignGMTicket('T1')), select('#', C_GMTicket.CloseTicket({}))") == V{ "0", "0" }));
    // The original's string slots take numbers, and extra arguments are not checked.
    CHECK(Sent(L, "return C_GMTicket.CloseTicket(5, 'extra')") == Packets{ Expect(0x705, CoATest::Bytes().Str("5")) });
    // The Can* twins answer (true, nil) and send nothing.
    CHECK(Sent(L, "return C_GMTicket.CanAssignGMTicket('a', 'b'), C_GMTicket.CanSetTicketTitle('a', 'b'), C_GMTicket.CanCloseTicket(), "
                  "C_GMTicket.CanMarkResponseSeen('a', 1), C_GMTicket.CanSendTicketMessage('a', 'b', false), "
                  "C_GMTicket.CanSetTicketPriority('a', 'LOW'), C_GMTicket.CanSetTicketStatus('a', 'OPEN')", &r).empty());
    CHECK((r == V{ "true", "true", "true", "true", "true", "true", "true", "nil" }));
    CHECK((Run(L, "return select('#', C_GMTicket.CanCloseTicket('a')), select('#', C_GMTicket.CanAssignGMTicket('a'))") == V{ "2", "0" }));
    Stop(L);
}

// The player CMSGs: the own ticket's id replaces the argument whenever there is a ticket.
COA_TEST(GMTicket_PlayerCmsgs) {
    lua_State* L = Start();
    V r;
    CHECK(Sent(L, "return C_PlayerTicket.CreateTicket('HIGH', 'GM_TICKET_CATEGORY_RAIDS', 'a', 'b', 'c')", &r) ==
        Packets{ Expect(0x703, CoATest::Bytes().U8(2).U8(7).Str("a").Str("b").Str("c")) });
    CHECK(r == V{ "true" });
    CHECK(Sent(L, "return C_PlayerTicket.CreateTicket('x', 'y', 'a', 'b', 'c')") == Packets{ Expect(0x703, CoATest::Bytes().U8(0).U8(0).Str("a").Str("b").Str("c")) });
    CHECK(Sent(L, "return C_PlayerTicket.CreateTicket('HIGH', 'x', 'a', 'b')").empty());
    CHECK((Run(L, "return C_PlayerTicket.CanCreateTicket('LOW', 'x', 'a', 'b', 'c')") == V{ "true", "nil" }));
    // No ticket yet: the argument as given.
    CHECK(Sent(L, "return C_PlayerTicket.CloseTicket('Mine')") == Packets{ Expect(0x705, CoATest::Bytes().Str("Mine")) });
    CHECK(Sent(L, "return C_PlayerTicket.ReopenTicket()") == Packets{ Expect(0x719, CoATest::Bytes()) });
    CHECK(Sent(L, "return C_PlayerTicket.SendTicketMessage(nil, 'yo', false)") == Packets{ Expect(0x707, CoATest::Bytes().Str("yo").U8(0)) });
    CHECK(Sent(L, "return C_PlayerTicket.MarkResponseSeen(nil, 3)") == Packets{ Expect(0x71D, CoATest::Bytes().U32(3)) });
    One(0x701, Simple("Own", 1));
    CHECK(Sent(L, "return C_PlayerTicket.CloseTicket('Mine')") == Packets{ Expect(0x705, CoATest::Bytes().Str("Own")) });
    CHECK(Sent(L, "return C_PlayerTicket.CloseTicket()") == Packets{ Expect(0x705, CoATest::Bytes().Str("Own")) });
    CHECK(Sent(L, "return C_PlayerTicket.ReopenTicket('Mine')") == Packets{ Expect(0x719, CoATest::Bytes().Str("Own")) });
    CHECK(Sent(L, "return C_PlayerTicket.SendTicketMessage(nil, 'yo', true)") == Packets{ Expect(0x707, CoATest::Bytes().Str("Own").Str("yo").U8(1)) });
    CHECK(Sent(L, "return C_PlayerTicket.MarkResponseSeen('x', 3)") == Packets{ Expect(0x71D, CoATest::Bytes().Str("Own").U32(3)) });
    // The GM natives never substitute.
    CHECK(Sent(L, "return C_GMTicket.CloseTicket()") == Packets{ Expect(0x705, CoATest::Bytes()) });
    CHECK((Run(L, "return C_PlayerTicket.CanCloseTicket(), C_PlayerTicket.CanReopenTicket(), C_PlayerTicket.CanSendTicketMessage(nil, 'a', true), "
                  "C_PlayerTicket.CanMarkResponseSeen(nil, 1)") == V{ "true", "true", "true", "true", "nil" }));
    CHECK(Sent(L, "return C_PlayerTicket.ReopenTicket({}), C_PlayerTicket.SendTicketMessage(nil, 'a', nil)", &r) == Packets{ Expect(0x707, CoATest::Bytes().Str("Own").Str("a").U8(0)) });
    Stop(L);
}

// SMSG 0x70A: appended, GM_TICKET_NEW_TICKET, GM_TICKETS_UPDATED, the view reset, GM_TICKET_FILTER_RESET.
COA_TEST(GMTicket_Created) {
    lua_State* L = Start();
    List(true, true, { Simple("A", 1) });
    CHECK(Ids(L, "", "", "") == "A ");
    CHECK((One(0x70A, Simple("B", 2)) == V{ "GM_TICKET_NEW_TICKET:B", "GM_TICKETS_UPDATED", "GM_TICKET_FILTER_RESET" }));
    CHECK(Run(L, "return C_GMTicket.GetNumTickets()") == V{ "0" });
    CHECK(Ids(L, "", "", "") == "B A ");
    Stop(L);
}

// SMSG 0x70B: the diff events, in order; an unknown id is dropped.
COA_TEST(GMTicket_UpdateDiffs) {
    lua_State* L = Start();
    T base = Simple("A", 1);
    M m1;
    m1.mid = 1;
    m1.receipts = { { "A", 1, "X", 1 } };
    base.messages = { m1 };
    List(true, true, { base });
    CHECK(Ids(L, "", "", "") == "A ");
    CHECK(One(0x70B, Simple("Z", 1)).empty());
    CHECK(Ids(L, "", "", "") == "A ");   // not reset
    CHECK(One(0x70B, base) == V{ "GM_TICKET_FILTER_RESET" });
    CHECK(Run(L, "return C_GMTicket.GetNumTickets()") == V{ "0" });
    T s = base;
    s.status = 3;   // OPEN -> WAITING: no closure change
    CHECK((One(0x70B, s) == V{ "GM_TICKET_STATUS_CHANGED:A", "GM_TICKET_FILTER_RESET" }));
    s.status = 5;   // -> LOCKED: closed
    CHECK((One(0x70B, s) == V{ "GM_TICKETS_UPDATED", "GM_TICKET_STATUS_CHANGED:A", "GM_TICKET_FILTER_RESET" }));
    s.status = 2;   // LOCKED -> CLOSED: both closed
    CHECK((One(0x70B, s) == V{ "GM_TICKET_STATUS_CHANGED:A", "GM_TICKET_FILTER_RESET" }));
    s.assigned = { "Gm" };
    CHECK((One(0x70B, s) == V{ "GM_TICKET_STATUS_CHANGED:A", "GM_TICKET_FILTER_RESET" }));
    s.assigned = { "Other" };   // same count: no event
    CHECK(One(0x70B, s) == V{ "GM_TICKET_FILTER_RESET" });
    M m2;
    m2.mid = 2;
    s.messages.push_back(m2);
    CHECK((One(0x70B, s) == V{ "GM_TICKET_MESSAGE:A", "GM_TICKET_FILTER_RESET" }));
    s.messages.pop_back();
    CHECK((One(0x70B, s) == V{ "GM_TICKET_MESSAGE:A", "GM_TICKET_FILTER_RESET" }));
    s.hasAI = true;
    s.ai = "x";
    CHECK((One(0x70B, s) == V{ "GM_TICKET_SUGGESTION_UPDATE:A", "GM_TICKET_FILTER_RESET" }));
    CHECK(One(0x70B, s) == V{ "GM_TICKET_FILTER_RESET" });
    s.ai = "y";
    CHECK((One(0x70B, s) == V{ "GM_TICKET_SUGGESTION_UPDATE:A", "GM_TICKET_FILTER_RESET" }));
    s.hasAI = false;
    CHECK((One(0x70B, s) == V{ "GM_TICKET_SUGGESTION_UPDATE:A", "GM_TICKET_FILTER_RESET" }));
    // Two new receipts on message 1: one event each; fewer receipts: none.
    s.messages[0].receipts = { { "A", 1, "X", 1 }, { "A", 1, "Y", 2 }, { "A", 1, "Z", 3 } };
    CHECK((One(0x70B, s) == V{ "GM_TICKET_MESSAGE_READ:A,1", "GM_TICKET_MESSAGE_READ:A,1", "GM_TICKET_FILTER_RESET" }));
    s.messages[0].receipts.clear();
    CHECK(One(0x70B, s) == V{ "GM_TICKET_FILTER_RESET" });
    // Everything at once, in the original's order.
    T all = s;
    all.status = 1;
    all.messages.push_back(m2);
    all.messages[0].receipts = { { "A", 1, "Q", 1 } };
    all.hasAI = true;
    CHECK((One(0x70B, all) == V{ "GM_TICKETS_UPDATED", "GM_TICKET_STATUS_CHANGED:A", "GM_TICKET_MESSAGE:A", "GM_TICKET_SUGGESTION_UPDATE:A",
                                 "GM_TICKET_MESSAGE_READ:A,1", "GM_TICKET_FILTER_RESET" }));
    CHECK(Run(L, "return C_GMTicket.GetTicketByID('A').Status") == V{ "OPEN" });
    Stop(L);
}

// The ticket readers.
COA_TEST(GMTicket_Readers) {
    lua_State* L = Start();
    List(true, true, { Full(), Simple("B", 1) });
    const std::string d = kDump;
    CHECK((Run(L, d + "return dump(C_GMTicket.GetTicketMessage('T1', 10))") ==
        V{ "GMOnly=false IsFromGM=false MessageID=10 ReadReceipts={} Sender=Player TicketID=T1 Timestamp=6200" }));
    CHECK((Run(L, "return C_GMTicket.GetTicketMessage('T1', 11), C_GMTicket.GetTicketMessage('Q', 9), #C_GMTicket.GetTicketMessages('T1'), "
                  "C_GMTicket.GetTicketMessages('Q')") == V{ "nil", "nil", "2", "nil" }));
    CHECK((Run(L, "return C_GMTicket.GetAutomatedMessage('T1'), C_GMTicket.GetAutomatedMessage('B'), C_GMTicket.GetAutomatedMessage('Q')") ==
        V{ "Try again", "nil", "nil" }));
    CHECK((Run(L, "return C_GMTicket.GetNumSuggestions('T1'), C_GMTicket.GetNumSuggestions('B'), select('#', C_GMTicket.GetNumSuggestions('Q'))") ==
        V{ "5", "0", "0" }));
    CHECK((Run(L, d + "local s = '' for i = 1, 6 do local t = C_GMTicket.GetSuggestionAtIndex('T1', i) s = s .. (t and dump(t) or 'nil') .. ' | ' end return s") ==
        V{ "Command=Sum Reason= Type=Summary | Command=.quest complete Reason=stuck Type=Command | Command=SELECT 1 Reason=check Type=SQL | "
           "Command=SELECT 2 Reason=again Type=SQL | Command=https://ascension.gg/bugtracker/view/77 Reason=known Type=BugTrackerReport | nil | " }));
    CHECK((Run(L, "return C_GMTicket.GetSuggestionAtIndex('B', 1).Command, select('#', C_GMTicket.GetSuggestionAtIndex('B', 0)), "
                  "select('#', C_GMTicket.GetSuggestionAtIndex('Q', 1)), C_GMTicket.GetSuggestionAtIndex('B', 2)") == V{ "<Empty Summary>", "0", "0", "nil" }));
    // Read receipts by the active player's name.
    s_player = "Bob";
    CHECK((Run(L, "return C_GMTicket.IsResponseSeen('T1', 9), C_GMTicket.IsResponseSeen('T1', 10), select('#', C_GMTicket.IsResponseSeen('Q', 9))") ==
        V{ "true", "false", "0" }));
    CHECK((Run(L, "return C_GMTicket.IsTicketAssignedToMe('T1'), C_GMTicket.IsTicketAssignedToMe('B'), select('#', C_GMTicket.IsTicketAssignedToMe('Q'))") ==
        V{ "false", "false", "0" }));
    s_player = "Other";
    CHECK(Run(L, "return C_GMTicket.IsTicketAssignedToMe('T1')") == V{ "true" });
    s_player = nullptr;
    CHECK((Run(L, "return C_GMTicket.IsTicketAssignedToMe('B'), select('#', C_GMTicket.IsResponseSeen('T1', 9))") == V{ "true", "0" }));
    Stop(L);
}

// SMSG 0x701 / 0x702 and the player readers.
COA_TEST(GMTicket_PlayerTicket) {
    lua_State* L = Start();
    CHECK((Run(L, "return select('#', C_PlayerTicket.GetCurrentTicket()), select('#', C_PlayerTicket.GetTicketMessages())") == V{ "0", "0" }));
    // 0x702 without a ticket: a ticket the player has not closed is a GM reopen.
    T t = Simple("P", 1);
    CHECK(One(0x702, t) == V{ "PLAYER_TICKET_UPDATE" });
    CHECK(Run(L, "return C_PlayerTicket.GetCurrentTicket().TicketID") == V{ "P" });
    CHECK(One(0x702, t).empty());   // unchanged
    t.status = 3;
    CHECK(One(0x702, t) == V{ "PLAYER_TICKET_UPDATE" });
    M a;
    a.mid = 4;
    t.messages = { a };
    CHECK((One(0x702, t) == V{ "PLAYER_TICKET_NEW_MESSAGE:4", "PLAYER_TICKET_UPDATE" }));
    // A hidden last message suppresses the message checks.
    M hidden;
    hidden.mid = 5;
    hidden.hasText = false;
    t.messages = { a, hidden };
    CHECK((One(0x702, t) == V{ "PLAYER_TICKET_NEW_MESSAGE:5", "PLAYER_TICKET_UPDATE" }));
    M c;
    c.mid = 6;
    t.messages = { a, hidden, c };
    CHECK(One(0x702, t).empty());
    t.messages = { a };
    CHECK(One(0x702, t) == V{ "PLAYER_TICKET_UPDATE" });   // fewer messages (last not hidden)
    // Closed by its creator: forgotten.
    t.closedByCreator = true;
    CHECK(One(0x702, t) == V{ "PLAYER_TICKET_UPDATE" });
    CHECK(Run(L, "return select('#', C_PlayerTicket.GetCurrentTicket())") == V{ "0" });
    CHECK(One(0x702, t).empty());   // still the player's own close: nothing
    CHECK(Run(L, "return select('#', C_PlayerTicket.GetCurrentTicket())") == V{ "1" });
    // 0x701 replaces and always fires.
    T own = Simple("Own", 1);
    M gmOnly;
    gmOnly.mid = 0;
    gmOnly.gmOnly = true;
    M zero;
    zero.mid = 0;
    zero.text = "zero";
    M seen;
    seen.mid = 8;
    seen.receipts = { { "Own", 8, "Me", 1 } };
    own.messages = { gmOnly, zero, seen };
    CHECK(One(0x701, own) == V{ "PLAYER_TICKET_UPDATE" });
    CHECK((Run(L, "local m = C_PlayerTicket.GetTicketMessages() return #m, m[1].Message, m[2].MessageID") == V{ "2", "zero", "8" }));
    // GetTicketMessage(id): a readable id answers nothing; an unreadable one searches for message 0 (not GM-only).
    CHECK((Run(L, "return select('#', C_PlayerTicket.GetTicketMessage(8)), C_PlayerTicket.GetTicketMessage().Message, "
                  "C_PlayerTicket.GetTicketMessage('x').Message") == V{ "0", "zero", "zero" }));
    s_player = "Me";
    CHECK((Run(L, "return C_PlayerTicket.IsResponseSeen(8), C_PlayerTicket.IsResponseSeen(0), select('#', C_PlayerTicket.IsResponseSeen())") ==
        V{ "true", "false", "0" }));
    s_player = nullptr;
    CHECK(Run(L, "return select('#', C_PlayerTicket.IsResponseSeen(8))") == V{ "0" });
    own.messages = { gmOnly };
    One(0x701, own);
    CHECK(Run(L, "return C_PlayerTicket.GetTicketMessage()") == V{ "nil" });
    Stop(L);
}

// The result packets: a C string signalled as "%s"; 0x706 / 0x71E / 0x708 by the account's GM level.
COA_TEST(GMTicket_Results) {
    lua_State* L = Start();
    const std::pair<uint32_t, const char*> plain[] = { { 0x704, "CREATE_PLAYER_TICKET_RESULT" }, { 0x70D, "UPDATE_GM_TICKET_STATUS_RESULT" },
        { 0x70F, "UPDATE_GM_TICKET_PRIORITY_RESULT" }, { 0x711, "UPDATE_GM_TICKET_TITLE_RESULT" }, { 0x713, "APPROVE_GM_TICKET_SUGGESTION_RESULT" },
        { 0x715, "DECLINE_GM_TICKET_SUGGESTION_RESULT" }, { 0x718, "ASSIGN_GM_TICKET_RESULT" }, { 0x71A, "REOPEN_PLAYER_TICKET_RESULT" } };

    for (const auto& p : plain) {
        CoATest::Bytes b;
        b.Str("OK");
        CHECK(Dispatch(p.first, b) == V{ std::string(p.second) + ":OK" });
    }

    auto result = [](uint32_t op) {
        CoATest::Bytes b;
        b.Str("R");
        return Dispatch(op, b);
    };
    CHECK(result(0x706) == V{ "CLOSE_GM_TICKET_RESULT:R" });
    CHECK(result(0x71E) == V{ "MARK_PLAYER_TICKET_MESSAGE_AS_READ_RESULT:R" });
    CoATest::Bytes m;
    m.Str("SENT").U8(1);
    M msg;
    msg.tid = "T9";
    msg.mid = 12;
    PutMessage(m, msg);
    CHECK(Dispatch(0x708, m) == V{ "SEND_GM_TICKET_MESSAGE_RESULT:SENT,T9,12" });
    CoATest::Bytes none;
    none.Str("FAIL").U8(0);
    CHECK(Dispatch(0x708, none) == V{ "SEND_GM_TICKET_MESSAGE_RESULT:FAIL,,0" });
    GmLevel(1);
    CHECK(result(0x706) == V{ "CLOSE_PLAYER_TICKET_RESULT:R" });
    CHECK(result(0x71E) == V{ "MARK_GM_TICKET_MESSAGE_AS_READ_RESULT:R" });
    CHECK(Dispatch(0x708, m) == V{ "SEND_PLAYER_TICKET_MESSAGE_RESULT:SENT,12" });
    CHECK(Dispatch(0x708, none) == V{ "SEND_PLAYER_TICKET_MESSAGE_RESULT:FAIL,0" });
    Stop(L);
}

// SMSG 0x71C and C_GM.GetPlayerInfo: records are appended, the first by name answers.
COA_TEST(GMTicket_PlayerInfo) {
    lua_State* L = Start();
    CoATest::Bytes miss;
    miss.Str("Nobody").U8(0);
    CHECK(Dispatch(0x71C, miss) == V{ "REQUEST_PLAYER_INFO_RESULT:Nobody" });
    auto record = [](const char* name, uint32_t guid, bool full) {
        CoATest::Bytes b;
        b.Str("asked").U8(1).Str(name).U32(guid).U8(80).U8(2).U8(3).U32(0x11);

        if (full) {
            b.U8(1).F32(250.5f).U8(1).U32(12345).U32(2).U32(5).U32(6).U32(7).U32(8);
            b.U8(1).U32(530).U8(1).F32(1.5f).U8(1).F32(-2.0f).U8(1).F32(3.25f).U8(1).U32(3483).U8(1).U32(3500);
            b.U32(99).U8(1).Str("Guild").U8(1).U8(4).U8(1).Str("Officer").U32(42).Str("ACCOUNT").U32(3);
            b.U8(1).Str("disc").U8(1).Str("hw1").U8(1).Str("hw2").U8(1).Str("Online");
            b.U32(1).Str("H1").Str("Hist").U32(10).U8(1).U32(20).U32(1).U8(1).U8(0).Str("S").U8(1).Str("txt").U32(15);
            b.U32(4);
            b.U32(1).U32(100).U32(60).Str("spam").Str("Mod").U8(2).Str("Realm");
            b.U32(1);
            b.U32(1).U32(200).U32(30).U8(1).U32(230).Str("cheat").Str("Admin");
            b.U32(2);
            b.U32(1).Str("R").Str("C").Str("rude").U32(300);
            b.U32(3);
            b.U32(1).U32(7).Str("Gm").Str("note").U32(400);
            b.U32(5);
            b.U8(1).U32(55);
        } else {
            b.U8(0).U8(0).U32(0).U8(0).U8(0).U8(0).U8(0).U8(0).U8(0).U32(0).U8(0).U8(0).U8(0).U32(0).Str("").U32(9);
            b.U8(0).U8(0).U8(0).U8(0).U32(0).U32(0).U32(0).U32(0).U32(0).U32(0).U32(0).U32(0).U32(0).U32(0).U8(0);
        }

        return b;
    };
    CoATest::Bytes first = record("Bob", 1, true);
    CHECK(Dispatch(0x71C, first) == V{ "REQUEST_PLAYER_INFO_RESULT:Bob,asked" });
    CoATest::Bytes second = record("Bob", 2, false);
    Dispatch(0x71C, second);
    CHECK((Run(L, kDump + "return dump(C_GM.GetPlayerInfo('Bob'))") ==
        V{ "AccountID=42 AccountName=ACCOUNT AreaID=3500 AverageItemLevel=250.5 BanHistory={1={BanDate=200 BanReason=cheat BannedBy=Admin "
           "IsActive=true RemainingTime=30 UnbanDate=230}} Challenges={1={ChallengeID=5 ChallengeLevel=6} 2={ChallengeID=7 "
           "ChallengeLevel=8}} CharacterGuid=1 CharacterLevel=80 CharacterName=Bob ClassID=3 DiscordName=disc GMLevel=SEC_GAMEMASTER2 GMNotes={1={Date=400 "
           "GM=Gm Message=note NoteID=7}} GameModeMask=17 GuildID=99 GuildName=Guild GuildRankID=4 GuildRankName=Officer HardwareID=hw1 "
           "HardwareIDv2=hw2 Latency=55 LocX=1.5 LocY=-2 LocZ=3.25 MapID=530 Money=12345 MuteHistory={1={MuteDate=100 MuteMode=2 "
           "MuteReason=spam MuteTime=60 MutedBy=Mod RealmName=Realm}} NumBans=2 NumMutes=1 NumNotes=5 NumTickets=4 NumWarnings=3 "
           "OnlineCharacterName=Online RaceID=2 TicketHistory={1={CloseDate=20 CreationDate=10 Messages={1={GMOnly=false IsFromGM=true "
           "Message=txt Sender=S Timestamp=15}} TicketID=H1 Title=Hist}} WarningHistory={1={CharacterName=C RealmName=R Reason=rude "
           "Timestamp=300}} ZoneID=3483" }));
    CHECK((Run(L, kDump + "C_GM.GetPlayerInfo('x') return C_GM.GetPlayerInfo('x'), C_GM.GetPlayerInfo('bob'), select('#', C_GM.GetPlayerInfo())") ==
        V{ "nil", "nil", "0" }));
    // The second (sparse) record, by a name only it has.
    CoATest::Bytes third = record("Ann", 3, false);
    Dispatch(0x71C, third);
    CHECK((Run(L, kDump + "return dump(C_GM.GetPlayerInfo('Ann'))") ==
        V{ "AccountID=0 AccountName= BanHistory={} Challenges={} CharacterGuid=3 CharacterLevel=80 CharacterName=Ann ClassID=3 "
           "GMLevel=UNEXPECTED_ENUM_VALUE_9 GMNotes={} GameModeMask=17 GuildID=0 MuteHistory={} NumBans=0 NumMutes=0 NumNotes=0 "
           "NumTickets=0 NumWarnings=0 RaceID=2 TicketHistory={} WarningHistory={}" }));
    Stop(L);
}

// Every glue screen: the list, the view, the player records and the player's ticket are dropped.
COA_TEST(GMTicket_GlueReset) {
    lua_State* L = Start();
    List(true, true, { Simple("A", 1) });
    CHECK(Ids(L, "", "", "") == "A ");
    One(0x701, Simple("Own", 1));
    CoATest::Bytes info;
    info.Str("x").U8(1).Str("Bob");
    Dispatch(0x71C, info);
    CHECK(Run(L, "return C_GM.GetPlayerInfo('Bob') ~= nil") == V{ "true" });
    CoA::Lifecycle::RunGlueScreen();
    CHECK((Run(L, "return C_GMTicket.GetNumTickets(), C_GMTicket.GetTicketByID('A'), C_GM.GetPlayerInfo('Bob'), "
                  "select('#', C_PlayerTicket.GetCurrentTicket())") == V{ "0", "nil", "nil", "0" }));
    CHECK(Ids(L, "", "", "") == "");
    Stop(L);
}
