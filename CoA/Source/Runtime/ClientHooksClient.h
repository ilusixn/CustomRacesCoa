#ifndef COA_SOURCE_RUNTIME_CLIENTHOOKSCLIENT_H
#define COA_SOURCE_RUNTIME_CLIENTHOOKSCLIENT_H

class CGObject_C;
class CGUnit_C;

// ClientHooks' parts that touch client objects; the decomp sites call these.
namespace CoA::ClientHooksClient {
    // The replaced CGObject_C::ClearEffectList(0) (0x10276310): only the effects ClientHooks::removeEffect votes out go
    // (MoveToDestroyList); the next one is read before the callbacks run.
    void FilterEffects(CGObject_C* object);

    // FUN_10111d20 (after CGGameUI::Target 0x524BF0 and OnReceiveMirrorImageDataFromServer 0x730290): a non-player
    // unit shown with another display id gets its cached portrait marked dirty and its UI model refreshed (flags 3).
    void RefreshMorphPortrait(CGUnit_C* unit);
}

#endif
