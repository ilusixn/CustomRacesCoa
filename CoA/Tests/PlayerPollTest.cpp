// C_PlayerPoll (PlayerPoll.cpp): SMSG 0x746 / 0x748, PLAYER_POLL_NEW_POLL / PLAYER_POLL_ANSWER_RESULT, the readers and their
// Lua errors, CanSubmitAnswer / SubmitAnswer (CMSG 0x747), RequestQuestionList (CMSG 0x745, also after world entry) and the
// glue reset.
#include "CoA/Source/Modules/PlayerPoll.h"
#include "CoA/Source/Runtime/ClientHooks.h"
#include "CoA/Source/Runtime/Lifecycle.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Tests/Support.h"
#include "CoA/Tests/TestMain.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <string>
#include <utility>
#include <vector>

namespace {
    using V = std::vector<std::string>;
    using Packets = std::vector<std::vector<uint8_t>>;

    struct Q {
        uint32_t id = 1;
        std::string text = "Question?";
        std::string s20 = "Sub";
        std::string s38 = "Foot";
        std::vector<std::pair<uint32_t, std::string>> choices = { { 11, "Yes" }, { 12, "No" } };
        bool answered = false;
        uint32_t answer = 0;
        bool hasFeedback = false;
        std::string feedback;
        bool changeable = false;
        bool has8C = false;
        std::string s8C;
        uint32_t type = 1;
    };

    void Put(CoATest::Bytes& b, const Q& q) {
        b.U32(q.id).U32(77).Str(q.text.c_str()).Str(q.s20.c_str()).Str(q.s38.c_str()).U32(5).U32(6);
        b.U32(static_cast<uint32_t>(q.choices.size()));

        for (const auto& c : q.choices) {
            b.U32(c.first).Str(c.second.c_str());
        }

        b.U8(q.answered ? 1 : 0);

        if (q.answered) {
            b.U32(q.answer);
        }

        b.U8(q.hasFeedback ? 1 : 0);

        if (q.hasFeedback) {
            b.Str(q.feedback.c_str());
        }

        b.U8(q.changeable ? 1 : 0).U8(q.has8C ? 1 : 0);

        if (q.has8C) {
            b.Str(q.s8C.c_str());
        }

        b.U32(9).U32(q.type);
    }

    V Dispatch(uint32_t opcode, CoATest::Bytes& b) {
        CoATest::RecordSignals();
        CDataStore msg(b.data.data(), static_cast<uint32_t>(b.data.size()));
        int32_t result = 0;
        CoA::Packets::Dispatch(static_cast<int32_t>(opcode), 0, &msg, result);
        CoATest::StopRecordingSignals();
        return CoATest::Signals();
    }

    V List(const std::vector<Q>& qs) {
        CoATest::Bytes b;
        b.U32(static_cast<uint32_t>(qs.size()));

        for (const Q& q : qs) {
            Put(b, q);
        }

        return Dispatch(0x746, b);
    }

    V Result(const char* result, const Q* q) {
        CoATest::Bytes b;
        b.Str(result);

        if (q) {
            b.U8(1);
            Put(b, *q);
        }

        return Dispatch(0x748, b);
    }

    lua_State* Start() {
        CoA::LuaReg::Reset();
        CoA::Packets::Clear();
        CoA::Lifecycle::Reset();
        CoA::ClientHooks::afterEnterWorld.Clear();
        CoA::PlayerPoll::Reset();
        CoA::PlayerPoll::Init();
        return CoATest::NewState(false);
    }

    void Stop(lua_State* L) {
        lua_close(L);
        CoATest::StopRecordingPackets();
        CoA::ClientHooks::afterEnterWorld.Clear();
        CoA::PlayerPoll::Reset();
    }

    V Call(lua_State* L, const char* name, const V& args = {}) {
        return CoATest::Call(L, "C_PlayerPoll", name, args);
    }

    Packets Sent(lua_State* L, const char* name, const V& args, V* r = nullptr) {
        CoATest::RecordPackets();
        const V out = Call(L, name, args);
        CoATest::StopRecordingPackets();

        if (r) {
            *r = out;
        }

        return CoATest::SentPackets();
    }

    std::vector<uint8_t> Answer(uint32_t question, bool hasChoice, uint32_t choice, bool hasFeedback, const char* feedback) {
        CoATest::Bytes b;
        b.U32(question).U8(hasChoice ? 1 : 0);

        if (hasChoice) {
            b.U32(choice);
        }

        b.U8(hasFeedback ? 1 : 0);

        if (hasFeedback) {
            b.Str(feedback);
        }

        return CoATest::Expected(0x747, b);
    }
}

