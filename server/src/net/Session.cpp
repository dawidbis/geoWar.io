#include "server/net/Session.hpp"

#include <iostream>

namespace gs::server {

    Session::Session(boost::asio::ip::tcp::socket socket, uint32_t sessionId)
        : socket_(std::move(socket))
        , strand_(boost::asio::make_strand(socket_.get_executor()))
        , sessionId_(sessionId)
    {
    }

    void Session::start(OnMessageCb onMsg, OnDisconnectCb onDisconnect) {
        onMessage_ = std::move(onMsg);
        onDisconnect_ = std::move(onDisconnect);
        readHeader();
    }

    // ── Odczyt ────────────────────────────────────────────────────────────────────

    void Session::readHeader() {
        auto self = shared_from_this();
        boost::asio::async_read(
            socket_,
            boost::asio::buffer(headerBuf_),
            boost::asio::bind_executor(strand_,
                [this, self](boost::system::error_code ec, size_t /*bytes*/) {
                    if (ec) { handleError(ec); return; }

                    inHeader_ = Message::parseHeader(
                        std::span<const uint8_t, sizeof(MessageHeader)>(headerBuf_));

                    if (inHeader_.payloadSize == 0) {
                        // Wiadomość bez payloadu
                        Message msg;
                        msg.header = inHeader_;
                        if (onMessage_) onMessage_(shared_from_this(), std::move(msg));
                        readHeader();
                    }
                    else {
                        readPayload();
                    }
                }));
    }

    void Session::readPayload() {
        inPayload_.resize(inHeader_.payloadSize);
        auto self = shared_from_this();
        boost::asio::async_read(
            socket_,
            boost::asio::buffer(inPayload_),
            boost::asio::bind_executor(strand_,
                [this, self](boost::system::error_code ec, size_t /*bytes*/) {
                    if (ec) { handleError(ec); return; }

                    Message msg;
                    msg.header = inHeader_;
                    msg.payload = std::move(inPayload_);

                    if (onMessage_) onMessage_(shared_from_this(), std::move(msg));
                    readHeader();
                }));
    }

    // ── Zapis ─────────────────────────────────────────────────────────────────────

    void Session::send(Message msg) {
        // Zawsze przez strand — bezpieczne z każdego wątku
        auto self = shared_from_this();
        boost::asio::post(strand_, [this, self, msg = std::move(msg)]() mutable {
            bool writeInProgress = !outQueue_.empty();
            outQueue_.push_back(std::move(msg));
            if (!writeInProgress) doWrite();
            });
    }

    void Session::doWrite() {
        if (outQueue_.empty()) return;

        auto& msg = outQueue_.front();

        // Spakuj nagłówek + payload do jednego ciągłego bufora
        outBuf_.clear();
        auto hdr = msg.headerBytes();
        outBuf_.insert(outBuf_.end(), hdr.begin(), hdr.end());
        outBuf_.insert(outBuf_.end(), msg.payload.begin(), msg.payload.end());

        auto self = shared_from_this();
        boost::asio::async_write(
            socket_,
            boost::asio::buffer(outBuf_),
            boost::asio::bind_executor(strand_,
                [this, self](boost::system::error_code ec, size_t /*bytes*/) {
                    if (ec) { handleError(ec); return; }
                    outQueue_.pop_front();
                    if (!outQueue_.empty()) doWrite();
                }));
    }

    // ── Obsługa błędów i rozłączenia ─────────────────────────────────────────────

    void Session::disconnect() {
        boost::asio::post(strand_, [self = shared_from_this()]() {
            if (!self->alive_) return;
            self->alive_ = false;
            boost::system::error_code ec;
            self->socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
            self->socket_.close(ec);
            });
    }

    void Session::handleError(const boost::system::error_code& ec) {
        if (!alive_) return;
        alive_ = false;

        if (ec != boost::asio::error::eof &&
            ec != boost::asio::error::connection_reset &&
            ec != boost::asio::error::operation_aborted) {
            std::cerr << "[Session " << sessionId_ << "] error: "
                << ec.message() << "\n";
        }
        else {
            std::cout << "[Session " << sessionId_ << "] disconnected ("
                << ec.message() << ")\n";
        }

        boost::system::error_code closeEc;
        socket_.close(closeEc);

        if (onDisconnect_) onDisconnect_(shared_from_this());
    }

} // namespace gs::server