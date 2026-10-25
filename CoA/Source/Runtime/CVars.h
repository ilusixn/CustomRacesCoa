#ifndef COA_SOURCE_RUNTIME_CVARS_H
#define COA_SOURCE_RUNTIME_CVARS_H

#include <cstddef>
#include <cstdint>

struct CVar;

namespace CoA {
    // One DLL CVar, as its registrar call passes it to CVar::Register (0x767FC0). `slot` receives the CVar* at every
    // registration (0x10114090); code reads the slot, never a pointer it looked up once.
    struct CVarEntry {
        const char* name;
        const char* help;
        uint32_t flags;
        const char* defaultValue;
        bool (*callback)(CVar* var, const char* oldValue, const char* value, void* arg);
        uint32_t category;
        bool setCommand;   // the registrar's last argument (1 only for useAlphaAmbienceSound)
        CVar** slot;
    };
}

// The DLL's two CVar lists (port of the scaffold's CVar.cpp and AscClientOptions' queue).
namespace CoA::CVars {
    // FUN_101145c0 (glue vector, world = false) / FUN_10114540 (world queue, world = true); called from module Init
    // in the original's order.
    void Queue(const CoA::CVarEntry& entry, bool world);
    // A client module attaches its callback to a queued entry before registration (the original registers each with
    // its callback; here the callbacks live in client-only files). False when no entry has the name.
    bool SetCallback(const char* name, bool (*callback)(CVar* var, const char* oldValue, const char* value, void* arg));
    // Tests only.
    void Reset();

    // FUN_10114180, the DLL's detour on ClientRegisterConsoleCommands (0x401B60): after the client's own glue
    // CVars, every glue entry.
    void RegisterGlue();
    // After the client's world CVar registration CGGameUI::RegisterGameCVars (0x51D9B0, from world UI load):
    // every world entry, each time the world loads. Per-character ones (flags 0x21) are deleted by the client at
    // logout (CVar::DeleteCharacterCVars) and come back here.
    void RegisterWorld();

    const CoA::CVarEntry* Glue(size_t& count);
    const CoA::CVarEntry* World(size_t& count);
}

// Queues a DLL CVar; Tools/coa/inventory.py counts these.
#define COA_CVAR(name, help, flags, def, callback, category, setCommand, slot, world) \
    CoA::CVars::Queue(CoA::CVarEntry { (name), (help), (flags), (def), (callback), (category), (setCommand), (slot) }, (world))

#endif
