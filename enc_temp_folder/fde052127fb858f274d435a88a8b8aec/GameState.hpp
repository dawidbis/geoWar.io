#pragma once
#include "server/sim/models/MapComponents.hpp"
#include "server/sim/models/UnitComponents.hpp"
#include "server/sim/models/Buildings.hpp"
#include "server/sim/models/Combat.hpp"
#include "server/sim/models/Diplomacy.hpp"
#include "server/sim/models/Entities.hpp"
#include "server/sim/models/Events.hpp"

#include <set>      // ZMIANA: Gwarancja determinizmu sieciowego
#include <deque>    // ZMIANA: Zabezpieczenie przed crashami (stabilne wskaźniki)
#include <vector>

namespace gs::server {

    struct GameState {
        // Meta
        uint32_t   currentTick{ 0 };
        uint64_t   mapSeed{ 0 };
        GamePhase  phase{ GamePhase::Lobby };

        // ── Mapa ─────────────────────────────────────────────────────────────
        // Kafelki i prowincje są stałe po wygenerowaniu (WorldGen).
        // Bezpiecznie możemy zostawić std::vector (najlepszy cache-locality).
        uint16_t  mapWidth{ 0 };
        uint16_t  mapHeight{ 0 };

        std::vector<Tile>         tiles;
        std::vector<LandProvince> landProvinces;
        std::vector<SeaProvince>  seaProvinces;

        // ── Encje i budynki ──────────────────────────────────────────────────
        // ZMIANA: Używamy std::deque. Dodawanie nowych budynków lub encji 
        // nie unieważni już surowych wskaźników używanych przez Gettery!
        std::deque<Entity>   entities;
        std::deque<Building> buildings;

        // ── Jednostki ────────────────────────────────────────────────────────
        std::deque<Warship>        warships;
        std::deque<Carrier>        carriers;
        std::deque<SeaCargo>       seaCargos;
        std::deque<Fighter>        fighters;
        std::deque<Bomber>         bombers;
        std::deque<AirCargo>       airCargos;
        std::deque<MissileFlight>  missiles;

        // ── Ataki i dyplomacja ───────────────────────────────────────────────
        std::deque<Attack>         attacks;
        std::deque<Relation>       relations;
        std::deque<Pact>           pacts;
        std::deque<ElectricGrid>   electricGrids;

        // ── Per-tick bufory ──────────────────────────────────────────────────
        // Czyszczone co tick, więc wektor jest w porządku.
        std::vector<GameEvent>   pendingEvents;
        std::vector<PlayerInput> pendingInputs;

        // ── Dirty tracking ───────────────────────────────────────────────────
        // ZMIANA: std::set wymusza zawsze tę samą kolejność iteracji.
        // Brak Desynców przy budowaniu paczek GameStateDelta!
        std::set<uint32_t>  dirtyTiles;
        std::set<uint32_t>  dirtyBuildings;
        std::set<uint32_t>  dirtyEntities;
        std::set<uint64_t>  dirtyShares;   // (provId<<32|entityId)

        // ── Generatory ID ────────────────────────────────────────────────────
        uint32_t currentBuildingId_{ 1 };
        uint32_t currentUnitId_{ 1 };
        uint32_t currentAttackId_{ 1 };

        uint32_t nextBuildingId() { return currentBuildingId_++; }
        uint32_t nextUnitId() { return currentUnitId_++; }
        uint32_t nextAttackId() { return currentAttackId_++; }

        // ── Helpers ──────────────────────────────────────────────────────────

        void clearDirtyFlags() {
            dirtyTiles.clear();
            dirtyBuildings.clear();
            dirtyEntities.clear();
            dirtyShares.clear();
            pendingEvents.clear();
            pendingInputs.clear();
        }

        // Dzięki std::deque te wskaźniki są teraz bezpieczne do zwracania!
        Entity* getEntity(uint32_t id) {
            if (id == 0 || id > entities.size()) return nullptr;
            auto& e = entities[id - 1];
            return (e.id == id) ? &e : nullptr;
        }

        Building* getBuilding(uint32_t id) {
            if (id == 0 || id > buildings.size()) return nullptr;
            auto& b = buildings[id - 1];
            return (b.id == id) ? &b : nullptr;
        }

        Tile* getTile(uint32_t id) {
            if (id == 0 || id > tiles.size()) return nullptr;
            return &tiles[id - 1];
        }

        LandProvince* getLandProvince(uint32_t id) {
            if (id == 0 || id > landProvinces.size()) return nullptr;
            return &landProvinces[id - 1];
        }

        SeaProvince* getSeaProvince(uint32_t id) {
            if (id == 0 || id > seaProvinces.size()) return nullptr;
            return &seaProvinces[id - 1];
        }

        PlayerProvinceShare* getShare(uint32_t provinceId, uint32_t entityId) {
            auto* p = getLandProvince(provinceId);
            return p ? p->getShare(entityId) : nullptr;
        }

        Relation* getRelation(uint32_t a, uint32_t b) {
            for (auto& r : relations) {
                if ((r.entityA == a && r.entityB == b) ||
                    (r.entityA == b && r.entityB == a))
                    return &r;
            }
            return nullptr;
        }

        Relation& getOrCreateRelation(uint32_t a, uint32_t b) {
            auto* r = getRelation(a, b);
            if (r) return *r;
            relations.push_back({ a, b, 0 });
            return relations.back();
        }

        bool isAtWar(uint32_t a, uint32_t b) const {
            for (const auto& r : relations) {
                if ((r.entityA == a && r.entityB == b) ||
                    (r.entityA == b && r.entityB == a))
                    return r.hasFlag(RelationFlag::AtWar);
            }
            return false;
        }

        bool inSameMilitaryPact(uint32_t a, uint32_t b) const {
            auto* ea = const_cast<GameState*>(this)->getEntity(a);
            auto* eb = const_cast<GameState*>(this)->getEntity(b);
            if (!ea || !eb) return false;
            return ea->militaryPactId != 0 && ea->militaryPactId == eb->militaryPactId;
        }

        bool inSameEconomicPact(uint32_t a, uint32_t b) const {
            auto* ea = const_cast<GameState*>(this)->getEntity(a);
            auto* eb = const_cast<GameState*>(this)->getEntity(b);
            if (!ea || !eb) return false;
            return ea->economicPactId != 0 && ea->economicPactId == eb->economicPactId;
        }

        void markTileDirty(uint32_t tid) { dirtyTiles.insert(tid); }
        void markBuildingDirty(uint32_t bid) { dirtyBuildings.insert(bid); }
        void markEntityDirty(uint32_t eid) { dirtyEntities.insert(eid); }
        void markShareDirty(uint32_t p, uint32_t e) {
            dirtyShares.insert((static_cast<uint64_t>(p) << 32) | e);
        }
    };

} // namespace gs::server