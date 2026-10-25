#include "CoA/Source/Modules/GlobalsG.h"
#include "CoA/Source/Modules/AttachPatches.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Script.h"
#include "Engine/Source/lua-5.1/src/lua.h"

namespace {
    int32_t s_afkTime = 300000;          // -(0x52B259)
    bool s_afkEnabled = true;            // the jl at 0x52B25F
    int32_t s_afkLogoutTime = 1800000;   // -(0x52B266)
    float s_namePlateRangeSq = 1681.0f;  // 0xADAA7C
    float s_namePlateWidth = 0.1f;       // 0xB2DA74
    float s_namePlateHeight = 0.025f;    // 0xB2DA70

    int32_t ArgInt(lua_State* L, int idx) {
        return CoA::Script::ToInt(lua_tonumber(L, idx));
    }

    int SetAFKTimer(lua_State* L) {   // FUN_10a50910
        const int32_t v = ArgInt(L, 1);
        s_afkEnabled = v != 0;
        s_afkTime = v;
        return 0;
    }

    int SetAFKLogoutTimer(lua_State* L) {   // FUN_10a503c0
        const int32_t v = ArgInt(L, 1);
        CoA::AttachPatches::SetAfkLogoutEnabled(v != 0);
        s_afkLogoutTime = v;
        return 0;
    }

    int SetNamePlateRange(lua_State* L) {
        const float x = static_cast<float>(lua_tonumber(L, 1));
        const float lo = 5.0f <= x ? x : 5.0f;
        const float r = x <= 60.0f ? lo : 60.0f;
        s_namePlateRangeSq = r * r;
        return 0;
    }

    float Clamp01(float x) {
        const float lo = 0.0f <= x ? x : 0.0f;
        return x <= 1.0f ? lo : 1.0f;
    }

    int SetNamePlateScaleX(lua_State* L) {
        s_namePlateWidth = Clamp01(static_cast<float>(lua_tonumber(L, 1)));
        return 0;
    }

    int SetNamePlateScaleY(lua_State* L) {
        s_namePlateHeight = Clamp01(static_cast<float>(lua_tonumber(L, 1)));
        return 0;
    }

    const CoA::Binding s_bindings[] = {
        COA_NATIVE("", "SetAFKLogoutTimer", SetAFKLogoutTimer),
        COA_NATIVE("", "SetAFKTimer", SetAFKTimer),
        COA_NATIVE("", "SetNamePlateRange", SetNamePlateRange),
        COA_NATIVE("", "SetNamePlateScaleX", SetNamePlateScaleX),
        COA_NATIVE("", "SetNamePlateScaleY", SetNamePlateScaleY),
    };
}

int32_t CoA::GlobalsG::AfkTime() {
    return s_afkTime;
}

bool CoA::GlobalsG::AfkEnabled() {
    return s_afkEnabled;
}

int32_t CoA::GlobalsG::AfkLogoutTime() {
    return s_afkLogoutTime;
}

float CoA::GlobalsG::NamePlateRangeSq() {
    return s_namePlateRangeSq;
}

float CoA::GlobalsG::NamePlateWidth() {
    return s_namePlateWidth;
}

float CoA::GlobalsG::NamePlateHeight() {
    return s_namePlateHeight;
}

void CoA::GlobalsG::Reset() {
    s_afkTime = 300000;
    s_afkEnabled = true;
    s_afkLogoutTime = 1800000;
    s_namePlateRangeSq = 1681.0f;
    s_namePlateWidth = 0.1f;
    s_namePlateHeight = 0.025f;
    CoA::AttachPatches::SetAfkLogoutEnabled(false);
}

void CoA::GlobalsG::Init() {
    CoA::LuaReg::AddModule(s_bindings, sizeof(s_bindings) / sizeof(s_bindings[0]));
}
