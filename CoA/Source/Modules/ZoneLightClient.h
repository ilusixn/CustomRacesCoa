#ifndef COA_SOURCE_MODULES_ZONELIGHTCLIENT_H
#define COA_SOURCE_MODULES_ZONELIGHTCLIENT_H

class C3Vector;

// AscZoneLight.cpp's client half: the detour bodies at the marked DayNight sites (see ZoneLight.h).
namespace CoA::ZoneLightClient {
    void AfterLoadMap();                             // FUN_10a4b030, after 0x7F2790
    void AfterFindZoneLight(const C3Vector* position);   // FUN_10a4b880, after 0x77EED0
    void AfterSetPlanets();                          // sub_10a4baa0, after 0x7EECC0
    void AfterSetColors();                           // sub_10a4ba70, after 0x7F3230
    void AfterRenderGlares();                        // sub_10a4ba30, after 0x7F0870
}

#endif
