#pragma once
#include <shared/types/Enums.hpp>

#include <cstdint>
#include <string>
#include <vector>
#include <map> // ZMIANA: std::map dla determinizmu i bezpiecznej iteracji

namespace gs::server {

    struct Relation {
        uint32_t  entityA{ 0 };
        uint32_t  entityB{ 0 };
        uint8_t   flags{ 0 };

        bool hasFlag(RelationFlag f) const {
            return (flags & static_cast<uint8_t>(f)) != 0;
        }
        void setFlag(RelationFlag f) { flags |= static_cast<uint8_t>(f); }
        void clearFlag(RelationFlag f) { flags &= ~static_cast<uint8_t>(f); }
    };

    struct PactApplication {
        uint32_t candidateEntityId{ 0 };
        uint32_t deadlineTick{ 0 };

        // ZMIANA na std::map
        std::map<uint32_t, VoteResult> votes;
    };

    struct Pact {
        uint32_t    id{ 0 };
        PactType    type{ PactType::Military };
        std::string name;
        uint32_t    founderId{ 0 };
        uint32_t    createdTick{ 0 };

        std::vector<uint32_t>        memberIds;
        std::vector<PactApplication> pendingApplications;
    };

} // namespace gs::server