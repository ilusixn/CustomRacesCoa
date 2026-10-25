#include "CoA/Source/Runtime/Packet.h"
#include "WoW/Source/WowSvcs/WowSvcsClient/ClientServices.h"

namespace {
    // 0x6B0B50: the current connection's Send.
    void SendToServer(CDataStore* msg) {
        ClientServices::Send2(msg);
    }
}

void CoA::Packet::InstallClient() {
    CoA::Packet::SetSender(SendToServer);
}
