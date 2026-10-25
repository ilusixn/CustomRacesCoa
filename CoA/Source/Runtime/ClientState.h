#ifndef COA_SOURCE_RUNTIME_CLIENTSTATE_H
#define COA_SOURCE_RUNTIME_CLIENTSTATE_H

// Client state the DLL reads directly (fixed exe globals in the original). The probes are installed by
// ClientStateClient.cpp; without one (tests) the answer is the default given.
namespace CoA::ClientState {
    // CGGameUI::s_inWorld (0xBD0792); default false.
    bool InWorld();
    void SetInWorldProbe(bool (*probe)());

    void InstallClient();
}

#endif
