// Port of AscAddonTaint.cpp (installer FUN_10312150). The detours are marked edits in the client source; this file
// holds what they ask. See AddonTaint.h.
#include "CoA/Source/Modules/AddonTaint.h"
#include "CoA/Source/Modules/GlobalsJ.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Modules/RealmInfo.h"
#include "CoA/Source/Runtime/ClientHooks.h"
#include "Engine/Source/ConsoleBase/ConsoleVar.h"
#include "Engine/Source/lua-5.1/src/lua.h"

namespace {
    const bool kAllSecure = false;   // SecureAddonMgr +0x10 (0x10BE3054): never written

    CoA::AddonTaint::Deps s_deps;
    CVar* s_loadUnknownAddOns = nullptr;        // 0x10BE3F7C
    const char* s_targetChangeRunning = nullptr;   // 0x10BE3F80

    bool SecureName(const char* running) {
        return (running && CoA::GlobalsJ::IsSecureAddon(running)) || kAllSecure;
    }

    // FUN_10313b70: the original stores 0xD4139C under NtProtectVirtualMemory; here it is the Lua library's global.
    void SetRunning(const char* name) {
        lua_tainted = name;
    }
}

CoA::AddonTaint::Deps& CoA::AddonTaint::Dependencies() {
    return s_deps;
}

bool CoA::AddonTaint::Secure() {
    return SecureName(lua_tainted);
}

bool CoA::AddonTaint::Issecure(lua_State* L) {
    if (!CoA::AddonTaint::Secure()) {
        return false;
    }

    lua_pushnumber(L, 1.0);
    return true;
}

const char* CoA::AddonTaint::BeginCleared() {
    const char* saved = lua_tainted;

    if (SecureName(saved)) {
        SetRunning(nullptr);
    }

    return saved;
}

void CoA::AddonTaint::EndCleared(const char* saved) {
    SetRunning(saved);
}

bool CoA::AddonTaint::ClearTargetRefused() {
    if (!CoA::GlobalsJ::InsideRunScript()) {
        return false;
    }

    if (s_deps.blockedActionFeedback) {
        s_deps.blockedActionFeedback(2);
    }

    return true;
}

bool CoA::AddonTaint::AddOnLoadRefused(const char* name) {
    return !CoA::RealmInfo::Get().trailingFlag && !CoA::GlobalsJ::IsKnownAddon(name);
}

bool CoA::AddonTaint::UnknownAddOnLoadRefused(const char* name) {
    return s_loadUnknownAddOns && s_loadUnknownAddOns->m_intValue == 0 && !CoA::GlobalsJ::IsKnownAddon(name);
}

CVar** CoA::AddonTaint::LoadUnknownAddOnsSlot() {
    return &s_loadUnknownAddOns;
}

void CoA::AddonTaint::BeforeTargetChange() {
    s_targetChangeRunning = lua_tainted;

    if (SecureName(s_targetChangeRunning)) {
        SetRunning(nullptr);
    }
}

void CoA::AddonTaint::AfterTargetChange() {
    SetRunning(s_targetChangeRunning);
    s_targetChangeRunning = nullptr;
}

void CoA::AddonTaint::Init() {
    CoA::ClientHooks::beforeTarget.Add(CoA::AddonTaint::BeforeTargetChange);
    CoA::ClientHooks::afterTarget.Add(CoA::AddonTaint::AfterTargetChange);
}

void CoA::AddonTaint::Reset() {
    s_deps = CoA::AddonTaint::Deps();
    s_loadUnknownAddOns = nullptr;
    s_targetChangeRunning = nullptr;
    CoA::ClientHooks::beforeTarget.Clear();
    CoA::ClientHooks::afterTarget.Clear();
}
