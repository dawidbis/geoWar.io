#include "server/sim/systems/AirSystem.hpp"
#include <shared/sim/Constants.hpp>

#include <algorithm>
#include <cmath>

namespace gs::server {

    static constexpr float FIGHTER_SPEED = 2.0f;
    static constexpr float BOMBER_SPEED = 1.5f;
    static constexpr float CARGO_SPEED = 1.2f;
    static constexpr float AIR_WEAPONS_RANGE = 5.0f;
    static constexpr float FIGHTER_DAMAGE_VS_FIGHTER = 4.0f;
    static constexpr float FIGHTER_DAMAGE_VS_BOMBER = 6.0f;
    static constexpr float FIGHTER_DAMAGE_VS_CARGO = 8.0f;

    static constexpr float PATROL_RADIUS[4] = { 0, 80.0f, 140.0f, 220.0f };

    // AA per poziom: zasięg i szansa/tick
    static constexpr float AA_RANGE[4] = { 0, 30.0f, 50.0f, 80.0f };
    static constexpr float AA_CHANCE[4] = { 0, 0.08f, 0.15f, 0.25f };

    void AirSystem::tick(GameState& state) {
        tickFighters(state);
        tickBombers(state);
        tickAirCargos(state);
        tickAntiAir(state);

        // Usuń zestrzelone (Bezpieczne dzięki std::deque w GameState)
        std::erase_if(state.fighters, [](const Fighter& f) {
            return f.state == FighterState::ShotDown;
            });
        std::erase_if(state.bombers, [](const Bomber& b) {
            return b.health <= 0.0f;
            });
        std::erase_if(state.airCargos, [](const AirCargo& c) {
            return c.health <= 0.0f;
            });
    }

    // ── Myśliwce ──────────────────────────────────────────────────────────────────

    void AirSystem::tickFighters(GameState& state) {
        for (auto& fighter : state.fighters) {
            if (fighter.state == FighterState::ShotDown) continue;

            switch (fighter.state) {
            case FighterState::AtBase: {
                // Szukaj wroga w strefie patrolu
                float baseX = 0, baseY = 0;
                getBasePosition(state, fighter, baseX, baseY);
                float patrol = getPatrolRadius(state, fighter);

                uint32_t enemyId = findEnemyInZone(state, fighter, baseX, baseY, patrol);
                if (enemyId != 0) {
                    fighter.targetId = enemyId;
                    fighter.state = FighterState::Engaging;
                }
                break;
            }
            case FighterState::Engaging: {
                tickFighterEngage(state, fighter);
                break;
            }
            case FighterState::Escorting: {
                tickFighterEscort(state, fighter);
                break;
            }
            case FighterState::Returning: {
                float bx = 0, by = 0;
                getBasePosition(state, fighter, bx, by);
                moveStraight(fighter.x, fighter.y, bx, by, FIGHTER_SPEED);

                // Używamy distanceSq dla uniknięcia pierwiastka (1.0f * 1.0f = 1.0f)
                if (distanceSq(fighter.x, fighter.y, bx, by) < 1.0f) {
                    fighter.state = FighterState::AtBase;
                    fighter.health = std::min(fighter.health + 5.0f, fighter.maxHealth);
                }
                break;
            }
            default: break;
            }
        }
    }

