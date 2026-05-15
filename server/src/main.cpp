#include "server/net/Server.hpp"
#include "server/sim/GameLoop.hpp"

#include <boost/asio.hpp>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <memory>

namespace {
    boost::asio::io_context* g_ioc = nullptr;
    void signalHandler(int) { if (g_ioc) g_ioc->stop(); }
}

int main(int argc, char* argv[]) {
    uint16_t port = 7777;
    if (argc > 1) {
        int p = std::atoi(argv[1]);
        if (p > 0 && p < 65536)
            port = static_cast<uint16_t>(p);
        else {
            std::cerr << "[main] Invalid port: " << argv[1] << "\n";
            return 1;
        }
    }

    std::cout << "[main] Grand Strategy Server v0.1\n";
    std::cout << "[main] Port: " << port << "\n";

    boost::asio::io_context ioc;
    g_ioc = &ioc;
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    // GameState — żyje przez cały czas działania serwera
    gs::server::GameState state;

    // Server + LobbyManager
    gs::server::Server server(ioc, port);

    // Broadcaster — łączy GameLoop z LobbyManager
    gs::server::LobbyBroadcaster broadcaster(server.lobby());

    // GameLoop
    gs::server::GameLoop gameLoop(ioc, state, broadcaster);

    // Gdy lobby zbierze graczy i wystartuje grę — uruchom GameLoop
    server.lobby().onStartGame([&](std::vector<gs::server::LobbyPlayer> players) {
        // Zainicjuj encje w GameState
        for (auto& p : players) {
            gs::server::Entity entity;
            entity.id = p.entityId;
            entity.name = p.name;
            entity.type = p.isBot
                ? gs::EntityType::Bot
                : gs::EntityType::Human;
            state.entities.push_back(entity);
        }
        state.phase = gs::GamePhase::Playing;

        std::cout << "[main] Game started with "
            << players.size() << " players\n";

        gameLoop.start();
        });

    // Inputy graczy → GameLoop
    server.lobby().onPlayerInput([&](uint32_t entityId,
        gs::Message msg) {
            // Obsługa wiadomości debug
            if (msg.header.type == gs::MessageType::DebugStep) {
                gs::Serializer s(std::span<const uint8_t>(msg.payload));
                uint32_t n = s.readU32();
                gameLoop.stepTicks(n);
                return;
            }
            if (msg.header.type == gs::MessageType::DebugSetTickrate) {
                gs::Serializer s(std::span<const uint8_t>(msg.payload));
                uint32_t hz = s.readU32();
                gameLoop.setTickrate(hz);
                return;
            }

            // Normalny input — TODO: parsuj PlayerInput z payloadu
            gs::server::PlayerInput input;
            input.entityId = entityId;
            gameLoop.enqueueInput(std::move(input));
        });

    server.start();

    std::cout << "[main] Server running. Press Ctrl+C to stop.\n";
    ioc.run();

    std::cout << "[main] Shutdown complete.\n";
    return 0;
}