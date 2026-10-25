#ifndef COA_SOURCE_MODULES_BRIDGESTORECLIENT_H
#define COA_SOURCE_MODULES_BRIDGESTORECLIENT_H

// The DBC manager's client hook point (BridgeStoreClient.cpp), called from the decomp where the original hooks.
namespace CoA::BridgeStoreClient {
    // ClientDBInitialize (orig hook 0x634E00): after the client's tables, the bridge store's six, the HD model
    // tables written over the client's rows, then the bridge indexes.
    void AfterClientDBInitialize();
    // AnimationCountClient.cpp: FUN_10a51180, the set's first callback (the animation count's 44 sites).
    void ApplyAnimationCount();
}

#endif
