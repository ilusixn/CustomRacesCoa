#ifndef COA_SOURCE_RUNTIME_VISUALS_H
#define COA_SOURCE_RUNTIME_VISUALS_H

// The graphics pack, chosen before the device exists: "-visuals <0-3>" or Visuals.txt beside the exe.
// See docs/2026-11-15-visuals-switch.md.
namespace CoA::Visuals {
    enum Choice : unsigned {
        Stock = 0,
        Lumenveil = 1,
        LuxVelum = 2,
        AzerothRemastered = 3,
    };

    // The pack this run started with; fixed until restart.
    unsigned Current();
    // What the next start will use (command line, else Visuals.txt).
    unsigned Pending();
    // Writes Visuals.txt for the next start. False when the file cannot be written.
    bool Set(unsigned choice);
    void Start();
}

#endif
