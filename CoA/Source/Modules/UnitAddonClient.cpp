// AscUnitAddon.cpp's client questions: the unit token (Script_GetGUIDFromToken 0x60ABF0) and the active player.
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Modules/UnitAddon.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"
#include "WoW/Source/Ui/ScriptEvents.h"

namespace {
    bool TokenGuid(const char* token, uint64_t& guid) {
        WOWGUID g;
        const bool found = Script_GetGUIDFromToken(token, g, false);
        guid = found ? g.m_guid : 0;
        return found;
    }

    uint64_t ActivePlayerGuid() {
        return ClntObjMgrGetActivePlayer().m_guid;
    }
}

void CoA::UnitAddonClient::Init() {
    CoA::UnitAddon::Deps& deps = CoA::UnitAddon::Dependencies();
    deps.tokenGuid = TokenGuid;
    deps.activePlayerGuid = ActivePlayerGuid;
}
