#ifndef COA_SOURCE_RUNTIME_LIFECYCLE_H
#define COA_SOURCE_RUNTIME_LIFECYCLE_H

// The DLL's client lifecycle callback lists (port of AscRuntime's On* lists). Each list runs at the point in the
// client the original's cave or detour sits; the client calls Run* there.
namespace CoA::Lifecycle {
    using Callback = void (*)();

    // The cave on CGlueMgr::Resume's epilogue (0x4DA9AC): every time a glue screen has finished loading.
    void OnGlueScreen(Callback cb);
    void RunGlueScreen();

    // AscRuntime's list after ClientIdle (0x403340): every client idle tick.
    void OnAfterIdle(Callback cb);
    void RunAfterIdle();

    // Tests only.
    void Reset();
}

#endif
