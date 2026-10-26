#ifndef COA_SOURCE_MODULES_ALPHAAMBIENCE_H
#define COA_SOURCE_MODULES_ALPHAAMBIENCE_H

#include <cstddef>
#include <cstdint>

struct CVar;

// AscAlphaAmbience.cpp (installer FUN_1031e8c0): CVar useAlphaAmbienceSound (FUN_10114540 world queue; flags 1, "0",
// category 7, last argument 1; slot 0x10BE4128). Its callback FUN_1031e840 rewrites, on "1", the day / night ambience
// ids (+4 / +8) of the SoundAmbience.dbc rows (client container 0xAD464C) in the table below; any other value does
// nothing (the stock ids come back only with a restart). The table is FUN_1007dd80's unordered_map of 15 triples;
// key 0x2A appears twice and the first one wins.
namespace CoA::AlphaAmbience {
    struct Entry {
        uint32_t id;
        uint32_t day;
        uint32_t night;
    };

    struct Deps {
        // SoundAmbience.dbc row id: +4 = day, +8 = night; false without the row.
        bool (*setAmbience)(uint32_t id, uint32_t day, uint32_t night) = nullptr;
    };

    Deps& Dependencies();
    const Entry* Table(size_t& count);
    bool OnChanged(CVar* var, const char* oldValue, const char* value, void* arg);   // FUN_1031e840

    void Init();
    // Tests only.
    void Reset();
}

#endif
