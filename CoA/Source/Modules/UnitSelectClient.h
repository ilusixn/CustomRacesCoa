#ifndef COA_SOURCE_MODULES_UNITSELECTCLIENT_H
#define COA_SOURCE_MODULES_UNITSELECTCLIENT_H

class CGUnit_C;

// AscUnitSelect.cpp's client half: what the decomp sites call.
namespace CoA::UnitSelectClient {
    // FUN_102da6c0, replacing 0x72B7E0 (CGUnit_C::Render, a jmp to 0x7275C0): returns true (it did the call).
    bool RenderUnit(CGUnit_C* unit);
    // FUN_102da340 before 0x727400 (CGPlayer_C::ClickToMove), with ClickToMove's own return address.
    void BeforeClickToMove(const void* returnAddress);
}

#endif
