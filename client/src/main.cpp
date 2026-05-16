#include "client/net/ClientConnection.hpp"
#include "client/state/ClientState.hpp"
#include "client/render/UIRenderer.hpp"
#include "client/render/MapRenderer.hpp"

#include <shared/net/Message.hpp>

#include <SFML/Graphics.hpp>
#include <imgui.h>
#include <imgui-SFML.h>

#include <boost/asio.hpp>
#include <thread>
#include <mutex>
#include <queue>
#include <iostream>

// ── Thread-safe kolejka wiadomości z sieci ────────────────────────────────────

struct IncomingQueue {
    std::mutex              mtx;
    std::queue<gs::Message> msgs;

    void push(gs::Message m) {
        std::lock_guard lock(mtx);
        msgs.push(std::move(m));
    }
    bool pop(gs::Message& out) {
        std::lock_guard lock(mtx);
        if (msgs.empty()) return false;
        out = std::move(msgs.front());
        msgs.pop();
        return true;
    }
};

// ─────────────────────────────────────────────────────────────────────────────

int main() {
    // ── Okno SFML ─────────────────────────────────────────────────────────────
    sf::RenderWindow window(
        sf::VideoMode(1280, 720),
        "Grand Strategy",
        sf::Style::Default);
    window.setFramerateLimit(60);

    // ── ImGui-SFML init ───────────────────────────────────────────────────────
    if (!ImGui::SFML::Init(window)) {
        std::cerr << "[main] ImGui::SFML::Init failed\n";
        return 1;
    }

    // Styl ciemny
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowBorderSize = 0.5f;
    style.FrameRounding = 4.0f;
    style.WindowRounding = 6.0f;
    style.FramePadding = { 6, 4 };
    style.WindowPadding = { 10, 10 };
    style.Colors[ImGuiCol_WindowBg] = { 0.10f, 0.12f, 0.18f, 0.95f };
    style.Colors[ImGuiCol_FrameBg] = { 0.15f, 0.17f, 0.25f, 1.0f };
    style.Colors[ImGuiCol_Button] = { 0.20f, 0.28f, 0.52f, 1.0f };
    style.Colors[ImGuiCol_ButtonHovered] = { 0.30f, 0.40f, 0.70f, 1.0f };
    style.Colors[ImGuiCol_Separator] = { 0.25f, 0.28f, 0.40f, 1.0f };

    // ── Sieć w osobnym wątku ──────────────────────────────────────────────────
    boost::asio::io_context ioc;
    auto work = boost::asio::make_work_guard(ioc);

    IncomingQueue inQueue;
    gs::client::ClientConnection conn(ioc);

    conn.setOnMessage([&](gs::Message msg) {
        inQueue.push(std::move(msg));
        });
    conn.setOnDisconnect([&](const std::string& reason) {
        inQueue.push(gs::messages::makeError(0, reason));
        });

    std::thread netThread([&]() { ioc.run(); });

    // ── Stan i renderery ──────────────────────────────────────────────────────
    gs::client::ClientState state;
    gs::client::UIRenderer  uiRenderer;
    gs::client::MapRenderer mapRenderer;
    mapRenderer.init(window);

    sf::Clock deltaClock;

    // ── Główna pętla ──────────────────────────────────────────────────────────
    while (window.isOpen()) {
        sf::Event event;
        while (window.pollEvent(event)) {
            ImGui::SFML::ProcessEvent(window, event);
            const auto& io = ImGui::GetIO();

            if (event.type == sf::Event::Closed)
                window.close();
            if (event.type == sf::Event::Resized)
                mapRenderer.init(window);

            if (!io.WantCaptureMouse && !io.WantCaptureKeyboard)
                mapRenderer.handleEvent(event, state, window);
        }

        // ── Wiadomości z sieci ────────────────────────────────────────────────
        gs::Message incoming;
        while (inQueue.pop(incoming)) {
            switch (incoming.header.type) {
            case gs::MessageType::ServerWelcome:
                state.applyServerWelcome(incoming);
                state.phase = gs::client::ClientPhase::InLobby;
                break;
            case gs::MessageType::LobbyState:
                state.applyLobbyState(incoming);
                break;
            case gs::MessageType::LobbyStartGame:
                state.applyLobbyStart(incoming);
                break;
            case gs::MessageType::GameStateFull:
                state.applyGameFull(incoming);
                break;
            case gs::MessageType::GameStateDelta:
                state.applyGameDelta(incoming);
                break;
            case gs::MessageType::ErrorResponse: {
                gs::Serializer s(std::span<const uint8_t>(incoming.payload));
                uint16_t code = s.readU16();
                std::string msg = s.hasData() ? s.readStr() : "unknown";
                state.addLog("✗ error " + std::to_string(code) + ": " + msg);
                if (state.phase == gs::client::ClientPhase::Connecting)
                    state.phase = gs::client::ClientPhase::Disconnected;
                break;
            }

            default: break;
            }
        }

        // ── ImGui update ──────────────────────────────────────────────────────
        ImGui::SFML::Update(window, deltaClock.restart());

        auto action = uiRenderer.render(state);

        switch (action.type) {
        case gs::client::UIAction::Type::Connect:  // <--- DODANO .Type::
            conn.setOnConnected([&, nick = action.nick]() {
                state.addLog("Connect: ClientHello");
                conn.send(gs::messages::makeClientHello(nick));
                });
            conn.connect(action.host, action.port);
            break;

        case gs::client::UIAction::Type::Disconnect: // <--- DODANO .Type::
            conn.disconnect();
            state = gs::client::ClientState{};
            state.phase = gs::client::ClientPhase::Disconnected;
            state.addLog("Disconnected");
            break;

        case gs::client::UIAction::Type::DebugStep: // <--- DODANO .Type::
            conn.send(gs::messages::makeDebugStep(action.debugSteps));
            break;

        case gs::client::UIAction::Type::DebugSetTickrate: // <--- DODANO .Type::
            conn.send(gs::messages::makeDebugSetTickrate(action.debugTickrate));
            break;

        case gs::client::UIAction::Type::DebugStartGame: // <--- NOWY CASE
            conn.send(gs::Message::make(gs::MessageType::DebugStartGame, {}));
            break;

        case gs::client::UIAction::Type::DebugDumpState: // <--- DODANO .Type::
            std::cout << "[Debug] tick=" << state.currentTick << "\n";
            break;

        default: break;
        }

        // ── Render ────────────────────────────────────────────────────────────
        window.clear({ 15, 18, 28 });

        if (state.phase == gs::client::ClientPhase::InGame)
            mapRenderer.render(window, state);

        ImGui::SFML::Render(window);
        window.display();
    }

    // ── Cleanup ───────────────────────────────────────────────────────────────
    ImGui::SFML::Shutdown();
    conn.disconnect();
    work.reset();
    ioc.stop();
    netThread.join();

    return 0;
}