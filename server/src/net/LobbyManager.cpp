#include "server/net/LobbyManager.hpp"

#include <iostream>

namespace gs::server {

    LobbyManager::LobbyManager(boost::asio::io_context& ioc)
        : ioc_(ioc)
    {
    }

    // ── Zarządzanie sesjami ───────────────────────────────────────────────────────

    void LobbyManager::onNewSession(SessionPtr session) {
        // Sesja jest podłączona ale nie zidentyfikowana — czekamy na ClientHello.
        // Nie robimy nic poza logiem.
        std::cout << "[Lobby] New TCP connection (session " << session->id() << ")\n";
    }

    void LobbyManager::onSessionDisconnect(SessionPtr session) {
        std::lock_guard lock(mutex_);

        auto it = sessionToEntity_.find(session->id());
        if (it == sessionToEntity_.end()) return; // sesja nigdy nie wysłała ClientHello

        uint32_t entityId = it->second;
        sessionToEntity_.erase(it);

        auto playerIt = players_.find(entityId);
        if (playerIt == players_.end()) return;

        auto& player = playerIt->second;
        player.session = nullptr; // sesja nieaktywna

        if (phase_ == LobbyPhase::WaitingForPlayers) {
            // W lobby — usuń gracza i poinformuj innych
            std::cout << "[Lobby] Player '" << player.name
                << "' left (session " << session->id() << ")\n";
            players_.erase(playerIt);
            broadcastLobbyState();
        }
        else {
            // W grze — sesja rozłączona, gracz pozostaje (bot przejmie po timeout)
            std::cout << "[Lobby] Player '" << player.name
                << "' disconnected mid-game (entityId=" << entityId << ")\n";
        }
    }

    // ── Routing wiadomości ────────────────────────────────────────────────────────

    void LobbyManager::onMessage(SessionPtr session, Message msg) {
        switch (msg.header.type) {
        case MessageType::ClientHello:
            handleClientHello(session, msg);
            break;

        case MessageType::PlayerInput:
            handlePlayerInput(session, msg);
            break;

        case MessageType::DebugStep:
        case MessageType::DebugSetTickrate:
        case MessageType::DebugQueryState:
            handleDebugMessage(session, msg);
            break;

        default:
            std::cerr << "[Lobby] Unknown message type 0x"
                << std::hex << static_cast<uint16_t>(msg.header.type)
                << std::dec << " from session " << session->id() << "\n";
            session->send(messages::makeError(0x01, "Unknown message type"));
            break;
        }
    }

    // ── ClientHello ───────────────────────────────────────────────────────────────

    void LobbyManager::handleClientHello(SessionPtr session, Message& msg) {
        // Parsuj payload: string name
        Serializer s(std::span<const uint8_t>(msg.payload));
        std::string name;
        try {
            name = s.readStr();
        }
        catch (...) {
            session->send(messages::makeError(0x02, "Malformed ClientHello"));
            session->disconnect();
            return;
        }

        if (name.empty() || name.size() > 32) {
            session->send(messages::makeError(0x03, "Invalid name"));
            session->disconnect();
            return;
        }

        std::lock_guard lock(mutex_);

        if (!isOpen()) {
            session->send(messages::makeError(0x04, "Lobby is full or in game"));
            session->disconnect();
            return;
        }

        // Sprawdź reconnect — czy gracz z tym imieniem już istnieje i jest rozłączony
        uint32_t entityId = 0;
        for (auto& [eid, player] : players_) {
            if (player.name == name && player.session == nullptr) {
                entityId = eid;
                break;
            }
        }

        if (entityId == 0) {
            // Nowy gracz
            entityId = nextEntityId();
            LobbyPlayer player;
            player.entityId = entityId;
            player.name = name;
            player.session = session;
            players_[entityId] = std::move(player);
        }
        else {
            // Reconnect
            players_[entityId].session = session;
            std::cout << "[Lobby] Player '" << name << "' reconnected\n";
        }

        session->setEntityId(entityId);
        session->setName(name);
        sessionToEntity_[session->id()] = entityId;

        std::cout << "[Lobby] Player '" << name
            << "' joined (entityId=" << entityId << ")\n";

        // Odpowiedz graczowi
        session->send(messages::makeServerWelcome(entityId, name));

        // Poinformuj wszystkich o nowym stanie lobby
        broadcastLobbyState();

        // Sprawdź czy możemy startować
        tryStartGame();
    }

