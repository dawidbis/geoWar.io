#include "server/sim/systems/NavalSystem.hpp"
#include <shared/sim/Constants.hpp>

#include <algorithm>
#include <cmath>

namespace gs::server {

    static constexpr float WARSHIP_SPEED = 1.0f;   // kafelki/tick
    static constexpr float WARSHIP_PATROL_RANGE = 60.0f;
    static constexpr float WARSHIP_DETECT_RANGE = 25.0f;
    static constexpr float WARSHIP_WEAPONS_RANGE = 8.0f;
    static constexpr float WARSHIP_DAMAGE = 5.0f;
    static constexpr float CARRIER_SPEED = 0.3f;
    static constexpr float SEA_CARGO_SPEED = 0.5f;

    void NavalSystem::tick(GameState& state) {
        // 1. ZMIANA: Najpierw obsługujemy inputy, aby statki mogły zareagować od razu
        for (auto& input : state.pendingInputs) {
            if (input.type == InputType::MoveCarrier) {
                handleMoveCarrierInput(state, input);
            }
            else if (input.type == InputType::SetUnitHomeBase) {
                handleSetHomeBaseInput(state, input);
            }
        }

        // 2. Właściwy tick
        tickWarships(state);
        tickCarriers(state);
        tickSeaCargos(state);

        // Usuń zatopione jednostki (Bezpieczne dzięki std::deque w GameState)
        std::erase_if(state.warships, [](const Warship& w) { return w.health <= 0.0f; });
        std::erase_if(state.seaCargos, [](const SeaCargo& c) { return c.health <= 0.0f; });
    }

    // ── Okręty wojenne ────────────────────────────────────────────────────────────

    void NavalSystem::tickWarships(GameState& state) {
        for (auto& ship : state.warships) {
            if (ship.health <= 0.0f) continue;

            switch (ship.state) {
            case WarshipState::Patrolling:  tickPatrol(state, ship); break;
            case WarshipState::Engaging:    tickEngage(state, ship); break;
            case WarshipState::Returning:   tickReturn(state, ship); break;
            case WarshipState::Resupply:    tickResupply(state, ship); break;
            case WarshipState::MovingToBase: tickMoveToBase(state, ship); break;
            }
        }
    }

    void NavalSystem::tickPatrol(GameState& state, Warship& ship) {
        // Szukaj celów w zasięgu detekcji
        float nearestDistSq = (WARSHIP_DETECT_RANGE + 1.0f) * (WARSHIP_DETECT_RANGE + 1.0f);
        uint32_t nearestId = 0;

        for (auto& other : state.warships) {
            if (other.id == ship.id) continue;
            if (other.health <= 0.0f) continue;
            if (!state.isAtWar(ship.ownerId, other.ownerId)) continue;

            float dSq = distanceSq(ship.x, ship.y, other.x, other.y);
            if (dSq < nearestDistSq) { nearestDistSq = dSq; nearestId = other.id; }
        }
        for (auto& cargo : state.seaCargos) {
            if (cargo.health <= 0.0f) continue;
            if (cargo.ownerId == ship.ownerId) continue; // Własnego cargo nie atakujemy (chyba że zdrajca?)
            if (!state.isAtWar(ship.ownerId, cargo.ownerId)) continue; // TODO: Pamiętaj że piractwo było na neutralnych też

            float dSq = distanceSq(ship.x, ship.y, cargo.x, cargo.y);
            if (dSq < nearestDistSq) { nearestDistSq = dSq; nearestId = cargo.id; }
        }

        if (nearestId != 0) {
            ship.targetId = nearestId;
            ship.state = WarshipState::Engaging;
            return;
        }

        // Patrol wokół home base
        float homeX = 0.0f, homeY = 0.0f;
        getHomeBasePosition(state, ship, homeX, homeY);

        float dx = homeX - ship.x;
        float dy = homeY - ship.y;
        float dSq = dx * dx + dy * dy;

        if (dSq > WARSHIP_PATROL_RANGE * WARSHIP_PATROL_RANGE) {
            // Wróć w kierunku bazy
            moveToward(ship.x, ship.y, homeX, homeY, WARSHIP_SPEED);
        }
        else {
            // ZMIANA: Deterministyczny i lokalny LCG dla ruchu patrolu.
            // Dodajemy ship.id, aby każdy statek poruszał się niezależnie.
            uint64_t prngState = state.mapSeed ^ (static_cast<uint64_t>(state.currentTick) * 2654435761ULL) ^ ship.id;
            prngState = prngState * 6364136223846793005ULL + 1442695040888963407ULL;

            uint32_t random32 = static_cast<uint32_t>(prngState >> 32);
            float normalized = static_cast<float>(random32) / static_cast<float>(0xFFFFFFFF);
            float angle = normalized * 6.2831853f; // 0 to 2*PI

            ship.x += std::cos(angle) * WARSHIP_SPEED;
            ship.y += std::sin(angle) * WARSHIP_SPEED;
        }
    }

