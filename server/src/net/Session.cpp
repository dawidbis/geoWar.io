// server/src/net/Session.cpp
#include "server/net/Session.hpp"
#include <iostream>

namespace gs::server {

    Session::Session(boost::asio::ip::tcp::socket socket)
        : socket_(std::move(socket)) {
    }

    void Session::start(MessageHandler onMsg, DisconnectHandler onDisconnect) {
        onMessage_ = std::move(onMsg);
        onDisconnect_ = std::move(onDisconnect);
        readHeader();
    }

    void Session::send(const net::Message& msg) {
        // Odpalamy na asio strand/io_context, aby unikn¹æ race conditions
        boost::asio::post(socket_.get_executor(), [this, self = shared_from_this(), msg]() {
            bool writeInProgress = !outQueue_.empty();
            outQueue_.push_back(msg);
            if (!writeInProgress) {
                doWrite();
            }
            });
    }

    void Session::disconnect() {
        boost::system::error_code ec;
        socket_.close(ec);
    }

    void Session::readHeader() {
        auto self = shared_from_this();
        boost::asio::async_read(
            socket_,
            boost::asio::buffer(&inHeader_, sizeof(net::MessageHeader)),
            [this, self](boost::system::error_code ec, std::size_t /*length*/) {
                if (!ec) {
                    readPayload();
                }
                else {
                    if (onDisconnect_) onDisconnect_(self);
                }
            });
    }

    void Session::readPayload() {
        inPayload_.resize(inHeader_.payloadSize);
        auto self = shared_from_this();
        boost::asio::async_read(
            socket_,
            boost::asio::buffer(inPayload_),
            [this, self](boost::system::error_code ec, std::size_t /*length*/) {
                if (!ec) {
                    if (onMessage_) {
                        net::Message msg{ inHeader_, inPayload_ };
                        onMessage_(self, msg);
                    }
                    readHeader(); // Czekaj na kolejn¹ wiadomoœæ
                }
                else {
                    if (onDisconnect_) onDisconnect_(self);
                }
            });
    }

    void Session::doWrite() {
        auto& msg = outQueue_.front();

        // Serializacja nag³ówka i payloadu do jednego bufora (unikamy podwójnego wysy³ania)
        outBuf_.resize(sizeof(net::MessageHeader) + msg.payload.size());
        std::memcpy(outBuf_.data(), &msg.header, sizeof(net::MessageHeader));
        if (!msg.payload.empty()) {
            std::memcpy(outBuf_.data() + sizeof(net::MessageHeader), msg.payload.data(), msg.payload.size());
        }

        auto self = shared_from_this();
        boost::asio::async_write(
            socket_,
            boost::asio::buffer(outBuf_),
            [this, self](boost::system::error_code ec, std::size_t /*length*/) {
                if (!ec) {
                    outQueue_.pop_front();
                    if (!outQueue_.empty()) {
                        doWrite();
                    }
                }
                else {
                    if (onDisconnect_) onDisconnect_(self);
                }
            });
    }

} // namespace gs::server