#pragma once
#include "server/sim/core/GameState.hpp"

namespace gs::server {

class PopulationSystem {
public:
    void tick(GameState& state);

private:
    float growthFormula    (float current, float cap) const;
    float computeMaxCap    (const GameState& state, const PlayerProvinceShare& share) const;
    void  adjustWorkerSplit(GameState& state, const LandProvince& prov,
                            PlayerProvinceShare& share) const;
    void  checkCapitalLoss (GameState& state) const;
    void  handleWorkerSplitInput(GameState& state, const PlayerInput& input) const;
    void  handleAutomationInput (GameState& state, const PlayerInput& input) const;
};

} // namespace gs::server
