// client/src/net/ClientConnection.cpp
#include "client/net/ClientConnection.hpp"
#include <iostream>

namespace gs::client {

    ClientConnection::ClientConnection(boost::asio::io_context& ioc)
        : socket_(ioc), resolver_(ioc) {
    }

    void ClientConnection::connect(const std::string& host, uint16_t port, ConnectHandler onConnect) {
        auto endpoints = resolver_.resolve(host, std::to_string(port));
        boost::asio::async_connect(socket_, endpoints,
            [this, onConnect](boost::system::error_code ec, boost::asio::ip::tcp::endpoint /*endpoint*/) {
                if (!ec) {
                    readHeader(); // Zaczynamy nas³uchiwaæ odpowiedzi serwera
                }
                if (onConnect) onConnect(ec);
            });
    }

    void ClientConnection::send(const net::Message& msg) {
        boost::asio::post(socket_.get_executor(), [this, msg]() {
            bool writeInProgress = !outQueue_.empty();
            outQueue_.push_back(msg);
            if (!writeInProgress) {
                doWrite();
            }
            });
    }

    void ClientConnection::setOnMessage(MessageHandler onMsg) {
        onMessage_ = std::move(onMsg);
    }

    void ClientConnection::disconnect() {
        boost::system::error_code ec;
        socket_.close(ec);
    }

    void ClientConnection::readHeader() {
        boost::asio::async_read(socket_, boost::asio::buffer(&inHeader_, sizeof(net::MessageHeader)),
            [this](boost::system::error_code ec, std::size_t /*length*/) {
                if (!ec) {
                    readPayload();
                }
                else {
                    std::cerr << "[Klient] Rozlaczono z serwerem: " << ec.message() << "\n";
                }
            });
    }

    void ClientConnection::readPayload() {
        inPayload_.resize(inHeader_.payloadSize);
        boost::asio::async_read(socket_, boost::asio::buffer(inPayload_),
            [this](boost::system::error_code ec, std::size_t /*length*/) {
                if (!ec) {
                    if (onMessage_) {
                        net::Message msg{ inHeader_, inPayload_ };
                        onMessage_(msg);
                    }
                    readHeader();
                }
            });
    }

    void ClientConnection::doWrite() {
        auto& msg = outQueue_.front();
        outBuf_.resize(sizeof(net::MessageHeader) + msg.payload.size());
        std::memcpy(outBuf_.data(), &msg.header, sizeof(net::MessageHeader));
        if (!msg.payload.empty()) {
            std::memcpy(outBuf_.data() + sizeof(net::MessageHeader), msg.payload.data(), msg.payload.size());
        }

        boost::asio::async_write(socket_, boost::asio::buffer(outBuf_),
            [this](boost::system::error_code ec, std::size_t /*length*/) {
                if (!ec) {
                    outQueue_.pop_front();
                    if (!outQueue_.empty()) doWrite();
                }
                else {
                    std::cerr << "[Klient] Blad wysylania: " << ec.message() << "\n";
                }
            });
    }

} // namespace gs::client