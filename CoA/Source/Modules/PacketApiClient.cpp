// AscPacketApi.cpp's client half: the send wrapper and the client's message table.
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Modules/PacketApi.h"
#include "CoA/Source/Runtime/ClientHooks.h"
#include "WoW/Source/Net/NetClient/NetClient.h"
#include "WoW/Source/WowSvcs/WowSvcsClient/ClientServices.h"

namespace {
    // The client's table takes its own handler type; FUN_1030f410 is the same function either way.
    int32_t StockHandler(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg) {
        return CoA::PacketApi::OnRegisteredPacket(param, static_cast<uint32_t>(msgId), time, msg);
    }

    // The registrar FUN_102c4590: FUN_102c4400 on the NetClient its constructor detour kept (0x632A40), which below
    // 0x55F is the client's SetMessageHandler (0x631FA0) on that object.
    void SetStockHandler(uint32_t opcode, CoA::PacketApi::Handler) {
        if (NetClient* net = static_cast<NetClient*>(CoA::ClientHooks::keptNetClient)) {
            net->SetMessageHandler(static_cast<NETMESSAGE>(opcode), StockHandler, nullptr);
        }
    }
}

void CoA::PacketApiClient::Init() {
    CoA::PacketApi::Deps& d = CoA::PacketApi::Dependencies();
    d.send = ClientServices::SendPacket;
    d.setStockHandler = SetStockHandler;
}
