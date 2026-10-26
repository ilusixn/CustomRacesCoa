#ifndef COA_SOURCE_MODULES_NAMEPLATESCLIENT_H
#define COA_SOURCE_MODULES_NAMEPLATESCLIENT_H

#include <cstdint>

class C3Vector;
class CGNamePlateFrame;
class CGUnit_C;
class CGWorldFrame;
struct WOWGUID;

// AscNamePlates.cpp's client half: what its detours read from client objects. The decomp sites call these.
namespace CoA::NamePlatesClient {
    // FUN_102be570, after 0x98E9F0 (CGNamePlateFrame::OnLayerUpdate).
    void AfterLayerUpdate(CGNamePlateFrame* frame);
    // FUN_102c2630, before 0x72B060 (CGUnit_C::ShouldShowNamePlate): true when it decided (result set).
    bool BeforeShouldShow(CGUnit_C* unit, CGWorldFrame* worldFrame, C3Vector* position, int32_t& result);
    // sub_102be530 / FUN_102c2e90 (0x715C30 / 0x98E910): the focused plate (0xCA1204) is the player's own.
    bool IsOwnFocus();
    // FUN_10a4ea40 before 0x60ABF0 (Script_GetGUIDFromToken): a registered "nameplateN" token's unit GUID.
    bool TokenGuid(const char* token, WOWGUID& guid);
}

#endif
