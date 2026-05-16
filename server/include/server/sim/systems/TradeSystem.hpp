#pragma once
#include "server/sim/core/GameState.hpp"

namespace gs::server {

class TradeSystem {
public:
    void tick(GameState& state);

private:
    void  generateLandTrade(GameState& state, Building& factory);
    void  generateSeaTrade (GameState& state, Building& port);
    void  generateAirTrade (GameState& state, Building& airport);
    void  processDeliveries(GameState& state);

    float computeGold      (const GameState& state, uint32_t srcEntity,
                             uint32_t dstEntity, float distance,
                             float baseValue, float levelFactor) const;
    float partnerTypeFactor(const GameState& state, uint32_t a, uint32_t b) const;
    void  distributeGold   (GameState& state, uint32_t srcEntity,
                             uint32_t dstEntity, float gold) const;
};

} // namespace gs::server
