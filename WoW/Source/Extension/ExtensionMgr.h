#ifndef WOW_SOURCE_EXTENSION_EXTENSIONMGR_H
#define WOW_SOURCE_EXTENSION_EXTENSIONMGR_H

// CoA: stand-in for the Esteria tree's extension manager (the retail adapter calls a few hooks on it). Only the calls the ported
// code makes exist; file hooks do nothing, and client messages go to the CoA channel when one is registered.
#include <cstddef>
#include <cstdint>

class ExtensionMgr {
public:
    bool OnFileOpen(const char*, char*, size_t) { return false; }
    void OnFileOpened(const char*, void*) {}
    // Client -> server extension message (opcode id + payload); CoA: sent through CoA::Esteria when wired, dropped otherwise.
    bool SendClientMessage(uint32_t id, const void* data, uint32_t size);
};

extern ExtensionMgr* sExtensionMgr;

#endif
