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
            uint32_t connId = conn->id();

            // KROK 1: Zapisujemy po³¹czenie w kontenerze tymczasowym mened¿era.
            // Licznik shared_ptr wzrasta, obiekt NIE MA PRAWA umrzeæ w locie!
            pendingConnections_[connId] = conn;

            auto lobby = getOrCreateOpenLobby();

            lobby->addConnection(conn, [this, lobby, conn, connId](bool success) {
                if (success) {
                    std::cout << "[MatchmakingManager] Connection " << connId
                        << " successfully routed to Lobby " << lobby->roomId() << ".\n";

                    // KROK 2: Podpinamy callback roz³¹czenia pod nasz tymczasowy kontener
                    conn->start(
                        [lobby](net::ConnectionPtr c, Message m) {
                            lobby->handleMessage(c, std::move(m));
                        },
                        [this, lobby, connId](net::ConnectionPtr c) {
                            lobby->removeConnection(c->id());

                            // Bezpiecznie usuwamy z oczekuj¹cych na strandzie mened¿era w razie awarii
                            boost::asio::post(this->strand_, [this, connId]() {
                                this->pendingConnections_.erase(connId);
                                });
                        }
                    );
                }
                else {
                    std::cerr << "[MatchmakingManager] Lobby refused connection.\n";
                    // Jeœli lobby odrzuci po³¹czenie, natychmiast je usuwamy
                    this->pendingConnections_.erase(connId);
                }
                });
            });
    }

    // Wewn¹trz MatchmakingManager::onLobbyStartGame:
    void MatchmakingManager::onLobbyStartGame(LobbyRoomPtr lobby) {
        lobby->extractPlayersAsync([this, lobby](std::vector<Player> players) {
            boost::asio::post(strand_, [this, lobby, players = std::move(players)]() mutable {
                uint32_t matchId = lobby->roomId();

                std::cout << "[MatchmakingManager] Launching game session for Match ID: " << matchId << "\n";

                // Tworzymy sesjê gry
                auto session = std::make_shared<match::GameSession>(ioc_, matchId, std::move(players));
                activeMatches_[matchId] = session;

                // KROK 3: Najpierw odpalamy grê, ¿eby przejê³a w³asnoœæ nad socketami!
                session->start();

                // Dopiero teraz bezpiecznie usuwamy lobby
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
        auto newLobby = std::make_shared<LobbyRoom>(ioc_, roomId, 20);
        // Zg³oœ mened¿erowi start gry, gdy lobby odliczy czas
        newLobby->onStartGameRequested([this](LobbyRoomPtr room) {
            this->onLobbyStartGame(room);
            });

        activeLobbies_[roomId] = newLobby;
        std::cout << "[MatchmakingManager] Created new LobbyRoom " << roomId << "\n";

        return newLobby;
    }

} // namespace gs::server::management