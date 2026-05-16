#include <shared/net/MessageTypes.hpp>
#include <shared/net/Message.hpp>
#include <shared/net/Serializer.hpp>
#include <shared/types/Enums.hpp>

#include <boost/asio.hpp>
#include <iostream>
#include <memory>
#include <vector>
#include <string>
#include <chrono>
#include <thread>
#include <deque>
#include <array>

using boost::asio::ip::tcp;
using namespace gs;

class StressClient : public std::enable_shared_from_this<StressClient> {
public:
    StressClient(boost::asio::io_context& ioc, const std::string& host, const std::string& port, int id)
        : socket_(ioc)
        , strand_(boost::asio::make_strand(ioc))
        , timer_(ioc)
        , id_(id)
        , resolver_(ioc)
        , host_(host)
        , port_(port)
    {
    }

    void start() {
        // Prawdziwe asynchroniczne opóźnienie startu (np. 20ms odstępu między botami)
        // Pozwala to gładko otworzyć połączenia, gdy io_context już działa.
        timer_.expires_after(std::chrono::milliseconds(id_ * 20));
        timer_.async_wait(
            boost::asio::bind_executor(strand_, [this, self = shared_from_this()](boost::system::error_code ec) {
                if (!ec && !isClosed_) resolveAndConnect();
                })
        );
    }

private:
    void resolveAndConnect() {
        resolver_.async_resolve(host_, port_,
            boost::asio::bind_executor(strand_, [this, self = shared_from_this()](boost::system::error_code ec, tcp::resolver::results_type results) {
                if (ec) {
                    std::cerr << "[Bot " << id_ << "] Resolve error: " << ec.message() << "\n";
                    return;
                }

                boost::asio::async_connect(socket_, results,
                    boost::asio::bind_executor(strand_, [this, self](boost::system::error_code ec, tcp::endpoint) {
                        if (!ec) {
                            boost::system::error_code optEc;
                            // Wyłącz Nagle's algorithm, aby pakiety szły od razu (jak w grze)
                            socket_.set_option(tcp::no_delay(true), optEc);
                            onConnected();
                        }
                        else {
                            std::cerr << "[Bot " << id_ << "] Connect error: " << ec.message() << "\n";
                        }
                        }));
                }));
    }

    void onConnected() {
        std::cout << "[Bot " << id_ << "] Connected. Sending Hello...\n";

        auto helloMsg = messages::makeClientHello("StressBot_" + std::to_string(id_));
        sendMessage(std::move(helloMsg));

        readLoop();

        timer_.expires_after(std::chrono::seconds(1));
        timer_.async_wait(
            boost::asio::bind_executor(strand_, [this, self = shared_from_this()](boost::system::error_code ec) {
                if (!ec && !isClosed_) {
                    std::cout << "[Bot " << id_ << "] Sending READY...\n";
                    Serializer s;
                    s.writeBool(true);
                    sendMessage(Message::make(MessageType::LobbyReady, s));

                    startSpamming();
                }
                })
        );
    }

    void startSpamming() {
        timer_.expires_after(std::chrono::seconds(6));
        timer_.async_wait(
            boost::asio::bind_executor(strand_, [this, self = shared_from_this()](boost::system::error_code ec) {
                if (!ec && !isClosed_) {
                    std::cout << "[Bot " << id_ << "] GAME ON! Starting to spam inputs...\n";
                    spamLoop();
                }
                })
        );
    }

    void spamLoop() {
        // Spamujemy co 50ms (20 pakietów na sekundę, dopasowane do tickrate gry)
        timer_.expires_after(std::chrono::milliseconds(50));
        timer_.async_wait(
            boost::asio::bind_executor(strand_, [this, self = shared_from_this()](boost::system::error_code ec) {
                if (!ec && !isClosed_) {
                    sendFakeInput();
                    spamLoop();
                }
                })
        );
    }

