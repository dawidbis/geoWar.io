#pragma once
#include <shared/types/Enums.hpp>

#include <cstdint>
#include <vector>

namespace gs::server {

    struct Building {
        uint32_t      id{ 0 };
        BuildingType  type{ BuildingType::City };
        uint8_t       level{ 1 };

        uint32_t  ownerId{ 0 };
        uint32_t  landProvinceId{ 0 };
        uint16_t  topLeftX{ 0 };
        uint16_t  topLeftY{ 0 };
        uint8_t   sizeX{ 1 };
        uint8_t   sizeY{ 1 };

        float   constructionProgress{ 0.0f };
        bool    isPowered{ false };
        bool    isActive{ false };

        float   supplyStored{ 0.0f };
        float   supplyCapacity{ 0.0f };

        uint32_t  electricGridId{ 0 };
        int8_t    electricPriority{ 50 };
        bool      isCapital{ false };

        NuclearPlantMode nuclearMode{ NuclearPlantMode::Civilian };
    };

    struct ElectricEdge {
        uint32_t  buildingA{ 0 };
        uint32_t  buildingB{ 0 };
        float     lengthTiles{ 0.0f };
    };

    struct ElectricGrid {
        uint32_t  id{ 0 };
        uint32_t  ownerId{ 0 };
        std::vector<uint32_t>     buildingIds;
        std::vector<ElectricEdge> edges;
        float totalProductionMW{ 0.0f };
        float totalDemandMW{ 0.0f };
    };

} // namespace gs::server