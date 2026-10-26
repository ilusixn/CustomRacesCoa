// AscPlayerPoll.cpp (see PlayerPoll.h): the poll questions, their packets and the C_PlayerPoll natives, transcribed as
// the decompile has them. The natives raise the original's Lua errors (luaL_error, 0x84F280).
#include "CoA/Source/Modules/PlayerPoll.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/ClientHooks.h"
#include "CoA/Source/Runtime/Events.h"
#include "CoA/Source/Runtime/Lifecycle.h"
#include "CoA/Source/Runtime/Logger.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Packet.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Source/Runtime/Read.h"
#include "CoA/Source/Runtime/Script.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <cstdio>
#include <string>
#include <vector>

using CoA::Script::CheckNumber;
using CoA::Script::CheckString;
using CoA::Script::PushBool;
using CoA::Script::PushInt;
using CoA::Script::PushStr;
using CoA::Script::ToInt;
using CoA::Script::ValidateInput;

namespace {
    struct Choice {
        uint32_t id = 0;
        std::string text;
    };

    struct Question {   // 0xB0
        uint32_t id = 0;
        uint32_t unk4 = 0;
        std::string text;    // +0x08
        std::string str20;   // +0x20
        std::string str38;   // +0x38
        uint32_t unk50 = 0;
        uint32_t unk54 = 0;
        std::vector<Choice> choices;   // +0x58
        bool answered = false;         // +0x68
        uint32_t answer = 0;           // +0x64
        bool hasFeedback = false;      // +0x84
        std::string feedback;          // +0x6C
        bool changeable = false;       // +0x88
        bool hasStr8C = false;
        std::string str8C;
        uint32_t unkA8 = 0;
        uint32_t type = 0;             // +0xAC

        // FUN_102d85f0 / FUN_102d7b30's "answered": a choice, or a non-empty feedback on an optional-choice question.
        bool Done() const {
            return this->answered || (this->type == 4 && this->hasFeedback && !this->feedback.empty());
        }
    };

    std::vector<Question> s_questions;
    bool s_unanswered = false;   // +0xC
    const uint32_t kMaxFeedback = 0x800;   // 0x10B55314

    bool OptStr(CDataStore* msg, std::string& out) {   // FUN_1023a8e0
        if (!CoA::Read::U8(msg)) {
            return false;
        }

        out = CoA::Read::Str(msg);
        return true;
    }

    Question ReadQuestion(CDataStore* msg) {   // FUN_102d6f80
        Question q;
        q.id = CoA::Read::U32(msg);
        q.unk4 = CoA::Read::U32(msg);
        q.text = CoA::Read::Str(msg);
        q.str20 = CoA::Read::Str(msg);
        q.str38 = CoA::Read::Str(msg);
        q.unk50 = CoA::Read::U32(msg);
        q.unk54 = CoA::Read::U32(msg);

        for (uint32_t n = CoA::Read::U32(msg); n && CoA::Read::Remaining(msg) != 0; --n) {   // FUN_102d6740
            Choice c;
            c.id = CoA::Read::U32(msg);
            c.text = CoA::Read::Str(msg);
            q.choices.push_back(c);
        }

        q.answered = CoA::Read::U8(msg) != 0;

        if (q.answered) {
            q.answer = CoA::Read::U32(msg);
        }

        q.hasFeedback = OptStr(msg, q.feedback);
        q.changeable = CoA::Read::U8(msg) != 0;
        q.hasStr8C = OptStr(msg, q.str8C);
        q.unkA8 = CoA::Read::U32(msg);
        q.type = CoA::Read::U32(msg);
        return q;
    }

    void Recompute(bool before) {
        s_unanswered = false;
        uint32_t first = 0;

        for (const Question& q : s_questions) {
            if (!q.Done()) {
                s_unanswered = true;
                first = q.id;
                break;
            }
        }

        if (!before && s_unanswered) {
            CoA::Events::Signal("PLAYER_POLL_NEW_POLL", "%u", first);
        }
    }

