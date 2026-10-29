#ifndef ESTERIA_APPEARANCE_H
#define ESTERIA_APPEARANCE_H

// CoA: the Esteria tree's native custom-race header. Our custom races go through CoA/Source/Modules/Esteria (the DLL port), so this
// only carries what the ported retail adapter (RetailRace.cpp) asks of it: the packed choices of Esteria's own races, which are not
// retail races here.
#include <array>
#include <cstdint>

class CCharacterComponent;

bool EsteriaRetailChoice(CCharacterComponent* character, uint32_t optionId, uint32_t& choiceId);
bool EsteriaRetailStoreChoice(CCharacterComponent* character, uint32_t optionId, uint32_t choiceId);
std::array<uint8_t, 13> EsteriaRetailFields(CCharacterComponent* character);
void EsteriaRetailFields(CCharacterComponent* character, const std::array<uint8_t, 13>& fields);

#endif
