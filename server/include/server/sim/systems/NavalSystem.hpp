#pragma once
#include "server/sim/core/GameState.hpp"

namespace gs::server {

    class NavalSystem {
    public:
        void tick(GameState& state);

    private:
        void tickWarships(GameState& state);
        void tickCarriers(GameState& state);
        void tickSeaCargos(GameState& state);

        void tickPatrol(GameState& state, Warship& ship);
        void tickEngage(GameState& state, Warship& ship);
        void tickReturn(GameState& state, Warship& ship);
        void tickResupply(GameState& state, Warship& ship);
        void tickMoveToBase(GameState& state, Warship& ship);

        void checkPiracy(GameState& state, Warship& ship, SeaCargo& cargo);

        void getHomeBasePosition(const GameState& state, const Warship& ship,
            float& outX, float& outY) const;
        void  moveToward(float& x, float& y, float tx, float ty, float speed) const;

        // ZMIANA: Szybsza i deterministyczna funkcja odleg³oœci
        float distanceSq(float ax, float ay, float bx, float by) const;

        void handleMoveCarrierInput(GameState& state, const PlayerInput& input);
        void handleSetHomeBaseInput(GameState& state, const PlayerInput& input);
    };

} // namespace gs::server