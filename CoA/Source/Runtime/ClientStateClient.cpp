#include "CoA/Source/Runtime/ClientState.h"
#include "WoW/Source/Ui/GameUI.h"

namespace {
    bool InWorld() {
        return CGGameUI::s_inWorld != 0;
    }
}

void CoA::ClientState::InstallClient() {
    CoA::ClientState::SetInWorldProbe(InWorld);
}
