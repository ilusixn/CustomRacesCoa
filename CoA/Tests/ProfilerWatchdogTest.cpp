#include "CoA/Source/Modules/Profiler.h"
#include "CoA/Source/Runtime/Logger.h"
#include "CoA/Tests/Support.h"
#include "CoA/Tests/TestMain.h"

// FUN_102c4470: 30 s without a received packet trips the watchdog once; the disconnect is taken on the next frame.
COA_TEST(Profiler_WatchdogDisconnectsAfter30sSilence) {
    CoATest::TempCwd cwd;
    CoA::Logger::Init();
    CoA::Profiler::WatchdogDisarm();
    CoA::Profiler::WatchdogArm(1000);

    CHECK(!CoA::Profiler::WatchdogCheck(30999));
    CHECK(!CoA::Profiler::TakeWatchdogDisconnect());
    CHECK(CoA::Profiler::WatchdogCheck(31000));
    CHECK(CoA::Profiler::TakeWatchdogDisconnect());
    CHECK(!CoA::Profiler::TakeWatchdogDisconnect());
    // Tripped: disarmed until the next world entry.
    CHECK(!CoA::Profiler::WatchdogCheck(90000));

    // A received packet restarts the 30 s.
    CoA::Profiler::WatchdogArm(1000);
    CoA::Profiler::PacketReceivedAt(20000);
    CHECK(!CoA::Profiler::WatchdogCheck(49999));
    CHECK(CoA::Profiler::WatchdogCheck(50000));

    // Not armed (glue): never.
    CoA::Profiler::WatchdogDisarm();
    CoA::Profiler::PacketReceivedAt(1);
    CHECK(!CoA::Profiler::WatchdogCheck(100000));
    CoA::Logger::Shutdown();
}
