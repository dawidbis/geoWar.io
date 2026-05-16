#pragma once
#include <client/state/ClientState.hpp>
#include <shared/net/Message.hpp>
#include <functional>
#include <string>

namespace gs::client {

    struct UIAction {
        enum class Type {
            None,
            Connect,
            Disconnect,
            SendInput,
            DebugStep,
            DebugSetTickrate,
            DebugDumpState,
            DebugStartGame
        };

        Type type{ Type::None };

        // Dane dla Type::Connect
        std::string host;
        uint16_t    port{ 7777 };
        std::string nick;

        // Dane dla Type::DebugStep / SendInput
        uint32_t    debugSteps{ 1 };
        uint32_t    debugTickrate{ 10 };
    };

    class UIRenderer {
    public:
        UIAction render(ClientState& state);

    private:
        UIAction renderConnect(ClientState& state);
        UIAction renderLobby(ClientState& state);
        UIAction renderGame(ClientState& state);

        void renderTopBar(ClientState& state);
        void renderLeftPanel(ClientState& state);
        void renderRightPanel(ClientState& state);
        void renderDebugBar(ClientState& state, UIAction& action);

        const char* terrainName(TerrainType t);
        const char* buildingName(BuildingType t);

        // Bufory ImGui
        char hostBuf_[128]{ "127.0.0.1" };
        char portBuf_[8]{ "7777" };
        char nickBuf_[33]{ "Player" };
        bool devMode_{ false };
    };

} // namespace gs::client