    void NavalSystem::tickEngage(GameState& state, Warship& ship) {
        // Znajdź cel
        float tx = 0.0f, ty = 0.0f;
        bool  targetAlive = false;

        for (auto& other : state.warships) {
            if (other.id == ship.targetId) {
                tx = other.x; ty = other.y;
                targetAlive = other.health > 0.0f;
                break;
            }
        }
        if (!targetAlive) {
            for (auto& cargo : state.seaCargos) {
                if (cargo.id == ship.targetId) {
                    tx = cargo.x; ty = cargo.y;
                    targetAlive = cargo.health > 0.0f;
                    break;
                }
            }
        }

        if (!targetAlive) {
            ship.state = WarshipState::Returning;
            ship.targetId = 0;
            return;
        }

        float dSq = distanceSq(ship.x, ship.y, tx, ty);
        if (dSq <= WARSHIP_WEAPONS_RANGE * WARSHIP_WEAPONS_RANGE) {
            // Wymiana ognia z okrętem / piractwo cargo
            for (auto& other : state.warships) {
                if (other.id == ship.targetId && other.health > 0.0f) {
                    other.health -= WARSHIP_DAMAGE;
                    ship.health -= WARSHIP_DAMAGE;
                    break;
                }
            }
            for (auto& cargo : state.seaCargos) {
                if (cargo.id == ship.targetId && cargo.health > 0.0f) {
                    checkPiracy(state, ship, cargo);
                    break;
                }
            }
        }
        else {
            moveToward(ship.x, ship.y, tx, ty, WARSHIP_SPEED);
        }

        // Ucieczka gdy niski health
        if (ship.health < ship.maxHealth * 0.3f) {
            ship.state = WarshipState::Returning;
        }
    }

    void NavalSystem::tickReturn(GameState& state, Warship& ship) {
        float homeX = 0.0f, homeY = 0.0f;
        getHomeBasePosition(state, ship, homeX, homeY);
        float dSq = distanceSq(ship.x, ship.y, homeX, homeY);

        if (dSq < 4.0f) { // 2.0f * 2.0f
            ship.state = WarshipState::Resupply;
        }
        else {
            moveToward(ship.x, ship.y, homeX, homeY, WARSHIP_SPEED);
        }
    }

    void NavalSystem::tickResupply(GameState& state, Warship& ship) {
        ship.health = std::min(ship.health + 1.0f, ship.maxHealth);
        if (ship.health >= ship.maxHealth) {
            ship.state = WarshipState::Patrolling;
        }
        (void)state;
    }

    void NavalSystem::tickMoveToBase(GameState& state, Warship& ship) {
        float homeX = 0.0f, homeY = 0.0f;
        getHomeBasePosition(state, ship, homeX, homeY);
        float dSq = distanceSq(ship.x, ship.y, homeX, homeY);

        if (dSq < 4.0f) { // 2.0f * 2.0f
            ship.state = WarshipState::Patrolling;
        }
        else {
            moveToward(ship.x, ship.y, homeX, homeY, WARSHIP_SPEED);
        }
    }

    // ── Lotniskowce ───────────────────────────────────────────────────────────────

    void NavalSystem::tickCarriers(GameState& state) {
        for (auto& carrier : state.carriers) {
            if (carrier.state != CarrierState::EnRoute) continue;

            auto* target = state.getSeaProvince(carrier.targetSeaProvinceId);
            if (!target) { carrier.state = CarrierState::Stationed; continue; }

            float dSq = distanceSq(carrier.visualX, carrier.visualY,
                target->centerX, target->centerY);

            if (dSq < (CARRIER_SPEED * CARRIER_SPEED)) {
                carrier.visualX = target->centerX;
                carrier.visualY = target->centerY;
                carrier.seaProvinceId = carrier.targetSeaProvinceId;
                carrier.targetSeaProvinceId = 0;
                carrier.state = CarrierState::Stationed;
            }
            else {
                moveToward(carrier.visualX, carrier.visualY,
                    target->centerX, target->centerY, CARRIER_SPEED);
            }
        }
    }

