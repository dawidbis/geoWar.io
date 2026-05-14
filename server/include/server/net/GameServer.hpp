// server/include/server/net/GameServer.hpp
#pragma once
#include "Session.hpp"
#include <boost/asio.hpp>
#include <memory>
#include <vector>

namespace gs::server {

    class GameServer {
    public:
        GameServer(boost::asio::io_context& ioc, uint16_t port);

        // Na razie proste callbacki symuluj¹ce pod³¹czenie pod LobbyManager
        void setOnSessionConnected(std::function<void(std::shared_ptr<Session>)> onConnect);

    private:
        void acceptNext();

        boost::asio::ip::tcp::acceptor acceptor_;
        std::function<void(std::shared_ptr<Session>)> onConnect_;
    };

} // namespace gs::server