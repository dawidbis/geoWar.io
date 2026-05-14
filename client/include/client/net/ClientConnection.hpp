// client/include/client/net/ClientConnection.hpp
#pragma once
#include "shared/net/Message.hpp"
#include <boost/asio.hpp>
#include <deque>
#include <functional>
#include <memory>
#include <string>

namespace gs::client {

    class ClientConnection {
    public:
        using MessageHandler = std::function<void(const net::Message&)>;
        using ConnectHandler = std::function<void(const boost::system::error_code&)>;

        explicit ClientConnection(boost::asio::io_context& ioc);

        void connect(const std::string& host, uint16_t port, ConnectHandler onConnect);
        void send(const net::Message& msg);
        void setOnMessage(MessageHandler onMsg);
        void disconnect();

    private:
        void readHeader();
        void readPayload();
        void doWrite();

        boost::asio::ip::tcp::socket socket_;
        boost::asio::ip::tcp::resolver resolver_;
        MessageHandler onMessage_;

        net::MessageHeader inHeader_{};
        std::vector<uint8_t> inPayload_;

        std::deque<net::Message> outQueue_;
        std::vector<uint8_t> outBuf_;
    };

} // namespace gs::client