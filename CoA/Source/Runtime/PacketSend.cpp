#include "CoA/Source/Runtime/Packet.h"
#include "WoW/Source/WowSvcs/WowSvcsClient/ClientServices.h"

// Finalize (0x401130), then 0x6B0B50: the current connection's Send.
void CoA::Packet::Send() {
    this->m_store.Finalize();
    ClientServices::Send2(&this->m_store);
}
