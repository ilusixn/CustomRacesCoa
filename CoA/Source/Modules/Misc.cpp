// Loose globals from the original's misc-bindings block (handlers at 0x10a4xxxx..0x10a7xxxx); port of AscMisc.cpp.
// Each function names its original. The natives over client objects are in MiscClient.cpp.
#include "CoA/Source/Modules/Misc.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Modules/RealmInfo.h"
#include "CoA/Source/Runtime/Events.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Packet.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Source/Runtime/Read.h"
#include "CoA/Source/Runtime/Script.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <cstring>
#include <ctime>
#include <vector>

namespace {
    using CoA::Script::PushBool;
    using CoA::Script::PushInt;

    uint8_t s_taxiEarlyLanding = 0;        // DAT_10d3d748
    uint8_t s_friendStatusOffline = 0;     // DAT_10d3d74a
    uint32_t s_lootSpecialization = 0;     // DAT_10d3d7d8
    int32_t s_serverTimeOffset = 0;        // DAT_10d3d80c
    std::vector<uint32_t> s_zoneInstances; // DAT_10d3d814
    uint32_t s_zoneInstanceIndex = 0;      // DAT_10d3d820
    int32_t s_gossipIcons[32] = {};        // 0x10d3d758
    uint8_t s_checkpointsKnown = 0;        // DAT_10bdb5ec
    uint8_t s_checkpointsEnabled = 0;      // DAT_10bdb5ed
    uint8_t s_bankKnown = 0;               // DAT_10bdb5ef
    uint8_t s_bankA = 0;                   // DAT_10bdb5fc
    uint8_t s_bankB = 0;                   // DAT_10bdb5fd

    int32_t ArgInt(lua_State* L, int idx) {
        return CoA::Script::ToInt(CoA::Script::CheckNumber(L, idx));
    }

    // ---- friend status ------------------------------------------------------------------------------------------
    // FUN_10a4cdb0: remember the choice and tell the server, CMSG 0x671 { u8 }.
    int FriendStatusSetOffline(lua_State* L) {
        s_friendStatusOffline = lua_toboolean(L, 1) != 0;
        CoA::Packet(0x671).U8(s_friendStatusOffline).Send();
        return 0;
    }

    int FriendStatusIsOffline(lua_State* L) {
        PushBool(L, s_friendStatusOffline != 0);
        return 1;
    }

    // handler_0x0672: the server's stored choice, one byte, straight into the same flag.
    int32_t OnFriendStatusOffline(void* param, uint32_t opcode, uint32_t time, CDataStore* msg) {
        s_friendStatusOffline = CoA::Read::U8(msg);
        return 1;
    }

    // ---- taxi early landing ---------------------------------------------------------------------------------------
    // handler_TaxiRequestEarlyLanding / TaxiCancelEarlyLanding: empty CMSG 0x5F2 / 0x9C6.
    int TaxiRequestEarlyLanding(lua_State*) {
        CoA::Packet(0x5F2).Send();
        s_taxiEarlyLanding = 1;
        return 0;
    }

    int TaxiCancelEarlyLanding(lua_State*) {
        CoA::Packet(0x9C6).Send();
        s_taxiEarlyLanding = 0;
        return 0;
    }

    int TaxiIsEarlyLanding(lua_State* L) {
        PushBool(L, s_taxiEarlyLanding != 0);
        return 1;
    }

    // ---- loot specialization --------------------------------------------------------------------------------------
    // handler_0x067a SMSG_SET_LOOT_SPECIALIZATION { u32 } -> PLAYER_LOOT_SPEC_UPDATED.
    int32_t OnSetLootSpecialization(void* param, uint32_t opcode, uint32_t time, CDataStore* msg) {
        s_lootSpecialization = CoA::Read::U32(msg);
        CoA::Events::Signal("PLAYER_LOOT_SPEC_UPDATED", nullptr);
        return 1;
    }

    int GetLootSpecialization(lua_State* L) {
        PushInt(L, static_cast<int32_t>(s_lootSpecialization));
        return 1;
    }

    // ---- server time ----------------------------------------------------------------------------------------------
    int GetServerTimeOffset(lua_State* L) {
        PushInt(L, s_serverTimeOffset);
        return 1;
    }

    int GetServerTime(lua_State* L) {
        PushInt(L, static_cast<int32_t>(_time64(nullptr)) + s_serverTimeOffset);
        return 1;
    }

