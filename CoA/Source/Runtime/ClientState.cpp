#include "CoA/Source/Runtime/ClientState.h"

namespace {
    bool (*s_inWorld)() = nullptr;
}

bool CoA::ClientState::InWorld() {
    return s_inWorld && s_inWorld();
}

void CoA::ClientState::SetInWorldProbe(bool (*probe)()) {
    s_inWorld = probe;
}
