#pragma once
#include <shared/net/Message.hpp>

#include <boost/asio.hpp>
#include <array>
#include <deque>
#include <functional>
#include <string>
#include <memory>

namespace gs::client {

enum class ConnectionState {
    Disconnected,
    Connecting,
    Connected,
    Error,
};

class ClientConnection {
public:
    using OnMessageCb    = std::function<void(Message)>;
    using OnConnectedCb  = std::function<void()>;
    using OnDisconnectCb = std::function<void(const std::string& reason)>;

    explicit ClientConnection(boost::asio::io_context& ioc);
    ~ClientConnection();

    /// Nawiąż połączenie (non-blocking)
    void connect(const std::string& host, uint16_t port);

    /// Wyślij wiadomość (thread-safe)
    void send(Message msg);

    /// Rozłącz
    void disconnect();

    void setOnMessage   (OnMessageCb    cb) { onMessage_    = std::move(cb); }
    void setOnConnected (OnConnectedCb  cb) { onConnected_  = std::move(cb); }
    void setOnDisconnect(OnDisconnectCb cb) { onDisconnect_ = std::move(cb); }

    ConnectionState state() const { return state_; }
    bool isConnected()      const { return state_ == ConnectionState::Connected; }

private:
    void doConnect(boost::asio::ip::tcp::resolver::results_type endpoints);
    void readHeader();
    void readPayload();
    void doWrite();
    void handleError(const boost::system::error_code& ec);

    boost::asio::io_context&        ioc_;
    boost::asio::ip::tcp::socket    socket_;
    boost::asio::ip::tcp::resolver  resolver_;
    boost::asio::strand<boost::asio::any_io_executor> strand_;

    ConnectionState state_{ConnectionState::Disconnected};

    std::array<uint8_t, sizeof(MessageHeader)> headerBuf_{};
    MessageHeader                               inHeader_{};
    std::vector<uint8_t>                        inPayload_;

    std::deque<Message>  outQueue_;
    std::vector<uint8_t> outBuf_;

    OnMessageCb    onMessage_;
    OnConnectedCb  onConnected_;
    OnDisconnectCb onDisconnect_;
};

} // namespace gs::client