    // ---- zone instances -------------------------------------------------------------------------------------------
    // handler_0x0743 SMSG_INSTANCED_AREAS_LIST { u32 count, u32 current, u32 id[count] } -> ZONE_INSTANCE_LIST.
    int32_t OnInstancedAreasList(void* param, uint32_t opcode, uint32_t time, CDataStore* msg) {
        s_zoneInstances.clear();
        const uint32_t count = CoA::Read::U32(msg);
        const uint32_t current = CoA::Read::U32(msg);

        for (uint32_t i = 0; i < count; ++i) {
            const uint32_t id = CoA::Read::U32(msg);

            if (id == current) {
                s_zoneInstanceIndex = i;
            }

            s_zoneInstances.push_back(id);
        }

        CoA::Events::Signal("ZONE_INSTANCE_LIST", nullptr);
        return 1;
    }

    int GetZoneInstanceID(lua_State* L) {
        PushInt(L, static_cast<int32_t>(s_zoneInstanceIndex + 1));
        return 1;
    }

    int GetZoneInstanceList(lua_State* L) {
        PushInt(L, static_cast<int32_t>(s_zoneInstances.size()));
        return 1;
    }

    // FUN_10a73310: a 1-based index into the list -> CMSG 0x744 { u32 id }.
    int SetZoneInstanceID(lua_State* L) {
        const uint32_t index = static_cast<uint32_t>(ArgInt(L, 1) - 1);

        if (static_cast<int32_t>(index) >= 0 && index < s_zoneInstances.size()) {
            CoA::Packet(0x744).U32(s_zoneInstances[index]).Send();
        }

        return 0;
    }

    // ---- gossip icons ---------------------------------------------------------------------------------------------
    // SMSG_UPDATE_GOSSIP_ICON 0x669 (FUN_10a4fd70): the 32-entry table cleared, then u32 n x u32 icon.
    // GetGossipIconOverride (FUN_10a4d3f0) indexes it unchecked; out-of-range indices read 0 here.
    int32_t OnUpdateGossipIcon(void* param, uint32_t opcode, uint32_t time, CDataStore* msg) {
        memset(s_gossipIcons, 0, sizeof(s_gossipIcons));
        const uint32_t n = CoA::Read::U32(msg);

        for (uint32_t i = 0; i < n; ++i) {
            const int32_t v = static_cast<int32_t>(CoA::Read::U32(msg));

            if (i < 32) {
                s_gossipIcons[i] = v;
            }
        }

        return 1;
    }

    int GetGossipIconOverride(lua_State* L) {
        const int32_t i = ArgInt(L, 1);
        PushInt(L, i >= 0 && i < 32 ? s_gossipIcons[i] : 0);
        return 1;
    }

    // ---- server flags ---------------------------------------------------------------------------------------------
    // FUN_100d3aa0 SMSG_CHECKPOINTS_ENABLED (0x767) { u8 }.
    int32_t OnCheckpointsEnabled(void* param, uint32_t opcode, uint32_t time, CDataStore* msg) {
        s_checkpointsKnown = 1;
        s_checkpointsEnabled = CoA::Read::U8(msg) != 0;
        return 1;
    }

    int GetCheckpointsEnabled(lua_State* L) {
        if (!s_checkpointsKnown) {
            lua_pushnil(L);
        } else {
            PushBool(L, s_checkpointsEnabled != 0);
        }

        return 1;
    }

    // FUN_100d3750 SMSG_BANK_PERMISSIONS (0x769) { u8, u8 } -> BANK_PERMISSIONS_PAYLOAD.
    int32_t OnBankPermissions(void* param, uint32_t opcode, uint32_t time, CDataStore* msg) {
        s_bankKnown = 1;
        s_bankA = CoA::Read::U8(msg) != 0;
        s_bankB = CoA::Read::U8(msg) != 0;
        CoA::Events::Signal("BANK_PERMISSIONS_PAYLOAD", nullptr);
        return 1;
    }

    int GetBankPermissions(lua_State* L) {
        if (!s_bankKnown) {
            lua_pushnil(L);
            return 1;
        }

        PushBool(L, s_bankA != 0);
        PushBool(L, s_bankB != 0);
        return 2;
    }

    // FUN_100d5770: rated brackets 1..3 may always be joined solo.
    int CanJoinRatedArenaWithoutGroup(lua_State* L) {
        const int32_t bracket = ArgInt(L, 1);
        PushBool(L, bracket == 1 || bracket == 2 || bracket == 3);
        return 1;
    }

    // handler_GetFileStreamingStatus: always 0.
    int GetFileStreamingStatus(lua_State* L) {
        PushInt(L, 0);
        return 1;
    }

    // ---- RealmInfo (FUN_1008e2d0, the object at 0x10bdb138) ----------------------------------------------------
    int GetRealmId(lua_State* L) {   // +0x04
        CoA::Script::PushInt(L, static_cast<int32_t>(CoA::RealmInfo::Get().field0));
        return 1;
    }

