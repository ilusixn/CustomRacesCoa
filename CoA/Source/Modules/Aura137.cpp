// AscAura137.cpp (see Aura137.h): turning while falling for aura type 0x137.
#include "CoA/Source/Modules/Aura137.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/ClientHooks.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Source/Runtime/Read.h"

namespace {
    CoA::Aura137::Deps s_deps;

    void SetPatch(bool on) {   // FUN_10a73ea0
        CoA::ClientHooks::aura137TurnWhileFalling = on;
    }

    bool PlayerAura137(void* unit, const void* spell) {
        return s_deps.isActivePlayer && s_deps.isActivePlayer(unit) && s_deps.hasAura137 && s_deps.hasAura137(spell);
    }
}

CoA::Aura137::Deps& CoA::Aura137::Dependencies() {
    return s_deps;
}

bool CoA::Aura137::BeforeAddAuraEffect(void* unit, uint32_t, const void* spell) {
    if (PlayerAura137(unit, spell)) {
        SetPatch(true);
    }

    return true;
}

bool CoA::Aura137::BeforeRemoveAuraEffect(void* unit, uint32_t, const void* spell) {
    if (PlayerAura137(unit, spell)) {
        SetPatch(false);
    }

    return true;
}

int32_t CoA::Aura137::OnOff(void*, uint32_t, uint32_t, CDataStore* msg) {
    static_cast<void>(CoA::Read::U64(msg));
    SetPatch(false);
    return 1;
}

int32_t CoA::Aura137::OnOn(void*, uint32_t, uint32_t, CDataStore* msg) {
    static_cast<void>(CoA::Read::U64(msg));
    SetPatch(true);
    return 1;
}

void CoA::Aura137::Init() {
    CoA::ClientHooks::addAuraEffect.Add(CoA::Aura137::BeforeAddAuraEffect, 0x10A683D0);
    CoA::ClientHooks::removeAuraEffect.Add(CoA::Aura137::BeforeRemoveAuraEffect, 0x10A683DA);
    COA_PACKET(0x6E8, CoA::Aura137::OnOff);
    COA_PACKET(0x6E9, CoA::Aura137::OnOn);
}

void CoA::Aura137::Reset() {
    s_deps = Deps();
    SetPatch(false);
}
