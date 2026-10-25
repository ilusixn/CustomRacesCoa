// AscSeasonal.cpp: the seasonal collection (SeasonalCollectionMgr, FUN_10310320 -> static 0x10BE3EB8) and custom
// points (AscensionCustomPointsMgr, FUN_100cf7b0 -> static 0x10BDB57C). Neither is reset on the glue screen.
//
// SMSG_SEASONAL_COLLECTION_INFO 0x9BE (FUN_10310430): u32 n x 8-byte achievement {i32 id, u8 node, u8 chapter, u8
// sortOrder, u8 isReward} APPENDED to the list (never cleared), then twice {icon, name, description} (u32-length
// strings) into slots 0 and 1. SMSG_SEASONAL_COLLECTION_UNLOCKED 0x9BF (handler_0x09bf) sets the unlocked flag (+0xA0).
// RequestDeliverSeasonalCollectionItem(id) (FUN_100cf8d0): CMSG 0x523 {u8 1, u32 id}.
//
// Custom points 0x905 (FUN_100cfb40): u8 initial, u8 type, i32 value. Initial packets only insert (an existing value is
// kept) and fire nothing; others store the value and fire the type's ASCENSION_CUSTOM_POINTS_*_VALUE_CHANGED("%u%u",
// old, new) -- old is 0 for a new type.
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/Events.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Packet.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Source/Runtime/Read.h"
#include "CoA/Source/Runtime/Script.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <string>
#include <unordered_map>
#include <vector>

namespace {
    // ---- seasonal collection ----
    struct Achievement {
        int32_t id;
        uint8_t node;
        uint8_t chapter;
        uint8_t sortOrder;
        uint8_t isReward;
    };

    static_assert(sizeof(Achievement) == 8, "seasonal achievement");

    struct Collection {
        std::vector<Achievement> achievements;
        std::string icons[2];
        std::string names[2];
        std::string descriptions[2];
        bool unlocked = false;
    };

    Collection s_seasonal;

    int32_t OnSeasonalInfo(void*, uint32_t, uint32_t, CDataStore* msg) {
        for (uint32_t n = CoA::Read::U32(msg); n && CoA::Read::Remaining(msg) >= sizeof(Achievement); --n) {
            Achievement a;
            CoA::Read::Data(msg, &a, sizeof(a));
            s_seasonal.achievements.push_back(a);
        }

        for (int i = 0; i < 2; ++i) {
            s_seasonal.icons[i] = CoA::Read::Name(msg);
            s_seasonal.names[i] = CoA::Read::Name(msg);
            s_seasonal.descriptions[i] = CoA::Read::Name(msg);
        }

        return 1;
    }

    int32_t OnSeasonalUnlocked(void*, uint32_t, uint32_t, CDataStore*) {
        s_seasonal.unlocked = true;
        return 1;
    }

    void SetNum(lua_State* L, const char* k, double v) {
        lua_pushstring(L, k);
        CoA::Script::PushNum(L, v);
        lua_settable(L, -3);
    }

    void SetPair(lua_State* L, const char* k, const std::string (&v)[2]) {
        lua_pushstring(L, k);
        lua_createtable(L, 0, 0);
        lua_checkstack(L, 2);

        for (int i = 0; i < 2; ++i) {
            CoA::Script::PushInt(L, i + 1);
            CoA::Script::PushStr(L, v[i].c_str());
            lua_settable(L, -3);
        }

        lua_settable(L, -3);
    }

    // FUN_10310e60
    int GetSeasonalCollectionInfo(lua_State* L) {
        lua_createtable(L, 0, 0);
        lua_checkstack(L, 2);
        lua_pushstring(L, "achievements");
        lua_createtable(L, 0, 0);
        lua_checkstack(L, 2);
        int i = 0;

        for (const Achievement& a : s_seasonal.achievements) {
            CoA::Script::PushNum(L, ++i);
            lua_createtable(L, 0, 0);
            lua_checkstack(L, 2);
            SetNum(L, "id", a.id);
            SetNum(L, "node", a.node);
            SetNum(L, "chapter", a.chapter);
            SetNum(L, "sortOrder", a.sortOrder);
            lua_pushstring(L, "isReward");
            CoA::Script::PushBool(L, a.isReward != 0);
            lua_settable(L, -3);
            lua_settable(L, -3);
        }

        lua_settable(L, -3);
        SetPair(L, "icons", s_seasonal.icons);
        SetPair(L, "names", s_seasonal.names);
        SetPair(L, "descriptions", s_seasonal.descriptions);
        return 1;
    }

    int IsSeasonalCollectionUnlocked(lua_State* L) {
        CoA::Script::PushBool(L, s_seasonal.unlocked);
        return 1;
    }

