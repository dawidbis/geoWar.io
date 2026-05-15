#pragma once
#include <shared/types/Enums.hpp>
#include <shared/sim/Constants.hpp>

#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace gs::server {

    // ─────────────────────────────────────────────────────────────────────────────
    // Kafelek
    // ─────────────────────────────────────────────────────────────────────────────

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

    // ─────────────────────────────────────────────────────────────────────────────
    // Populacja
    // ─────────────────────────────────────────────────────────────────────────────

    struct ProvincePopulation {
        float workers{ 0.0f };
        float military{ 0.0f };
        float maxCap{ 0.0f };
    };

    // ─────────────────────────────────────────────────────────────────────────────
    // Sub-prowincja gracza
    // ─────────────────────────────────────────────────────────────────────────────

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

    // ─────────────────────────────────────────────────────────────────────────────
    // Prowincje
    // ─────────────────────────────────────────────────────────────────────────────

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

        std::unordered_map<uint32_t, PlayerProvinceShare> playerShares;

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

    // ─────────────────────────────────────────────────────────────────────────────
    // Budynek
    // ─────────────────────────────────────────────────────────────────────────────

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

    // ─────────────────────────────────────────────────────────────────────────────
    // Atak lądowy
    // ─────────────────────────────────────────────────────────────────────────────

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

    // ─────────────────────────────────────────────────────────────────────────────
    // Jednostki
    // ─────────────────────────────────────────────────────────────────────────────

    struct Warship {
        uint32_t      id{ 0 };
        uint32_t      ownerId{ 0 };
        uint32_t      homeBaseId{ 0 };
        HomeBaseType  homeBaseType{ HomeBaseType::Port };
        float         x{ 0.0f };
        float         y{ 0.0f };
        float         health{ 100.0f };
        float         maxHealth{ 100.0f };
        WarshipState  state{ WarshipState::Patrolling };
        uint32_t      targetId{ 0 };
    };

    struct Carrier {
        uint32_t      id{ 0 };
        uint32_t      ownerId{ 0 };
        uint32_t      seaProvinceId{ 0 };
        uint32_t      targetSeaProvinceId{ 0 };
        float         visualX{ 0.0f };
        float         visualY{ 0.0f };
        float         health{ 300.0f };
        float         maxHealth{ 300.0f };
        CarrierState  state{ CarrierState::Stationed };
        uint8_t       level{ 1 };
        std::vector<uint32_t> escortWarshipIds;
    };

    struct SeaCargo {
        uint32_t  id{ 0 };
        uint32_t  ownerId{ 0 };
        uint32_t  homePortId{ 0 };
        uint32_t  destinationPortId{ 0 };
        float     x{ 0.0f };
        float     y{ 0.0f };
        float     health{ 10.0f };
        float     value{ 0.0f };
        bool      isReturning{ false };
        std::vector<std::pair<float, float>> route;
    };

    struct Fighter {
        uint32_t      id{ 0 };
        uint32_t      ownerId{ 0 };
        uint32_t      homeBaseId{ 0 };
        HomeBaseType  homeBaseType{ HomeBaseType::Airport };
        float         x{ 0.0f };
        float         y{ 0.0f };
        float         health{ 80.0f };
        float         maxHealth{ 80.0f };
        FighterState  state{ FighterState::AtBase };
        uint32_t      targetId{ 0 };
        uint32_t      escortTargetId{ 0 };
    };

    struct Bomber {
        uint32_t  id{ 0 };
        uint32_t  ownerId{ 0 };
        uint32_t  homeAirportId{ 0 };
        float     x{ 0.0f };
        float     y{ 0.0f };
        float     health{ 60.0f };
        uint16_t  targetX{ 0 };
        uint16_t  targetY{ 0 };
        bool      hasReleased{ false };
    };

    struct AirCargo {
        uint32_t  id{ 0 };
        uint32_t  ownerId{ 0 };
        uint32_t  homeAirportId{ 0 };
        uint32_t  destinationAirportId{ 0 };
        float     x{ 0.0f };
        float     y{ 0.0f };
        float     health{ 30.0f };
        float     value{ 0.0f };
        bool      isReturning{ false };
    };

    struct MissileFlight {
        uint32_t  id{ 0 };
        uint32_t  ownerId{ 0 };
        float     currentX{ 0.0f };
        float     currentY{ 0.0f };
        float     targetX{ 0.0f };
        float     targetY{ 0.0f };
        bool      intercepted{ false };
        std::unordered_set<uint32_t> triedInterceptors;
    };

    // ─────────────────────────────────────────────────────────────────────────────
    // Encja
    // ─────────────────────────────────────────────────────────────────────────────

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

    // ─────────────────────────────────────────────────────────────────────────────
    // Dyplomacja
    // ─────────────────────────────────────────────────────────────────────────────

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
        std::unordered_map<uint32_t, VoteResult> votes;
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

    // ─────────────────────────────────────────────────────────────────────────────
    // Sieć elektryczna
    // ─────────────────────────────────────────────────────────────────────────────

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

    // ─────────────────────────────────────────────────────────────────────────────
    // Zdarzenia i inputy
    // ─────────────────────────────────────────────────────────────────────────────

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

    // ─────────────────────────────────────────────────────────────────────────────
    // GameState
    // ─────────────────────────────────────────────────────────────────────────────

    struct GameState {
        // Meta
        uint32_t   currentTick{ 0 };
        uint64_t   mapSeed{ 0 };
        GamePhase  phase{ GamePhase::Lobby };

        // Mapa
        uint16_t  mapWidth{ 0 };
        uint16_t  mapHeight{ 0 };

        std::vector<Tile>         tiles;
        std::vector<LandProvince> landProvinces;
        std::vector<SeaProvince>  seaProvinces;

        // Encje i budynki
        std::vector<Entity>   entities;
        std::vector<Building> buildings;

        // Jednostki
        std::vector<Warship>       warships;
        std::vector<Carrier>       carriers;
        std::vector<SeaCargo>      seaCargos;
        std::vector<Fighter>       fighters;
        std::vector<Bomber>        bombers;
        std::vector<AirCargo>      airCargos;
        std::vector<MissileFlight> missiles;

        // Ataki i dyplomacja
        std::vector<Attack>       attacks;
        std::vector<Relation>     relations;
        std::vector<Pact>         pacts;
        std::vector<ElectricGrid> electricGrids;

        // Per-tick bufory
        std::vector<GameEvent>   pendingEvents;
        std::vector<PlayerInput> pendingInputs;

        // Dirty tracking
        std::unordered_set<uint32_t>  dirtyTiles;
        std::unordered_set<uint32_t>  dirtyBuildings;
        std::unordered_set<uint32_t>  dirtyEntities;
        std::unordered_set<uint64_t>  dirtyShares;   // (provId<<32|entityId)

        // ── Helpers ───────────────────────────────────────────────────────────────

        void clearDirtyFlags() {
            dirtyTiles.clear();
            dirtyBuildings.clear();
            dirtyEntities.clear();
            dirtyShares.clear();
            pendingEvents.clear();
        }

        Entity* getEntity(uint32_t id) {
            if (id == 0 || id > entities.size()) return nullptr;
            auto& e = entities[id - 1];
            return (e.id == id) ? &e : nullptr;
        }

        Building* getBuilding(uint32_t id) {
            if (id == 0 || id > buildings.size()) return nullptr;
            auto& b = buildings[id - 1];
            return (b.id == id) ? &b : nullptr;
        }

        Tile* getTile(uint32_t id) {
            if (id == 0 || id > tiles.size()) return nullptr;
            return &tiles[id - 1];
        }

        LandProvince* getLandProvince(uint32_t id) {
            if (id == 0 || id > landProvinces.size()) return nullptr;
            return &landProvinces[id - 1];
        }

        SeaProvince* getSeaProvince(uint32_t id) {
            if (id == 0 || id > seaProvinces.size()) return nullptr;
            return &seaProvinces[id - 1];
        }

        PlayerProvinceShare* getShare(uint32_t provinceId, uint32_t entityId) {
            auto* p = getLandProvince(provinceId);
            return p ? p->getShare(entityId) : nullptr;
        }

        Relation* getRelation(uint32_t a, uint32_t b) {
            for (auto& r : relations) {
                if ((r.entityA == a && r.entityB == b) ||
                    (r.entityA == b && r.entityB == a))
                    return &r;
            }
            return nullptr;
        }

        Relation& getOrCreateRelation(uint32_t a, uint32_t b) {
            auto* r = getRelation(a, b);
            if (r) return *r;
            relations.push_back({ a, b, 0 });
            return relations.back();
        }

        bool isAtWar(uint32_t a, uint32_t b) const {
            for (const auto& r : relations) {
                if ((r.entityA == a && r.entityB == b) ||
                    (r.entityA == b && r.entityB == a))
                    return r.hasFlag(RelationFlag::AtWar);
            }
            return false;
        }

        bool inSameMilitaryPact(uint32_t a, uint32_t b) const {
            auto* ea = const_cast<GameState*>(this)->getEntity(a);
            auto* eb = const_cast<GameState*>(this)->getEntity(b);
            if (!ea || !eb) return false;
            return ea->militaryPactId != 0
                && ea->militaryPactId == eb->militaryPactId;
        }

        bool inSameEconomicPact(uint32_t a, uint32_t b) const {
            auto* ea = const_cast<GameState*>(this)->getEntity(a);
            auto* eb = const_cast<GameState*>(this)->getEntity(b);
            if (!ea || !eb) return false;
            return ea->economicPactId != 0
                && ea->economicPactId == eb->economicPactId;
        }

        void markTileDirty(uint32_t tid) { dirtyTiles.insert(tid); }
        void markBuildingDirty(uint32_t bid) { dirtyBuildings.insert(bid); }
        void markEntityDirty(uint32_t eid) { dirtyEntities.insert(eid); }
        void markShareDirty(uint32_t p, uint32_t e) {
            dirtyShares.insert((static_cast<uint64_t>(p) << 32) | e);
        }
    };

} // namespace gs::server