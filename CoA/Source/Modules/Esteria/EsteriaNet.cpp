// Client side of the Esteria extension messages (ported from Extensions/BodyShapeSync): body sliders, the full Retail look and the
// Two Forms commit acknowledgement travel as custom opcodes. The server of this realm sends them (SMSG 0x523 / 0x524 / 0x526); the form
// commit goes back as 0x525.
#include "CoA/Source/Modules/EsteriaRaces.h"
#include "Engine/Source/Base/CDataStore.h"
#include "Engine/Source/Retail/BodyShape.h"
#include "Engine/Source/Retail/RetailLookWire.h"
#include "WoW/Common/ComponentCore/RetailRace.h"
#include "WoW/Source/Extension/ExtensionMgr.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"
#include "WoW/Source/WowSvcs/WowSvcsClient/ClientServices.h"
#include <cstdio>
#include <cstring>

namespace {
constexpr uint32_t SMSG_ESTERIA_BODY_SHAPE = 0x575;   // 0x51F..0x55E have no handler table in this client, so these sit above 0x55E (free in Ascension's table)
constexpr uint32_t SMSG_ESTERIA_RETAIL_LOOK = 0x576;
constexpr uint32_t SMSG_ESTERIA_FORM_LOOK_ACK = 0x577;

int32_t ReceiveBody(void*, NETMESSAGE, uint32_t, CDataStore* message) {
    { char line[96]; std::snprintf(line, sizeof(line), "recv 0x575 size=%u read=%u", message ? message->m_size : 0u, message ? message->m_read : 0u); BodyShape::Log(line); }
    constexpr uint32_t Size = 8 + CharacterBodyShape::PayloadSize;
    if (!message || message->m_read > message->m_size || message->m_size - message->m_read != Size || !message->FetchRead(message->m_read, Size)) {
        return 0;
    }
    uint8_t bytes[Size];
    message->GetData(bytes, Size);
    uint64_t guid;
    std::memcpy(&guid, bytes, sizeof(guid));
    CharacterBodyShape::Values wire{};
    if (!CharacterBodyShape::ReadPayload(bytes + 8, CharacterBodyShape::PayloadSize, wire)) {
        return 0;
    }
    BodyShape::Values values = BodyShape::FromWire(wire);
    BodyShape::SetUnit(guid, values);
    if (guid == ClntObjMgrGetActivePlayer().m_guid) {
        BodyShape::Local() = values;
    }
    return 1;
}

int32_t ReceiveLook(void*, NETMESSAGE, uint32_t, CDataStore* message) {
    { char line[96]; std::snprintf(line, sizeof(line), "recv 0x576 size=%u read=%u", message ? message->m_size : 0u, message ? message->m_read : 0u); BodyShape::Log(line); }
    constexpr uint32_t Size = 8 + CharacterRetailLook::PayloadSize;
    if (!message || message->m_read > message->m_size || message->m_size - message->m_read != Size || !message->FetchRead(message->m_read, Size)) {
        return 0;
    }
    uint8_t bytes[Size];
    message->GetData(bytes, Size);
    uint64_t guid;
    std::memcpy(&guid, bytes, sizeof(guid));
    CharacterRetailLook::Look look;
    if (!CharacterRetailLook::ReadPayload(bytes + 8, CharacterRetailLook::PayloadSize, look)) {
        return 0;
    }
    char line[120];
    std::snprintf(line, sizeof(line), "look received: guid=%llx race=%u sex=%u options=%u", static_cast<unsigned long long>(guid),
        unsigned(look.race), unsigned(look.sex), unsigned(look.count));
    BodyShape::Log(line);
    RetailRaceSetLook(guid, look);
    return 1;
}

int32_t ReceiveFormAck(void*, NETMESSAGE, uint32_t, CDataStore* message) {
    { char line[96]; std::snprintf(line, sizeof(line), "recv 0x577 size=%u read=%u", message ? message->m_size : 0u, message ? message->m_read : 0u); BodyShape::Log(line); }
    constexpr uint32_t Size = 8 + 4 + 1;
    if (!message || message->m_read > message->m_size || message->m_size - message->m_read != Size) {
        return 0;
    }
    uint64_t guid = 0;
    uint32_t revision = 0;
    uint8_t result = 0;
    message->Get(guid);
    message->Get(revision);
    message->Get(result);
    RetailFormCommitAck(guid, revision, result);
    return 1;
}
}

namespace CoA::Esteria {
void NetInit() {
    ClientServices::SetMessageHandler(static_cast<NETMESSAGE>(SMSG_ESTERIA_BODY_SHAPE), ReceiveBody, nullptr);
    ClientServices::SetMessageHandler(static_cast<NETMESSAGE>(SMSG_ESTERIA_RETAIL_LOOK), ReceiveLook, nullptr);
    ClientServices::SetMessageHandler(static_cast<NETMESSAGE>(SMSG_ESTERIA_FORM_LOOK_ACK), ReceiveFormAck, nullptr);
}

void LeaveWorld() {
    BodyShape::Units().clear();
    RetailRaceClearLooks();
    BodyShape::Local() = BodyShape::Values();
    CoA::Esteria::BodySliders::ForgetLoaded();
}
}

// Client -> server extension message: the 32-bit opcode, then the payload bytes.
bool ExtensionMgr::SendClientMessage(uint32_t id, const void* data, uint32_t size) {
    CDataStore msg;
    msg.Put(id);
    msg.PutData(data, size);
    msg.Finalize();
    // The command manager's connection is not set before the world is entered (character screen), the current one is.
    ClientConnection* connection = ClientServices::s_currentConnection;
    if (!connection) return false;
    connection->Send(&msg);
    return true;
}
