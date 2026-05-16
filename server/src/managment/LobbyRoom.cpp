#include "server/management/LobbyRoom.hpp"
#include "server/net/Connection.hpp"
#include <shared/net/MessageTypes.hpp>
#include <shared/net/Serializer.hpp>

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <iostream>
#include <span>

namespace gs::server::management {

    LobbyRoom::LobbyRoom(boost::asio::io_context& ioc, uint32_t roomId, uint32_t maxPlayers)
        : ioc_(ioc)
        , strand_(boost::asio::make_strand(ioc))
        , countdownTimer_(ioc)
        , roomId_(roomId)
        , maxPlayers_(maxPlayers)
    {
    }

    LobbyRoom::~LobbyRoom() {
        countdownTimer_.cancel();
    }

    void LobbyRoom::updateOpenState() {
        bool open = !gameStarted_ && !isLobbyPaused_ && players_.size() < maxPlayers_;
        isOpen_.store(open, std::memory_order_relaxed);
    }

    void LobbyRoom::addConnection(net::ConnectionPtr conn, std::function<void(bool)> completionCb) {
        boost::asio::post(strand_, [this, conn, completionCb]() {
            if (gameStarted_ || isLobbyPaused_ || players_.size() >= maxPlayers_) {
                // Przekazanie operacji sieciowej na executor konkretnego gniazda (brak wyścigów wątków)
                boost::asio::post(conn->socket().get_executor(), [conn]() {
                    conn->send(messages::makeError(0x04, "Lobby is inaccessible (full, paused or started)"));
                    });
                completionCb(false);
            }
            else {
                completionCb(true);
            }
            });
    }

    void LobbyRoom::removeConnection(uint32_t connectionId) {
        boost::asio::post(strand_, [self = shared_from_this(), connectionId]() {
            auto it = self->connectionToEntity_.find(connectionId);
            if (it != self->connectionToEntity_.end()) {
                uint32_t entityId = it->second;
                self->players_.erase(entityId);
                self->connectionToEntity_.erase(it);

                self->playerCount_.store(self->players_.size(), std::memory_order_relaxed);
                self->updateOpenState();

                std::cout << "[LobbyRoom " << self->roomId_ << "] Connection lost. Checking conditions.\n";

                self->checkCountdownConditions();
                self->broadcastLobbyState();
            }
            });
    }

    void LobbyRoom::handleMessage(net::ConnectionPtr conn, Message msg) {
        boost::asio::post(strand_, [self = shared_from_this(), conn, msg = std::move(msg)]() mutable {
            self->processMessage(conn, msg);
            });
    }

    void LobbyRoom::processMessage(net::ConnectionPtr conn, Message& msg) {
        switch (msg.header.type) {
        case MessageType::ClientHello:
            handleClientHello(conn, msg);
            break;
        case MessageType::LobbyReady:
            handleLobbyReady(conn, msg);
            break;
        case MessageType::DebugLobbyConfig:
            handleDebugLobbyConfig(conn, msg);
            break;
        case MessageType::DebugStartGame: {
            if (!gameStarted_ && onStartGame_) {
                stopCountdown();
                gameStarted_ = true;
                updateOpenState();
                onStartGame_(shared_from_this());
            }
            break;
        }
        default: break;
        }
    }

    void LobbyRoom::handleClientHello(net::ConnectionPtr conn, Message& msg) {
        Serializer s(std::span<const uint8_t>(msg.payload));
        std::string name;
        try { name = s.readStr(); }
        catch (...) {
            boost::asio::post(conn->socket().get_executor(), [conn]() { conn->disconnect(); });
            return;
        }

        if (gameStarted_ || players_.size() >= maxPlayers_) {
            boost::asio::post(conn->socket().get_executor(), [conn]() { conn->disconnect(); });
            return;
        }

        uint32_t entityId = nextEntityId_++;
        Player newPlayer(entityId, name, false, conn);
        players_.emplace(entityId, std::move(newPlayer));
        connectionToEntity_[conn->id()] = entityId;

        playerCount_.store(players_.size(), std::memory_order_relaxed);
        updateOpenState();

        conn->setEntityId(entityId);

        // Zsynchronizowany zapis powitalny przez executor przypisany do połączenia bota
        boost::asio::post(conn->socket().get_executor(), [conn, entityId, name]() {
            conn->send(messages::makeServerWelcome(entityId, name));
            });

        checkCountdownConditions();
        broadcastLobbyState();
    }