    int32_t OnPollList(void*, uint32_t opcode, uint32_t, CDataStore* msg) {   // 0x746 FUN_102d7d90
        const bool before = s_unanswered;
        std::vector<Question> list;

        for (uint32_t n = CoA::Read::U32(msg); n && CoA::Read::Remaining(msg) != 0; --n) {
            list.push_back(ReadQuestion(msg));
        }

        s_questions.swap(list);
        Recompute(before);
        char line[128];
        snprintf(line, sizeof(line), "PlayerPollMgr::HandlePlayerPollListOpcode: Received %u polls for opcode %u",
            static_cast<uint32_t>(s_questions.size()), opcode);
        CoA::Logger::WriteStamped(CoA::Logger::Debug, line);   // FUN_102936d0
        return 1;
    }

    int32_t OnSubmitResult(void*, uint32_t opcode, uint32_t, CDataStore* msg) {   // 0x748 FUN_102d8030
        const std::string result = CoA::Read::Str(msg);
        uint32_t pollId = 0;
        const bool before = s_unanswered;

        if (CoA::Read::Remaining(msg) != 0 && CoA::Read::U8(msg)) {
            const Question q = ReadQuestion(msg);
            pollId = q.id;
            bool replaced = false;

            for (Question& e : s_questions) {
                if (e.id == q.id) {
                    e = q;
                    replaced = true;
                    break;
                }
            }

            if (!replaced) {
                s_questions.push_back(q);
            }

            Recompute(before);
        } else {
            Recompute(true);
        }

        CoA::Events::Signal("PLAYER_POLL_ANSWER_RESULT", "%s%u", result.c_str(), pollId);
        CoA::Logger::WriteStamped(CoA::Logger::Debug, "PlayerPollMgr::HandlePlayerPollSubmitResultOpcode: result=" + result +
            " pollID=" + std::to_string(pollId) + " opcode=" + std::to_string(opcode));
        return 1;
    }

    // The question for a 1-based index, raising the original's Lua errors otherwise (luaL_error does not return).
    Question& QuestionAt(lua_State* L, uint32_t index) {
        if (index == 0) {
            luaL_error(L, "questionIndex must be greater than 0");
        }

        if (index - 1 >= s_questions.size()) {
            luaL_error(L, "%s", ("questionIndex " + std::to_string(index) + " is out of range").c_str());
        }

        return s_questions[index - 1];
    }

    uint32_t ChoiceIndex(lua_State* L, const Question& q, uint32_t index) {   // FUN_102d8700
        if (index == 0) {
            luaL_error(L, "choiceIndex must be greater than 0");
        }

        if (index - 1 >= q.choices.size()) {
            luaL_error(L, "%s", ("choiceIndex " + std::to_string(index) + " is out of range").c_str());
        }

        return index - 1;
    }

    struct SubmitArgs {
        uint32_t question = 0;
        bool hasChoice = false;
        uint32_t choice = 0;
        bool hasFeedback = false;
        std::string feedback;
    };

    // FUN_102d5db0: (number, number?, string?).
    bool ReadSubmitArgs(lua_State* L, SubmitArgs& a) {
        const int t2 = lua_type(L, 2) > 0 ? LUA_TNUMBER : LUA_TNIL;
        const int t3 = lua_type(L, 3) > 0 ? LUA_TSTRING : LUA_TNIL;

        if (!ValidateInput(L, { LUA_TNUMBER, t2, t3 })) {
            return false;
        }

        a.question = static_cast<uint32_t>(static_cast<int64_t>(CheckNumber(L, 1)));

        if (lua_type(L, 2) > 0) {
            a.hasChoice = true;
            a.choice = static_cast<uint32_t>(ToInt(CheckNumber(L, 2)));
        }

        if (lua_type(L, 3) > 0) {
            a.hasFeedback = true;
            a.feedback = CheckString(L, 3);
        }

        return true;
    }

    // FUN_102d7b30.
    bool CanSubmit(const Question& q, bool hasChoice, uint32_t choiceIndex, bool hasFeedback, const std::string& feedback) {
        if (q.Done() && !q.changeable) {
            return false;
        }

        if (hasFeedback && feedback.size() > kMaxFeedback) {
            return false;
        }

        uint32_t choiceId = 0;

        if (!hasChoice) {
            if (q.type != 4 || !hasFeedback || feedback.empty()) {
                return false;
            }
        } else {
            if (choiceIndex >= q.choices.size()) {
                return false;
            }

            choiceId = q.choices[choiceIndex].id;
        }

        const bool sameChoice = (q.answered && hasChoice) ? q.answer == choiceId : q.answered == hasChoice;

        if (!sameChoice) {
            return true;
        }

        const bool sameFeedback = (q.hasFeedback && hasFeedback) ? q.feedback == feedback : q.hasFeedback == hasFeedback;
        return !sameFeedback;
    }

