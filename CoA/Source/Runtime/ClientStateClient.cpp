#include "CoA/Source/Runtime/ClientState.h"
#include "Engine/Source/BlizzardCore/SFile2-Core.h"
#include "Engine/Source/FrameScript/FrameScript.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Source/DB/Rec/ItemDisplayInfoRec.h"
#include "WoW/Source/DB/Rec/ItemRec.h"
#include "WoW/Source/DB/Rec/SpellRec.h"
#include "WoW/Source/Ui/GameUI.h"
#include "WoW/Source/WowSvcs/WowSvcsClient/ClientServices.h"

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

    uint32_t ClientItemDisplayId(uint32_t item) {
        const ItemRec* rec = g_itemDB.GetRecord(static_cast<int32_t>(item));
        return rec ? static_cast<uint32_t>(rec->m_displayInfoID) : 0;
    }

    // 0x4CFD90 on ItemDisplayInfo (0xAD3DDC): the row is copied out; its strings point into the table.
    const char* ClientItemIcon(uint32_t item) {
        const ItemRec* rec = g_itemDB.GetRecord(static_cast<int32_t>(item));
        ItemDisplayInfoRec display;

        if (!rec || !g_itemDisplayInfoDB.GetRecord(rec->m_displayInfoID, &display)) {
            return nullptr;
        }

        return display.m_inventoryIcon[0];
    }

    uint8_t ClientCharacterClass() {
        return ClientServices::GetCharacterClass();
    }

    uint8_t ClientCharacterRace() {
        return ClientServices::GetCharacterRace();
    }

    int32_t ClientSpellMechanic(uint32_t spell) {
        SpellRec rec;
        return g_spellDB.GetRecord(static_cast<int32_t>(spell), &rec) ? rec.m_mechanic : -1;
    }
}

void CoA::ClientState::InstallClient() {
    CoA::ClientState::SetInWorldProbe(ClientInWorld);
    CoA::ClientState::SetActionProbe(ClientCanPerformAction);
    CoA::ClientState::SetMainLuaStateProbe(FrameScript_GetContext);
    CoA::ClientState::SetBasePathProbe(ClientBasePath);
    CoA::ClientState::Probes().itemDisplayId = ClientItemDisplayId;
    CoA::ClientState::Probes().itemIcon = ClientItemIcon;
    CoA::ClientState::Probes().characterClass = ClientCharacterClass;
    CoA::ClientState::Probes().characterRace = ClientCharacterRace;
    CoA::ClientState::Probes().spellMechanic = ClientSpellMechanic;
}
