#pragma once
#include "server/sim/core/GameState.hpp"

namespace gs::server {

    class ElectricSystem {
    public:
        void tick(GameState& state);
        void rebuildGrids(GameState& state);

    private:
        void  balanceGrid(GameState& state, ElectricGrid& grid);
        void  tickUraniumProduction(GameState& state);
        void  handleCableInput(GameState& state, const PlayerInput& input);

        bool  isProducer(const Building& b) const;
        float computeOutput(const GameState& state, const Building& b) const;
        float powerDemand(const Building& b) const;
        int   getPriority(BuildingType type) const; // Nowa bezpieczna metoda
    };

} // namespace gs::server