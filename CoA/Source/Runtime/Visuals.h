#ifndef COA_SOURCE_RUNTIME_VISUALS_H
#define COA_SOURCE_RUNTIME_VISUALS_H

// Which graphics mod the client honours: Stock, Lumenveil or LuxVelum. Chosen with "-visuals <0|1|2>" or the first
// number in Visuals.txt beside the exe (the launcher/add-on writes it); without either, LuxVelum stays the default, as
// before the choice existed. Start() loads the chosen host at startup (WinMain).
namespace CoA::Visuals {
    enum Choice : unsigned {
        Stock = 0,
        Lumenveil = 1,
        LuxVelum = 2,
    };

    unsigned Current();
    // Writes Visuals.txt; the choice applies on the next start. False when the file cannot be written.
    bool Set(unsigned choice);
    void Start();
}

#endif