    int RequestDeliverSeasonalCollectionItem(lua_State* L) {
        CoA::Packet p(0x523);
        p.U8(1);
        p.U32(static_cast<uint32_t>(CoA::Script::ToInt(CoA::Script::CheckNumber(L, 1))));
        p.Send();
        return 0;
    }

    // ---- custom points ----
    std::unordered_map<uint8_t, int32_t> s_points;

    const char* PointsEvent(uint8_t type) {   // FUN_100cf550
        switch (type) {
            case 0: return "ASCENSION_CUSTOM_POINTS_UNKNOWN_VALUE_CHANGED";
            case 1: return "ASCENSION_CUSTOM_POINTS_SEASONAL_POINTS_VALUE_CHANGED";
            case 2: return "ASCENSION_CUSTOM_POINTS_DONATION_POINTS_VALUE_CHANGED";
            case 3: return "ASCENSION_CUSTOM_POINTS_BAZAAR_TOKENS_VALUE_CHANGED";
            case 4: return "ASCENSION_CUSTOM_POINTS_REDEMPTION_POINTS_VALUE_CHANGED";
            default: return "ASCENSION_CUSTOM_POINTS_UNHANDLED_VALUE_CHANGED";
        }
    }

    int32_t OnCustomPoints(void*, uint32_t, uint32_t, CDataStore* msg) {
        const bool initial = CoA::Read::U8(msg) != 0;
        const uint8_t type = CoA::Read::U8(msg);
        const int32_t value = CoA::Read::I32(msg);

        if (initial) {
            s_points.emplace(type, value);
            return 1;
        }

        int32_t old = 0;
        const auto it = s_points.find(type);

        if (it == s_points.end()) {
            s_points.emplace(type, value);
        } else {
            old = it->second;
            it->second = value;
        }

        CoA::Events::Signal(PointsEvent(type), "%u%u", old, value);
        return 1;
    }

    int32_t Points(uint8_t type) {   // FUN_100cfad0
        const auto it = s_points.find(type);
        return it != s_points.end() ? it->second : 0;
    }

    int GetAscensionSeasonalPoints(lua_State* L) {
        CoA::Script::PushInt(L, Points(1));
        return 1;
    }

    int GetAscensionDonationPoints(lua_State* L) {
        CoA::Script::PushInt(L, Points(2));
        return 1;
    }

    // FUN_100cf600: the type is the low byte of the truncated number.
    int GetCustomAscensionPointValue(lua_State* L) {
        CoA::Script::PushInt(L, Points(static_cast<uint8_t>(CoA::Script::ToInt(CoA::Script::CheckNumber(L, 1)))));
        return 1;
    }

    const CoA::Binding s_bindings[] = {
        COA_NATIVE("", "GetSeasonalCollectionInfo", GetSeasonalCollectionInfo),
        COA_NATIVE("", "IsSeasonalCollectionUnlocked", IsSeasonalCollectionUnlocked),
        COA_NATIVE("", "RequestDeliverSeasonalCollectionItem", RequestDeliverSeasonalCollectionItem),
        COA_NATIVE("", "GetAscensionSeasonalPoints", GetAscensionSeasonalPoints),
        COA_NATIVE("", "GetAscensionDonationPoints", GetAscensionDonationPoints),
        COA_NATIVE("", "GetCustomAscensionPointValue", GetCustomAscensionPointValue),
    };
}

void CoA::Seasonal::Init() {
    CoA::LuaReg::AddModule(s_bindings, sizeof(s_bindings) / sizeof(s_bindings[0]));
    COA_PACKET(0x9BE, OnSeasonalInfo);
    COA_PACKET(0x9BF, OnSeasonalUnlocked);
    COA_PACKET(0x905, OnCustomPoints);
    COA_EVENT("ASCENSION_CUSTOM_POINTS_UNKNOWN_VALUE_CHANGED");
    COA_EVENT("ASCENSION_CUSTOM_POINTS_SEASONAL_POINTS_VALUE_CHANGED");
    COA_EVENT("ASCENSION_CUSTOM_POINTS_DONATION_POINTS_VALUE_CHANGED");
    COA_EVENT("ASCENSION_CUSTOM_POINTS_BAZAAR_TOKENS_VALUE_CHANGED");
    COA_EVENT("ASCENSION_CUSTOM_POINTS_REDEMPTION_POINTS_VALUE_CHANGED");
    COA_EVENT("ASCENSION_CUSTOM_POINTS_UNHANDLED_VALUE_CHANGED");
}

void CoA::Seasonal::Reset() {
    s_seasonal = Collection();
    s_points.clear();
}