    void AirSystem::tickFighterEngage(GameState& state, Fighter& fighter) {
        // Znajdź cel (bomber, inny fighter, cargo)
        float tx = 0, ty = 0;
        bool  alive = false;
        float rangeSq = AIR_WEAPONS_RANGE * AIR_WEAPONS_RANGE;

        for (auto& bomber : state.bombers) {
            if (bomber.id == fighter.targetId) {
                tx = bomber.x; ty = bomber.y;
                alive = bomber.health > 0.0f;
                if (alive && distanceSq(fighter.x, fighter.y, tx, ty) <= rangeSq) {
                    bomber.health -= FIGHTER_DAMAGE_VS_BOMBER;
                }
                break;
            }
        }
        for (auto& other : state.fighters) {
            if (other.id == fighter.targetId && other.id != fighter.id) {
                tx = other.x; ty = other.y;
                alive = other.state != FighterState::ShotDown;
                if (alive && distanceSq(fighter.x, fighter.y, tx, ty) <= rangeSq) {
                    other.health -= FIGHTER_DAMAGE_VS_FIGHTER;
                    fighter.health -= FIGHTER_DAMAGE_VS_FIGHTER;
                    if (other.health <= 0.0f) other.state = FighterState::ShotDown;
                }
                break;
            }
        }
        for (auto& cargo : state.airCargos) {
            if (cargo.id == fighter.targetId) {
                tx = cargo.x; ty = cargo.y;
                alive = cargo.health > 0.0f;
                if (alive && distanceSq(fighter.x, fighter.y, tx, ty) <= rangeSq) {
                    cargo.health -= FIGHTER_DAMAGE_VS_CARGO;
                }
                break;
            }
        }

        if (!alive || fighter.health <= 0.0f) {
            fighter.state = FighterState::Returning;
            fighter.targetId = 0;
            if (fighter.health <= 0.0f) fighter.state = FighterState::ShotDown;
            return;
        }

        moveStraight(fighter.x, fighter.y, tx, ty, FIGHTER_SPEED);
    }

    void AirSystem::tickFighterEscort(GameState& state, Fighter& fighter) {
        for (auto& bomber : state.bombers) {
            if (bomber.id != fighter.escortTargetId) continue;
            if (bomber.health <= 0.0f) {
                fighter.state = FighterState::Returning;
                fighter.escortTargetId = 0;
                return;
            }

            // Przesuń się obok bombowca
            moveStraight(fighter.x, fighter.y,
                bomber.x + 3.0f, bomber.y, FIGHTER_SPEED);

            // Atakuj wrogów w pobliżu
            uint32_t nearEnemy = findEnemyNearPoint(state, fighter.ownerId,
                bomber.x, bomber.y,
                AIR_WEAPONS_RANGE * 3.0f);
            if (nearEnemy != 0) {
                fighter.targetId = nearEnemy;
                fighter.state = FighterState::Engaging;
            }
            return;
        }

        // Bombowiec znikł
        fighter.state = FighterState::Returning;
        fighter.escortTargetId = 0;
    }

    // ── Bombowce ──────────────────────────────────────────────────────────────────

    void AirSystem::tickBombers(GameState& state) {
        for (auto& bomber : state.bombers) {
            if (bomber.health <= 0.0f) continue;

            if (bomber.hasReleased) {
                // Powrót do bazy
                auto* base = state.getBuilding(bomber.homeAirportId);
                if (!base) { bomber.health = 0.0f; continue; }
                float bx = base->topLeftX, by = base->topLeftY;
                moveStraight(bomber.x, bomber.y, bx, by, BOMBER_SPEED);
                if (distanceSq(bomber.x, bomber.y, bx, by) < 1.0f) {
                    bomber.health = 0.0f; // jednorazowy — usuń po powrocie
                }
            }
            else {
                // Leć do celu
                float tx = bomber.targetX, ty = bomber.targetY;
                if (distanceSq(bomber.x, bomber.y, tx, ty) < (BOMBER_SPEED * BOMBER_SPEED)) {
                    // Zrzuć bombę
                    bomber.hasReleased = true;
                    GameEvent evt;
                    evt.type = GameEventType::AtomicBombDetonation;
                    evt.entityId = bomber.ownerId;
                    evt.x = tx;
                    evt.y = ty;
                    evt.radius = static_cast<float>(constants::NUKE_RADIUS_ATOMIC);
                    evt.tick = state.currentTick;
                    state.pendingEvents.push_back(evt);
                }
                else {
                    moveStraight(bomber.x, bomber.y, tx, ty, BOMBER_SPEED);
                }
            }
        }

        for (auto& input : state.pendingInputs) {
            if (input.type == InputType::LaunchBomber)
                handleLaunchBomberInput(state, input);
        }
    }

    // ── Cargo lotnicze ────────────────────────────────────────────────────────────

