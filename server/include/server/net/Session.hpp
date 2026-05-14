// server/include/server/net/Session.hpp
#pragma once
#include "shared/net/Message.hpp"
#include <boost/asio.hpp>
#include <deque>
#include <memory>
#include <functional>

namespace gs::server {

    class Session : public std::enable_shared_from_this<Session> {
    public:
        using MessageHandler = std::function<void(std::shared_ptr<Session>, const net::Message&)>;
        using DisconnectHandler = std::function<void(std::shared_ptr<Session>)>;

        explicit Session(boost::asio::ip::tcp::socket socket);

        void start(MessageHandler onMsg, DisconnectHandler onDisconnect);
        void send(const net::Message& msg);
        void disconnect();

        uint32_t getEntityId() const { return entityId_; }
        void setEntityId(uint32_t id) { entityId_ = id; }

    private:
        void readHeader();
        void readPayload();
        void doWrite();

        boost::asio::ip::tcp::socket socket_;
        MessageHandler onMessage_;
        DisconnectHandler onDisconnect_;

        net::MessageHeader inHeader_{};
        std::vector<uint8_t> inPayload_;

        std::deque<net::Message> outQueue_;
        std::vector<uint8_t> outBuf_; // Bufor na z³¹czony nag³ówek i payload

        uint32_t entityId_{ 0 }; // Powi¹zanie sesji z encj¹ z GameState
    };

} // namespace gs::server