#ifndef COA_SOURCE_MODULES_BRIDGESTORECLIENT_H
#define COA_SOURCE_MODULES_BRIDGESTORECLIENT_H

// The DBC manager's client hook point (BridgeStoreClient.cpp), called from the decomp where the original hooks.
namespace CoA::BridgeStoreClient {
    // ClientDBInitialize (orig hook 0x634E00): after the client's tables, the bridge store's six, the HD model
    // tables written over the client's rows, then the bridge indexes.
    void AfterClientDBInitialize();
}

#endif
