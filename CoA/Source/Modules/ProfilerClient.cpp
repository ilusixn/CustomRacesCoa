// AscProfiler.cpp's client half: the packet watchdog (installer FUN_102c3540) -- its thread (0x102C2ED0 ->
// FUN_102c4470), armed on entering the world, disarmed on leaving it and before CGGameUI::Shutdown, and the disconnect
// taken on the frame after ClientIdle (FUN_102c3510 -> ClientConnection::HandleDisconnect, 0x6B12D0).
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Modules/Profiler.h"
#include "CoA/Source/Runtime/ClientHooks.h"
#include "CoA/Source/Runtime/Lifecycle.h"
#include "WoW/Source/WowSvcs/WowSvcsClient/ClientServices.h"
#include <process.h>
#include <windows.h>

namespace {
    unsigned __stdcall Watchdog(void*) {
        for (;;) {
            CoA::Profiler::WatchdogCheck(GetTickCount64());
            Sleep(1000);
        }
    }

    void Arm() {
        CoA::Profiler::WatchdogArm(GetTickCount64());
    }

    void Disarm() {
        CoA::Profiler::WatchdogDisarm();
    }

    void DisconnectIfTripped() {
        ClientConnection* connection = ClientServices::Connection();

        // The flag stays set until there is a connection to drop (as the original).
        if (connection && CoA::Profiler::TakeWatchdogDisconnect()) {
            connection->HandleDisconnect();
        }
    }
}

void CoA::ProfilerClient::Init() {
    CloseHandle(reinterpret_cast<HANDLE>(_beginthreadex(nullptr, 0, &Watchdog, nullptr, 0, nullptr)));
    CoA::ClientHooks::enterWorld.Add(Arm);
    CoA::ClientHooks::leaveWorld.Add(Disarm);
    CoA::ClientHooks::beforeShutdown.Add(Disarm);
    CoA::Lifecycle::OnAfterIdle(DisconnectIfTripped);
}
