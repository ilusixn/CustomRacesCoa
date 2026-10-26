#ifndef COA_SOURCE_MODULES_PLAYERPOLL_H
#define COA_SOURCE_MODULES_PLAYERPOLL_H

// AscPlayerPoll.cpp: C_PlayerPoll (10) over PlayerPollMgr (FUN_102d8590 -> static 0x10BE37DC; module init 0x102d8530):
// the questions (+0, 0xB0 each) and the has-unanswered flag (+0xC).
//
// Question on the wire (FUN_102d6f80): u32 id, u32, str text, str, str, u32, u32, u32 n x {u32 id, str text} choices,
// u8 answered [u32 choice id], opt str feedback, u8 changeable, opt str, u32, u32 type (0..4, PLAYER_POLL_*).
//   SMSG 0x746 PLAYER_POLL_LIST (FUN_102d7d90): u32 n x question, replacing the list.
//   SMSG 0x748 PLAYER_POLL_SUBMIT_RESULT (FUN_102d8030): str result, then (while bytes remain) u8 present + question,
//        which replaces the question with its id or is appended; PLAYER_POLL_ANSWER_RESULT("%s%u", result, poll id or 0).
// Either packet fires PLAYER_POLL_NEW_POLL("%u", first unanswered id) when the flag goes 0 -> 1.
// RequestQuestionList: CMSG 0x745 (also sent after every world entry, FUN_102d8640). SubmitAnswer: CMSG 0x747 {u32
// question id, u8 has choice [u32 choice id], u8 has feedback [str feedback]}.
namespace CoA::PlayerPoll {
    void Init();
    // Tests only.
    void Reset();
}

#endif
