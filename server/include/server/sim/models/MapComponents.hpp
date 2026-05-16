#pragma once
#include <shared/types/Enums.hpp>
#include <shared/sim/Constants.hpp>

#include <cstdint>
#include <string>
#include <vector>
#include <map> // ZMIANA: Gwarantuje deterministyczn¹ sieæ i stabilne wskaŸniki

namespace gs::server {

    struct Tile {
        uint32_t    id{ 0 };
        uint16_t    x{ 0 };
        uint16_t    y{ 0 };
        uint32_t    landProvinceId{ 0 };
        uint32_t    seaProvinceId{ 0 };
        TerrainType terrain{ TerrainType::Plains };
        uint32_t    ownerId{ 0 };
        uint32_t    buildingId{ 0 };
    };

    struct ProvincePopulation {
        float workers{ 0.0f };
        float military{ 0.0f };
        float maxCap{ 0.0f };
    };

    struct PlayerProvinceShare {
        uint32_t    entityId{ 0 };
        uint32_t    provinceId{ 0 };

        std::vector<uint32_t>  ownedTileIds;
        std::vector<uint32_t>  buildingIds;

        ProvincePopulation  population;
        float               supplyStored{ 0.0f };
        float               workerSplitRatio{ constants::DEFAULT_WORKER_SPLIT };
        AutomationLevel     automationLevel{ AutomationLevel::FullAuto };
        float               powerAvailableMW{ 0.0f };

        bool hasDominance(size_t totalTiles) const {
            return ownedTileIds.size() * 2 > totalTiles;
        }
    };

    struct ProvinceAdjacency {
        uint32_t neighborProvinceId{ 0 };
        uint32_t sharedBorderTilesCount{ 0 };
    };

    struct LandProvince {
        uint32_t    id{ 0 };
        std::string name;

        std::vector<uint32_t>           tileIds;
        std::vector<ProvinceAdjacency>  neighbors;
        std::vector<uint32_t>           coastalSeaProvinceIds;

        // ZMIANA: std::map gwarantuje, ¿e getShare() bêdzie w 100% bezpieczne
        // przed dangling pointers, a pêtla w buildFullSnapshot() zachowa kolejnoœæ
        std::map<uint32_t, PlayerProvinceShare> playerShares;

        PlayerProvinceShare* getShare(uint32_t eid) {
            auto it = playerShares.find(eid);
            return it != playerShares.end() ? &it->second : nullptr;
        }
        const PlayerProvinceShare* getShare(uint32_t eid) const {
            auto it = playerShares.find(eid);
            return it != playerShares.end() ? &it->second : nullptr;
        }
    };

    struct SeaProvince {
        uint32_t    id{ 0 };
        std::string name;

        std::vector<uint32_t>  tileIds;
        std::vector<uint32_t>  neighborSeaProvinceIds;
        std::vector<uint32_t>  coastalLandProvinceIds;

        float centerX{ 0.0f };
        float centerY{ 0.0f };
    };

} // namespace gs::server