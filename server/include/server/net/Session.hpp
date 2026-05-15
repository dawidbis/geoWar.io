#pragma once
#include <shared/net/Message.hpp>

#include <boost/asio.hpp>
#include <array>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <string>

namespace gs::server {

    class Session;
    using SessionPtr = std::shared_ptr<Session>;

    /// Jedna aktywna sesja TCP — jeden podłączony klient.
    /// Zarządza asynchronicznym odczytem i zapisem.
    /// Żyje jako shared_ptr — lambdy async trzymają kopię by nie
    /// unieważnić this przed zakończeniem operacji.
    class Session : public std::enable_shared_from_this<Session> {
    public:
        using OnMessageCb = std::function<void(SessionPtr, Message)>;
        using OnDisconnectCb = std::function<void(SessionPtr)>;

        Session(boost::asio::ip::tcp::socket socket, uint32_t sessionId);
        ~Session() = default;

        /// Uruchom pętlę odczytu. Wywołaj raz po skonstruowaniu.
        void start(OnMessageCb onMsg, OnDisconnectCb onDisconnect);

        /// Kolejkuje wiadomość do wysłania. Bezpieczne z dowolnego wątku
        /// (przez strand).
        void send(Message msg);

        /// Zamknij połączenie gracefulnie.
        void disconnect();

        // ── Gettery ──────────────────────────────────────────────────────────────
        uint32_t          id()       const noexcept { return sessionId_; }
        uint32_t          entityId() const noexcept { return entityId_; }
        const std::string& name()    const noexcept { return name_; }
        bool              isAlive()  const noexcept { return alive_; }

        // ── Settery (ustawiane przez LobbyManager) ────────────────────────────
        void setEntityId(uint32_t eid) { entityId_ = eid; }
        void setName(std::string n) { name_ = std::move(n); }

    private:
        void readHeader();
        void readPayload();
        void doWrite();
        void handleError(const boost::system::error_code& ec);

        boost::asio::ip::tcp::socket socket_;

        /// Strand gwarantuje że handlery nie są wywoływane współbieżnie
        /// nawet przy wielowątkowym io_context.
        boost::asio::strand<boost::asio::any_io_executor> strand_;

        uint32_t    sessionId_;
        uint32_t    entityId_{ 0 };
        std::string name_;
        bool        alive_{ true };

        // Odczyt
        std::array<uint8_t, sizeof(MessageHeader)> headerBuf_{};
        MessageHeader                               inHeader_{};
        std::vector<uint8_t>                        inPayload_;

        // Zapis — kolejka + spłaszczony bufor do async_write
        std::deque<Message>  outQueue_;
        std::vector<uint8_t> outBuf_;

        OnMessageCb    onMessage_;
        OnDisconnectCb onDisconnect_;
    };

} // namespace gs::server