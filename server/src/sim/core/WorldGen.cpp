#include "server/sim/core/WorldGen.hpp"
#include <shared/sim/Constants.hpp>

#include <algorithm>
#include <cmath>
#include <iostream>

namespace gs::server {

    void WorldGen::generate(GameState& state,
        const std::vector<LobbyPlayer>& players,
        uint64_t                        seed,
        uint16_t                        width,
        uint16_t                        height) {
        mapWidth_ = width;
        mapHeight_ = height;

        state.mapSeed = seed;
        state.mapWidth = width;
        state.mapHeight = height;

        std::cout << "[WorldGen] Generating map " << width << "x" << height
            << " seed=" << seed
            << " players=" << players.size() << "\n";

        // Etapy generacji
        generateTerrain(state, seed);
        generateSeaProvinces(state);

        // ~8-10× liczba graczy prowincji lądowych
        uint32_t numProvinces = static_cast<uint32_t>(players.size()) * 9;
        generateLandProvinces(state, numProvinces);

        computeAdjacencies(state);
        computeCoastlines(state);
        assignSpawnPoints(state, players);
        initEntities(state, players);

        std::cout << "[WorldGen] Done. Tiles: " << state.tiles.size()
            << "  LandProvinces: " << state.landProvinces.size()
            << "  SeaProvinces: " << state.seaProvinces.size()
            << "  Entities: " << state.entities.size() << "\n";
    }

    // ── Gwarantowany Cross-Platform RNG ───────────────────────────────────────────

    float WorldGen::deterministicFloat(uint64_t& prngState, float min, float max) const {
        // Klasyczny algorytm LCG (Linear Congruential Generator)
        prngState = prngState * 6364136223846793005ULL + 1442695040888963407ULL;
        // Bierzemy wyższe 32 bity, ponieważ są bardziej "pseudolosowe" niż niższe
        uint32_t random32 = static_cast<uint32_t>(prngState >> 32);

        // Normalizacja do przedziału [0.0, 1.0]
        float normalized = static_cast<float>(random32) / static_cast<float>(0xFFFFFFFF);
        return min + normalized * (max - min);
    }

    // ── Generacja terenu ──────────────────────────────────────────────────────────

    void WorldGen::generateTerrain(GameState& state, uint64_t seed) {
        state.tiles.clear();
        state.tiles.reserve(static_cast<size_t>(mapWidth_) * mapHeight_);

        float cx = mapWidth_ / 2.0f;
        float cy = mapHeight_ / 2.0f;
        float maxR = std::min(cx, cy) * 0.75f;

        // Inicjalizacja naszego bezpiecznego stanu PRNG
        uint64_t prngState = seed;

        uint32_t id = 1;
        for (uint16_t y = 0; y < mapHeight_; ++y) {
            for (uint16_t x = 0; x < mapWidth_; ++x) {
                Tile tile;
                tile.id = id++;
                tile.x = x;
                tile.y = y;

                float dx = (x - cx) / maxR;
                float dy = (y - cy) / maxR;

                // Używamy własnego LCG zamiast std::uniform_real_distribution
                float jitter = deterministicFloat(prngState, -0.05f, 0.05f);
                float r = std::sqrt(dx * dx + dy * dy) + jitter;

                if (r > 1.0f)       tile.terrain = TerrainType::DeepWater;
                else if (r > 0.85f) tile.terrain = TerrainType::ShallowWater;
                else if (r > 0.70f) tile.terrain = TerrainType::Highlands;
                else if (r > 0.55f && (x + y) % 7 == 0)
                    tile.terrain = TerrainType::Mountains;
                else                tile.terrain = TerrainType::Plains;

                state.tiles.push_back(tile);
            }
        }
    }

    // ── Prowincje morskie ─────────────────────────────────────────────────────────

    void WorldGen::generateSeaProvinces(GameState& state) {
        SeaProvince sea;
        sea.id = 1;
        sea.name = "Ocean";

        for (auto& tile : state.tiles) {
            if (isWater(tile.terrain)) {
                tile.seaProvinceId = 1;
                sea.tileIds.push_back(tile.id);
            }
        }

        if (!sea.tileIds.empty()) {
            float sumX = 0, sumY = 0;
            for (uint32_t tid : sea.tileIds) {
                auto& t = state.tiles[tid - 1];
                sumX += t.x;
                sumY += t.y;
            }
            sea.centerX = sumX / sea.tileIds.size();
            sea.centerY = sumY / sea.tileIds.size();
        }

        state.seaProvinces.push_back(std::move(sea));
    }

    // ── Prowincje lądowe ──────────────────────────────────────────────────────────

