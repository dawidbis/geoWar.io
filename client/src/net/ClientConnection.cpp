#include "client/net/ClientConnection.hpp"
#include <iostream>

namespace gs::client {

ClientConnection::ClientConnection(boost::asio::io_context& ioc)
    : ioc_(ioc)
    , socket_(ioc)
    , resolver_(ioc)
    , strand_(boost::asio::make_strand(socket_.get_executor()))
{}

ClientConnection::~ClientConnection() {
    disconnect();
}

void ClientConnection::connect(const std::string& host, uint16_t port) {
    state_ = ConnectionState::Connecting;
    resolver_.async_resolve(host, std::to_string(port),
        [this](boost::system::error_code ec,
               boost::asio::ip::tcp::resolver::results_type results) {
            if (ec) { handleError(ec); return; }
            doConnect(results);
        });
}

void ClientConnection::doConnect(
    boost::asio::ip::tcp::resolver::results_type endpoints) {

    boost::asio::async_connect(socket_, endpoints,
        [this](boost::system::error_code ec,
               const boost::asio::ip::tcp::endpoint&) {
            if (ec) { handleError(ec); return; }
            state_ = ConnectionState::Connected;
            std::cout << "[Client] Connected\n";
            if (onConnected_) onConnected_();
            readHeader();
        });
}

void ClientConnection::readHeader() {
    boost::asio::async_read(
        socket_,
        boost::asio::buffer(headerBuf_),
        boost::asio::bind_executor(strand_,
            [this](boost::system::error_code ec, size_t) {
                if (ec) { handleError(ec); return; }
                inHeader_ = Message::parseHeader(
                    std::span<const uint8_t, sizeof(MessageHeader)>(headerBuf_));
                if (inHeader_.payloadSize == 0) {
                    Message msg; msg.header = inHeader_;
                    if (onMessage_) onMessage_(std::move(msg));
                    readHeader();
                } else {
                    readPayload();
                }
            }));
}

void ClientConnection::readPayload() {
    inPayload_.resize(inHeader_.payloadSize);
    boost::asio::async_read(
        socket_,
        boost::asio::buffer(inPayload_),
        boost::asio::bind_executor(strand_,
            [this](boost::system::error_code ec, size_t) {
                if (ec) { handleError(ec); return; }
                Message msg;
                msg.header  = inHeader_;
                msg.payload = std::move(inPayload_);
                if (onMessage_) onMessage_(std::move(msg));
                readHeader();
            }));
}

void ClientConnection::send(Message msg) {
    boost::asio::post(strand_, [this, m = std::move(msg)]() mutable {
        bool writing = !outQueue_.empty();
        outQueue_.push_back(std::move(m));
        if (!writing) doWrite();
    });
}

void ClientConnection::doWrite() {
    if (outQueue_.empty()) return;
    auto& msg = outQueue_.front();
    outBuf_.clear();
    auto hdr = msg.headerBytes();
    outBuf_.insert(outBuf_.end(), hdr.begin(), hdr.end());
    outBuf_.insert(outBuf_.end(), msg.payload.begin(), msg.payload.end());

    boost::asio::async_write(
        socket_,
        boost::asio::buffer(outBuf_),
        boost::asio::bind_executor(strand_,
            [this](boost::system::error_code ec, size_t) {
                if (ec) { handleError(ec); return; }
                outQueue_.pop_front();
                if (!outQueue_.empty()) doWrite();
            }));
}

void ClientConnection::disconnect() {
    if (state_ == ConnectionState::Disconnected) return;
    state_ = ConnectionState::Disconnected;
    boost::system::error_code ec;
    socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
    socket_.close(ec);
}

void ClientConnection::handleError(const boost::system::error_code& ec) {
    if (state_ == ConnectionState::Disconnected) return;
    std::string reason = ec.message();
    std::cerr << "[Client] Connection error: " << reason << "\n";
    state_ = ConnectionState::Error;
    boost::system::error_code closeEc;
    socket_.close(closeEc);
    if (onDisconnect_) onDisconnect_(reason);
}

} // namespace gs::client
