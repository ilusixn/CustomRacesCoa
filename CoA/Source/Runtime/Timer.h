#ifndef COA_SOURCE_RUNTIME_TIMER_H
#define COA_SOURCE_RUNTIME_TIMER_H

#include <cstdint>

// The client's timers as the DLL uses them (AscRuntime::Schedule / Cancel): ClientSetTimer (0x403370) and
// ClientKillTimer (0x4033B0). TimerClient.cpp installs them; without a backend (tests) Schedule returns 0.
namespace CoA::Timer {
    using Fn = int32_t (*)(const void* data, void* param);

    uint32_t Schedule(uint32_t ms, Fn fn, void* param);
    void Cancel(uint32_t id, Fn fn, void* param);

    void SetBackend(uint32_t (*schedule)(uint32_t ms, Fn fn, void* param), void (*cancel)(uint32_t id, Fn fn, void* param));
    void InstallClient();
}

#endif
