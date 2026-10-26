// AscSmallHooks.cpp (see SmallHooks.h): the spell queue window and the bank equipment manager.
#include "CoA/Source/Modules/SmallHooks.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/CVars.h"
#include "CoA/Source/Runtime/ClientHooks.h"
#include "CoA/Source/Runtime/Packet.h"
#include "Engine/Source/ConsoleBase/ConsoleVar.h"
#include <cstring>

namespace {
    CoA::SmallHooks::Deps s_deps;
    CVar* s_spellQueueWindow = nullptr;   // 0x10BE4468, refilled by each world registration
    CVar* s_bankManager = nullptr;        // 0x10BE24E0

    // The CVar's int (+0x30); 0 before it is registered.
    bool BankManagerOn() {
        return s_bankManager && s_bankManager->m_intValue != 0;
    }

    // CMSG 0x55D: one byte, the bank equipment manager's state.
    void SendBankManager(uint8_t on) {
        CoA::Packet(0x55D).U8(on).Send();
    }
}

CoA::SmallHooks::Deps& CoA::SmallHooks::Dependencies() {
    return s_deps;
}

// FUN_10322440.
bool CoA::SmallHooks::SpellQueueReady() {
    uint32_t castEnd = 0;

    if (!s_deps.castEndTime || !s_deps.castEndTime(castEnd) || !castEnd) {
        return false;
    }

    const uint32_t window = s_spellQueueWindow ? static_cast<uint32_t>(s_spellQueueWindow->m_intValue) : 0;
    const uint32_t now = s_deps.now ? s_deps.now() : 0;

    if (castEnd <= now) {
        return true;
    }

    return castEnd - now > window;
}

// FUN_10228610: the GUID is written through NtProtectVirtualMemory in the original (it lies in the client's .data).
bool CoA::SmallHooks::BeforeUseEquipmentSet() {
    if (!BankManagerOn() || !s_deps.bankerGuid || s_deps.bankerGuid() != 0) {
        return false;
    }

    if (s_deps.setBankerGuid) {
        s_deps.setBankerGuid(1);
    }

    return true;
}

void CoA::SmallHooks::AfterUseEquipmentSet(bool wrote) {
    if (BankManagerOn() && wrote && s_deps.setBankerGuid) {
        s_deps.setBankerGuid(0);
    }
}

// FUN_102277e0: in world, 1 for "1", else 0.
bool CoA::SmallHooks::OnBankManager(CVar*, const char*, const char* value, void*) {
    if (s_deps.activePlayerGuid && s_deps.activePlayerGuid() != 0) {
        SendBankManager(value && strcmp(value, "1") == 0 ? 1 : 0);
    }

    return true;
}

// FUN_10227700.
void CoA::SmallHooks::BankManagerEnterWorld() {
    if (BankManagerOn()) {
        SendBankManager(1);
    }
}

void CoA::SmallHooks::Reset() {
    s_deps = Deps();
    s_spellQueueWindow = nullptr;
    s_bankManager = nullptr;
}

void CoA::SmallHooks::Init() {
    // FUN_103223a0.
    COA_CVAR("SpellQueueWindow", "Sets how early you can pre-activate/queue a spell/ability. (In Milliseconds)", 1, "400",
        nullptr, 3, false, &s_spellQueueWindow, true);
    // FUN_10227970.
    COA_CVAR("bankEquipmentManager", nullptr, 1, "0", OnBankManager, 4, false, &s_bankManager, true);
    CoA::ClientHooks::enterWorld.Add(BankManagerEnterWorld);   // FUN_10278510
}