// SMSG 0x746 replaces the list; PLAYER_POLL_NEW_POLL(first unanswered id) only when the flag goes 0 -> 1.
COA_TEST(PlayerPoll_ListAndNewPoll) {
    lua_State* L = Start();
    CHECK(Call(L, "HasUnansweredQuestions") == V{ "false" });
    Q a;
    a.id = 5;
    a.answered = true;
    a.answer = 11;
    Q b;
    b.id = 6;
    CHECK(List({ a, b }) == V{ "PLAYER_POLL_NEW_POLL:6" });
    CHECK(Call(L, "HasUnansweredQuestions") == V{ "true" });
    CHECK(Call(L, "GetNumQuestions") == V{ "2" });
    CHECK(List({ b }).empty());   // still unanswered: no event
    CHECK(List({ a }).empty());
    CHECK(Call(L, "HasUnansweredQuestions") == V{ "false" });
    CHECK(Call(L, "GetNumQuestions") == V{ "1" });
    // An optional-choice question with non-empty feedback counts as answered.
    Q c;
    c.id = 7;
    c.type = 4;
    c.hasFeedback = true;
    c.feedback = "fine";
    CHECK(List({ c }).empty());
    c.feedback = "";
    CHECK(List({ c }) == V{ "PLAYER_POLL_NEW_POLL:7" });
    CHECK(List({}).empty());
    CHECK(Call(L, "GetNumQuestions") == V{ "0" });
    Stop(L);
}

// SMSG 0x748: the result, then an optional question replacing its id or appended.
COA_TEST(PlayerPoll_SubmitResult) {
    lua_State* L = Start();
    Q a;
    a.id = 5;
    a.answered = true;
    List({ a });
    CHECK(Result("OK", nullptr) == V{ "PLAYER_POLL_ANSWER_RESULT:OK,0" });
    Q b;
    b.id = 9;
    CHECK((Result("NEW", &b) == V{ "PLAYER_POLL_NEW_POLL:9", "PLAYER_POLL_ANSWER_RESULT:NEW,9" }));
    CHECK(Call(L, "GetNumQuestions") == V{ "2" });
    b.answered = true;
    b.answer = 12;
    b.text = "Changed";
    CHECK(Result("DONE", &b) == V{ "PLAYER_POLL_ANSWER_RESULT:DONE,9" });
    CHECK(Call(L, "GetNumQuestions") == V{ "2" });
    CHECK(Call(L, "GetQuestionInfo", { "n:2" })[0] == "Changed");
    CHECK(Call(L, "HasUnansweredQuestions") == V{ "false" });
    // A present byte of 0: no question.
    CoATest::Bytes z;
    z.Str("Z").U8(0);
    CHECK(Dispatch(0x748, z) == V{ "PLAYER_POLL_ANSWER_RESULT:Z,0" });
    Stop(L);
}

