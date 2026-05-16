#include "server/net/Connection.hpp"
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <iostream>
#include <span>

namespace gs::server::net {

    Connection::Connection(boost::asio::ip::tcp::socket socket, uint32_t connectionId)
        : socket_(std::move(socket))
        , strand_(boost::asio::make_strand(socket_.get_executor()))
        , connectionId_(connectionId)
    {
        // WYŁĄCZENIE ALGORYTMU NAGLE'A - krytyczne dla opóźnień w grach real-time!
        boost::system::error_code ec;
        socket_.set_option(boost::asio::ip::tcp::no_delay(true), ec);
        if (ec) {
            std::cerr << "[Connection " << connectionId_
                << "] Failed to set TCP_NODELAY: " << ec.message() << "\n";
        }
    }

    void Connection::start(OnMessageCb onMsg, OnDisconnectCb onDisconnect) {
        onMessage_ = std::move(onMsg);
        onDisconnect_ = std::move(onDisconnect);

        // Uruchamiamy pętlę odczytu wewnątrz zabezpieczonego stranda.
        boost::asio::co_spawn(strand_, readLoop(), boost::asio::detached);
    }

    // ── Odczyt (Korutyna) ────────────────────────────────────────────────────────

    boost::asio::awaitable<void> Connection::readLoop() {
        try {
            while (alive_) {
                // 1. Czekamy na nagłówek
                co_await boost::asio::async_read(
                    socket_,
                    boost::asio::buffer(headerBuf_),
                    boost::asio::use_awaitable);

                inHeader_ = Message::parseHeader(
                    std::span<const uint8_t, sizeof(MessageHeader)>(headerBuf_));

                Message msg;
                msg.header = inHeader_;

                // 2. Jeśli jest payload, tworzymy LOKALNY bezpieczny bufor
                if (inHeader_.payloadSize > 0) {
                    // ZMIANA KRYTYCZNA: Tworzymy lokalny wektor na stosie korutyny.
                    // Żaden inny wątek nie ma do niego dostępu, zero szans na inwalidację iteratora!
                    std::vector<uint8_t> localPayload(inHeader_.payloadSize);

                    co_await boost::asio::async_read(
                        socket_,
                        boost::asio::buffer(localPayload),
                        boost::asio::use_awaitable);

                    msg.payload = std::move(localPayload);
                }

                // 3. Przekazujemy wiadomość wyżej
                if (onMessage_) {
                    onMessage_(shared_from_this(), std::move(msg));
                }
            }
        }
        catch (const boost::system::system_error& e) {
            handleError(e.code());
        }
    }

    // ── Zapis (Korutyna) ─────────────────────────────────────────────────────────

    void Connection::send(Message msg) {
        // Dodajemy nową wiadomość w kontekście stranda.
        boost::asio::post(strand_, [self = shared_from_this(), msg = std::move(msg)]() mutable {
            if (!self->alive_) return;

            bool writeInProgress = !self->outQueue_.empty();
            self->outQueue_.push_back(std::move(msg));

            // Jeśli kolejka była pusta, to znaczy, że nie działa żadna pętla zapisująca. Startujemy ją!
            if (!writeInProgress) {
                boost::asio::co_spawn(self->strand_, self->writeLoop(), boost::asio::detached);
            }
            });
    }

    boost::asio::awaitable<void> Connection::writeLoop() {
        try {
            while (alive_ && !outQueue_.empty()) {
                // ZMIANA: Pobieramy całą wiadomość z frontu kolejki (wraz z jej pamięcią i strukturą).
                // Dzięki std::move, ta wiadomość i jej wewnętrzne bufory będą ŻYĆ na stosie korutyny
                // przez cały czas trwania asynchronicznego zapisu, aż do końca tej iteracji pętli!
                auto msg = std::move(outQueue_.front());

                // Przygotowujemy spakowany bufor bajtów jako zmienną lokalną korutyny
                auto hdr = msg.headerBytes();
                std::vector<uint8_t> localOutBuf;
                localOutBuf.reserve(hdr.size() + msg.payload.size());
                localOutBuf.insert(localOutBuf.end(), hdr.begin(), hdr.end());
                localOutBuf.insert(localOutBuf.end(), msg.payload.begin(), msg.payload.end());

                // Wywołujemy asynchroniczny zapis w czystej, korutynowej formie.
                // Ponieważ localOutBuf jest zadeklarowany w tym bloku, a msg żyje na stosie korutyny,
                // Asio ma gwarancję, że pamięć pod buforem nie zniknie w trakcie operacji jądra systemu!
                co_await boost::asio::async_write(
                    socket_,
                    boost::asio::buffer(localOutBuf),
                    boost::asio::use_awaitable);

                // Dopiero po udanym zakończeniu zapisu (gdy co_await powróci), 
                // faktycznie usuwamy element z oryginalnej kolejki i pozwalamy zmiennym lokalnym wygasnąć.
                outQueue_.pop_front();
            }
        }
        catch (const boost::system::system_error& e) {
            handleError(e.code());
        }
    }

    // ── Obsługa błędów i rozłączenia ─────────────────────────────────────────────

    void Connection::disconnect() {
        boost::asio::post(strand_, [self = shared_from_this()]() {
            if (!self->alive_) return;
            self->alive_ = false;

            boost::system::error_code ec;
            self->socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
            self->socket_.close(ec);
            });
    }

    void Connection::handleError(const boost::system::error_code& ec) {
        if (!alive_) return;
        alive_ = false; // Gwarancja, że kod poniżej wykona się tylko raz

        if (ec != boost::asio::error::eof &&
            ec != boost::asio::error::connection_reset &&
            ec != boost::asio::error::operation_aborted) {
            std::cerr << "[Connection " << connectionId_ << "] error: " << ec.message() << "\n";
        }
        else {
            std::cout << "[Connection " << connectionId_ << "] disconnected (" << ec.message() << ")\n";
        }

        boost::system::error_code closeEc;
        socket_.close(closeEc);

        if (onDisconnect_) {
            onDisconnect_(shared_from_this());
        }
    }

} // namespace gs::server::net