    void sendFakeInput() {
        Serializer s;
        s.writeU8(static_cast<uint8_t>(InputType::Attack));
        s.writeU32(1);
        s.writeU32(2);
        s.writeU32(0);
        s.writeU32(0);
        s.writeU32(0);
        s.writeF32(0.5f);
        s.writeU16(0);
        s.writeU16(0);
        s.writeU16(0);

        sendMessage(Message::make(MessageType::PlayerInput, s));

        packetsSent_++;
        if (packetsSent_ % 200 == 0) {
            std::cout << "[Bot " << id_ << "] Sent " << packetsSent_ << " inputs so far.\n";
        }
    }

    // ── Bezpieczna Kolejka Zapisu ─────────────────────────────────────────────

    void sendMessage(Message msg) {
        boost::asio::post(strand_, [this, self = shared_from_this(), msg = std::move(msg)]() mutable {
            if (isClosed_) return;

            bool writeInProgress = !outQueue_.empty();
            outQueue_.push_back(std::move(msg));

            if (!writeInProgress) {
                doWrite();
            }
            });
    }

    void doWrite() {
        if (outQueue_.empty() || isClosed_) return;

        auto& msg = outQueue_.front();
        outBuf_.clear();
        auto hdr = msg.headerBytes();
        outBuf_.insert(outBuf_.end(), hdr.begin(), hdr.end());
        outBuf_.insert(outBuf_.end(), msg.payload.begin(), msg.payload.end());

        boost::asio::async_write(socket_, boost::asio::buffer(outBuf_),
            boost::asio::bind_executor(strand_, [this, self = shared_from_this()](boost::system::error_code ec, std::size_t) {
                if (isClosed_) return;

                if (!ec) {
                    outQueue_.pop_front();
                    if (!outQueue_.empty()) {
                        doWrite();
                    }
                }
                else {
                    handleError("Write error: " + ec.message());
                }
                }));
    }

    // ── Bezpieczna Pętla Odczytu ──────────────────────────────────────────────

    void readLoop() {
        socket_.async_read_some(boost::asio::buffer(inBuf_),
            boost::asio::bind_executor(strand_, [this, self = shared_from_this()](boost::system::error_code ec, std::size_t) {
                if (isClosed_) return;

                if (!ec) {
                    readLoop();
                }
                else if (ec != boost::asio::error::eof && ec != boost::asio::error::connection_reset) {
                    handleError("Read error: " + ec.message());
                }
                else {
                    handleError("Disconnected by server.");
                }
                }));
    }

    void handleError(const std::string& msg) {
        if (isClosed_) return;
        isClosed_ = true;
        std::cerr << "[Bot " << id_ << "] " << msg << "\n";

        boost::system::error_code ec;
        socket_.close(ec);

        // POPRAWKA: cancel() dla timerów w tej wersji Boost nie przyjmuje argumentu (ec)
        timer_.cancel();
    }

    tcp::socket socket_;
    boost::asio::strand<boost::asio::any_io_executor> strand_;
    boost::asio::steady_timer timer_;

    int id_;
    tcp::resolver resolver_;
    std::string host_, port_;
    uint32_t packetsSent_{ 0 };
    bool isClosed_{ false };

    // Bufory
    std::array<uint8_t, 4096> inBuf_;
    std::deque<Message> outQueue_;
    std::vector<uint8_t> outBuf_;
};

int main(int argc, char* argv[]) {
    std::string host = "127.0.0.1";
    std::string port = "7777";
    int numBots = 50;

    std::cout << "========================================\n";
    std::cout << "   STARTING STRESS TEST (" << numBots << " BOTS) \n";
    std::cout << "========================================\n";

    boost::asio::io_context ioc;
    std::vector<std::shared_ptr<StressClient>> clients;

    for (int i = 0; i < numBots; ++i) {
        auto client = std::make_shared<StressClient>(ioc, host, port, i + 1);
        clients.push_back(client);
        client->start();
    }

    std::vector<std::thread> threads;
    unsigned int numThreads = std::thread::hardware_concurrency();
    if (numThreads == 0) numThreads = 2;

    for (unsigned int i = 0; i < numThreads; ++i) {
        threads.emplace_back([&ioc]() { ioc.run(); });
    }

    for (auto& t : threads) {
        t.join();
    }

    return 0;
}