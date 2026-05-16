#include "client/state/ClientState.hpp"

namespace gs::client {

    void ClientState::applyServerWelcome(const Message& msg) {
        Serializer s(std::span<const uint8_t>(msg.payload));
        myEntityId = s.readU32();
        myName = s.readStr();
        addLog("→ server welcome, entityId=" + std::to_string(myEntityId));
    }

    void ClientState::applyLobbyState(const Message& msg) {
        Serializer s(std::span<const uint8_t>(msg.payload));
        uint8_t count = s.readU8();
        lobbyPlayers.clear();
        for (uint8_t i = 0; i < count; ++i) {
            LobbyPlayerInfo info;
            info.id = s.readU32();
            info.name = s.readStr();
            info.isBot = s.readBool();
            lobbyPlayers.push_back(info);
        }
    }

    void ClientState::applyLobbyStart(const Message& msg) {
        Serializer s(std::span<const uint8_t>(msg.payload));
        uint32_t tickrate = s.readU32();
        uint64_t seed = s.readU64();
        phase = ClientPhase::InGame;
        addLog("→ gra startuje (tickrate=" + std::to_string(tickrate)
            + " seed=" + std::to_string(seed) + ")");
    }

    void ClientState::applyGameFull(const Message& msg) {
        if (msg.payload.empty()) return;
        Serializer s(std::span<const uint8_t>(msg.payload));

        currentTick = s.readU32();
        mapWidth = s.readU16();
        mapHeight = s.readU16();

        // Kafelki
        tiles.clear();
        uint32_t numTiles = s.readU32();
        tiles.reserve(numTiles);
        for (uint32_t i = 0; i < numTiles; ++i) {
            ClientTile t;
            t.id = s.readU32();
            t.x = s.readU16();
            t.y = s.readU16();
            t.ownerId = s.readU32();
            t.terrain = static_cast<TerrainType>(s.readU8());
            t.landProvinceId = s.readU32();
            t.seaProvinceId = s.readU32();
            tiles.push_back(t);
        }

        // Prowincje
        provinces.clear();
        uint32_t numProvs = s.readU32();
        provinces.reserve(numProvs);
        for (uint32_t i = 0; i < numProvs; ++i) {
            ClientProvince prov;
            prov.id = s.readU32();
            prov.name = s.readStr();
            uint16_t numShares = s.readU16();
            for (uint16_t j = 0; j < numShares; ++j) {
                ClientProvinceShare sh;
                uint32_t provId = s.readU32(); (void)provId;
                sh.entityId = s.readU32();
                sh.workers = s.readF32();
                sh.military = s.readF32();
                sh.maxCap = s.readF32();
                sh.supplyStored = s.readF32();
                sh.workerSplitRatio = s.readF32();
                prov.shares[sh.entityId] = sh;
            }
            provinces.push_back(std::move(prov));
        }

        // Encje
        entities.clear();
        uint32_t numEntities = s.readU32();
        entities.reserve(numEntities);
        for (uint32_t i = 0; i < numEntities; ++i) {
            ClientEntity e;
            e.id = s.readU32();
            e.name = s.readStr();
            e.type = static_cast<EntityType>(s.readU8());
            e.gold = s.readI64();
            e.uranium = s.readF32();
            e.isEliminated = s.readBool();
            e.militaryPactId = s.readU32();
            e.economicPactId = s.readU32();
            if (e.id == myEntityId) {
                myGold = e.gold;
                myUranium = e.uranium;
            }
            entities.push_back(std::move(e));
        }

        // Budynki
        buildings.clear();
        uint32_t numBuildings = s.readU32();
        buildings.reserve(numBuildings);
        for (uint32_t i = 0; i < numBuildings; ++i) {
            ClientBuilding b;
            b.id = s.readU32();
            b.type = static_cast<BuildingType>(s.readU8());
            b.level = s.readU8();
            b.ownerId = s.readU32();
            b.provinceId = s.readU32();
            b.x = s.readU16();
            b.y = s.readU16();
            b.isActive = s.readBool();
            b.isCapital = s.readBool();
            buildings.push_back(b);
        }

        // Aktualizuj supply
        mySupply = 0;
        for (auto& p : provinces) {
            auto it = p.shares.find(myEntityId);
            if (it != p.shares.end()) mySupply += it->second.supplyStored;
        }

        addLog("→ snapshot: " + std::to_string(tiles.size()) + " kafelków, "
            + std::to_string(provinces.size()) + " prowincji, "
            + std::to_string(entities.size()) + " graczy");
    }