    int GetRealmExpansion(lua_State* L) {   // +0x08
        CoA::Script::PushInt(L, static_cast<int32_t>(CoA::RealmInfo::Get().ruleset));
        return 1;
    }

    int GetRealmAuctionCutRate(lua_State* L) {   // +0x1C
        CoA::Script::PushNum(L, CoA::RealmInfo::Get().rate3);
        return 1;
    }

    int GetAuctionHouseDepositRateVanity(lua_State* L) {   // +0x20
        CoA::Script::PushNum(L, CoA::RealmInfo::Get().rateAuctionDepositVanity);
        return 1;
    }

    int HasVisitedArea52ThisSession(lua_State* L) {   // +0x49
        CoA::Script::PushBool(L, CoA::RealmInfo::Get().visitedArea52 != 0);
        return 1;
    }

    int GetAuctionHouseDepositMaxVanity(lua_State* L) {   // +0x24
        CoA::Script::PushNum(L, static_cast<double>(CoA::RealmInfo::Get().maxAuctionDepositVanity));
        return 1;
    }

    // The realm object's +0x48 flag: whether the realm allows addons.
    int IsAddOnsAllowed(lua_State* L) {
        CoA::Script::PushBool(L, CoA::RealmInfo::Get().trailingFlag != 0);
        return 1;
    }

    const CoA::Binding s_bindings[] = {
        COA_NATIVE("", "GetRealmId", GetRealmId),
        COA_NATIVE("", "GetRealmExpansion", GetRealmExpansion),
        COA_NATIVE("", "GetRealmAuctionCutRate", GetRealmAuctionCutRate),
        COA_NATIVE("", "GetAuctionHouseDepositRateVanity", GetAuctionHouseDepositRateVanity),
        COA_NATIVE("", "HasVisitedArea52ThisSession", HasVisitedArea52ThisSession),
        COA_NATIVE("", "GetAuctionHouseDepositMaxVanity", GetAuctionHouseDepositMaxVanity),
        COA_NATIVE("", "IsAddOnsAllowed", IsAddOnsAllowed),
        COA_NATIVE("", "CanJoinRatedArenaWithoutGroup", CanJoinRatedArenaWithoutGroup),
        COA_NATIVE("", "FriendStatusIsOffline", FriendStatusIsOffline),
        COA_NATIVE("", "FriendStatusSetOffline", FriendStatusSetOffline),
        COA_NATIVE("", "GetBankPermissions", GetBankPermissions),
        COA_NATIVE("", "GetCheckpointsEnabled", GetCheckpointsEnabled),
        COA_NATIVE("", "GetFileStreamingStatus", GetFileStreamingStatus),
        COA_NATIVE("", "GetGossipIconOverride", GetGossipIconOverride),
        COA_NATIVE("", "GetLootSpecialization", GetLootSpecialization),
        COA_NATIVE("", "GetServerTime", GetServerTime),
        COA_NATIVE("", "GetServerTimeOffset", GetServerTimeOffset),
        COA_NATIVE("", "GetZoneInstanceID", GetZoneInstanceID),
        COA_NATIVE("", "GetZoneInstanceList", GetZoneInstanceList),
        COA_NATIVE("", "SetZoneInstanceID", SetZoneInstanceID),
        COA_NATIVE("", "TaxiCancelEarlyLanding", TaxiCancelEarlyLanding),
        COA_NATIVE("", "TaxiIsEarlyLanding", TaxiIsEarlyLanding),
        COA_NATIVE("", "TaxiRequestEarlyLanding", TaxiRequestEarlyLanding),
    };
}

void CoA::Misc::ServerTimeReceived(int32_t serverTime) {
    const __time64_t now = _time64(nullptr);
    tm gm = *_gmtime64(&now);
    tm local = *_localtime64(&now);
    const __time64_t tLocal = _mktime64(&local);
    const __time64_t tGm = _mktime64(&gm);
    s_serverTimeOffset = (serverTime - static_cast<int32_t>(now)) - (static_cast<int32_t>(tLocal) - static_cast<int32_t>(tGm));
}

void CoA::Misc::Init() {
    COA_PACKET(0x672, OnFriendStatusOffline);
    COA_PACKET(0x767, OnCheckpointsEnabled);
    COA_PACKET(0x769, OnBankPermissions);
    COA_PACKET(0x669, OnUpdateGossipIcon);
    COA_PACKET(0x67A, OnSetLootSpecialization);
    COA_PACKET(0x743, OnInstancedAreasList);
    COA_EVENT("PLAYER_LOOT_SPEC_UPDATED");
    COA_EVENT("ZONE_INSTANCE_LIST");
    COA_EVENT("BANK_PERMISSIONS_PAYLOAD");
    CoA::LuaReg::AddModule(s_bindings, sizeof(s_bindings) / sizeof(s_bindings[0]));
}
