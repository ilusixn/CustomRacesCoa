#ifndef COA_SOURCE_MODULES_CHATCHECK_H
#define COA_SOURCE_MODULES_CHATCHECK_H

// AscChatCheck.cpp: the global GetChatCheck (0x10192C40) over the Chat manager (static 0x10BDEB8C, FUN_101928b0): per
// chat type, a sorted set of phrases the server sends.
//   SMSG 0x928 CHAT_CHECK_ADD (FUN_101929e0): u32 type, u32 length + bytes (FUN_100d3680) with the last byte dropped
//        (the sender's NUL); inserted into the type's set.
//   SMSG 0x929 CHAT_CHECK_REMOVE (FUN_10192ac0): the same layout; erased.
// GetChatCheck(chatType, text): the chat type NAME maps to its id through the static table 0x10BDEB68 (FUN_1006c3b0's
// initializer list; unknown names give 0), and the answer is true when the text contains any of that type's phrases.
// Every check is dropped by SMSG_LOGIN_VERIFY_WORLD (FUN_101929d0 from the attach hook on 0x403DE0).
namespace CoA::ChatCheck {
    void Init();
    // Tests only.
    void Reset();
}

#endif
