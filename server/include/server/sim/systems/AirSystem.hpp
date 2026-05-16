#pragma once
#include "server/sim/core/GameState.hpp"

namespace gs::server {

    class AirSystem {
    public:
        void tick(GameState& state);

    private:
        void tickFighters(GameState& state);
        void tickBombers(GameState& state);
        void tickAirCargos(GameState& state);
        void tickAntiAir(GameState& state);

        void tickFighterEngage(GameState& state, Fighter& fighter);
        void tickFighterEscort(GameState& state, Fighter& fighter);

        void getBasePosition(const GameState& state, const Fighter& f,
            float& outX, float& outY) const;
        float getPatrolRadius(const GameState& state, const Fighter& f) const;

        uint32_t findEnemyInZone(const GameState& state, const Fighter& f,
            float cx, float cy, float radius) const;
        uint32_t findEnemyNearPoint(const GameState& state, uint32_t ownerId,
            float px, float py, float radius) const;

        void  moveStraight(float& x, float& y, float tx, float ty, float speed) const;

        // ZMIANA: Zastêpujemy distance() szybszym i bezpieczniejszym distanceSq()
        float distanceSq(float ax, float ay, float bx, float by) const;

        void handleLaunchBomberInput(GameState& state, const PlayerInput& input);
    };

} // namespace gs::server