// AscGMTicket.cpp's client question: the active player's name (ClntObjMgrGetActivePlayerObj 0x4038F0, vtable +0xD8).
#include "CoA/Source/Modules/GMTicket.h"
#include "CoA/Source/Modules/Modules.h"
#include "WoW/Source/Object/ObjectClient/Player_C.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"

namespace {
    // The original builds a std::string from the name; a null name is read as "".
    const char* ActivePlayerName() {
        CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();

        if (!player) {
            return nullptr;
        }

        const char* name = player->GetObjectName();
        return name ? name : "";
    }
}

void CoA::GMTicketClient::Init() {
    CoA::GMTicket::Dependencies().activePlayerName = ActivePlayerName;
}
