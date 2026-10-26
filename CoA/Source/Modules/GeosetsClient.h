#ifndef COA_SOURCE_MODULES_GEOSETSCLIENT_H
#define COA_SOURCE_MODULES_GEOSETSCLIENT_H

class CM2Model;
class CreatureStats_C;

// AscGeosets.cpp's client half: the detour bodies the client calls at its marked sites (see Geosets.h).
namespace CoA::GeosetsClient {
    void AfterApplyMonsterGeosets(CM2Model* model);   // 0x101A12D0, after 0x4E7790
    void AfterShowCharacter();                        // 0x101A12F0, after 0x4E3CD0
    void AfterSetUpModel(CreatureStats_C* stats);     // 0x101A1340, after 0x597700
}

#endif
