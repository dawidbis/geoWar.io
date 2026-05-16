#pragma once
#include <shared/types/Enums.hpp>
#include <shared/sim/Constants.hpp>

#include <cstdint>
#include <string>

namespace gs::server {

    struct Entity {
        uint32_t    id{ 0 };
        std::string name;
        EntityType  type{ EntityType::Human };

        int64_t  gold{ constants::START_GOLD };
        float    uranium{ 0.0f };

        uint32_t  capitalBuildingId{ 0 };
        BotLevel  botLevel{ BotLevel::Medium };

        uint32_t  disconnectedSinceTick{ 0 };
        bool      isEliminated{ false };

        uint32_t  militaryPactId{ 0 };
        uint32_t  economicPactId{ 0 };
        uint32_t  traitorDebuffUntilTick{ 0 };
    };

} // namespace gs::server