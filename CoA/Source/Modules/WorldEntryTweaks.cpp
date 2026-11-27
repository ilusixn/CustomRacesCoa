#include "CoA/Source/Modules/WorldEntryTweaks.h"
#include "CoA/Source/Runtime/ClientHooks.h"
#include "CoA/Source/Runtime/Packet.h"

namespace {
    CoA::WorldEntryTweaks::Deps s_deps;
    bool s_skipAmmoCheck = false;
    bool s_skipProficiency = false;

    bool HasPlayer() {
        return s_deps.hasPlayer && s_deps.hasPlayer();
    }

    bool Stock() {
        return CoA::WorldEntryTweaks::StockClass(s_deps.playerClass ? s_deps.playerClass() : 0);
    }

    void StanceJump() {   // FUN_10a5dc40
        if (HasPlayer()) {
            s_skipProficiency = !Stock();
        }
    }

    void StanceForms() {   // FUN_10a60440: the warrior stances
        if (!HasPlayer() || !s_deps.setFormFlag) {
            return;
        }

        const bool on = !Stock();

        for (uint32_t form : { 0x11u, 0x12u, 0x13u }) {
            s_deps.setFormFlag(form, on);
        }
    }

    void Send561() {   // FUN_10a73b70: CMSG_EXTENSION_INITIALIZED
        CoA::Packet(0x561).U32(0).U32(1).Send();
    }

    void SkipCheck809572() {   // FUN_10a50e70
        if (HasPlayer() && !Stock()) {
            s_skipAmmoCheck = true;
        }
    }
}

CoA::WorldEntryTweaks::Deps& CoA::WorldEntryTweaks::Dependencies() {
    return s_deps;
}

bool CoA::WorldEntryTweaks::StockClass(uint8_t cls) {
    return (cls >= 1 && cls <= 9) || cls == 11;
}

void CoA::WorldEntryTweaks::OnEnteredWorld() {
    StanceJump();
    StanceForms();
    Send561();
    SkipCheck809572();
}

bool CoA::WorldEntryTweaks::SkipAmmoCheck() {
    return s_skipAmmoCheck;
}

bool CoA::WorldEntryTweaks::SkipProficiency() {
    return s_skipProficiency;
}

void CoA::WorldEntryTweaks::Reset() {
    s_deps = Deps();
    s_skipAmmoCheck = false;
    s_skipProficiency = false;
}

void CoA::WorldEntryTweaks::Init() {
    CoA::ClientHooks::afterEnterWorld.Add(OnEnteredWorld);
}
