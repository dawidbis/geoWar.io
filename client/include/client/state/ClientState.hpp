#pragma once
#include <shared/types/Enums.hpp>
#include <shared/net/Message.hpp>
#include <shared/net/Serializer.hpp>

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>

namespace gs::client {

    // ── Uproszczone struktury po stronie klienta (tylko to co potrzebne do renderowania) ──

    struct ClientTile {
        uint32_t    id{ 0 };
        uint16_t    x{ 0 }, y{ 0 };
        TerrainType terrain{ TerrainType::Plains };
        uint32_t    ownerId{ 0 };
        uint32_t    landProvinceId{ 0 };
        uint32_t    seaProvinceId{ 0 };
    };

    struct ClientProvinceShare {
        uint32_t entityId{ 0 };
        float    workers{ 0 }, military{ 0 }, maxCap{ 0 };
        float    supplyStored{ 0 };
        float    workerSplitRatio{ 0.6f };
    };

    struct ClientProvince {
        uint32_t    id{ 0 };
        std::string name;
        std::unordered_map<uint32_t, ClientProvinceShare> shares;
    };

    struct ClientBuilding {
        uint32_t     id{ 0 };
        BuildingType type{ BuildingType::City };
        uint8_t      level{ 1 };
        uint32_t     ownerId{ 0 };
        uint32_t     provinceId{ 0 };
        uint16_t     x{ 0 }, y{ 0 };
        bool         isActive{ false };
        bool         isCapital{ false };
    };

    struct ClientUnit {
        uint32_t id{ 0 };
        UnitType type{ UnitType::Warship };
        uint32_t ownerId{ 0 };
        float    x{ 0 }, y{ 0 };
        float    health{ 0 }, maxHealth{ 0 };
    };

    struct ClientEntity {
        uint32_t    id{ 0 };
        std::string name;
        EntityType  type{ EntityType::Human };
        int64_t     gold{ 0 };
        float       uranium{ 0 };
        bool        isEliminated{ false };
        uint32_t    militaryPactId{ 0 };
        uint32_t    economicPactId{ 0 };
    };

    struct ClientAttack {
        uint32_t     id{ 0 };
        uint32_t     attackerId{ 0 };
        uint32_t     sourceProvinceId{ 0 };
        uint32_t     targetProvinceId{ 0 };
        float        troops{ 0 };
        AttackStatus status{ AttackStatus::Active };
    };

    struct ClientEvent {
        GameEventType type{ GameEventType::PlayerJoined };
        uint32_t      entityId{ 0 };
        uint32_t      targetId{ 0 };
        float         x{ 0 }, y{ 0 }, radius{ 0 };
        uint32_t      tick{ 0 };
    };

    // ── Stan lobby ─────────────────────────────────────────────────────────────────

    struct LobbyPlayerInfo {
        uint32_t    id{ 0 };
        std::string name;
        bool        isBot{ false };
    };

    // ── Główna struktura stanu klienta ─────────────────────────────────────────────

    enum class ClientPhase {
        Disconnected,
        Connecting,
        InLobby,
        InGame,
    };

    struct ClientState {
        ClientPhase phase{ ClientPhase::Disconnected };

        // Tożsamość gracza
        uint32_t    myEntityId{ 0 };
        std::string myName;

        // Lobby
        std::vector<LobbyPlayerInfo> lobbyPlayers;
        std::string                  statusMessage;

        // Stan gry
        uint32_t currentTick{ 0 };
        uint16_t mapWidth{ 0 };
        uint16_t mapHeight{ 0 };

        std::vector<ClientTile>     tiles;
        std::vector<ClientProvince> provinces;
        std::vector<ClientBuilding> buildings;
        std::vector<ClientUnit>     units;
        std::vector<ClientEntity>   entities;
        std::vector<ClientAttack>   attacks;
        std::vector<ClientEvent>    recentEvents; // ostatnie 50

        // Zasoby gracza (skrót z entities)
        int64_t myGold{ 0 };
        float   myUranium{ 0 };
        float   mySupply{ 0 };
        float   myPowerMW{ 0 };

        // Zaznaczona prowincja
        uint32_t selectedProvinceId{ 0 };

        // Log połączenia
        std::vector<std::string> connectionLog;

        void addLog(const std::string& msg) {
            connectionLog.push_back(msg);
            if (connectionLog.size() > 100)
                connectionLog.erase(connectionLog.begin());
        }

        // ── Deserializacja wiadomości z serwera ────────────────────────────────────

        void applyServerWelcome(const Message& msg);
        void applyLobbyState(const Message& msg);
        void applyLobbyStart(const Message& msg);
        void applyGameFull(const Message& msg);
        void applyGameDelta(const Message& msg);

        // ── Helpers ────────────────────────────────────────────────────────────────

        ClientEntity* getMyEntity() {
            for (auto& e : entities)
                if (e.id == myEntityId) return &e;
            return nullptr;
        }

        ClientProvince* getProvince(uint32_t id) {
            for (auto& p : provinces)
                if (p.id == id) return &p;
            return nullptr;
        }

        ClientProvinceShare* getMyShare(uint32_t provinceId) {
            auto* p = getProvince(provinceId);
            if (!p) return nullptr;
            auto it = p->shares.find(myEntityId);
            return it != p->shares.end() ? &it->second : nullptr;
        }

        // Zwraca sumę wojska gracza we wszystkich prowincjach
        float totalMilitary() const {
            float sum = 0;
            for (const auto& p : provinces) {
                auto it = p.shares.find(myEntityId);
                if (it != p.shares.end()) sum += it->second.military;
            }
            return sum;
        }

        // Liczba kafelków gracza
        uint32_t myTileCount() const {
            uint32_t count = 0;
            for (const auto& t : tiles)
                if (t.ownerId == myEntityId) ++count;
            return count;
        }

        // % terytorium gracza
        float myTerritoryPercent() const {
            if (tiles.empty()) return 0;
            uint32_t total = 0;
            for (const auto& t : tiles)
                if (t.landProvinceId != 0 && t.ownerId != 0) ++total;
            if (total == 0) return 0;
            return static_cast<float>(myTileCount()) / static_cast<float>(total) * 100.0f;
        }
    };

} // namespace gs::client