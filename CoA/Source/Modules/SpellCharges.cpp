// AscSpellCharges.cpp (see SpellCharges.h): the charge store behind SMSG 0x9C2..0x9C5 and GetSpellCharges.
#include "CoA/Source/Modules/SpellCharges.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/ClientHooks.h"
#include "CoA/Source/Runtime/ClientState.h"
#include "CoA/Source/Runtime/Dbc.h"
#include "CoA/Source/Runtime/Events.h"
#include "CoA/Source/Runtime/Lifecycle.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Source/Runtime/Read.h"
#include "CoA/Source/Runtime/Script.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <unordered_map>

namespace {
    using CoA::Dbc::Table;

    struct Charges {
        uint32_t used = 0;       // +0x0C
        uint32_t next = 0;       // +0x10
        uint32_t start = 0;      // +0x14
        uint32_t duration = 0;   // +0x18
    };

    CoA::SpellCharges::Deps s_deps;
    std::unordered_map<uint32_t, Charges> s_charges;   // 0x10BE43E4

    uint32_t Now() {
        return s_deps.now ? s_deps.now() : 0;
    }

    void Updated() {
        if (CoA::ClientState::InWorld()) {   // *0xBD0792
            CoA::Events::Signal("SPELL_UPDATE_CHARGES", nullptr);
        }
    }

    const uint8_t* CategoryRow(uint32_t id) {
        return CoA::Dbc::Get("DBFilesClient\\SpellChargesCategory.dbc").Row(id);
    }

    // FUN_103204c0 GetSpellCharges(spell): charges left, max, start and duration (seconds) -- max, max, 0 and the
    // category's recharge without an entry; 0, 0, 0, 0 for a non-number or a spell without charges.
    int GetSpellCharges(lua_State* L) {
        if (lua_isnumber(L, 1)) {
            const uint32_t spell = static_cast<uint32_t>(CoA::Script::ToInt(CoA::Script::CheckNumber(L, 1)));

            if (const uint8_t* charges = CoA::Dbc::Get("DBFilesClient\\SpellCharges.dbc").Row(spell)) {
                const uint32_t category = Table::U32(charges, 4);

                if (const uint8_t* row = CategoryRow(category)) {
                    const int32_t max = Table::I32(row, 4);
                    auto it = s_charges.find(Table::U32(row, 0));
                    int32_t duration;

                    if (it == s_charges.end()) {
                        CoA::Script::PushInt(L, max);
                        CoA::Script::PushInt(L, max);
                        CoA::Script::PushNum(L, 0.0);
                        duration = Table::I32(row, 8);
                    } else {
                        CoA::Script::PushInt(L, max - static_cast<int32_t>(it->second.used));
                        CoA::Script::PushInt(L, max);
                        CoA::Script::PushNum(L, static_cast<double>(it->second.start) * 0.001);
                        duration = static_cast<int32_t>(it->second.next - it->second.start);
                    }

                    // The original converts the duration as unsigned.
                    CoA::Script::PushNum(L, static_cast<double>(static_cast<uint32_t>(duration)) * 0.001);
                    return 4;
                }
            }
        }

        for (int i = 0; i < 4; ++i) {
            CoA::Script::PushInt(L, 0);
        }

        return 4;
    }

    const CoA::Binding s_bindings[] = {   // registrar 0x10320B00
        COA_NATIVE("", "GetSpellCharges", GetSpellCharges),
    };
}

CoA::SpellCharges::Deps& CoA::SpellCharges::Dependencies() {
    return s_deps;
}

int32_t CoA::SpellCharges::OnClear(void*, uint32_t, uint32_t, CDataStore*) {   // handler_0x09c2
    s_charges.clear();
    Updated();
    return 1;
}

int32_t CoA::SpellCharges::OnRemove(void*, uint32_t, uint32_t, CDataStore* msg) {   // FUN_10320160
    s_charges.erase(CoA::Read::U32(msg));
    Updated();
    return 1;
}

// FUN_10320230: the reads are unchecked in the original; a short packet reads zeros and the count loop stops when the
// packet is used up.
int32_t CoA::SpellCharges::OnList(void*, uint32_t, uint32_t, CDataStore* msg) {
    for (uint32_t n = CoA::Read::U32(msg); n && CoA::Read::Remaining(msg) != 0; --n) {
        const uint32_t category = CoA::Read::U32(msg);
        const int32_t remaining = CoA::Read::I32(msg);
        const uint8_t used = CoA::Read::U8(msg);
        const uint8_t* row = CategoryRow(category);

        if (!row || remaining == 0 || Table::U32(row, 4) == 0) {
            continue;
        }

        const uint32_t next = Now() + static_cast<uint32_t>(remaining);
        const uint32_t duration = Table::U32(row, 8);
        const uint32_t start = Now();
        Charges& c = s_charges[category];
        c.used = used;
        c.next = next;
        c.start = start;
        c.duration = duration;
    }

    Updated();
    return 1;
}

int32_t CoA::SpellCharges::OnUpdate(void*, uint32_t, uint32_t, CDataStore* msg) {   // FUN_10320350
    const uint32_t category = CoA::Read::U32(msg);
    const uint32_t ms = CoA::Read::U32(msg);
    const uint32_t used = CoA::Read::U32(msg);
    const uint8_t* row = CategoryRow(category);

    if (!row || ms == 0 || Table::U32(row, 4) == 0) {
        return 1;
    }

    if (used == 0) {
        s_charges.erase(category);
    } else {
        auto it = s_charges.find(category);

        if (it == s_charges.end()) {
            Charges c;   // FUN_10320030(used, next, duration): start = now
            c.used = used;
            c.next = Now() + ms;
            c.start = Now();
            c.duration = Table::U32(row, 8);
            s_charges[category] = c;
        } else {
            it->second.used = used;
            it->second.next = Now() + ms;
        }
    }

    Updated();
    return 1;
}

void CoA::SpellCharges::Tick() {
    const uint32_t now = Now();

    for (auto it = s_charges.begin(); it != s_charges.end();) {
        if (now < it->second.next) {
            ++it;
            continue;
        }

        if (--it->second.used == 0) {
            it = s_charges.erase(it);
        } else {
            it->second.start = Now();
            it->second.next = it->second.duration + it->second.start;
            ++it;
        }

        Updated();
    }
}

void CoA::SpellCharges::EnterWorld() {
    s_charges.clear();
}

void CoA::SpellCharges::Init() {
    COA_PACKET(0x9C2, CoA::SpellCharges::OnClear);
    COA_PACKET(0x9C3, CoA::SpellCharges::OnRemove);
    COA_PACKET(0x9C4, CoA::SpellCharges::OnList);
    COA_PACKET(0x9C5, CoA::SpellCharges::OnUpdate);
    COA_EVENT("SPELL_UPDATE_CHARGES");
    CoA::ClientHooks::enterWorld.Add(CoA::SpellCharges::EnterWorld);
    CoA::Lifecycle::OnAfterIdle(CoA::SpellCharges::Tick);
    CoA::LuaReg::AddModule(s_bindings, sizeof(s_bindings) / sizeof(s_bindings[0]));
}

void CoA::SpellCharges::Reset() {
    s_deps = Deps();
    s_charges.clear();
}