    void AirSystem::tickAirCargos(GameState& state) {
        for (auto& cargo : state.airCargos) {
            if (cargo.health <= 0.0f) continue;

            auto* dest = state.getBuilding(
                cargo.isReturning ? cargo.homeAirportId : cargo.destinationAirportId);
            if (!dest) { cargo.health = 0.0f; continue; }

            float dx = dest->topLeftX - cargo.x;
            float dy = dest->topLeftY - cargo.y;
            float dSq = dx * dx + dy * dy;

            if (dSq < (CARGO_SPEED * CARGO_SPEED)) {
                cargo.x = dest->topLeftX;
                cargo.y = dest->topLeftY;
                if (!cargo.isReturning) {
                    cargo.isReturning = true;
                    // Złoto wypłaca TradeSystem::processDeliveries()
                }
                else {
                    cargo.health = 0.0f;
                }
            }
            else {
                float d = std::sqrt(dSq);
                cargo.x += (dx / d) * CARGO_SPEED;
                cargo.y += (dy / d) * CARGO_SPEED;
            }
        }
    }

    // ── Obrona przeciwlotnicza ────────────────────────────────────────────────────

    void AirSystem::tickAntiAir(GameState& state) {
        // ZMIANA: Usunięty 'static mt19937' - izolowany stan LCG dla determinizmu meczu
        uint64_t prngState = state.mapSeed ^ (static_cast<uint64_t>(state.currentTick) * 2654435761ULL);

        auto rollFloat = [&prngState]() -> float {
            prngState = prngState * 6364136223846793005ULL + 1442695040888963407ULL;
            uint32_t val = static_cast<uint32_t>(prngState >> 32);
            return static_cast<float>(val) / static_cast<float>(0xFFFFFFFF);
            };

        for (auto& b : state.buildings) {
            if (b.type != BuildingType::AntiAir) continue;
            if (!b.isActive) continue;
            int lvl = std::min((int)b.level, 3);

            float rangeSq = AA_RANGE[lvl] * AA_RANGE[lvl];
            float chance = AA_CHANCE[lvl];

            // Strzelaj do myśliwców
            for (auto& fighter : state.fighters) {
                if (fighter.state == FighterState::ShotDown) continue;
                if (fighter.ownerId == b.ownerId) continue;
                if (!state.isAtWar(b.ownerId, fighter.ownerId)) continue;
                if (distanceSq(b.topLeftX, b.topLeftY, fighter.x, fighter.y) > rangeSq) continue;
                if (rollFloat() < chance) fighter.state = FighterState::ShotDown;
            }
            // Strzelaj do bombowców
            for (auto& bomber : state.bombers) {
                if (bomber.health <= 0.0f) continue;
                if (bomber.ownerId == b.ownerId) continue;
                if (!state.isAtWar(b.ownerId, bomber.ownerId)) continue;
                if (distanceSq(b.topLeftX, b.topLeftY, bomber.x, bomber.y) > rangeSq) continue;
                if (rollFloat() < chance) bomber.health = 0.0f;
            }
            // Strzelaj do cargo
            for (auto& cargo : state.airCargos) {
                if (cargo.health <= 0.0f) continue;
                if (cargo.ownerId == b.ownerId) continue;
                if (!state.isAtWar(b.ownerId, cargo.ownerId)) continue;
                if (distanceSq(b.topLeftX, b.topLeftY, cargo.x, cargo.y) > rangeSq) continue;
                if (rollFloat() < chance) cargo.health = 0.0f;
            }
        }
    }

    // ── Helpers ───────────────────────────────────────────────────────────────────

    void AirSystem::getBasePosition(const GameState& state, const Fighter& fighter,
        float& outX, float& outY) const {
        if (fighter.homeBaseType == HomeBaseType::Airport) {
            auto* b = const_cast<GameState&>(state).getBuilding(fighter.homeBaseId);
            if (b) { outX = b->topLeftX; outY = b->topLeftY; return; }
        }
        else if (fighter.homeBaseType == HomeBaseType::Carrier) {
            for (const auto& c : state.carriers) {
                if (c.id == fighter.homeBaseId) { outX = c.visualX; outY = c.visualY; return; }
            }
        }
        outX = fighter.x; outY = fighter.y;
    }