    void ClientState::applyGameDelta(const Message& msg) {
        if (msg.payload.size() < 4) return;
        Serializer s(std::span<const uint8_t>(msg.payload));

        currentTick = s.readU32();
        uint16_t numTiles = s.readU16();
        uint16_t numShares = s.readU16();
        uint16_t numUnits = s.readU16();
        uint16_t numEvents = s.readU16();

        for (uint16_t i = 0; i < numTiles; ++i) {
            uint32_t id = s.readU32();
            uint32_t ownerId = s.readU32();
            uint8_t  terrain = s.readU8();
            bool found = false;
            for (auto& t : tiles) {
                if (t.id == id) {
                    t.ownerId = ownerId;
                    t.terrain = static_cast<TerrainType>(terrain);
                    found = true;
                    break;
                }
            }
            if (!found) {
                ClientTile t;
                t.id = id; t.ownerId = ownerId;
                t.terrain = static_cast<TerrainType>(terrain);
                tiles.push_back(t);
            }
        }

        for (uint16_t i = 0; i < numShares; ++i) {
            uint32_t provId = s.readU32();
            uint32_t entityId = s.readU32();
            float workers = s.readF32();
            float military = s.readF32();
            float maxCap = s.readF32();
            float supply = s.readF32();
            float split = s.readF32();

            auto* prov = getProvince(provId);
            if (!prov) {
                ClientProvince p; p.id = provId;
                provinces.push_back(p);
                prov = &provinces.back();
            }
            auto& sh = prov->shares[entityId];
            sh.entityId = entityId;
            sh.workers = workers;
            sh.military = military;
            sh.maxCap = maxCap;
            sh.supplyStored = supply;
            sh.workerSplitRatio = split;
        }

        for (uint16_t i = 0; i < numUnits; ++i) {
            uint32_t id = s.readU32();
            uint8_t  type = s.readU8();
            uint32_t ownerId = s.readU32();
            float x = s.readF32(), y = s.readF32();
            float health = s.readF32(), maxHp = s.readF32();
            bool found = false;
            for (auto& u : units) {
                if (u.id == id) { u.x = x; u.y = y; u.health = health; found = true; break; }
            }
            if (!found) {
                ClientUnit u;
                u.id = id; u.type = static_cast<UnitType>(type);
                u.ownerId = ownerId; u.x = x; u.y = y;
                u.health = health; u.maxHealth = maxHp;
                units.push_back(u);
            }
        }

        for (uint16_t i = 0; i < numEvents; ++i) {
            ClientEvent evt;
            evt.type = static_cast<GameEventType>(s.readU8());
            evt.entityId = s.readU32();
            evt.targetId = s.readU32();
            evt.x = s.readF32();
            evt.y = s.readF32();
            evt.radius = s.readF32();
            evt.tick = currentTick;
            recentEvents.push_back(evt);
            if (recentEvents.size() > 50)
                recentEvents.erase(recentEvents.begin());
        }

        // Aktualizuj zasoby gracza
        for (auto& e : entities)
            if (e.id == myEntityId) { myGold = e.gold; myUranium = e.uranium; break; }

        mySupply = 0;
        for (auto& p : provinces) {
            auto it = p.shares.find(myEntityId);
            if (it != p.shares.end()) mySupply += it->second.supplyStored;
        }
    }

} // namespace gs::client