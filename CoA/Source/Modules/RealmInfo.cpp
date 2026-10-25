#include "CoA/Source/Modules/RealmInfo.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/Events.h"
#include "CoA/Source/Runtime/Log.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Source/Runtime/Read.h"

namespace {
    CoA::RealmInfo::Info s_info;
    void (*s_dataPathHandler)(const char* path) = nullptr;
}

const CoA::RealmInfo::Info& CoA::RealmInfo::Get() {
    return s_info;
}

void CoA::RealmInfo::SetDataPathHandler(void (*handler)(const char* path)) {
    s_dataPathHandler = handler;
}

void CoA::RealmInfo::Reset() {
    s_info = CoA::RealmInfo::Info();
}

// SMSG_REALM_INFO 0x9BC -- FUN_102fc6c0. Realm object FUN_1008e2d0: +4 id (0xB also sets the sticky +0x49 Area-52
// flag), +8 ruleset, +0xC..+0x14 rates, +0x18 maintenance, +0x1C/+0x20 rates, +0x24 max deposit, +0x40..+0x47
// gates, data path, realm name, +0x48 flag, +0x64 an optional trailing u32.
int32_t CoA::RealmInfo::OnRealmInfo(void* param, uint32_t opcode, uint32_t time, CDataStore* msg) {
    CoA::RealmInfo::Info r;
    r.field0 = CoA::Read::U32(msg);
    r.ruleset = CoA::Read::U32(msg);
    r.rateXpQuest = CoA::Read::F32(msg);
    r.rate1 = CoA::Read::F32(msg);
    r.rate2 = CoA::Read::F32(msg);
    r.flag14 = CoA::Read::U32(msg);
    r.rate3 = CoA::Read::F32(msg);
    r.rateAuctionDepositVanity = CoA::Read::F32(msg);
    r.maxAuctionDepositVanity = CoA::Read::U32(msg);

    for (uint8_t& gate : r.gates) {
        gate = CoA::Read::U8(msg);
    }

    CoA::Read::CStr(msg, r.dataPath, sizeof(r.dataPath));
    CoA::Read::CStr(msg, r.realmName, sizeof(r.realmName));
    r.trailingFlag = CoA::Read::U8(msg);

    // The last u32 is read only when bytes remain (+0x64 keeps 0 otherwise).
    if (CoA::Read::Remaining(msg)) {
        r.field64 = CoA::Read::U32(msg);
    }

    r.visitedArea52 = s_info.visitedArea52 || r.field0 == 0xB;
    r.received = 1;
    s_info = r;

    // The tail, in its order: a non-empty data path that differs from the current one starts the realm data
    // hot-swap (FUN_102fac20; the handler compares against the current path); then a set maintenance flag fires
    // REALM_ALERT_MAINTENANCE.
    if (r.dataPath[0] && s_dataPathHandler) {
        s_dataPathHandler(r.dataPath);
    }

    if (r.flag14) {
        CoA::Events::Signal("REALM_ALERT_MAINTENANCE", nullptr);
    }

    CoA::Log::Printf("SMSG_REALM_INFO(0x%X): field0=%u ruleset=%u rates=%.4f/%.4f/%.4f/%.4f auctionDeposit=%.4f(max %u) "
        "gates=%d%d%d%d%d%d%d%d realm='%s' trailingFlag=%d field64=%u",
        opcode, r.field0, r.ruleset, r.rateXpQuest, r.rate1, r.rate2, r.rate3, r.rateAuctionDepositVanity,
        r.maxAuctionDepositVanity, r.gates[0], r.gates[1], r.gates[2], r.gates[3], r.gates[4], r.gates[5],
        r.gates[6], r.gates[7], r.realmName, r.trailingFlag, r.field64);
    return 1;
}

void CoA::RealmInfo::Init() {
    COA_PACKET(0x9BC, CoA::RealmInfo::OnRealmInfo);
    COA_EVENT("REALM_ALERT_MAINTENANCE");
}