    int CheckedSubmit(lua_State* L, bool send, const char* usage) {   // 0x102d8970 / 0x102d9520 -> FUN_102d97b0
        SubmitArgs a;

        if (!ReadSubmitArgs(L, a)) {
            return luaL_error(L, "%s", usage);
        }

        if (a.hasFeedback && a.feedback.size() > kMaxFeedback) {
            return luaL_error(L, "%s", ("feedbackText must not exceed " + std::to_string(kMaxFeedback) + " characters").c_str());
        }

        Question& q = QuestionAt(L, a.question);
        bool hasChoice = false;
        uint32_t choiceIndex = 0;

        if (a.hasChoice && a.choice != 0) {
            choiceIndex = ChoiceIndex(L, q, a.choice);
            hasChoice = true;
        }

        const bool ok = CanSubmit(q, hasChoice, choiceIndex, a.hasFeedback, a.feedback);

        if (ok && send) {
            CoA::Packet p(0x747);
            p.U32(q.id).U8(hasChoice ? 1 : 0);

            if (hasChoice) {
                p.U32(q.choices[choiceIndex].id);
            }

            p.U8(a.hasFeedback ? 1 : 0);

            if (a.hasFeedback) {
                p.Str(a.feedback.c_str());
            }

            p.Send();
        }

        PushBool(L, ok);
        return 1;
    }

    int CanSubmitAnswer(lua_State* L) {
        return CheckedSubmit(L, false, "Usage: C_PlayerPoll.CanSubmitAnswer(questionIndex, choiceIndex?, feedbackText?)");
    }

    int SubmitAnswer(lua_State* L) {
        return CheckedSubmit(L, true, "Usage: C_PlayerPoll.SubmitAnswer(questionIndex, choiceIndex?, feedbackText?)");
    }

    int CanChangeQuestionChoice(lua_State* L) {   // 0x102d87f0
        uint32_t index = 0;

        if (!CoA::Script::ReadNumber(L, index)) {
            return luaL_error(L, "Usage: C_PlayerPoll.CanChangeQuestionChoice(questionIndex)");
        }

        const Question& q = QuestionAt(L, index);
        PushBool(L, q.changeable || !q.Done());
        return 1;
    }

    int GetNumQuestionChoices(lua_State* L) {   // 0x102d8be0
        uint32_t index = 0;

        if (!CoA::Script::ReadNumber(L, index)) {
            return luaL_error(L, "Usage: C_PlayerPoll.GetNumQuestionChoices(questionIndex)");
        }

        PushInt(L, static_cast<int32_t>(QuestionAt(L, index).choices.size()));
        return 1;
    }

    int GetNumQuestions(lua_State* L) {   // 0x102d8d20
        if (lua_gettop(L) != 0) {
            return luaL_error(L, "C_PlayerPoll.GetNumQuestions takes no arguments");
        }

        PushInt(L, static_cast<int32_t>(s_questions.size()));
        return 1;
    }

    // 0x102d8d80: (text, isSelected).
    int GetQuestionChoiceInfo(lua_State* L) {
        if (!ValidateInput(L, { LUA_TNUMBER, LUA_TNUMBER })) {
            return luaL_error(L, "Usage: C_PlayerPoll.GetQuestionChoiceInfo(questionIndex, choiceIndex)");
        }

        const uint32_t qi = static_cast<uint32_t>(ToInt(CheckNumber(L, 1)));
        const uint32_t ci = static_cast<uint32_t>(ToInt(CheckNumber(L, 2)));
        const Question& q = QuestionAt(L, qi);
        const Choice& c = q.choices[ChoiceIndex(L, q, ci)];
        PushStr(L, c.text.c_str());
        PushBool(L, q.answered && q.answer == c.id);
        return 2;
    }

