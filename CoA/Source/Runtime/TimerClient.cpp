#include "CoA/Source/Runtime/Timer.h"
#include "WoW/Source/Client.h"

// Named apart from CoA::Timer's own functions (inside InstallClient an unqualified Schedule would name
// CoA::Timer::Schedule).
namespace {
    uint32_t ClientSchedule(uint32_t ms, CoA::Timer::Fn fn, void* param) {
        return ClientSetTimer(ms, fn, param);
    }

    // The DLL passes its param where the client's ClientKillTimer takes a name; only the id and handler match.
    void ClientCancel(uint32_t id, CoA::Timer::Fn fn, void* param) {
        ClientKillTimer(id, fn, static_cast<const char*>(param));
    }
}

void CoA::Timer::InstallClient() {
    CoA::Timer::SetBackend(ClientSchedule, ClientCancel);
}