    void LobbyRoom::handleLobbyReady(net::ConnectionPtr conn, Message& msg) {
        Serializer s(std::span<const uint8_t>(msg.payload));
        bool readyStatus = false;
        try { readyStatus = s.readBool(); }
        catch (...) { return; }

        auto it = connectionToEntity_.find(conn->id());
        if (it == connectionToEntity_.end()) return;

        auto playerIt = players_.find(it->second);
        if (playerIt != players_.end()) {
            playerIt->second.setReady(readyStatus);
            std::cout << "[LobbyRoom " << roomId_ << "] Player " << playerIt->second.name()
                << " set READY = " << std::boolalpha << readyStatus << "\n";

            checkCountdownConditions();
            broadcastLobbyState();
        }
    }

    void LobbyRoom::handleDebugLobbyConfig(net::ConnectionPtr conn, Message& msg) {
        try {
            Serializer s(std::span<const uint8_t>(msg.payload));

            minPlayersToStart_ = s.readU32();
            maxPlayers_ = s.readU32();
            isLobbyPaused_ = s.readBool();
            updateOpenState();

            std::cout << "[LobbyRoom " << roomId_ << "] DEV CONFIG UPDATED: MinPlayers="
                << minPlayersToStart_ << ", MaxPlayers=" << maxPlayers_
                << ", Paused=" << isLobbyPaused_ << "\n";

            checkCountdownConditions();
            broadcastLobbyState();
        }
        catch (...) {
            boost::asio::post(conn->socket().get_executor(), [conn]() {
                conn->send(messages::makeError(0xD6, "Malformed DebugLobbyConfig payload"));
                });
        }
    }

    void LobbyRoom::checkCountdownConditions() {
        if (gameStarted_ || isLobbyPaused_) {
            if (isCountingDown_) stopCountdown();
            return;
        }

        uint32_t readyCount = 0;
        for (const auto& [id, player] : players_) {
            if (player.isReady()) readyCount++;
        }

        bool metConditions = (players_.size() >= minPlayersToStart_) && (readyCount == players_.size());

        if (metConditions && !isCountingDown_) {
            startCountdown();
        }
        else if (!metConditions && isCountingDown_) {
            stopCountdown();
        }
    }

    void LobbyRoom::startCountdown() {
        isCountingDown_ = true;
        std::cout << "[LobbyRoom " << roomId_ << "] Launching match countdown...\n";
        boost::asio::co_spawn(strand_, countdownTask(), boost::asio::detached);
    }

    void LobbyRoom::stopCountdown() {
        isCountingDown_ = false;
        countdownTimer_.cancel();
        std::cout << "[LobbyRoom " << roomId_ << "] Countdown aborted.\n";
    }

    boost::asio::awaitable<void> LobbyRoom::countdownTask() {
        try {
            for (int secondsLeft = 5; secondsLeft > 0; --secondsLeft) {
                if (!isCountingDown_ || gameStarted_) co_return;

                std::cout << "[LobbyRoom " << roomId_ << "] Game starts in " << secondsLeft << "s...\n";

                countdownTimer_.expires_after(std::chrono::seconds(1));
                co_await countdownTimer_.async_wait(boost::asio::use_awaitable);
            }

            if (isCountingDown_ && !gameStarted_) {
                gameStarted_ = true;
                isCountingDown_ = false;
                updateOpenState();

                if (onStartGame_) {
                    onStartGame_(shared_from_this());
                }
            }
        }
        catch (const boost::system::system_error& e) {
            if (e.code() != boost::asio::error::operation_aborted) {
                std::cerr << "[LobbyRoom " << roomId_ << "] Countdown error: " << e.what() << "\n";
            }
        }
    }

    void LobbyRoom::broadcastLobbyState() {
        std::vector<messages::LobbyPlayerInfo> info;
        info.reserve(players_.size());
        for (const auto& [eid, p] : players_) {
            info.push_back({ p.entityId(), p.name(), p.isBot(), p.isReady() });
        }
        auto msg = messages::makeLobbyState(info);

        // ZMIANA KRYTYCZNA: Lambdy poprawione (brak podwójnych nawiasów).
        // Każda operacja zapisu jest oddelegowana do executora gniazda przypisanego dla wątku danego bota.
        for (auto& [eid, player] : players_) {
            auto conn = player.connection();
            if (conn) {
                boost::asio::post(conn->socket().get_executor(), [conn, msg] {
                    conn->send(msg);
                    });
            }
        }
    }

    void LobbyRoom::extractPlayersAsync(OnPlayersExtractedCb cb) {
        boost::asio::post(strand_, [this, cb = std::move(cb)]() {
            std::vector<Player> extracted;
            extracted.reserve(players_.size());

            for (auto& [eid, pl] : players_) {
                extracted.push_back(std::move(pl));
            }

            players_.clear();
            connectionToEntity_.clear();

            playerCount_.store(0, std::memory_order_relaxed);
            updateOpenState();

            cb(std::move(extracted));
            });
    }

} // namespace gs::server::management