    // 0x102d8ff0: text, str20, str8C or nil, feedback or nil, str38.
    int GetQuestionInfo(lua_State* L) {
        uint32_t index = 0;

        if (!CoA::Script::ReadNumber(L, index)) {
            return luaL_error(L, "Usage: C_PlayerPoll.GetQuestionInfo(questionIndex)");
        }

        const Question& q = QuestionAt(L, index);
        PushStr(L, q.text.c_str());
        PushStr(L, q.str20.c_str());

        if (q.hasStr8C) {
            PushStr(L, q.str8C.c_str());
        } else {
            lua_pushnil(L);
        }

        if (q.hasFeedback) {
            PushStr(L, q.feedback.c_str());
        } else {
            lua_pushnil(L);
        }

        PushStr(L, q.str38.c_str());
        return 5;
    }

    // 0x102d91e0: PLAYER_POLL_* by type; an out-of-range type reads as index 1.
    int GetQuestionType(lua_State* L) {
        static const char* const kTypes[5] = { "NONE", "PLAYER_POLL_CHOOSE_ONE", "PLAYER_POLL_MULTIPLE_CHOICE",
            "PLAYER_POLL_RATING_SCALE", "PLAYER_POLL_OPTIONAL_CHOICE" };
        uint32_t index = 0;

        if (!CoA::Script::ReadNumber(L, index)) {
            return luaL_error(L, "Usage: C_PlayerPoll.GetQuestionType(questionIndex)");
        }

        const Question& q = QuestionAt(L, index);
        PushStr(L, kTypes[q.type <= 4 ? q.type : 1]);
        return 1;
    }

    int HasUnansweredQuestions(lua_State* L) {   // 0x102d9480
        if (lua_gettop(L) != 0) {
            return luaL_error(L, "C_PlayerPoll.HasUnansweredQuestions takes no arguments");
        }

        PushBool(L, s_unanswered);
        return 1;
    }

    int RequestQuestionList(lua_State* L) {   // 0x102d94d0
        if (lua_gettop(L) != 0) {
            return luaL_error(L, "C_PlayerPoll.RequestQuestionList takes no arguments");
        }

        CoA::Packet(0x745).Send();
        PushBool(L, true);
        return 1;
    }

    void ResetAll() {   // FUN_102d7ce0 (every glue screen)
        s_questions.clear();
        s_unanswered = false;
    }

    void RequestAfterEnterWorld() {   // FUN_102d8640: CMSG 0x745 through the static send 0x406F40
        CoA::Packet(0x745).Send();
    }

    const CoA::Binding s_bindings[] = {
        COA_NATIVE("C_PlayerPoll", "HasUnansweredQuestions", HasUnansweredQuestions),
        COA_NATIVE("C_PlayerPoll", "GetNumQuestions", GetNumQuestions),
        COA_NATIVE("C_PlayerPoll", "GetNumQuestionChoices", GetNumQuestionChoices),
        COA_NATIVE("C_PlayerPoll", "GetQuestionInfo", GetQuestionInfo),
        COA_NATIVE("C_PlayerPoll", "GetQuestionType", GetQuestionType),
        COA_NATIVE("C_PlayerPoll", "CanChangeQuestionChoice", CanChangeQuestionChoice),
        COA_NATIVE("C_PlayerPoll", "GetQuestionChoiceInfo", GetQuestionChoiceInfo),
        COA_NATIVE("C_PlayerPoll", "CanSubmitAnswer", CanSubmitAnswer),
        COA_NATIVE("C_PlayerPoll", "SubmitAnswer", SubmitAnswer),
        COA_NATIVE("C_PlayerPoll", "RequestQuestionList", RequestQuestionList),
    };
}

void CoA::PlayerPoll::Init() {   // 0x102d8530
    CoA::LuaReg::AddModule(s_bindings, sizeof(s_bindings) / sizeof(s_bindings[0]));
    COA_EVENT("PLAYER_POLL_NEW_POLL");
    COA_EVENT("PLAYER_POLL_ANSWER_RESULT");
    CoA::Lifecycle::OnGlueScreen(ResetAll);                        // FUN_10278760
    CoA::ClientHooks::afterEnterWorld.Add(RequestAfterEnterWorld);  // FUN_10278780
    COA_PACKET(0x746, OnPollList);
    COA_PACKET(0x748, OnSubmitResult);
}

void CoA::PlayerPoll::Reset() {
    ResetAll();
}
