#pragma once
#include <shared/net/Message.hpp>

#include <boost/asio.hpp>
#include <boost/asio/awaitable.hpp>
#include <array>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <vector>

namespace gs::server::net {

    class Connection;
    using ConnectionPtr = std::shared_ptr<Connection>;

    /// Reprezentuje jedno fizyczne połączenie TCP.
    /// Wykorzystuje korutyny C++20 do obsługi odczytu i zapisu.
    class Connection : public std::enable_shared_from_this<Connection> {
    public:
        using OnMessageCb = std::function<void(ConnectionPtr, Message)>;
        using OnDisconnectCb = std::function<void(ConnectionPtr)>;

        Connection(boost::asio::ip::tcp::socket socket, uint32_t connectionId);
        ~Connection() = default;

        /// Uruchamia korutynę nasłuchującą na wiadomości
        void start(OnMessageCb onMsg, OnDisconnectCb onDisconnect);

        /// Dodaje wiadomość do kolejki wysyłania (Thread-safe dzięki strand)
        void send(Message msg);

        /// Bezpieczne zamknięcie połączenia
        void disconnect();

        // ── Gettery ───────────────────────────────────────────────────────────
        uint32_t id() const noexcept { return connectionId_; }
        bool isAlive() const noexcept { return alive_; }

        boost::asio::ip::tcp::socket& socket() noexcept { return socket_; }

        uint32_t entityId() const noexcept { return entityId_; }
        void setEntityId(uint32_t eid) { entityId_ = eid; }

    private:
        /// Główna pętla odczytu zrealizowana jako korutyna
        boost::asio::awaitable<void> readLoop();

        /// Korutyna zdejmująca wiadomości z kolejki i wysyłająca je przez socket
        boost::asio::awaitable<void> writeLoop();

        void handleError(const boost::system::error_code& ec);

        boost::asio::ip::tcp::socket socket_;

        // Strand chroni przed wyścigami (Race Conditions)
        boost::asio::strand<boost::asio::any_io_executor> strand_;

        uint32_t connectionId_;
        uint32_t entityId_{ 0 };
        bool alive_{ true };

        // Bufory wejściowe
        std::array<uint8_t, sizeof(MessageHeader)> headerBuf_{};
        MessageHeader inHeader_{};
        std::vector<uint8_t> inPayload_;

        // Kolejka wyjściowa
        std::deque<Message> outQueue_;
        std::vector<uint8_t> outBuf_;

        OnMessageCb onMessage_;
        OnDisconnectCb onDisconnect_;
    };

} // namespace gs::server::net