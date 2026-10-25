#include "CoA/Source/Runtime/ClientState.h"
#include "Engine/Source/BlizzardCore/SFile2-Core.h"
#include "Engine/Source/FrameScript/FrameScript.h"
#include "WoW/Source/Ui/GameUI.h"

// The helpers are named apart from CoA::ClientState's own functions: inside InstallClient an unqualified InWorld
// would name CoA::ClientState::InWorld, installing the probe as itself (endless recursion).
namespace {
    bool ClientInWorld() {
        return CGGameUI::s_inWorld != 0;
    }

    bool ClientCanPerformAction(int32_t action) {
        return CGGameUI::CanPerformAction(static_cast<uint32_t>(action)) != 0;
    }

    const char* ClientBasePath() {
        return SFile::GetDataPath();
    }
}

void CoA::ClientState::InstallClient() {
    CoA::ClientState::SetInWorldProbe(ClientInWorld);
    CoA::ClientState::SetActionProbe(ClientCanPerformAction);
    CoA::ClientState::SetMainLuaStateProbe(FrameScript_GetContext);
    CoA::ClientState::SetBasePathProbe(ClientBasePath);
}
