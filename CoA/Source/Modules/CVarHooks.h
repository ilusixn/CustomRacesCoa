#ifndef COA_SOURCE_MODULES_CVARHOOKS_H
#define COA_SOURCE_MODULES_CVARHOOKS_H

#include <cstdint>

// The CVar module's client hooks (port of AscCVarHooks.cpp; installer FUN_101141e0) and the login-state hooks of
// AscSmallHooks.cpp (FUN_101952e0). The client calls these at the original hook points (CVarHooksClient.cpp).
namespace CoA::CVarHooks {
    // CCharacterSelection::UpdateCharacterList (0x4E4610): before the body, WTF\Account\<account>\config-cache.wtf is
    // loaded when an account name is known; after it, on the first call only, "-character <name>" selects that
    // character and enters the world.
    void BeforeUpdateCharacterList();
    void AfterUpdateCharacterList();

    // SaveGameCVarsCallback (0x512890): an invalid file handle is refused with a Fatal log line; after a save of
    // category bit 0x10, lastCharacterIndex is written as the character-select index.
    void InvalidSaveFile();
    void AfterSaveGameCVars(void* file, uint32_t category);

    // CVGxResolutionCallback (0x76A220): the loading screen's two aspect values follow the resolution string.
    void BeforeResolution(const char* resolution);

    // ClientServices::RealmEnumCallback (0x6B2F60): while the realmList CVar has no value, the login-server-type
    // test takes the realm-join branch; after the callback, once per process, "-realm <name>" selects that realm.
    bool RealmListMissing();
    void AfterRealmEnumCallback();
}

#endif
