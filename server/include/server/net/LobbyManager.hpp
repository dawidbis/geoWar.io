#pragma once
#include "Session.hpp"
#include <shared/net/Message.hpp>

#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace gs::server {

    /// Stan lobby
    enum class LobbyPhase : uint8_t {
        WaitingForPlayers,
        InGame,
        Finished,
    };

    /// Informacja o graczu w lobby
    struct LobbyPlayer {
        uint32_t    entityId;
        std::string name;
        SessionPtr  session;   // nullptr jeśli bot lub rozłączony
        bool        isBot{ false };
        bool        isReady{ false };
    };

    /// LobbyManager zarządza jedynym aktywnym lobby (v1).
    /// Odpowiada za:
    ///   - przyjmowanie graczy (ClientHello → ServerWelcome + LobbyState broadcast)
    ///   - start gry (LobbyStartGame)
    ///   - reconnect (ClientHello z istniejącym entityId → GameStateFull)
    ///   - rozłączenia (bot przejmuje po DISCONNECT_TIMEOUT_TICKS)
    class LobbyManager {
    public:
        static constexpr uint32_t MAX_PLAYERS = 100;
        static constexpr uint32_t MIN_PLAYERS_TO_START = 2;
        static constexpr uint32_t DISCONNECT_TIMEOUT_TICKS = 300; // 30 s przy 10 Hz

        using OnStartGameCb = std::function<void(std::vector<LobbyPlayer>)>;
        using OnInputCb = std::function<void(uint32_t entityId, Message)>;

        explicit LobbyManager(boost::asio::io_context& ioc);

        /// Nowa sesja TCP — klient właśnie się połączył (przed ClientHello)
        void onNewSession(SessionPtr session);

        /// Klient rozłączył się
        void onSessionDisconnect(SessionPtr session);

        /// Wiadomość od klienta
        void onMessage(SessionPtr session, Message msg);

        /// Czy można jeszcze dołączyć do lobby?
        bool isOpen() const;

        /// Wyślij wiadomość do konkretnej encji
        void sendTo(uint32_t entityId, Message msg);

        /// Wyślij wiadomość do wszystkich graczy (broadcast)
        void broadcast(const Message& msg, uint32_t excludeEntityId = 0);

        /// Rejestruj callback wywoływany gdy gra startuje
        void onStartGame(OnStartGameCb cb) { onStartGame_ = std::move(cb); }

        /// Rejestruj callback wywoływany gdy gracz wysyła input (w trakcie gry)
        void onPlayerInput(OnInputCb cb) { onPlayerInput_ = std::move(cb); }

        LobbyPhase phase() const noexcept { return phase_; }

        const std::unordered_map<uint32_t, LobbyPlayer>& players() const {
            return players_;
        }

    private:
        void handleClientHello(SessionPtr session, Message& msg);
        void handlePlayerInput(SessionPtr session, Message& msg);
        void handleDebugMessage(SessionPtr session, Message& msg);
        void tryStartGame();
        void broadcastLobbyState();
        uint32_t nextEntityId();

        boost::asio::io_context& ioc_;
        LobbyPhase                                   phase_{ LobbyPhase::WaitingForPlayers };
        std::unordered_map<uint32_t, LobbyPlayer>    players_;   // entityId → player
        std::unordered_map<uint32_t, uint32_t>       sessionToEntity_; // sessionId → entityId

        uint32_t    nextEntityId_{ 1 };

        OnStartGameCb   onStartGame_;
        OnInputCb       onPlayerInput_;

        mutable std::mutex mutex_; // chroni players_ i sessionToEntity_
    };

} // namespace gs::server