#pragma once
#include "server/management/Player.hpp"
#include <shared/net/Message.hpp>

#include <boost/asio.hpp>
#include <boost/asio/awaitable.hpp>
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace gs::server::net {
    class Connection;
    using ConnectionPtr = std::shared_ptr<Connection>;
}

namespace gs::server::management {

    class LobbyRoom : public std::enable_shared_from_this<LobbyRoom> {
    public:
        using OnStartGameCb = std::function<void(std::shared_ptr<LobbyRoom>)>;
        using OnPlayersExtractedCb = std::function<void(std::vector<Player>)>;

        LobbyRoom(boost::asio::io_context& ioc, uint32_t roomId, uint32_t maxPlayers = 100);
        ~LobbyRoom();

        uint32_t roomId() const noexcept { return roomId_; }

        // Bezpieczne, atomowe odczyty dla MatchmakingManagera (lock-free)
        bool isOpen() const { return isOpen_.load(std::memory_order_relaxed); }
        size_t playerCount() const { return playerCount_.load(std::memory_order_relaxed); }

        // W pełni asynchroniczne i bez-blokujące zarządzanie połączeniami
        void addConnection(net::ConnectionPtr conn, std::function<void(bool)> completionCb);
        void removeConnection(uint32_t connectionId);
        void handleMessage(net::ConnectionPtr conn, Message msg);
        void onStartGameRequested(OnStartGameCb cb) { onStartGame_ = std::move(cb); }

        void extractPlayersAsync(OnPlayersExtractedCb cb);

    private:
        void processMessage(net::ConnectionPtr conn, Message& msg);
        void handleClientHello(net::ConnectionPtr conn, Message& msg);
        void handleLobbyReady(net::ConnectionPtr conn, Message& msg);
        void handleDebugLobbyConfig(net::ConnectionPtr conn, Message& msg);

        void checkCountdownConditions();
        void startCountdown();
        void stopCountdown();
        void updateOpenState();

        // Korutyna odliczania czasu
        boost::asio::awaitable<void> countdownTask();

        void broadcastLobbyState();

        boost::asio::io_context& ioc_;
        boost::asio::strand<boost::asio::any_io_executor> strand_;
        boost::asio::steady_timer countdownTimer_;

        uint32_t roomId_;
        uint32_t maxPlayers_;
        uint32_t minPlayersToStart_{ 2 };

        std::atomic<bool> isOpen_{ true };
        std::atomic<size_t> playerCount_{ 0 };

        bool     isLobbyPaused_{ false };
        bool     gameStarted_{ false };
        bool     isCountingDown_{ false };

        std::unordered_map<uint32_t, Player> players_;
        std::unordered_map<uint32_t, uint32_t> connectionToEntity_;

        uint32_t nextEntityId_{ 1 };
        OnStartGameCb onStartGame_;
    };

    using LobbyRoomPtr = std::shared_ptr<LobbyRoom>;

} // namespace gs::server::management