    // ── Transporty morskie ────────────────────────────────────────────────────────

    void NavalSystem::tickSeaCargos(GameState& state) {
        for (auto& cargo : state.seaCargos) {
            if (cargo.health <= 0.0f) continue;

            auto* dest = state.getBuilding(
                cargo.isReturning ? cargo.homePortId : cargo.destinationPortId);
            if (!dest) { cargo.health = 0.0f; continue; }

            moveToward(cargo.x, cargo.y,
                static_cast<float>(dest->topLeftX),
                static_cast<float>(dest->topLeftY),
                SEA_CARGO_SPEED);
        }
    }

    // ── Piractwo ──────────────────────────────────────────────────────────────────

    void NavalSystem::checkPiracy(GameState& state, Warship& ship, SeaCargo& cargo) {
        auto* pirate = state.getEntity(ship.ownerId);
        if (!pirate) return;

        if (state.isAtWar(ship.ownerId, cargo.ownerId)) {
            pirate->gold += static_cast<int64_t>(cargo.value * 0.2f);
        }
        else {
            // Piractwo na neutralnym — incydent
            pirate->gold += static_cast<int64_t>(cargo.value * 0.5f);
            GameEvent evt;
            evt.type = GameEventType::PiracyIncident;
            evt.entityId = ship.ownerId;
            evt.targetId = cargo.ownerId;
            evt.tick = state.currentTick;
            state.pendingEvents.push_back(evt);
        }

        cargo.health = 0.0f;
        state.markEntityDirty(ship.ownerId); // Tutaj dirty jest bezpieczne (event driven)
        ship.state = WarshipState::Returning;
        ship.targetId = 0;
    }

    // ── Helpers ───────────────────────────────────────────────────────────────────

    void NavalSystem::getHomeBasePosition(const GameState& state,
        const Warship& ship,
        float& outX, float& outY) const {
        if (ship.homeBaseType == HomeBaseType::Port) {
            auto* b = const_cast<GameState&>(state).getBuilding(ship.homeBaseId);
            if (b) { outX = b->topLeftX; outY = b->topLeftY; return; }
        }
        else if (ship.homeBaseType == HomeBaseType::Carrier) {
            for (const auto& carrier : state.carriers) {
                if (carrier.id == ship.homeBaseId) {
                    outX = carrier.visualX; outY = carrier.visualY; return;
                }
            }
        }
        outX = ship.x; outY = ship.y;
    }

    void NavalSystem::moveToward(float& x, float& y,
        float tx, float ty, float speed) const {
        float dx = tx - x;
        float dy = ty - y;
        float dSq = dx * dx + dy * dy;
        if (dSq < speed * speed) { x = tx; y = ty; return; }

        float d = std::sqrt(dSq); // Tu pierwiastek musi zostać (normalizacja wektora)
        x += (dx / d) * speed;
        y += (dy / d) * speed;
    }

    float NavalSystem::distanceSq(float ax, float ay,
        float bx, float by) const {
        float dx = ax - bx;
        float dy = ay - by;
        return (dx * dx) + (dy * dy);
    }

    void NavalSystem::handleMoveCarrierInput(GameState& state,
        const PlayerInput& input) {
        for (auto& carrier : state.carriers) {
            if (carrier.id != input.unitId) continue;
            if (carrier.ownerId != input.entityId) continue;
            carrier.targetSeaProvinceId = input.targetId;
            carrier.state = CarrierState::EnRoute;
            break;
        }
    }

    void NavalSystem::handleSetHomeBaseInput(GameState& state,
        const PlayerInput& input) {
        for (auto& ship : state.warships) {
            if (ship.id != input.unitId) continue;
            if (ship.ownerId != input.entityId) continue;
            ship.homeBaseId = input.targetId;
            ship.homeBaseType = static_cast<HomeBaseType>(
                static_cast<uint8_t>(input.floatParam));
            ship.state = WarshipState::MovingToBase;
            break;
        }
    }

} // namespace gs::server