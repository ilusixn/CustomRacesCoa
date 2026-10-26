// AscPOI.cpp (see POI.h): the server's points of interest and GetNumActivePOI / GetPOIInfo.
#include "CoA/Source/Modules/POI.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/Events.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Source/Runtime/Read.h"
#include "CoA/Source/Runtime/Script.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <string>
#include <unordered_map>

namespace {
    struct Poi {
        std::string key;    // +0x20
        std::string text;   // +0x38
        uint32_t a = 0;     // +0x50
        uint32_t b = 0;     // +0x54
        uint32_t c = 0;     // +0x58 (read as a float, passed and pushed as its bits)
        float d = 0.0f;     // +0x5C
        float e = 0.0f;     // +0x60
    };

    std::unordered_map<std::string, Poi> s_pois;

    int GetNumActivePOI(lua_State* L) {   // FUN_102cfa30
        CoA::Script::PushInt(L, static_cast<int32_t>(s_pois.size()));
        return 1;
    }

    // FUN_102cfa50 GetPOIInfo(index): the index-th element in map order (0-based), 7 values; nothing out of range.
    int GetPOIInfo(lua_State* L) {
        const uint32_t index = static_cast<uint32_t>(static_cast<int64_t>(CoA::Script::CheckNumber(L, 1)));

        if (index >= s_pois.size()) {
            return 0;
        }

        auto it = s_pois.begin();
        std::advance(it, index);
        const Poi& p = it->second;
        CoA::Script::PushStr(L, p.key.c_str());
        CoA::Script::PushStr(L, p.text.c_str());
        CoA::Script::PushInt(L, static_cast<int32_t>(p.a));
        CoA::Script::PushInt(L, static_cast<int32_t>(p.b));
        CoA::Script::PushInt(L, static_cast<int32_t>(p.c));
        CoA::Script::PushNum(L, static_cast<double>(p.d));
        CoA::Script::PushNum(L, static_cast<double>(p.e));
        return 7;
    }

    const CoA::Binding s_bindings[] = {
        COA_NATIVE("", "GetNumActivePOI", GetNumActivePOI),
        COA_NATIVE("", "GetPOIInfo", GetPOIInfo),
    };
}

// The reads are unchecked in the original; a short packet reads zeros / "" (CoA::Read).
int32_t CoA::POI::OnAdd(void*, uint32_t, uint32_t, CDataStore* msg) {
    Poi p;
    p.key = CoA::Read::Name(msg);
    p.text = CoA::Read::Name(msg);
    p.a = CoA::Read::U32(msg);
    p.b = CoA::Read::U32(msg);
    p.c = CoA::Read::U32(msg);
    p.d = CoA::Read::F32(msg);
    p.e = CoA::Read::F32(msg);
    CoA::Events::Signal("ASCENSION_POI_ADD", "%s%s%u%u%u%f%f", p.key.c_str(), p.text.c_str(), p.a, p.b, p.c,
        static_cast<double>(p.d), static_cast<double>(p.e));
    s_pois[p.key] = p;
    return 1;
}

int32_t CoA::POI::OnRemove(void*, uint32_t, uint32_t, CDataStore* msg) {
    const std::string key = CoA::Read::Name(msg);
    CoA::Events::Signal("ASCENSION_POI_REMOVE", "%s", key.c_str());
    s_pois.erase(key);
    return 1;
}

int32_t CoA::POI::OnClear(void*, uint32_t, uint32_t, CDataStore*) {
    CoA::Events::Signal("ASCENSION_POI_CLEAR", nullptr);
    s_pois.clear();
    return 1;
}

void CoA::POI::Init() {
    COA_PACKET(0x90F, CoA::POI::OnAdd);
    COA_PACKET(0x910, CoA::POI::OnRemove);
    COA_PACKET(0x911, CoA::POI::OnClear);
    CoA::LuaReg::AddModule(s_bindings, sizeof(s_bindings) / sizeof(s_bindings[0]));
    COA_EVENT("ASCENSION_POI_ADD");
    COA_EVENT("ASCENSION_POI_REMOVE");
    COA_EVENT("ASCENSION_POI_CLEAR");
}

void CoA::POI::Reset() {
    s_pois.clear();
}