    // ── PlayerInput (podczas gry) ────────────────────────────────────────────────

    void LobbyManager::handlePlayerInput(SessionPtr session, Message& msg) {
        if (phase_ != LobbyPhase::InGame) return;

        uint32_t entityId = session->entityId();
        if (entityId == 0) return; // sesja nie zidentyfikowana

        if (onPlayerInput_) onPlayerInput_(entityId, std::move(msg));
    }

    // ── Debug ─────────────────────────────────────────────────────────────────────

    void LobbyManager::handleDebugMessage(SessionPtr session, Message& msg) {
        // Przekaż dalej — GameLoop obsługuje
        uint32_t entityId = session->entityId();
        if (onPlayerInput_) onPlayerInput_(entityId, std::move(msg));
    }

    // ── Start gry ─────────────────────────────────────────────────────────────────

    void LobbyManager::tryStartGame() {
        // W v1: startujemy gdy >= MIN_PLAYERS_TO_START
        // TODO: dodać przycisk "Ready" i host może startować ręcznie
        if (phase_ != LobbyPhase::WaitingForPlayers) return;
        if (players_.size() < MIN_PLAYERS_TO_START)  return;

        phase_ = LobbyPhase::InGame;

        // Generuj seed
        uint64_t seed = static_cast<uint64_t>(
            std::chrono::steady_clock::now().time_since_epoch().count());

        // Poinformuj graczy
        auto startMsg = messages::makeLobbyStartGame(10 /*Hz*/, seed);
        for (auto& [_, player] : players_) {
            if (player.session && player.session->isAlive()) {
                player.session->send(startMsg);
            }
        }

        std::cout << "[Lobby] Game starting! Players: " << players_.size()
            << " seed=" << seed << "\n";

        // Przekaż listę graczy do GameLoop
        if (onStartGame_) {
            std::vector<LobbyPlayer> list;
            list.reserve(players_.size());
            for (auto& [_, p] : players_) list.push_back(p);
            onStartGame_(std::move(list));
        }
    }

    // ── Broadcast ─────────────────────────────────────────────────────────────────

    void LobbyManager::broadcastLobbyState() {
        // Buduj listę graczy
        std::vector<messages::LobbyPlayerInfo> info;
        info.reserve(players_.size());
        for (auto& [eid, p] : players_) {
            info.push_back({ p.entityId, p.name, p.isBot });
        }
        auto msg = messages::makeLobbyState(info);

        for (auto& [_, player] : players_) {
            if (player.session && player.session->isAlive()) {
                player.session->send(msg);
            }
        }
    }

    void LobbyManager::broadcast(const Message& msg, uint32_t excludeEntityId) {
        std::lock_guard lock(mutex_);
        for (auto& [eid, player] : players_) {
            if (eid == excludeEntityId) continue;
            if (player.session && player.session->isAlive()) {
                player.session->send(msg);
            }
        }
    }

    void LobbyManager::sendTo(uint32_t entityId, Message msg) {
        std::lock_guard lock(mutex_);
        auto it = players_.find(entityId);
        if (it == players_.end()) return;
        if (it->second.session && it->second.session->isAlive()) {
            it->second.session->send(std::move(msg));
        }
    }

    // ── Helpers ───────────────────────────────────────────────────────────────────

    bool LobbyManager::isOpen() const {
        return phase_ == LobbyPhase::WaitingForPlayers
            && players_.size() < MAX_PLAYERS;
    }

    uint32_t LobbyManager::nextEntityId() {
        return nextEntityId_++;
    }

} // namespace gs::server