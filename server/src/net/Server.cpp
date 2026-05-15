#include "server/net/Server.hpp"

#include <iostream>

namespace gs::server {

    Server::Server(boost::asio::io_context& ioc, uint16_t port)
        : ioc_(ioc)
        , acceptor_(ioc,
            boost::asio::ip::tcp::endpoint(
                boost::asio::ip::tcp::v4(), port))
        , lobby_(ioc)
    {
        // Ustaw opcje gniazda
        acceptor_.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));

        std::cout << "[Server] Listening on port " << port << "\n";
    }

    void Server::start() {
        acceptNext();
    }

    void Server::stop() {
        boost::system::error_code ec;
        acceptor_.close(ec);
        std::cout << "[Server] Acceptor closed\n";
    }

    // ── Akceptowanie połączeń ─────────────────────────────────────────────────────

    void Server::acceptNext() {
        acceptor_.async_accept(
            [this](boost::system::error_code ec,
                boost::asio::ip::tcp::socket socket) {
                    if (ec) {
                        if (ec != boost::asio::error::operation_aborted) {
                            std::cerr << "[Server] Accept error: " << ec.message() << "\n";
                        }
                        return; // acceptor zamknięty — nie akceptuj kolejnych
                    }

                    uint32_t sid = nextSessionId_.fetch_add(1);
                    auto session = std::make_shared<Session>(std::move(socket), sid);

                    {
                        std::lock_guard lock(sessionsMutex_);
                        sessions_[sid] = session;
                    }

                    // Poinformuj LobbyManager o nowej sesji
                    lobby_.onNewSession(session);

                    // Uruchom odczyt — callbacki idą do LobbyManager
                    session->start(
                        [this](SessionPtr s, Message msg) {
                            lobby_.onMessage(s, std::move(msg));
                        },
                        [this](SessionPtr s) {
                            onSessionDisconnect(s);
                        }
                    );

                    // Akceptuj następne połączenie
                    acceptNext();
            });
    }

    // ── Rozłączenie sesji ─────────────────────────────────────────────────────────

    void Server::onSessionDisconnect(SessionPtr session) {
        lobby_.onSessionDisconnect(session);

        {
            std::lock_guard lock(sessionsMutex_);
            sessions_.erase(session->id());
        }

        std::cout << "[Server] Session " << session->id() << " removed\n";
    }

} // namespace gs::server