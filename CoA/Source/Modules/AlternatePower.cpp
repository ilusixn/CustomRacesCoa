// AscAlternatePower.cpp (see AlternatePower.h): the alternate power event, the jump-cast spell and the forced death.
#include "CoA/Source/Modules/AlternatePower.h"
#include "CoA/Source/Modules/CA.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Modules/ObjectAddon.h"
#include "CoA/Source/Runtime/ClientHooks.h"
#include "CoA/Source/Runtime/Dbc.h"
#include "CoA/Source/Runtime/Events.h"
#include <vector>

namespace {
    CoA::AlternatePower::Deps s_deps;
    uint32_t s_flagSpell = 0;   // 0x10BE4454 (written only)
    uint32_t s_jumpSpell = 0;   // 0x10BE4458

    // LAB_10321bb0: the event with field 0 of the unit's record.
    void AlternatePowerChanged(uint64_t guid, uint32_t, void*, CoA::ObjectAddon::Field*) {
        const uint32_t type = CoA::ObjectAddon::Unit(guid)[0].value;
        CoA::Events::Signal("UNIT_POWER_ALTERNATIVE_UPDATE", "%u", type);
    }

    // 0x10A4BAF0: field 4 == 1 kills the unit.
    void ForcedDeath(uint64_t guid, uint32_t, void*, CoA::ObjectAddon::Field*) {
        if (CoA::ObjectAddon::Unit(guid)[4].value != 1) {
            return;
        }

        if (s_deps.forceDeath) {
            s_deps.forceDeath(guid);
        }
    }

    bool CustomAttr14(uint32_t spell, uint32_t mask) {   // FUN_10324a10
        const uint8_t* row = CoA::CA::SpellCustomAttrRow(spell);
        return row && (CoA::Dbc::Table::U32(row, 0x14) & mask) != 0;
    }
}

CoA::AlternatePower::Deps& CoA::AlternatePower::Dependencies() {
    return s_deps;
}

// FUN_10321b60: FUN_102ce900(guid, kind 4, field 0 / 1, arg 4, LAB_10321bb0).
void CoA::AlternatePower::WatchAlternatePower(void* player) {
    if (!s_deps.objectGuid) {
        return;
    }

    std::vector<CoA::ObjectAddon::Field>& fields = CoA::ObjectAddon::Unit(s_deps.objectGuid(player));

    for (uint32_t index = 0; index < 2; ++index) {
        fields[index].callback = AlternatePowerChanged;
        fields[index].arg = reinterpret_cast<void*>(4);
    }
}

// 0x10A3AB50: FUN_102ce930(kind 3, field 4, arg 4, 0x10A4BAF0).
void CoA::AlternatePower::RegisterForcedDeath(void*) {
    CoA::ObjectAddon::RegisterTypeCallback(3, 4, ForcedDeath, reinterpret_cast<void*>(4));
}

void CoA::AlternatePower::AddedSpell(uint32_t spell) {   // FUN_10322270
    if (CustomAttr14(spell, 0x1000)) {
        s_flagSpell = spell;
    }

    if (CustomAttr14(spell, 0x2000)) {
        s_jumpSpell = spell;
    }
}

void CoA::AlternatePower::KeyDown(const void* event) {   // FUN_103222b0
    if (*static_cast<const uint32_t*>(event) != 0x20 || s_jumpSpell == 0) {
        return;
    }

    if (!s_deps.hasPlayer || !s_deps.hasPlayer()) {
        return;
    }

    const bool exists = s_deps.spellExists && s_deps.spellExists(s_jumpSpell);

    if (s_deps.hasAura && s_deps.hasAura(s_jumpSpell)) {
        if (exists && s_deps.cancelAura) {
            s_deps.cancelAura(s_jumpSpell);
        }

        return;
    }

    if (!exists || !s_deps.falling || !s_deps.falling()) {
        return;
    }

    if (s_deps.castOnSelf) {
        s_deps.castOnSelf(s_jumpSpell);
    }
}

uint32_t CoA::AlternatePower::FlagSpell() {
    return s_flagSpell;
}

uint32_t CoA::AlternatePower::JumpSpell() {
    return s_jumpSpell;
}

void CoA::AlternatePower::Init() {
    COA_EVENT("UNIT_POWER_ALTERNATIVE_UPDATE");
    CoA::ClientHooks::afterActivePlayerInit.Add(CoA::AlternatePower::WatchAlternatePower);   // FUN_10321bf0
    CoA::ClientHooks::afterAddKnownSpell.Add(CoA::AlternatePower::AddedSpell);               // FUN_10322230
    CoA::ClientHooks::beforeKeyDown.Add(CoA::AlternatePower::KeyDown);
    CoA::ClientHooks::afterActivePlayerInit.Add(CoA::AlternatePower::RegisterForcedDeath);   // FUN_10a63e24
}

void CoA::AlternatePower::Reset() {
    s_deps = Deps();
    s_flagSpell = 0;
    s_jumpSpell = 0;
}
