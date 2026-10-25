#ifndef COA_SOURCE_MODULES_ATTACHMISC_H
#define COA_SOURCE_MODULES_ATTACHMISC_H

#include "CoA/Source/Runtime/ClientHooks.h"
#include <cstdint>

// AscAttachMisc.cpp: the attach init's second hook installer (FUN_10a66950), seven hooks as marked source edits.
namespace CoA::AttachMisc {
    // 0x7668C0 (CVar::Set): scriptProfile can only be set to "0" (any value becomes "0"); others pass unchanged.
    const char* CVarSetValue(const char* name, const char* value);
    // 0x7668C0: the UI_INFO_MESSAGE text key posted when nameplateShowEnemies is set to value; nullptr for any other.
    const char* NameplateMessage(const char* name, const char* value);

    // FUN_10a79130 (0x81AC90 vFrameScript_SignalEvent): an id past the client's event table, or with no entry, is
    // refused and logged to the Fatal channel; format and va are logged as pointers.
    bool AllowFormattedSignal(int32_t eventId, uint32_t registeredCount, bool hasEventData, bool hasEventObject,
        const void* format, const void* va);

    // 0x403DE0 (SMSG_LOGIN_VERIFY_WORLD), before: an empty CMSG 0x53B, then the auto-quest pop-ups and the chat checks
    // are emptied (their modules register here).
    inline CoA::ClientHooks::Hooks<> loginVerifyWorld;
    void LoginVerifyWorld();

    // 0x5B3020 / 0x5B32F0 / 0x5B3610 (achievement packets), after: the challenge data stores are rebuilt (the
    // challenge module registers here).
    inline CoA::ClientHooks::Hooks<> achievementsChanged;

    // FUN_10a771c0 (0x808200 Spell_C_SpellFailed): a failure 0x55 of a spell with Attributes 0x404, AttributesEx 0x200
    // or AttributesEx2 0x100000 asks for the melee swing to be toggled when it is not on.
    bool StopsSwing(uint32_t attributes, uint32_t attributesEx, uint32_t attributesEx2, int32_t reason);
}

#endif
