// server/src/net/GameServer.cpp
#include "server/net/GameServer.hpp"
#include <iostream>

namespace gs::server {

    GameServer::GameServer(boost::asio::io_context& ioc, uint16_t port)
        : acceptor_(ioc, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port)) {
        acceptNext();
    }

    void GameServer::setOnSessionConnected(std::function<void(std::shared_ptr<Session>)> onConnect) {
        onConnect_ = std::move(onConnect);
    }

    void GameServer::acceptNext() {
        acceptor_.async_accept(
            [this](boost::system::error_code ec, boost::asio::ip::tcp::socket socket) {
                if (!ec) {
                    auto session = std::make_shared<Session>(std::move(socket));
                    if (onConnect_) {
                        onConnect_(session);
                    }
                }
                else {
                    std::cerr << "Accept error: " << ec.message() << std::endl;
                }
                acceptNext();
            });
    }

} // namespace gs::server