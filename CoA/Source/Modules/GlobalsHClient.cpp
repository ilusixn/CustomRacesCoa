// AscGlobalsH.cpp's native over client objects: AbandonSkillID.
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Packet.h"
#include "CoA/Source/Runtime/Script.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Source/DB/Rec/SkillLineRec.h"
#include "WoW/Source/Object/ObjectClient/Player_C.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"

namespace {
    // CMSG_UNLEARN_SKILL (0x202) for a SkillLine the player has a slot for (CGPlayer_C::GetSkillIndex, 0x6DC1C0).
    int AbandonSkillID(lua_State* L) {
        const int32_t id = CoA::Script::ToInt(lua_tonumber(L, 1));

        if (g_skillLineDB.GetRecord(id)) {
            if (CGPlayer_C* player = ClntObjMgrGetActivePlayerObj()) {
                if (player->GetSkillIndex(id) != -1) {
                    CoA::Packet(0x202).U32(static_cast<uint32_t>(id)).Send();
                }
            }
        }

        return 0;
    }

    const CoA::Binding s_bindings[] = {
        COA_NATIVE("", "AbandonSkillID", AbandonSkillID),
    };
}

void CoA::GlobalsHClient::Init() {
    CoA::LuaReg::AddModule(s_bindings, sizeof(s_bindings) / sizeof(s_bindings[0]));
}