    float AirSystem::getPatrolRadius(const GameState& state,
        const Fighter& fighter) const {
        if (fighter.homeBaseType == HomeBaseType::Airport) {
            auto* b = const_cast<GameState&>(state).getBuilding(fighter.homeBaseId);
            if (b) return PATROL_RADIUS[std::min((int)b->level, 3)];
        }
        return 80.0f;
    }

    uint32_t AirSystem::findEnemyInZone(const GameState& state,
        const Fighter& fighter,
        float cx, float cy, float radius) const {
        float radiusSq = radius * radius;
        for (const auto& bomber : state.bombers) {
            if (bomber.health <= 0.0f) continue;
            if (!state.isAtWar(fighter.ownerId, bomber.ownerId)) continue;
            if (distanceSq(cx, cy, bomber.x, bomber.y) <= radiusSq) return bomber.id;
        }
        for (const auto& other : state.fighters) {
            if (other.id == fighter.id) continue;
            if (other.state == FighterState::ShotDown) continue;
            if (!state.isAtWar(fighter.ownerId, other.ownerId)) continue;
            if (distanceSq(cx, cy, other.x, other.y) <= radiusSq) return other.id;
        }
        for (const auto& cargo : state.airCargos) {
            if (cargo.health <= 0.0f) continue;
            if (!state.isAtWar(fighter.ownerId, cargo.ownerId)) continue;
            if (distanceSq(cx, cy, cargo.x, cargo.y) <= radiusSq) return cargo.id;
        }
        return 0;
    }

    uint32_t AirSystem::findEnemyNearPoint(const GameState& state,
        uint32_t ownerId,
        float px, float py, float radius) const {
        float radiusSq = radius * radius;
        for (const auto& other : state.fighters) {
            if (other.state == FighterState::ShotDown) continue;
            if (!state.isAtWar(ownerId, other.ownerId)) continue;
            if (distanceSq(px, py, other.x, other.y) <= radiusSq) return other.id;
        }
        return 0;
    }

    void AirSystem::moveStraight(float& x, float& y,
        float tx, float ty, float speed) const {
        float dx = tx - x;
        float dy = ty - y;
        float dSq = dx * dx + dy * dy;

        if (dSq < speed * speed) { x = tx; y = ty; return; }

        // std::sqrt tu musi zostać dla normalizacji wektora
        float d = std::sqrt(dSq);
        x += (dx / d) * speed;
        y += (dy / d) * speed;
    }

    // ZMIANA: Operujemy na kwadratach dystansu
    float AirSystem::distanceSq(float ax, float ay,
        float bx, float by) const {
        float dx = ax - bx;
        float dy = ay - by;
        return (dx * dx) + (dy * dy);
    }

    void AirSystem::handleLaunchBomberInput(GameState& state,
        const PlayerInput& input) {
        auto* airport = state.getBuilding(input.unitId);
        if (!airport || airport->ownerId != input.entityId) return;

        auto* entity = state.getEntity(input.entityId);
        if (!entity) return;

        if (entity->uranium < constants::URANIUM_COST_ATOMIC) return;
        if (entity->gold < 4000) return;

        entity->uranium -= constants::URANIUM_COST_ATOMIC;
        entity->gold -= 4000;

        Bomber bomber;
        bomber.id = state.nextUnitId();
        bomber.ownerId = input.entityId;
        bomber.homeAirportId = input.unitId;
        bomber.x = static_cast<float>(airport->topLeftX);
        bomber.y = static_cast<float>(airport->topLeftY);
        bomber.targetX = input.tileX;
        bomber.targetY = input.tileY;
        bomber.health = 60.0f;
        bomber.hasReleased = false;

        state.bombers.push_back(std::move(bomber));
        state.markEntityDirty(input.entityId);
    }

} // namespace gs::server