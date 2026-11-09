#ifndef COA_SOURCE_MODULES_JOINEDNAMESCLIENT_H
#define COA_SOURCE_MODULES_JOINEDNAMESCLIENT_H

class CGObject_C;
class CGUnit_C;
class PLAYERNAMEDESC;

// JoinedNames' client half: what the decomp sites call.
namespace CoA::JoinedNamesClient {
    // CGUnit_C::GetUnitChosenTitle (0x72A290): the unit's own name, or "Melee'Ranged" for a seated Two-Players body.
    const char* BodyName(CGUnit_C* unit);
    // PlayerNameUpdateDrawCallback (0x7E6390): rebuilds the name over a body when its rider sits down or leaves.
    void RefreshName(CGObject_C* object, PLAYERNAMEDESC* desc);
}

#endif
