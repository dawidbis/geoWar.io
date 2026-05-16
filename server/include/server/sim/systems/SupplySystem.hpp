#pragma once
#include "server/sim/core/GameState.hpp"

namespace gs::server {

class SupplySystem {
public:
    void tick(GameState& state);

private:
    void  produceSupply      (GameState& state);
    void  consumeCombatSupply(GameState& state);
    void  distributeSupply   (GameState& state);
    void  tryPullLocalSupply (GameState& state, Attack& attack, float needed);

    float workerEfficiency      (float workers, int level) const;
    float getWorkersAssigned    (const GameState& state, const Building& b) const;
    float computeTotalCapacity  (const GameState& state,
                                 const PlayerProvinceShare& share) const;
};

} // namespace gs::server
