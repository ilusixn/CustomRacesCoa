#ifndef COA_SOURCE_MODULES_REALMDATACLIENT_H
#define COA_SOURCE_MODULES_REALMDATACLIENT_H

#include <cstdint>

struct OS_FILE_DATA;

// The realm data module's client hook points (RealmDataClient.cpp), called from the decomp where the original hooks.
namespace CoA::RealmDataClient {
    // OpenArchives (orig hook 0x405DD0): after the client's archives, the realm's.
    void AfterOpenArchives();
    // InitializeAddOns (orig hook 0x5F8F50): after the client's scan, each realm archive's Interface\AddOns\ and
    // -addonspath <dir>, through the client's own callbacks (param is passed through; RescanAddOns collects with it).
    void AfterInitializeAddOns(int32_t (*archiveCallback)(const char* fileName, void* param), int32_t (*diskCallback)(OS_FILE_DATA& data, void* param), void* param = nullptr);
    // -addonspath <dir>, or nullptr.
    const char* AddOnsPath();
    // GetCacheDirectory (orig hook 0x667EF9): "Cache/WDB/<locale>/<realm>" into path (0x104 bytes).
    void CacheDirectory(char* path);
}

#endif
