#pragma once
#include "GameState.hpp"
#include <shared/net/Message.hpp>

#include <boost/asio.hpp>
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>

namespace gs::server {

    // ── Broadcaster interface ─────────────────────────────────────────────────────
    // GameLoop nie zna LobbyManager bezpośrednio — komunikuje się przez interfejs.
    // Dzięki temu GameLoop.hpp nie includuje LobbyManager.hpp (unikamy głębokiego
    // łańcucha includów przez boost::asio).

    class SessionBroadcaster {
    public:
        virtual ~SessionBroadcaster() = default;
        virtual void sendTo(uint32_t entityId, Message msg) = 0;
        virtual void broadcast(const Message& msg, uint32_t excludeEntityId = 0) = 0;
    };

    // ── GameLoop ──────────────────────────────────────────────────────────────────

    class GameLoop {
    public:
        GameLoop(boost::asio::io_context& ioc,
            GameState& state,
            SessionBroadcaster& broadcaster);

        ~GameLoop();

        void start();
        void stop();

        /// Tryb debug — wykonaj N ticków i wstrzymaj
        void stepTicks(uint32_t n);

        /// Zmień tickrate w runtime
        void setTickrate(uint32_t hz);

        /// Dodaj input gracza (thread-safe)
        void enqueueInput(PlayerInput input);

        bool isRunning() const noexcept { return running_.load(); }

    private:
        void scheduleTick();
        void tick();
        void processInputs();
        void checkVictory();
        Message buildDelta() const;

        boost::asio::io_context& ioc_;
        boost::asio::steady_timer timer_;
        GameState& state_;
        SessionBroadcaster& broadcaster_;

        std::atomic<bool>     running_{ false };
        std::atomic<uint32_t> tickrateHz_{ 10 };

        bool     debugPaused_{ false };
        uint32_t debugStepsRemaining_{ 0 };

        boost::asio::strand<boost::asio::io_context::executor_type> strand_;
    };

} // namespace gs::server