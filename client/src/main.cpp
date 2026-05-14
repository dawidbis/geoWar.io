// client/src/main.cpp
#include "client/net/ClientConnection.hpp"
#include "shared/net/Serializer.hpp"
#include <boost/asio.hpp>
#include <iostream>
#include <thread>

int main() {
    try {
        std::cout << "======================================\n";
        std::cout << " Grand Strategy Client - Test Sieci \n";
        std::cout << "======================================\n";

        boost::asio::io_context ioc;
        gs::client::ClientConnection client(ioc);

        // Obs³uga przychodz¹cych wiadomoœci z serwera
        client.setOnMessage([](const gs::net::Message& msg) {
            std::cout << "[Klient] Odebrano wiadomosc typu: 0x"
                << std::hex << static_cast<uint16_t>(msg.header.type) << std::dec
                << " (Rozmiar payloadu: " << msg.header.payloadSize << " bajtow)\n";

            // Jeœli to ServerWelcome, mo¿emy zdekodowaæ przydzielone ID (na razie tylko informacyjnie)
            if (msg.header.type == gs::net::MessageType::ServerWelcome) {
                std::cout << "[Klient] Polaczenie w pelni zaakceptowane przez serwer!\n";
            }
            });

        // Nawi¹zywanie po³¹czenia
        std::string host = "127.0.0.1";
        uint16_t port = 7777;

        std::cout << "[Klient] Laczenie z " << host << ":" << port << "...\n";

        client.connect(host, port, [&client](const boost::system::error_code& ec) {
            if (!ec) {
                std::cout << "[Klient] Nawiazano fizyczne polaczenie TCP!\n";

                // Budujemy pakiet ClientHello
                gs::net::Serializer out;
                out.writeStr("Dawid"); // Nazwa gracza
                auto payload = out.take();

                gs::net::Message helloMsg;
                helloMsg.header.type = gs::net::MessageType::ClientHello;
                helloMsg.header.payloadSize = static_cast<uint16_t>(payload.size());
                helloMsg.payload = std::move(payload);

                std::cout << "[Klient] Wysylanie ClientHello...\n";
                client.send(helloMsg);
            }
            else {
                std::cerr << "[Klient] Blad polaczenia: " << ec.message() << "\n";
            }
            });

        // Blokuje i obs³uguje pêtlê sieciow¹
        ioc.run();

    }
    catch (const std::exception& e) {
        std::cerr << "[FATAL ERROR] Wyjatek klienta: " << e.what() << "\n";
        return 1;
    }

    return 0;
}