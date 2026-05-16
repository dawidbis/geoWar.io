#pragma once
#include "server/sim/core/GameState.hpp"
#include <vector>
#include <string>
#include <cstdint>

namespace gs::server {

    // Tymczasowa struktura przenosząca dane graczy z sieci do generatora mapy
    struct LobbyPlayer {
        uint32_t    entityId;
        std::string name;
        bool        isBot{ false };
    };

    class WorldGen {
    public:
        void generate(GameState& state,
            const std::vector<LobbyPlayer>& players,
            uint64_t seed,
            uint16_t width = 100,
            uint16_t height = 100);

    private:
        void generateTerrain(GameState& state, uint64_t seed);
        void generateSeaProvinces(GameState& state);
        void generateLandProvinces(GameState& state, uint32_t numProvinces);
        void computeAdjacencies(GameState& state);
        void computeCoastlines(GameState& state);
        void assignSpawnPoints(GameState& state, const std::vector<LobbyPlayer>& players);
        void initEntities(GameState& state, const std::vector<LobbyPlayer>& players);

        float noise(float x, float y, uint64_t seed) const;
        float tileDistance(uint16_t ax, uint16_t ay, uint16_t bx, uint16_t by) const;

        // ZMIANA: Własna, 100% deterministyczna funkcja RNG.
        // Działa identycznie na Windowsie (MSVC), MacOS (Clang) i Linuxie (GCC).
        float deterministicFloat(uint64_t& prngState, float min, float max) const;

        uint16_t mapWidth_{ 0 };
        uint16_t mapHeight_{ 0 };
    };

} // namespace gs::server