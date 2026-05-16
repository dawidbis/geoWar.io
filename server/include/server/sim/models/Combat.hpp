#pragma once
#include <shared/types/Enums.hpp>
#include <cstdint>

namespace gs::server {
    struct Attack {
        uint32_t      id{ 0 };
        uint32_t      attackerId{ 0 };
        uint32_t      sourceProvinceId{ 0 };
        uint32_t      targetProvinceId{ 0 };

        float         troops{ 0.0f };
        float         supply{ 0.0f };
        uint32_t      startTick{ 0 };

        float         capturedTileProgress{ 0.0f };
        AttackStatus  status{ AttackStatus::Active };
        uint32_t      retreatEndTick{ 0 };
        uint32_t      currentFrontTileId{ 0 };
    };
} // namespace gs::server