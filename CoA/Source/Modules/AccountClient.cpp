#include "CoA/Source/Modules/Account.h"
#include "WoW/Source/Object/Movement.h"
#include "WoW/Source/Ui/ChatFrame.h"
#include <cfloat>

namespace {
    // FUN_1008c2b0's patches, as source variables: the float at 0xA32904 becomes FLT_MAX and the conditional jumps
    // at 0x50D641 / 0x50D2B7 become jmp.
    void ApplyGmStatus() {
        g_maxShortRangeTeleportSquared = FLT_MAX;
        g_chatGmChecksSkipped = 1;
    }
}

void CoA::Account::InstallClient() {
    CoA::Account::SetGmEffect(ApplyGmStatus);
}
