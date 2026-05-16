#pragma once
#include <shared/types/Enums.hpp>

#include <cstdint>
#include <vector>
#include <utility>

namespace gs::server {

    struct GameEvent {
        GameEventType  type{ GameEventType::PlayerJoined };
        uint32_t       entityId{ 0 };
        uint32_t       targetId{ 0 };
        float          x{ 0.0f };
        float          y{ 0.0f };
        float          radius{ 0.0f };
        uint32_t       tick{ 0 };
    };

    struct PlayerInput {
        uint32_t  entityId{ 0 };
        InputType type{ InputType::Attack };
        uint32_t  tick{ 0 };

        uint32_t  provinceId{ 0 };
        uint32_t  targetProvinceId{ 0 };
        uint32_t  buildingType{ 0 };
        uint32_t  unitId{ 0 };
        uint32_t  targetId{ 0 };
        float     floatParam{ 0.0f };
        uint16_t  tileX{ 0 };
        uint16_t  tileY{ 0 };
        std::vector<std::pair<uint32_t, uint32_t>> multiTargets;
    };

} // namespace gs::server