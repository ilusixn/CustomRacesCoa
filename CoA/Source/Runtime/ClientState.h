#ifndef COA_SOURCE_RUNTIME_CLIENTSTATE_H
#define COA_SOURCE_RUNTIME_CLIENTSTATE_H

// Client state the DLL reads directly (fixed exe globals in the original). The probes are installed by
// ClientStateClient.cpp; without one (tests) the answer is the default given.
#include <cstdint>

struct lua_State;

namespace CoA::ClientState {
    // CGGameUI::s_inWorld (0xBD0792); default false.
    bool InWorld();
    void SetInWorldProbe(bool (*probe)());

    // CGGameUI::CanPerformAction (0x5191C0); default true.
    bool CanPerformAction(int32_t action);
    void SetActionProbe(bool (*probe)(int32_t action));

    // The client's main Lua state (FrameScript_GetContext, 0x817DB0); default nullptr.
    lua_State* MainLuaState();
    void SetMainLuaStateProbe(lua_State* (*probe)());

    // The client directory with a trailing separator, as 0x4218B0 returns it; default "".
    const char* BasePath();
    void SetBasePathProbe(const char* (*probe)());

    // Client tables and the character the glue has selected, as the DLL reads them; null entries answer 0 / "" / -1.
    struct Data {
        uint32_t (*itemDisplayId)(uint32_t item) = nullptr;   // Item.dbc display id (+0x14); 0 without a row
        const char* (*itemIcon)(uint32_t item) = nullptr;     // the inventory icon of the item's ItemDisplayInfo row
        uint8_t (*characterClass)() = nullptr;                // ClientServices::GetCharacterClass (0x6B1080)
        uint8_t (*characterRace)() = nullptr;                 // ClientServices::GetCharacterRace (0x6B1070)
        int32_t (*spellMechanic)(uint32_t spell) = nullptr;  // Spell.dbc +0xC; -1 without a row
    };
    Data& Probes();
    uint32_t ItemDisplayId(uint32_t item);
    const char* ItemIcon(uint32_t item);
    uint8_t CharacterClass();
    uint8_t CharacterRace();
    int32_t SpellMechanic(uint32_t spell);

    void InstallClient();
}

#endif
