#include "CoA/Source/Modules/JoinedNames.h"
#include <unordered_map>

namespace CoA::JoinedNames {
    namespace {
        std::unordered_map<uint64_t, uint64_t> s_riders;
    }

    std::string Joined(const std::string& body, const std::string& rider) {
        if (body.empty() || rider.empty()) {
            return body;
        }

        return body + "'" + rider;
    }

    bool RiderChanged(uint64_t body, uint64_t rider) {
        auto found = s_riders.find(body);
        uint64_t last = found == s_riders.end() ? 0 : found->second;

        if (last == rider) {
            return false;
        }

        if (rider) {
            s_riders[body] = rider;
        } else {
            s_riders.erase(found);
        }

        return true;
    }

    void Reset() {
        s_riders.clear();
    }
}
