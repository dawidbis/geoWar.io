#pragma once
#include "server/sim/core/GameState.hpp"

namespace gs::server {

    class DiplomacySystem {
    public:
        void tick(GameState& state);

    private:
        void processWarDeclaration(GameState& state, const PlayerInput& input);
        void processPactProposal(GameState& state, const PlayerInput& input);
        void processPactVote(GameState& state, const PlayerInput& input);
        void processPactLeave(GameState& state, const PlayerInput& input);
        void processProvinceSale(GameState& state, const PlayerInput& input);

        void tickPactVotings(GameState& state);
        void tickTraitorDebuffs(GameState& state);

        void applyTraitorDebuff(GameState& state, uint32_t entityId,
            uint32_t durationTicks);
        void addEntityToPact(GameState& state, uint32_t entityId, uint32_t pactId);
        void leavePact(GameState& state, uint32_t entityId, uint32_t pactId);
    };

} // namespace gs::server