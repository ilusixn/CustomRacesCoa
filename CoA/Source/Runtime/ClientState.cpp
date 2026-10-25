#include "CoA/Source/Runtime/ClientState.h"

namespace {
    bool (*s_inWorld)() = nullptr;
    bool (*s_action)(int32_t action) = nullptr;
    lua_State* (*s_mainState)() = nullptr;
    const char* (*s_basePath)() = nullptr;
}

bool CoA::ClientState::InWorld() {
    return s_inWorld && s_inWorld();
}

void CoA::ClientState::SetInWorldProbe(bool (*probe)()) {
    s_inWorld = probe;
}

bool CoA::ClientState::CanPerformAction(int32_t action) {
    return !s_action || s_action(action);
}

void CoA::ClientState::SetActionProbe(bool (*probe)(int32_t action)) {
    s_action = probe;
}

lua_State* CoA::ClientState::MainLuaState() {
    return s_mainState ? s_mainState() : nullptr;
}

void CoA::ClientState::SetMainLuaStateProbe(lua_State* (*probe)()) {
    s_mainState = probe;
}

const char* CoA::ClientState::BasePath() {
    return s_basePath ? s_basePath() : "";
}

void CoA::ClientState::SetBasePathProbe(const char* (*probe)()) {
    s_basePath = probe;
}
