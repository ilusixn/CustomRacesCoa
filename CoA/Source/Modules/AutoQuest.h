#ifndef COA_SOURCE_MODULES_AUTOQUEST_H
#define COA_SOURCE_MODULES_AUTOQUEST_H

// AscAutoQuest.cpp: the auto-quest pop-ups -- the original's AutoQuest object (accessor FUN_100e6190, static
// 0x10bdbc18), a list of { type, questId } with type 0 = "OFFER", 1 = "COMPLETE". Lua (AddAutoQuestPopUp) and
// SMSG 0x927 (FUN_100e6220) fill it; SMSG_LOGIN_VERIFY_WORLD (the attach hook on 0x403DE0) empties it.
namespace CoA::AutoQuest {
    // FUN_100e6210: the list emptied.
    void Clear();

    void Init();
    // Tests only.
    void Reset();
}

#endif
