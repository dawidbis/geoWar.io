#pragma once
#include "Session.hpp"
#include "LobbyManager.hpp"
#include <server/sim/GameLoop.hpp>

#include <boost/asio.hpp>
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace gs::server {

    /// Implementacja SessionBroadcaster korzystająca z LobbyManager.
    /// Zdefiniowana tutaj bo Server.hpp już includuje LobbyManager.hpp.
    class LobbyBroadcaster : public SessionBroadcaster {
    public:
        explicit LobbyBroadcaster(LobbyManager& lobby) : lobby_(lobby) {}

        void sendTo(uint32_t entityId, Message msg) override {
            lobby_.sendTo(entityId, std::move(msg));
        }
        void broadcast(const Message& msg, uint32_t excludeEntityId = 0) override {
            lobby_.broadcast(msg, excludeEntityId);
        }

    private:
        LobbyManager& lobby_;
    };

    // ── Server ────────────────────────────────────────────────────────────────────

    class Server {
    public:
        Server(boost::asio::io_context& ioc, uint16_t port);

        void start();
        void stop();

        LobbyManager& lobby() { return lobby_; }

    private:
        void acceptNext();
        void onSessionDisconnect(SessionPtr session);

        boost::asio::io_context& ioc_;
        boost::asio::ip::tcp::acceptor        acceptor_;
        LobbyManager                          lobby_;

        std::unordered_map<uint32_t, SessionPtr> sessions_;
        std::mutex                               sessionsMutex_;
        std::atomic<uint32_t>                    nextSessionId_{ 1 };
    };

} // namespace gs::server