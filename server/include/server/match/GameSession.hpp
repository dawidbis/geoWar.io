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
#include <map>

namespace gs::server::match {

    /// Zarządza pojedynczym aktywnym meczem, łącząc sieć z symulacją.
    /// W pełni bezpieczna wielowątkowo dzięki dedykowanemu kontekstowi strand.
    class GameSession : public std::enable_shared_from_this<GameSession>, public SessionBroadcaster {
    public:
        GameSession(boost::asio::io_context& ioc, uint32_t matchId, std::vector<management::Player> players);
        ~GameSession() override;

        /// Rozpoczyna proces generowania świata i uruchamia pętlę gry
        void start();

        /// Zatrzymuje pętlę gry i rozłącza graczy
        void stop();

        // ── Implementacja interfejsu SessionBroadcaster ─────────────────────
        void sendTo(uint32_t entityId, Message msg) override;
        void broadcast(const Message& msg, uint32_t excludeEntityId = 0) override;

    private:
        /// Obsługuje input w trakcie gry (Zsynchronizowane na strandzie gry)
        void handleMessage(uint32_t entityId, Message msg);

        /// Oznacza gracza jako rozłączonego (Zsynchronizowane na strandzie gry)
        void onPlayerDisconnect(uint32_t entityId);

        boost::asio::io_context& ioc_;

        // Strand chroniący całą sesję gry, jej stan oraz mapę graczy
        boost::asio::strand<boost::asio::any_io_executor> strand_;

        uint32_t matchId_;

        // Deterministyczna mapa graczy
        std::map<uint32_t, management::Player> players_;

        // DODAJ TO: Pancerne trzymanie wskaźników sieciowych wewnątrz sesji gry!
        std::vector<net::ConnectionPtr> sessionConnections_;

        // Komponenty symulacji
        GameState state_;
        GameLoop  gameLoop_;
        WorldGen  worldGen_;
    };

    using GameSessionPtr = std::shared_ptr<GameSession>;

} // namespace gs::server::match