#include "WoW/Common/ComponentCore/EsteriaAppearance.h"

bool EsteriaRetailChoice(CCharacterComponent*, uint32_t, uint32_t&) { return false; }
bool EsteriaRetailStoreChoice(CCharacterComponent*, uint32_t, uint32_t) { return false; }
std::array<uint8_t, 13> EsteriaRetailFields(CCharacterComponent*) { return {}; }
void EsteriaRetailFields(CCharacterComponent*, const std::array<uint8_t, 13>&) {}

// CoA: stand-in for the Esteria tree's extension manager (see WoW/Source/Extension/ExtensionMgr.h).
#include "WoW/Source/Extension/ExtensionMgr.h"

static ExtensionMgr s_extensionMgr;
ExtensionMgr* sExtensionMgr = &s_extensionMgr;

// No extension channel yet (the server side of the retail form toggle is a later step).
bool ExtensionMgr::SendClientMessage(uint32_t, const void*, uint32_t) { return false; }
