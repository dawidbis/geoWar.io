#include "server/management/MatchmakingManager.hpp"
#include "server/match/GameSession.hpp"

#include <iostream>

namespace gs::server::management {

    MatchmakingManager::MatchmakingManager(boost::asio::io_context& ioc)
        : ioc_(ioc)
        , strand_(boost::asio::make_strand(ioc)) // Inicjalizacja stranda
    {
    }
  
    void MatchmakingManager::handleNewConnection(net::ConnectionPtr conn) {
        boost::asio::post(strand_, [this, conn]() {
            auto lobby = getOrCreateOpenLobby();

            // Rejestrujemy po³¹czenie w lobby
            lobby->addConnection(conn, [this, lobby, conn](bool success) {
                if (success) {
                    std::cout << "[MatchmakingManager] Connection " << conn->id()
                        << " successfully routed to Lobby " << lobby->roomId() << ".\n";

                    // ZMIANA KRYTYCZNA: Odpalamy nas³uchiwanie korutynowe bota,
                    // przekazuj¹c mu routing bezpoœrednio do metod LobbyRoom!
                    conn->start(
                        [lobby](net::ConnectionPtr c, Message m) {
                            lobby->handleMessage(c, std::move(m));
                        },
                        [lobby](net::ConnectionPtr c) {
                            lobby->removeConnection(c->id());
                        }
                    );
                }
                else {
                    std::cerr << "[MatchmakingManager] Lobby refused connection.\n";
                }
                });
            });
    }

    // Wewn¹trz MatchmakingManager::onLobbyStartGame:
    void MatchmakingManager::onLobbyStartGame(LobbyRoomPtr lobby) {
        // Wyci¹gamy graczy asynchronicznie poprzez bezpieczny callback
        lobby->extractPlayersAsync([this, lobby](std::vector<Player> players) {
            boost::asio::post(strand_, [this, lobby, players = std::move(players)]() mutable {
                uint32_t matchId = lobby->roomId();

                std::cout << "[MatchmakingManager] Launching game session for Match ID: " << matchId << "\n";
                // Tutaj tworzysz instancjê GameSession(ioc_, matchId, std::move(players)) ...

                activeLobbies_.erase(matchId);
                });
            });
    }

    // Upewnij siê, ¿e ta definicja istnieje i ma dok³adnie tak¹ sygnaturê:
    LobbyRoomPtr MatchmakingManager::getOrCreateOpenLobby() {
        // Przeszukaj aktywne pokoje w poszukiwaniu otwartego
        for (auto& [id, lobby] : activeLobbies_) {
            if (lobby && lobby->isOpen()) {
                return lobby;
            }
        }

        // Jeœli nie ma otwartego pokoju, stwórz nowy
        uint32_t roomId = nextRoomId_++;
        auto newLobby = std::make_shared<LobbyRoom>(ioc_, roomId);

        // Zg³oœ mened¿erowi start gry, gdy lobby odliczy czas
        newLobby->onStartGameRequested([this](LobbyRoomPtr room) {
            this->onLobbyStartGame(room);
            });

        activeLobbies_[roomId] = newLobby;
        std::cout << "[MatchmakingManager] Created new LobbyRoom " << roomId << "\n";

        return newLobby;
    }

} // namespace gs::server::management