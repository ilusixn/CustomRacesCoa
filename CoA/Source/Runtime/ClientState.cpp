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

CoA::ClientState::Data& CoA::ClientState::Probes() {
    static Data data;
    return data;
}

uint32_t CoA::ClientState::ItemDisplayId(uint32_t item) {
    return Probes().itemDisplayId ? Probes().itemDisplayId(item) : 0;
}

const char* CoA::ClientState::ItemIcon(uint32_t item) {
    const char* icon = Probes().itemIcon ? Probes().itemIcon(item) : nullptr;
    return icon ? icon : "";
}

uint8_t CoA::ClientState::CharacterClass() {
    return Probes().characterClass ? Probes().characterClass() : 0;
}

uint8_t CoA::ClientState::CharacterRace() {
    return Probes().characterRace ? Probes().characterRace() : 0;
}

int32_t CoA::ClientState::SpellMechanic(uint32_t spell) {
    return Probes().spellMechanic ? Probes().spellMechanic(spell) : -1;
}
