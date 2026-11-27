#ifndef COA_SOURCE_MODULES_JOINEDNAMES_H
#define COA_SOURCE_MODULES_JOINEDNAMES_H

#include <cstdint>
#include <string>

// Two-Players pairs (mod-subclass): the server seats the Ranged Head on the Melee Head's body in vehicle 9600. The
// name over the body, its tooltip and UnitPVPName join both as "Melee'Ranged"; chat keeps each head's own name.
namespace CoA::JoinedNames {
    constexpr int32_t kTwoPlayersVehicle = 9600;
    constexpr int32_t kTwinsTwoPlayersVehicle = 9601;

    std::string Joined(const std::string& body, const std::string& rider);
    // True when the rider on a body differs from the one last seen, so the body's name is built again.
    bool RiderChanged(uint64_t body, uint64_t rider);
    void Reset();
}

#endif
