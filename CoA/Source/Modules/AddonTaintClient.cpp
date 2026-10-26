// AscAddonTaint.cpp's client half: the blocked-action feedback and the Engine hook pointers (issecure 0x816DA0,
// ProtectedFunctionsAllowed 0x488540, Script_AllowAttributeChanges 0x49F210). The WoW-side detours call
// CoA::AddonTaint directly.
#include "CoA/Source/Modules/AddonTaint.h"
#include "CoA/Source/Modules/Modules.h"
#include "Engine/Source/FrameScript/FrameScript.h"
#include "WoW/Source/Ui/GameUI.h"

namespace {
    void BlockedActionFeedback(uint32_t type) {   // 0x513530
        CGGameUI::ShowBlockedActionFeedback(nullptr, type);
    }
}

void CoA::AddonTaintClient::Init() {
    CoA::AddonTaint::Dependencies().blockedActionFeedback = BlockedActionFeedback;
    g_issecureHook = CoA::AddonTaint::Issecure;
    g_addonTaintSecureHook = CoA::AddonTaint::Secure;
    g_addonTaintBeginClearedHook = CoA::AddonTaint::BeginCleared;
    g_addonTaintEndClearedHook = CoA::AddonTaint::EndCleared;
}