COA_TEST(PlayerPoll_Readers) {
    lua_State* L = Start();
    Q a;
    a.answered = true;
    a.answer = 12;
    a.hasFeedback = true;
    a.feedback = "meh";
    a.has8C = true;
    a.s8C = "Extra";
    a.type = 3;
    Q b;
    b.id = 2;
    b.type = 7;
    List({ a, b });
    CHECK((Call(L, "GetQuestionInfo", { "n:1" }) == V{ "Question?", "Sub", "Extra", "meh", "Foot" }));
    CHECK((Call(L, "GetQuestionInfo", { "n:2" }) == V{ "Question?", "Sub", "nil", "nil", "Foot" }));
    CHECK(Call(L, "GetQuestionType", { "n:1" }) == V{ "PLAYER_POLL_RATING_SCALE" });
    CHECK(Call(L, "GetQuestionType", { "n:2" }) == V{ "PLAYER_POLL_CHOOSE_ONE" });   // past the table
    CHECK(Call(L, "GetNumQuestionChoices", { "n:1" }) == V{ "2" });
    CHECK((Call(L, "GetQuestionChoiceInfo", { "n:1", "n:1" }) == V{ "Yes", "false" }));
    CHECK((Call(L, "GetQuestionChoiceInfo", { "n:1", "n:2" }) == V{ "No", "true" }));
    CHECK((Call(L, "GetQuestionChoiceInfo", { "n:2", "n:2" }) == V{ "No", "false" }));
    // The original's Lua errors.
    CHECK(Call(L, "GetQuestionInfo", { "n:0" }) == V{ "<error> questionIndex must be greater than 0" });
    CHECK(Call(L, "GetQuestionInfo", { "n:3" }) == V{ "<error> questionIndex 3 is out of range" });
    CHECK(Call(L, "GetQuestionInfo") == V{ "<error> Usage: C_PlayerPoll.GetQuestionInfo(questionIndex)" });
    CHECK(Call(L, "GetQuestionType") == V{ "<error> Usage: C_PlayerPoll.GetQuestionType(questionIndex)" });
    CHECK(Call(L, "GetNumQuestionChoices") == V{ "<error> Usage: C_PlayerPoll.GetNumQuestionChoices(questionIndex)" });
    CHECK(Call(L, "GetNumQuestionChoices", { "n:9" }) == V{ "<error> questionIndex 9 is out of range" });
    CHECK(Call(L, "GetQuestionChoiceInfo", { "n:1" }) == V{ "<error> Usage: C_PlayerPoll.GetQuestionChoiceInfo(questionIndex, choiceIndex)" });
    CHECK(Call(L, "GetQuestionChoiceInfo", { "n:1", "n:0" }) == V{ "<error> choiceIndex must be greater than 0" });
    CHECK(Call(L, "GetQuestionChoiceInfo", { "n:1", "n:3" }) == V{ "<error> choiceIndex 3 is out of range" });
    CHECK(Call(L, "GetNumQuestions", { "n:1" }) == V{ "<error> C_PlayerPoll.GetNumQuestions takes no arguments" });
    CHECK(Call(L, "HasUnansweredQuestions", { "n:1" }) == V{ "<error> C_PlayerPoll.HasUnansweredQuestions takes no arguments" });
    CHECK(Call(L, "RequestQuestionList", { "n:1" }) == V{ "<error> C_PlayerPoll.RequestQuestionList takes no arguments" });
    CHECK(Call(L, "CanChangeQuestionChoice") == V{ "<error> Usage: C_PlayerPoll.CanChangeQuestionChoice(questionIndex)" });
    CHECK(Call(L, "CanChangeQuestionChoice", { "n:3" }) == V{ "<error> questionIndex 3 is out of range" });
    // CanChangeQuestionChoice: changeable, or not yet answered.
    CHECK(Call(L, "CanChangeQuestionChoice", { "n:1" }) == V{ "false" });
    CHECK(Call(L, "CanChangeQuestionChoice", { "n:2" }) == V{ "true" });
    a.changeable = true;
    List({ a });
    CHECK(Call(L, "CanChangeQuestionChoice", { "n:1" }) == V{ "true" });
    Stop(L);
}

