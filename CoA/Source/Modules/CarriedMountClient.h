#ifndef COA_SOURCE_MODULES_CARRIEDMOUNTCLIENT_H
#define COA_SOURCE_MODULES_CARRIEDMOUNTCLIENT_H

#include <cstdint>

class CGUnit_C;
class CM2Model;

// CarriedMount's client half: CGUnit_C::CreateUnitMount / DestroyUnitMount (0x73D5D0 / 0x73D370) ask it first. A carried
// mount hangs from the body's held grip; the unit counts as unmounted to the client (m_clientFlags 0x10000000), so it
// keeps its own run and stand animations while the server applies the mount's speed.
namespace CoA::CarriedMountClient {
    bool Carry(CGUnit_C* unit);
    bool Drop(CGUnit_C* unit);
    void Note(CGUnit_C* unit, const char* stage);
    bool CarryingAnim(CGUnit_C* unit, uint32_t anim, CM2Model* model, uint32_t* carrying);
}

#endif
