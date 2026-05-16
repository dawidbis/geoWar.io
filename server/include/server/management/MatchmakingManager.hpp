#pragma once
#include "server/management/LobbyRoom.hpp"
#include "server/net/Connection.hpp"

#include <boost/asio.hpp>
#include <memory>
#include <unordered_map>

// ZapowiedŸ klasy, któr¹ zrobimy w nastêpnym kroku (Match)
namespace gs::server::match {
    class GameSession;
    using GameSessionPtr = std::shared_ptr<GameSession>;
}

namespace gs::server::management {

    /// Zarz¹dza cyklem ¿ycia poczekalni (LobbyRoom) i instancji gier (GameSession).
    /// Odpowiada za kierowanie nowych graczy do odpowiednich pokoi.
    /// W pe³ni asynchroniczna i bez-mutexowa dziêki u¿yciu boost::asio::strand.
    class MatchmakingManager {
    public:
        MatchmakingManager(boost::asio::io_context& ioc);

        /// Wywo³ywane przez serwer TCP, gdy nadejdzie nowe po³¹czenie.
        void handleNewConnection(net::ConnectionPtr conn);

    private:
        // Te funkcje prywatne zak³adaj¹, ¿e s¹ ju¿ wywo³ywane w obrêbie strand_
        LobbyRoomPtr getOrCreateOpenLobby();
        void onLobbyStartGame(LobbyRoomPtr lobby);

        boost::asio::io_context& ioc_;

        // Strand zapewniaj¹cy sekwencyjny i bezpieczny dostêp do zmiennych poni¿ej
        boost::asio::strand<boost::asio::any_io_executor> strand_;

        // nextRoomId_ NIE MUSI BYÆ ju¿ std::atomic, poniewa¿ modyfikujemy go 
        // tylko i wy³¹cznie w kontekœcie bezpiecznego stranda!
        uint32_t nextRoomId_{ 1 };

        // Kontenery na aktywne pokoje i trwaj¹ce mecze
        std::unordered_map<uint32_t, LobbyRoomPtr> activeLobbies_;
        std::unordered_map<uint32_t, match::GameSessionPtr> activeMatches_;

        // DODAJ TO: Trzyma po³¹czenia przy ¿yciu na czas negocjacji ClientHello
        std::unordered_map<uint32_t, net::ConnectionPtr> pendingConnections_;
    };

} // namespace gs::server::management