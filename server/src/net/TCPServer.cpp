#include "server/net/TcpServer.hpp"
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <iostream>

namespace gs::server::net {

    TcpServer::TcpServer(boost::asio::io_context& ioc, uint16_t port)
        : ioc_(ioc)
        , acceptor_(ioc, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port))
    {
        // Ustaw opcje gniazda
        acceptor_.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
        std::cout << "[TcpServer] Listening on port " << port << "\n";
    }

    void TcpServer::start() {
        std::cout << "[TcpServer] Starting acceptor...\n";

        // boost::asio::co_spawn "odpala" korutynę na podanym egzekutorze (ioc_)
        // i pozwala jej działać niezależnie (detached).
        boost::asio::co_spawn(ioc_, acceptLoop(), boost::asio::detached);
    }

    void TcpServer::stop() {
        boost::system::error_code ec;
        acceptor_.close(ec);
        std::cout << "[TcpServer] Acceptor closed.\n";
    }

    boost::asio::awaitable<void> TcpServer::acceptLoop() {
        try {
            // Dopóki serwer działa i gniazdo jest otwarte
            while (acceptor_.is_open()) {
                // Oddajemy wątek na czas oczekiwania na nowe połączenie
                auto socket = co_await acceptor_.async_accept(boost::asio::use_awaitable);

                // Wykonuje się dopiero, gdy gracz się połączy
                uint32_t connId = nextConnectionId_.fetch_add(1, std::memory_order_relaxed);
                auto connection = std::make_shared<Connection>(std::move(socket), connId);

                if (onNewConnection_) {
                    onNewConnection_(connection);
                }
            }
        }
        catch (const boost::system::system_error& e) {
            // Błąd "operation_aborted" występuje naturalnie podczas zamykania gniazda w stop()
            if (e.code() != boost::asio::error::operation_aborted) {
                std::cerr << "[TcpServer] Accept error: " << e.what() << "\n";
            }
        }
    }

} // namespace gs::server::net