    void WorldGen::generateLandProvinces(GameState& state, uint32_t numProvinces) {
        std::vector<uint32_t> landTileIds;
        for (auto& tile : state.tiles) {
            if (!isWater(tile.terrain)) landTileIds.push_back(tile.id);
        }

        if (landTileIds.empty()) return;

        numProvinces = std::min(numProvinces, static_cast<uint32_t>(landTileIds.size()));
        numProvinces = std::max(numProvinces, 1u);

        state.landProvinces.resize(numProvinces);
        for (uint32_t i = 0; i < numProvinces; ++i) {
            state.landProvinces[i].id = i + 1;
            state.landProvinces[i].name = "Province " + std::to_string(i + 1);
        }

        for (size_t i = 0; i < landTileIds.size(); ++i) {
            uint32_t provIdx = static_cast<uint32_t>(i % numProvinces);
            uint32_t provId = provIdx + 1;
            uint32_t tileId = landTileIds[i];

            state.tiles[tileId - 1].landProvinceId = provId;
            state.landProvinces[provIdx].tileIds.push_back(tileId);
        }
    }

    // ── Sąsiedztwa ────────────────────────────────────────────────────────────────

    void WorldGen::computeAdjacencies(GameState& state) {
        for (size_t i = 0; i < state.landProvinces.size(); ++i) {
            auto& prov = state.landProvinces[i];
            if (i + 1 < state.landProvinces.size()) {
                prov.neighbors.push_back({ state.landProvinces[i + 1].id, 10 });
            }
            if (i > 0) {
                prov.neighbors.push_back({ state.landProvinces[i - 1].id, 10 });
            }
        }
    }

    // ── Przybrzeżność ─────────────────────────────────────────────────────────────

    void WorldGen::computeCoastlines(GameState& state) {
        if (state.seaProvinces.empty()) return;

        for (auto& prov : state.landProvinces) {
            bool isCoastal = false;
            for (uint32_t tid : prov.tileIds) {
                auto& t = state.tiles[tid - 1];
                if (t.x == 0 || t.y == 0 || t.x == mapWidth_ - 1 || t.y == mapHeight_ - 1) {
                    isCoastal = true;
                    break;
                }
            }
            if (isCoastal) {
                prov.coastalSeaProvinceIds.push_back(state.seaProvinces[0].id);
            }
        }
    }

    // ── Spawn pointy ──────────────────────────────────────────────────────────────

    void WorldGen::assignSpawnPoints(GameState& state,
        const std::vector<LobbyPlayer>& players) {

        if (state.landProvinces.empty()) return;

        for (size_t i = 0; i < players.size() && i < state.landProvinces.size(); ++i) {
            auto& prov = state.landProvinces[i];
            uint32_t eid = players[i].entityId;

            PlayerProvinceShare share;
            share.entityId = eid;
            share.provinceId = prov.id;
            share.ownedTileIds = prov.tileIds;
            share.population.workers = constants::START_WORKERS;
            share.population.military = constants::START_MILITARY;

            for (uint32_t tid : prov.tileIds) {
                state.tiles[tid - 1].ownerId = eid;
            }

            if (!prov.tileIds.empty()) {
                Building capital;
                capital.id = state.nextBuildingId();
                capital.type = BuildingType::City;
                capital.level = 1;
                capital.ownerId = eid;
                capital.landProvinceId = prov.id;
                capital.isCapital = true;
                capital.isActive = true;
                capital.isPowered = true;
                capital.constructionProgress = 1.0f;
                capital.supplyCapacity = 500.0f;
                capital.supplyStored = constants::START_SUPPLY;

                auto& tile = state.tiles[prov.tileIds[0] - 1];
                capital.topLeftX = tile.x;
                capital.topLeftY = tile.y;

                tile.buildingId = capital.id;
                share.buildingIds.push_back(capital.id);

                state.buildings.push_back(capital);
            }

            prov.playerShares[eid] = std::move(share);
        }
    }

    // ── Inicjalizacja encji ───────────────────────────────────────────────────────

    void WorldGen::initEntities(GameState& state,
        const std::vector<LobbyPlayer>& players) {
        for (auto& player : players) {
            auto* entity = state.getEntity(player.entityId);
            if (!entity) continue;

            entity->gold = constants::START_GOLD;

            for (auto& building : state.buildings) {
                if (building.ownerId == player.entityId &&
                    building.isCapital) {
                    entity->capitalBuildingId = building.id;
                    break;
                }
            }
        }
    }

    // ── Helpers ───────────────────────────────────────────────────────────────────

    float WorldGen::noise(float x, float y, uint64_t seed) const {
        uint64_t h = seed
            ^ (static_cast<uint64_t>(x * 1000) * 2654435761ULL)
            ^ (static_cast<uint64_t>(y * 1000) * 2246822519ULL);
        return static_cast<float>(h & 0xFFFF) / 65535.0f;
    }

    float WorldGen::tileDistance(uint16_t ax, uint16_t ay,
        uint16_t bx, uint16_t by) const {
        float dx = static_cast<float>(ax) - static_cast<float>(bx);
        float dy = static_cast<float>(ay) - static_cast<float>(by);
        return std::sqrt(dx * dx + dy * dy);
    }

} // namespace gs::server