// CanSubmitAnswer / SubmitAnswer: the same checks; only SubmitAnswer sends CMSG 0x747.
COA_TEST(PlayerPoll_Submit) {
    lua_State* L = Start();
    Q fresh;
    fresh.id = 40;
    Q done;
    done.id = 41;
    done.answered = true;
    done.answer = 11;
    done.changeable = true;
    Q locked = done;
    locked.id = 42;
    locked.changeable = false;
    Q optional;
    optional.id = 43;
    optional.type = 4;
    List({ fresh, done, locked, optional });
    V r;
    CHECK(Sent(L, "CanSubmitAnswer", { "n:1", "n:2" }, &r).empty());
    CHECK(r == V{ "true" });
    CHECK(Sent(L, "SubmitAnswer", { "n:1", "n:2" }, &r) == Packets{ Answer(40, true, 12, false, nullptr) });
    CHECK(r == V{ "true" });
    CHECK(Sent(L, "SubmitAnswer", { "n:1", "n:1", "s:why" }) == Packets{ Answer(40, true, 11, true, "why") });
    // No choice: only an optional-choice question with non-empty feedback.
    CHECK(Sent(L, "SubmitAnswer", { "n:1", "nil", "s:why" }, &r).empty());
    CHECK(r == V{ "false" });
    CHECK(Sent(L, "SubmitAnswer", { "n:4", "nil", "s:why" }) == Packets{ Answer(43, false, 0, true, "why") });
    CHECK(Sent(L, "SubmitAnswer", { "n:4", "nil", "s:" }, &r).empty());
    CHECK(r == V{ "false" });
    CHECK(Sent(L, "SubmitAnswer", { "n:4", "n:0", "s:x" }) == Packets{ Answer(43, false, 0, true, "x") });   // choice 0 = none
    // An answered question: only when changeable, and only with a different answer.
    CHECK(Sent(L, "SubmitAnswer", { "n:3", "n:2" }, &r).empty());
    CHECK(r == V{ "false" });
    CHECK(Sent(L, "SubmitAnswer", { "n:2", "n:1" }, &r).empty());   // the same choice, no feedback either time
    CHECK(r == V{ "false" });
    CHECK(Sent(L, "SubmitAnswer", { "n:2", "n:2" }) == Packets{ Answer(41, true, 12, false, nullptr) });
    CHECK(Sent(L, "SubmitAnswer", { "n:2", "n:1", "s:now" }) == Packets{ Answer(41, true, 11, true, "now") });
    // Feedback past 0x800 characters is an error before the question is looked at.
    CHECK(Sent(L, "SubmitAnswer", { "n:9", "n:1", "s:" + std::string(0x801, 'f') }, &r).empty());
    CHECK(r == V{ "<error> feedbackText must not exceed 2048 characters" });
    CHECK(Sent(L, "SubmitAnswer", { "n:1", "n:1", "s:" + std::string(0x800, 'f') }).size() == 1);
    CHECK(Call(L, "SubmitAnswer", { "n:9", "n:1" }) == V{ "<error> questionIndex 9 is out of range" });
    CHECK(Call(L, "SubmitAnswer", { "n:1", "n:3" }) == V{ "<error> choiceIndex 3 is out of range" });
    CHECK(Call(L, "SubmitAnswer") == V{ "<error> Usage: C_PlayerPoll.SubmitAnswer(questionIndex, choiceIndex?, feedbackText?)" });
    CHECK(Call(L, "CanSubmitAnswer", { "s:x" }) == V{ "<error> Usage: C_PlayerPoll.CanSubmitAnswer(questionIndex, choiceIndex?, feedbackText?)" });
    CHECK(Call(L, "CanSubmitAnswer", { "n:1", "s:x" }) == V{ "<error> Usage: C_PlayerPoll.CanSubmitAnswer(questionIndex, choiceIndex?, feedbackText?)" });
    CHECK(Call(L, "CanSubmitAnswer", { "n:0" }) == V{ "<error> questionIndex must be greater than 0" });
    Stop(L);
}

// Feedback compared too: an answered question with the same choice and new feedback may be resubmitted.
COA_TEST(PlayerPoll_SubmitFeedbackChange) {
    lua_State* L = Start();
    Q q;
    q.answered = true;
    q.answer = 11;
    q.changeable = true;
    q.hasFeedback = true;
    q.feedback = "old";
    List({ q });
    CHECK(Call(L, "CanSubmitAnswer", { "n:1", "n:1", "s:old" }) == V{ "false" });
    CHECK(Call(L, "CanSubmitAnswer", { "n:1", "n:1", "s:new" }) == V{ "true" });
    CHECK(Call(L, "CanSubmitAnswer", { "n:1", "n:1" }) == V{ "true" });   // feedback dropped
    Stop(L);
}

// RequestQuestionList and every world entry send CMSG 0x745; the glue screen drops the questions.
COA_TEST(PlayerPoll_RequestAndReset) {
    lua_State* L = Start();
    V r;
    CHECK(Sent(L, "RequestQuestionList", {}, &r) == Packets{ CoATest::Expected(0x745, CoATest::Bytes()) });
    CHECK(r == V{ "true" });
    CoATest::RecordPackets();
    CoA::ClientHooks::afterEnterWorld.Run();
    CoATest::StopRecordingPackets();
    CHECK(CoATest::SentPackets() == Packets{ CoATest::Expected(0x745, CoATest::Bytes()) });
    Q q;
    List({ q });
    CHECK(Call(L, "HasUnansweredQuestions") == V{ "true" });
    CoA::Lifecycle::RunGlueScreen();
    CHECK(Call(L, "GetNumQuestions") == V{ "0" });
    CHECK(Call(L, "HasUnansweredQuestions") == V{ "false" });
    CHECK(List({ q }) == V{ "PLAYER_POLL_NEW_POLL:1" });   // the flag was cleared too
    Stop(L);
}

// A count past the bytes left stops at the end.
COA_TEST(PlayerPoll_ShortPacket) {
    lua_State* L = Start();
    CoATest::Bytes b;
    b.U32(0xFFFFFFFF).U32(3);
    List({});
    CHECK(Dispatch(0x746, b) == V{ "PLAYER_POLL_NEW_POLL:3" });
    CHECK(Call(L, "GetNumQuestions") == V{ "1" });
    CHECK(Call(L, "GetNumQuestionChoices", { "n:1" }) == V{ "0" });
    Stop(L);
}
