#ifndef COA_SOURCE_MODULES_PROFILER_H
#define COA_SOURCE_MODULES_PROFILER_H

#include <cstdint>

// The network profiler -- LogOpcodes / ResetOpcodeLogging / SetOpcodeLoggingEnabled and their signal-event
// siblings (0x102D35A0..0x102D5CD0); port of AscProfiler.cpp.
//
// With opcode logging on, every packet received (NetClient::ProcessMessage) and sent (NetClient::Send) is counted in
// a map keyed "<name> (0x%08X)" / "SENT <name> (0x%08X)". LogOpcodes writes the table to the Trace channel, sorted by
// bytes/s descending, then the warnings pass, then resets. Set*LoggingEnabled(on, periodic) set the flag, cancel the
// two shared timers, re-arm both at 5 s when on and periodic (each callback dumps and schedules itself again WITHOUT
// storing the new handle, so a later cancel misses it -- kept), and reset. Nothing ever fills the signal-event map,
// so LogSignalEvents writes nothing.
namespace CoA::Profiler {
    // FUN_102c3ab0's receive side, for every packet: remembers the opcode and, while logging, counts it. Returns the
    // start time for Handled (0 while not logging).
    int64_t Received(int32_t opcode, uint32_t size);
    void Handled(int32_t opcode, int64_t start);
    // FUN_102c3ff0 around NetClient::Send: counted before the send, timed after.
    int64_t SendStarted(int32_t opcode, uint32_t size);
    void SendFinished(int32_t opcode, int64_t start);
    // The opcode being dispatched (DAT_10BE3728).
    int32_t CurrentOpcode();

    // The packet watchdog (installer FUN_102c3540): armed on entering the world, disarmed on leaving it and before
    // CGGameUI::Shutdown. Received stamps every packet (PacketReceivedAt with GetTickCount64). A thread checks every
    // second (WatchdogCheck, FUN_102c4470): armed, not yet tripped, with a stamp, and 30 s or more since it -- the
    // Error channel line, tripped, disarmed, and a disconnect flagged that the next frame after ClientIdle takes.
    void WatchdogArm(uint64_t now);
    void WatchdogDisarm();
    void PacketReceivedAt(uint64_t now);
    bool WatchdogCheck(uint64_t now);
    bool TakeWatchdogDisconnect();

    // Tests: the received record for an opcode.
    uint32_t Count(int32_t opcode);
    uint32_t Bytes(int32_t opcode);
    void Reset();
}

#endif
