#pragma once
#include "server/net/Connection.hpp"

#include <boost/asio.hpp>
#include <boost/asio/awaitable.hpp>
#include <atomic>
#include <cstdint>
#include <functional>

namespace gs::server::net {

    /// Nasłuchuje na wskazanym porcie TCP i akceptuje nowe połączenia.
    /// Jest całkowicie odcięty od logiki gry (Lobby/Matchmaking).
    class TcpServer {
    public:
        using OnNewConnectionCb = std::function<void(ConnectionPtr)>;

        TcpServer(boost::asio::io_context& ioc, uint16_t port);

        void start();
        void stop();

        /// Rejestruje callback wywoływany przy każdym nowym połączeniu
        void onNewConnection(OnNewConnectionCb cb) { onNewConnection_ = std::move(cb); }

    private:
        /// Korutyna akceptująca nowe połączenia w nieskończonej pętli
        boost::asio::awaitable<void> acceptLoop();

        boost::asio::io_context& ioc_;
        boost::asio::ip::tcp::acceptor acceptor_;

        OnNewConnectionCb onNewConnection_;
        std::atomic<uint32_t> nextConnectionId_{ 1 };
    };

} // namespace gs::server::net