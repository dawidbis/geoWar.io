#pragma once
#include "server/management/Player.hpp"
#include "server/sim/core/GameLoop.hpp"
#include "server/sim/core/GameState.hpp"
#include "server/sim/core/WorldGen.hpp"
#include <shared/net/Message.hpp>

#include <boost/asio.hpp>
#include <cstdint>
#include <memory>
#include <vector>
#include <map> // ZMIANA: używamy std::map dla 100% determinizmu

namespace gs::server::match {

    /// Zarządza pojedynczym aktywnym meczem, łącząc sieć z symulacją.
    class GameSession : public std::enable_shared_from_this<GameSession>, public SessionBroadcaster {
    public:
        GameSession(boost::asio::io_context& ioc, uint32_t matchId, std::vector<management::Player> players);
        ~GameSession() override;

        /// Rozpoczyna proces generowania świata i uruchamia pętlę gry
        void start();

        /// Zatrzymuje pętlę gry i rozłącza graczy (jeśli to koniec)
        void stop();

        // ── Implementacja interfejsu SessionBroadcaster ─────────────────────
        void sendTo(uint32_t entityId, Message msg) override;
        void broadcast(const Message& msg, uint32_t excludeEntityId = 0) override;

    private:
        /// Obsługuje input w trakcie gry
        void handleMessage(uint32_t entityId, Message msg);

        /// Oznacza gracza jako rozłączonego (bot może przejąć kontrolę)
        void onPlayerDisconnect(uint32_t entityId);

        boost::asio::io_context& ioc_;

        // ZMIANA: Własny strand do chronienia kontenera players_ przed wyścigami
        boost::asio::strand<boost::asio::any_io_executor> strand_;

        uint32_t matchId_;

        // ZMIANA: std::map gwarantuje zawsze tę samą kolejność (po entityId),
        // co zapobiega desynchronizacji (Desync) przy WorldGen i Broadcast!
        std::map<uint32_t, management::Player> players_;

        // Komponenty symulacji
        GameState state_;
        GameLoop  gameLoop_;
        WorldGen  worldGen_;
    };

    using GameSessionPtr = std::shared_ptr<GameSession>;

} // namespace gs::server::match