#include "client/render/UIRenderer.hpp"
#include <imgui.h>
#include <shared/net/Message.hpp>
#include <cstdio>
#include <cstring>

namespace gs::client {

    static const ImVec4 PLAYER_COLORS[] = {
        {0.31f, 0.44f, 0.83f, 1.0f}, {0.85f, 0.35f, 0.19f, 1.0f},
        {0.37f, 0.79f, 0.65f, 1.0f}, {0.83f, 0.33f, 0.49f, 1.0f},
        {0.93f, 0.78f, 0.15f, 1.0f}, {0.53f, 0.33f, 0.70f, 1.0f},
        {0.19f, 0.72f, 0.86f, 1.0f}, {0.80f, 0.55f, 0.22f, 1.0f}
    };

    static ImVec4 playerColor(uint32_t entityId) {
        if (entityId == 0) return { 0.5f, 0.5f, 0.5f, 1.0f };
        return PLAYER_COLORS[(entityId - 1) % 8];
    }

    UIAction UIRenderer::render(ClientState& state) {
        switch (state.phase) {
        case ClientPhase::Disconnected:
        case ClientPhase::Connecting:
            return renderConnect(state);
        case ClientPhase::InLobby:
            return renderLobby(state);
        case ClientPhase::InGame:
            return renderGame(state);
        }
        return {};
    }

    UIAction UIRenderer::renderConnect(ClientState& state) {
        UIAction action;
        ImGuiIO& io = ImGui::GetIO();
        ImGui::SetNextWindowPos({ io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f }, ImGuiCond_Always, { 0.5f, 0.5f });
        ImGui::SetNextWindowSize({ 320, 0 }, ImGuiCond_Always);
        ImGui::Begin("##connect", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

        ImGui::Text("Server IP"); ImGui::InputText("##host", hostBuf_, sizeof(hostBuf_));
        ImGui::Text("Port");      ImGui::InputText("##port", portBuf_, sizeof(portBuf_));
        ImGui::Text("Nick");      ImGui::InputText("##nick", nickBuf_, sizeof(nickBuf_));

        ImGui::Checkbox("Dev mode", &devMode_);

        if (state.phase == ClientPhase::Connecting) {
            ImGui::BeginDisabled(); ImGui::Button("Łączenie...", { -1, 36 }); ImGui::EndDisabled();
        }
        else {
            if (ImGui::Button("Połącz", { -1, 36 })) {
                action.type = UIAction::Type::Connect;
                action.host = hostBuf_;
                action.port = static_cast<uint16_t>(std::atoi(portBuf_));
                action.nick = nickBuf_;
            }
        }
        ImGui::End();
        return action;
    }

    UIAction UIRenderer::renderLobby(ClientState& state) {
        UIAction action;
        ImGuiIO& io = ImGui::GetIO();
        ImGui::SetNextWindowPos({ io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f }, ImGuiCond_Always, { 0.5f, 0.5f });
        ImGui::SetNextWindowSize({ 480, 0 }, ImGuiCond_Always);
        ImGui::Begin("##lobby", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize);

        ImGui::Text("Gracze w lobby: %zu", state.lobbyPlayers.size());
        ImGui::BeginChild("##players", { 0, 180 }, true);
        for (const auto& p : state.lobbyPlayers) {
            ImGui::TextColored(playerColor(p.id), "%s %s", p.name.c_str(), p.isBot ? "[bot]" : "");
        }
        ImGui::EndChild();

        if (devMode_) {
            ImGui::PushStyleColor(ImGuiCol_Button, { 0.2f, 0.5f, 0.2f, 1.0f });
            if (ImGui::Button("DEV: FORCE START", { -1, 30 })) action.type = UIAction::Type::DebugStartGame;
            ImGui::PopStyleColor();
        }

        if (ImGui::Button("Rozłącz", { 120, 0 })) action.type = UIAction::Type::Disconnect;

        ImGui::End();
        return action;
    }

    UIAction UIRenderer::renderGame(ClientState& state) {
        UIAction action;
        renderTopBar(state);

        ImGuiIO& io = ImGui::GetIO();
        float debugH = devMode_ ? 36.0f : 0.0f;

        ImGui::SetNextWindowPos({ 0, 30 });
        ImGui::SetNextWindowSize({ 200, io.DisplaySize.y - 30 - debugH });
        ImGui::Begin("##left", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize);
        renderLeftPanel(state);
        ImGui::End();

        ImGui::SetNextWindowPos({ io.DisplaySize.x - 180, 30 });
        ImGui::SetNextWindowSize({ 180, io.DisplaySize.y - 30 - debugH });
        ImGui::Begin("##right", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize);
        renderRightPanel(state);
        ImGui::End();

        if (devMode_) {
            ImGui::SetNextWindowPos({ 0, io.DisplaySize.y - 36 });
            ImGui::SetNextWindowSize({ io.DisplaySize.x, 36 });
            ImGui::Begin("##debug", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize);
            renderDebugBar(state, action);
            ImGui::End();
        }
        return action;
    }

    void UIRenderer::renderTopBar(ClientState& state) {
        ImGui::SetNextWindowPos({ 0, 0 });
        ImGui::SetNextWindowSize({ ImGui::GetIO().DisplaySize.x, 30 });
        ImGui::Begin("##topbar", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize);
        ImGui::Text("Tick: %u | Gold: %lld | Power: %.0f MW", state.currentTick, (long long)state.myGold, state.myPowerMW);
        ImGui::End();
    }

    void UIRenderer::renderLeftPanel(ClientState& state) {
        if (state.selectedProvinceId == 0) { ImGui::TextDisabled("Wybierz prowincję"); return; }
        ImGui::Text("Prowincja #%u", state.selectedProvinceId);
    }

    void UIRenderer::renderRightPanel(ClientState& state) {
        ImGui::TextDisabled("Gracze");
        for (const auto& e : state.entities) {
            ImGui::TextColored(playerColor(e.id), "■ %s", e.name.c_str());
        }
    }

    void UIRenderer::renderDebugBar(ClientState& state, UIAction& action) {
        ImGui::Text("DEV:"); ImGui::SameLine();
        if (ImGui::SmallButton("+1")) { action.type = UIAction::Type::DebugStep; action.debugSteps = 1; }
        ImGui::SameLine();
        if (ImGui::SmallButton("+10")) { action.type = UIAction::Type::DebugStep; action.debugSteps = 10; }
        ImGui::SameLine();
        if (ImGui::SmallButton("DUMP")) { action.type = UIAction::Type::DebugDumpState; }
    }

    const char* UIRenderer::terrainName(TerrainType t) { return "Terrain"; }
    const char* UIRenderer::buildingName(BuildingType t) { return "Building"; }

} // namespace gs::client