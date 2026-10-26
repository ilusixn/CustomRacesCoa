#ifndef COA_SOURCE_MODULES_ADDONTAINT_H
#define COA_SOURCE_MODULES_ADDONTAINT_H

#include <cstdint>

struct CVar;
struct lua_State;

// The AddOn-taint module (port of AscAddonTaint.cpp, installer FUN_10312150): around protected client functions, an
// addon the server lists as secure (SMSG 0x94E, GlobalsJ) runs as if it were Blizzard code.
//
// "Secure" is (the running taint 0xD4139C -- lua_tainted -- is set and listed secure) or SecureAddonMgr +0x10
// (0x10BE3054), which nothing writes (always false).
//
//   gate     0x5191C0 CanPerformAction, 0x488540 ProtectedFunctionsAllowed: secure -> allowed without the client
//   push     0x816DA0 issecure: secure -> pushes 1 and returns 1
//   cleared  0x49F210 0x510430 0x510450 0x510A00 0x51A7A0 0x51AC90 0x51FB80 0x51FF20 0x5252D0 0x527F00 0x52B470
//            0x52B4E0: secure -> the taint is cleared for the call; the saved one is stored back after, either way
//   0x525FC0 Script_ClearTarget inside Script_RunScript (GlobalsJ's flag 0x10BE3F60): 0x513530(0, 2) and 0 results
//   0x5F7E90 IsAddOnLoadable: a realm without addons (realm +0x48 clear) refuses an addon the server does not list;
//            called from 0x5F8071 / 0x5F80FA, loadUnknownAddOns 0 refuses the same. A refusal writes no reason.
//   0x524BF0 a target change runs with a secure addon's taint cleared (lists 0x10BE2950 / 0x10BE2970).
namespace CoA::AddonTaint {
    // The client questions; AddonTaintClient.cpp installs them.
    struct Deps {
        void (*blockedActionFeedback)(uint32_t type) = nullptr;   // 0x513530 CGGameUI::ShowBlockedActionFeedback(0, type)
    };

    Deps& Dependencies();

    // FUN_103115b0 / FUN_103119c0's test: the running addon is listed secure (or the never-set all-secure flag).
    bool Secure();

    // FUN_10313a50: true when it answered issecure() (pushed 1); false runs the client's body.
    bool Issecure(lua_State* L);

    // The twelve cleared hooks (FUN_10311ac0 ...): Begin returns the running taint and clears it when secure (through
    // FUN_10313b70); End stores the saved taint back. A Lua error out of the call skips End, as in the original.
    const char* BeginCleared();
    void EndCleared(const char* saved);

    // LAB_10313000: inside Script_RunScript, the blocked-action feedback (type 2) and true -- Script_ClearTarget returns
    // 0 without running; false runs the client's body.
    bool ClearTargetRefused();

    // FUN_10312630, every call: realm +0x48 clear and the addon not listed by SMSG 0x94E.
    bool AddOnLoadRefused(const char* name);
    // FUN_10312630, only from 0x5F8071 (LoadAddOnFileCount, the addon's own entry) and 0x5F80FA (LoadAddOn):
    // loadUnknownAddOns registered, its integer 0, and the addon not listed.
    bool UnknownAddOnLoadRefused(const char* name);
    // 0x10BE3F7C: the loadUnknownAddOns CVar, filled by its registration (GlueCVars).
    CVar** LoadUnknownAddOnsSlot();

    // FUN_10313d90 (list 0x10BE2950) / LAB_10313d70 (list 0x10BE2970): the running taint is kept in 0x10BE3F80 and
    // cleared when secure before a target change; stored back after, and the keep is zeroed.
    void BeforeTargetChange();
    void AfterTargetChange();

    // FUN_10312150's target-change registrations (the detours are the marked client edits).
    void Init();
    // Tests only.
    void Reset();
}

#endif
