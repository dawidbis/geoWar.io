#include "server/match/GameSession.hpp"
#include "server/net/Connection.hpp"
#include <shared/net/MessageTypes.hpp>
#include <shared/net/Serializer.hpp>

#include <iostream>
#include <chrono>

namespace gs::server::match {

    GameSession::GameSession(boost::asio::io_context& ioc, uint32_t matchId, std::vector<management::Player> players)
        : ioc_(ioc)
        , strand_(boost::asio::make_strand(ioc))
        , matchId_(matchId)
        , gameLoop_(ioc, state_, *this) // GameSession wstrzykuje samo siebie jako SessionBroadcaster
    {
        // Przenosimy graczy do deterministycznej mapy O(log N)
        for (auto& p : players) {
            uint32_t eid = p.entityId();
            players_.emplace(eid, std::move(p));
        }
    }

    GameSession::~GameSession() {
        stop();
    }

    void GameSession::start() {
        std::cout << "[GameSession " << matchId_ << "] Initializing match...\n";

        // 1. Zmiana nasłuchu w połączeniach z Lobby na GameSession
        for (auto& [eid, player] : players_) {
            if (auto conn = player.connection()) {
                std::weak_ptr<GameSession> weakSelf = shared_from_this();
                uint32_t connectionEntityId = eid;

                // Od teraz, gdy przyjdzie pakiet, trafi on do GameSession::handleMessage
                conn->start(
                    [weakSelf, connectionEntityId](net::ConnectionPtr /*c*/, Message msg) {
                        if (auto self = weakSelf.lock()) {
                            self->handleMessage(connectionEntityId, std::move(msg));
                        }
                    },
                    [weakSelf, connectionEntityId](net::ConnectionPtr /*c*/) {
                        if (auto self = weakSelf.lock()) {
                            self->onPlayerDisconnect(connectionEntityId);
                        }
                    }
                );
            }
        }

        // 2. Inicjalizacja meta-danych w GameState
        for (const auto& [eid, p] : players_) {
            Entity entity;
            entity.id = eid;
            entity.name = p.name();
            entity.type = p.isBot() ? EntityType::Bot : EntityType::Human;
            state_.entities.push_back(entity);
        }

        // 3. Generowanie świata
        uint64_t seed = static_cast<uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());

        std::vector<LobbyPlayer> legacyPlayers;
        // Ponieważ używamy std::map, pętla TA zawsze wykona się identycznie! (Zapobieganie Desync)
        for (const auto& [eid, p] : players_) {
            LobbyPlayer lp;
            lp.entityId = eid;
            lp.name = p.name();
            lp.isBot = p.isBot();
            legacyPlayers.push_back(lp);
        }

        worldGen_.generate(state_, legacyPlayers, seed);
        state_.phase = GamePhase::Playing;

        // 4. Poinformuj klientów o starcie gry i wyślij konfigurację
        auto startMsg = messages::makeLobbyStartGame(constants::TICKRATE_HZ, seed);
        broadcast(startMsg);

        std::cout << "[GameSession " << matchId_ << "] Starting GameLoop...\n";
        gameLoop_.start();
    }

    void GameSession::stop() {
        gameLoop_.stop();
        std::cout << "[GameSession " << matchId_ << "] Match stopped.\n";
    }

    // ── Obsługa sieci (w trakcie gry) ───────────────────────────────────────

    void GameSession::handleMessage(uint32_t entityId, Message msg) {
        if (msg.header.type == MessageType::PlayerInput) {
            try {
                Serializer s(std::span<const uint8_t>(msg.payload));

                PlayerInput input;
                input.entityId = entityId;
                input.type = static_cast<InputType>(s.readU8());
                input.provinceId = s.readU32();
                input.targetProvinceId = s.readU32();
                input.buildingType = s.readU32();
                input.unitId = s.readU32();
                input.targetId = s.readU32();
                input.floatParam = s.readF32();
                input.tileX = s.readU16();
                input.tileY = s.readU16();

                uint16_t multiCount = s.readU16();
                input.multiTargets.reserve(multiCount);
                for (uint16_t i = 0; i < multiCount; ++i) {
                    uint32_t src = s.readU32();
                    uint32_t tgt = s.readU32();
                    input.multiTargets.push_back({ src, tgt });
                }

                // Wrzucamy do kolejki GameLoop - to jest bezpieczne, bo GameLoop też ma stranda!
                gameLoop_.enqueueInput(std::move(input));
            }
            catch (const std::exception& e) {
                std::cerr << "[GameSession " << matchId_ << "] Malformed PlayerInput from entity "
                    << entityId << ": " << e.what() << "\n";
                sendTo(entityId, messages::makeError(0x90, "Malformed PlayerInput payload"));
            }
        }
        else if (msg.header.type == MessageType::DebugStep) {
            Serializer s(std::span<const uint8_t>(msg.payload));
            gameLoop_.stepTicks(s.readU32());
        }
        else if (msg.header.type == MessageType::DebugSetTickrate) {
            Serializer s(std::span<const uint8_t>(msg.payload));
            gameLoop_.setTickrate(s.readU32());
        }
    }

    void GameSession::onPlayerDisconnect(uint32_t entityId) {
        // Wątek sieciowy wywołuje ten callback. Przekazujemy to na nasz bezpieczny strand!
        boost::asio::post(strand_, [self = shared_from_this(), entityId]() {
            auto it = self->players_.find(entityId);
            if (it != self->players_.end()) {
                std::cout << "[GameSession " << self->matchId_ << "] Player '" << it->second.name() << "' disconnected mid-game.\n";
                it->second.setConnection(nullptr);

                self->gameLoop_.enqueueDisconnect(entityId);
            }
            });
    }

    // ── Implementacja SessionBroadcaster ────────────────────────────────────

    void GameSession::sendTo(uint32_t entityId, Message msg) {
        // GameLoop zrzuca żądanie wysyłki na strand GameSession, by uniknąć wyścigów pamięci
        boost::asio::post(strand_, [self = shared_from_this(), entityId, msg = std::move(msg)]() {
            auto it = self->players_.find(entityId);
            if (it != self->players_.end() && it->second.isConnected()) {
                it->second.send(msg); // Player::send zajmuje się dalszym thread-safety
            }
            });
    }

    void GameSession::broadcast(const Message& msg, uint32_t excludeEntityId) {
        // Asio przechwytuje kopię 'msg' i wrzuca do kolejki. Kiedy wątek na strandzie do niej dotrze,
        // roześle ją bezpiecznie do graczy.
        boost::asio::post(strand_, [self = shared_from_this(), msg, excludeEntityId]() {
            for (auto& [eid, player] : self->players_) {
                if (eid != excludeEntityId && player.isConnected()) {
                    player.send(msg);
                }
            }
            });
    }

} // namespace gs::server::match