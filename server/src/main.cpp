// server/src/main.cpp
#include "shared/net/Message.hpp"
#include "server/net/GameServer.hpp"
#include <boost/asio.hpp>
#include <iostream>
#include <exception>

int main() {
    try {
        std::cout << "======================================\n";
        std::cout << " Grand Strategy Server - Start \n";
        std::cout << "======================================\n";

        boost::asio::io_context ioc;

        // Uruchamiamy serwer na porcie 7777 (domyœlny z architektury)
        uint16_t port = 7777;
        gs::server::GameServer server(ioc, port);

        std::cout << "[Serwer] Nasluchiwanie na porcie " << port << "...\n";

        // Proste logowanie nowych po³¹czeñ
        server.setOnSessionConnected([](std::shared_ptr<gs::server::Session> session) {
            std::cout << "[Serwer] Nowy klient polaczony (TCP)!\n";

            // Reakcja serwera na przychodz¹ce wiadomoœci od tej konkretnej sesji
            session->start(
                [](std::shared_ptr<gs::server::Session> s, const gs::net::Message& msg) {
                    if (msg.header.type == gs::net::MessageType::ClientHello) {
                        std::cout << "[Serwer] Odebrano ClientHello od klienta! Odsylam ServerWelcome...\n";

                        gs::net::Message welcomeMsg;
                        welcomeMsg.header.type = gs::net::MessageType::ServerWelcome;
                        welcomeMsg.header.payloadSize = 0; // Pusty payload na ten moment testów
                        s->send(welcomeMsg);
                    }
                },
                [](std::shared_ptr<gs::server::Session> s) {
                    std::cout << "[Serwer] Klient sie rozlaczyl.\n";
                }
            );
            });

        // Blokuje w¹tek i obs³uguje wszystkie operacje asynchroniczne
        ioc.run();

    }
    catch (const std::exception& e) {
        std::cerr << "[FATAL ERROR] Wyjatek krytyczny: " << e.what() << "\n";
        return 1;
    }

    return 0;
}