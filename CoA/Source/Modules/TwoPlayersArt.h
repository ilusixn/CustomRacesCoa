#ifndef COA_SOURCE_MODULES_TWOPLAYERSART_H
#define COA_SOURCE_MODULES_TWOPLAYERSART_H

#include <cstdint>

// A race whose female slot shows the male model (Two-Players Twins, ChrRaces female display = male display) dresses
// that slot in male item textures and helmets.
namespace CoA::TwoPlayersArt {
    uint32_t ItemSex(uint32_t sex, int32_t maleDisplay, int32_t femaleDisplay);
}

#endif
