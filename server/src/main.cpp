#include "server/net/TcpServer.hpp"
#include "server/management/MatchmakingManager.hpp"

#include <boost/asio.hpp>
#include <iostream>
#include <thread>
#include <vector>
#include <cstdlib>

int main(int argc, char* argv[]) {
    uint16_t port = 7777;
    if (argc > 1) {
        int p = std::atoi(argv[1]);
        if (p > 0 && p < 65536) {
            port = static_cast<uint16_t>(p);
        }
        else {
            std::cerr << "[main] Invalid port: " << argv[1] << "\n";
            return 1;
        }
    }

    std::cout << "=======================================\n";
    std::cout << "      Grand Strategy Server v0.2       \n";
    std::cout << "=======================================\n";

    boost::asio::io_context ioc;

    // ZMIANA 1: Zamiast niebezpiecznego std::signal, używamy asynchronicznego signal_set.
    // To gwarantuje, że zamknięcie ioc.stop() wykona się bezpiecznie na wątku roboczym Asio.
    boost::asio::signal_set signals(ioc, SIGINT, SIGTERM);
    signals.async_wait([&ioc](const boost::system::error_code& ec, int signal_number) {
        if (!ec) {
            std::cout << "\n[main] Signal " << signal_number << " received. Stopping io_context...\n";
            ioc.stop();
        }
        });

    // ZMIANA 2: Work Guard zabezpiecza przed sytuacją, w której ioc.run() mogłoby
    // przedwcześnie zakończyć pracę z powodu chwilowego braku asynchronicznych zadań.
    auto workGuard = boost::asio::make_work_guard(ioc);

    // 1. Inicjalizacja głównych menedżerów
    gs::server::management::MatchmakingManager matchmaking(ioc);
    gs::server::net::TcpServer server(ioc, port);

    // 2. Routing z warstwy sieci do warstwy zarządzania logiką
    server.onNewConnection([&matchmaking](gs::server::net::ConnectionPtr conn) {
        matchmaking.handleNewConnection(std::move(conn));
        });

    // 3. Uruchamiamy nasłuchiwanie
    server.start();

    // 4. Konfiguracja Puli Wątków (Thread Pool)
    unsigned int threadCount = std::thread::hardware_concurrency();
    if (threadCount == 0) threadCount = 4; // Zabezpieczenie (fallback)

    std::cout << "[main] Starting Server on " << threadCount << " hardware threads.\n";
    std::cout << "[main] Press Ctrl+C to stop.\n";

    std::vector<std::thread> ioThreads;
    ioThreads.reserve(threadCount - 1);

    // Odpalamy ioc.run() na dodatkowych wątkach (wszystkie z wyjątkiem głównego)
    for (unsigned int i = 0; i < threadCount - 1; ++i) {
        ioThreads.emplace_back([&ioc]() {
            ioc.run();
            });
    }

    // Główny wątek też blokujemy na obsłudze zdarzeń
    ioc.run();

    // 5. Zamykanie serwera (wykonywane po Ctrl+C i ioc.stop())
    std::cout << "[main] Shutting down, joining threads...\n";

    // Zwalniamy work guatd, pozwalając wątkom gładko zakończyć przetwarzanie reszty buforów sieciowych
    workGuard.reset();

    for (auto& t : ioThreads) {
        if (t.joinable()) {
            t.join();
        }
    }

    std::cout << "[main] Shutdown complete. Goodbye.\n";
    return 0;
}