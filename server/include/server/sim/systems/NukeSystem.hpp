#pragma once
#include "server/sim/core/GameState.hpp"
#include <vector>

namespace gs::server {

    class NukeSystem {
    public:
        void tick(GameState& state);

    private:
        void tickMissiles(GameState& state);
        void tryIntercept(GameState& state, MissileFlight& missile);
        void detonate(GameState& state, float cx, float cy, int radius);
        void applyPopulationDamage(GameState& state,
            const std::vector<uint32_t>& tileIds,
            float cx, float cy);
        void destroyBuildings(GameState& state, const std::vector<uint32_t>& tileIds);
        void applyTerrainChange(GameState& state, const std::vector<uint32_t>& tileIds);

        std::vector<uint32_t> getLandTilesInRadius(const GameState& state,
            float cx, float cy,
            int radius) const;

        void handleLaunchMissileInput(GameState& state, const PlayerInput& input);
        void handleNuclearModeInput(GameState& state, const PlayerInput& input);
    };

} // namespace gs::server