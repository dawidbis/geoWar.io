#pragma once
#include "GameState.hpp"
#include <server/sim/systems/CombatSystem.hpp>
#include <server/sim/systems/PopulationSystem.hpp>
#include <server/sim/systems/SupplySystem.hpp>
#include <server/sim/systems/ElectricSystem.hpp>
#include <server/sim/systems/TradeSystem.hpp>
#include <server/sim/systems/NavalSystem.hpp>
#include <server/sim/systems/AirSystem.hpp>
#include <server/sim/systems/NukeSystem.hpp>
#include <server/sim/systems/DiplomacySystem.hpp>
#include <shared/net/Message.hpp>

#include <boost/asio.hpp>
#include <boost/asio/awaitable.hpp>
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>

namespace gs::server {

    class SessionBroadcaster {
    public:
        virtual ~SessionBroadcaster() = default;
        virtual void sendTo(uint32_t entityId, Message msg) = 0;
        virtual void broadcast(const Message& msg, uint32_t excludeEntityId = 0) = 0;
    };

    class GameLoop {
    public:
        GameLoop(boost::asio::io_context& ioc,
            GameState& state,
            SessionBroadcaster& broadcaster);
        ~GameLoop();

        void start();
        void stop();
        void stepTicks(uint32_t n);
        void setTickrate(uint32_t hz);

        void enqueueInput(PlayerInput input);
        void enqueueDisconnect(uint32_t entityId);

        bool isRunning() const noexcept { return running_.load(std::memory_order_relaxed); }

    private:
        /// Główna pętla gry zrealizowana jako korutyna
        boost::asio::awaitable<void> runLoopTask();

        void tick();
        void processInputs();
        void checkVictory();
        Message buildDelta() const;
        Message buildFullSnapshot() const;

        boost::asio::io_context& ioc_;
        boost::asio::steady_timer timer_;
        GameState& state_;
        SessionBroadcaster& broadcaster_;

        // Ochrona wielowątkowa dla logiki gry
        boost::asio::strand<boost::asio::any_io_executor> strand_;

        std::atomic<bool>     running_{ false };
        std::atomic<uint32_t> tickrateHz_{ 10 };

        bool     debugPaused_{ false };
        uint32_t debugStepsRemaining_{ 0 };

        // Systemy symulacji
        CombatSystem     combatSys_;
        PopulationSystem populationSys_;
        SupplySystem     supplySys_;
        ElectricSystem   electricSys_;
        TradeSystem      tradeSys_;
        NavalSystem      navalSys_;
        AirSystem        airSys_;
        NukeSystem       nukeSys_;
        DiplomacySystem  diplomacySys_;
    };

} // namespace gs::server