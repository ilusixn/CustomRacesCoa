#ifndef COA_SOURCE_MODULES_CLIENTOPTIONSCLIENT_H
#define COA_SOURCE_MODULES_CLIENTOPTIONSCLIENT_H

#include <cstdint>

class C44Matrix;
class CGObject_C;
class CGUnit_C;

// AscClientOptions.cpp's client half: what its hooks read from client objects. The decomp sites call these.
namespace CoA::ClientOptionsClient {
    // sub_10a429a0, before 0x56C500 (CGChatBubbleFrame::Update): the bubble height for the unit.
    void BeforeChatBubbleUpdate(CGUnit_C* unit);
    // sub_10a44df0 over 0x744150 (BuildSelectionRotMatrix): true when the arrow matrix replaced the original.
    bool BuildArrowMatrix(const CGObject_C* object, C44Matrix* matrix);
    // sub_10a462f0 over 0x720330 (UpdateSelectionRadius): true when the radius was set in place of the original.
    bool UpdateSelectionRadius(CGUnit_C* unit);
    // FUN_10a468f0, after 0x71F990 (AddWorldText).
    void AfterAddWorldText(CGUnit_C* unit, uint32_t type);
}

#endif
