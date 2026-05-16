#include "server/sim/systems/NukeSystem.hpp"
#include <shared/sim/Constants.hpp>

#include <algorithm>
#include <cmath>
#include <map>

namespace gs::server {

    // Obrona przeciwrakietowa per poziom
    static constexpr float ANTI_MISSILE_RANGE[4] = { 0, 80.0f, 150.0f, 250.0f };
    static constexpr float ANTI_MISSILE_CHANCE[4] = { 0, 0.35f, 0.60f,  0.80f };
    static constexpr float MISSILE_SPEED = 5.0f; // kafelki/tick — rakieta balistyczna

    void NukeSystem::tick(GameState& state) {
        // 1. Ruch rakiet + próby przechwycenia
        tickMissiles(state);

        // Kopiujemy wektor, by bezpiecznie iterować (detonate dodaje event wizualny)
        auto currentEvents = state.pendingEvents;

        for (auto& evt : currentEvents) {
            if (evt.type == GameEventType::AtomicBombDetonation) {
                detonate(state, evt.x, evt.y, static_cast<int>(constants::NUKE_RADIUS_ATOMIC));
            }
            else if (evt.type == GameEventType::HydrogenDetonation) {
                detonate(state, evt.x, evt.y, static_cast<int>(constants::NUKE_RADIUS_HYDROGEN));
            }
        }

        // 3. Inputy — LaunchMissile i SetNuclearMode
        for (auto& input : state.pendingInputs) {
            if (input.type == InputType::LaunchMissile)
                handleLaunchMissileInput(state, input);
            else if (input.type == InputType::SetNuclearMode)
                handleNuclearModeInput(state, input);
        }
    }

    // ── Rakiety balistyczne ───────────────────────────────────────────────────────

    void NukeSystem::tickMissiles(GameState& state) {
        for (auto& missile : state.missiles) {
            if (missile.intercepted) continue;

            // Próba przechwycenia przez obronę przeciwrakietową
            tryIntercept(state, missile);
            if (missile.intercepted) continue;

            // Przesuń rakietę
            float dx = missile.targetX - missile.currentX;
            float dy = missile.targetY - missile.currentY;
            float dSq = dx * dx + dy * dy;

            if (dSq < MISSILE_SPEED * MISSILE_SPEED) {
                // Eksplozja
                GameEvent evt;
                evt.type = GameEventType::HydrogenDetonation;
                evt.entityId = missile.ownerId;
                evt.x = missile.targetX;
                evt.y = missile.targetY;
                evt.radius = static_cast<float>(constants::NUKE_RADIUS_HYDROGEN);
                evt.tick = state.currentTick;
                state.pendingEvents.push_back(evt);
                missile.intercepted = true; // oznacz do usunięcia
            }
            else {
                float d = std::sqrt(dSq);
                missile.currentX += (dx / d) * MISSILE_SPEED;
                missile.currentY += (dy / d) * MISSILE_SPEED;
            }
        }

        std::erase_if(state.missiles, [](const MissileFlight& m) {
            return m.intercepted;
            });
    }

    void NukeSystem::tryIntercept(GameState& state, MissileFlight& missile) {
        // ZMIANA: Zastępujemy `static mt19937` na deterministyczny, lokalny kod LCG!
        uint64_t prngState = state.mapSeed ^ (static_cast<uint64_t>(state.currentTick) * 2654435761ULL) ^ missile.id;

        auto rollFloat = [&prngState]() -> float {
            prngState = prngState * 6364136223846793005ULL + 1442695040888963407ULL;
            uint32_t val = static_cast<uint32_t>(prngState >> 32);
            return static_cast<float>(val) / static_cast<float>(0xFFFFFFFF);
            };

        for (auto& b : state.buildings) {
            if (b.type != BuildingType::AntiMissile) continue;
            if (!b.isActive) continue;
            if (missile.triedInterceptors.count(b.id)) continue;

            int   lvl = std::min((int)b.level, 3);
            float rangeSq = ANTI_MISSILE_RANGE[lvl] * ANTI_MISSILE_RANGE[lvl];
            float chance = ANTI_MISSILE_CHANCE[lvl];

            float bx = static_cast<float>(b.topLeftX);
            float by = static_cast<float>(b.topLeftY);

            float distSq = (missile.currentX - bx) * (missile.currentX - bx) +
                (missile.currentY - by) * (missile.currentY - by);

            if (distSq > rangeSq) continue;

            missile.triedInterceptors.insert(b.id);

            if (rollFloat() < chance) {
                missile.intercepted = true;
                // Zdarzenie wizualne przechwycenia
                GameEvent evt;
                evt.type = GameEventType::ExplosionVisual;
                evt.x = missile.currentX;
                evt.y = missile.currentY;
                evt.radius = 5.0f;
                evt.tick = state.currentTick;
                state.pendingEvents.push_back(evt);
                return;
            }
        }
    }

    // ── Eksplozja ─────────────────────────────────────────────────────────────────

    void NukeSystem::detonate(GameState& state, float cx, float cy, int radius) {
        auto affected = getLandTilesInRadius(state, cx, cy, radius);

        // Kolejność: straty populacji PRZED zmianą własności
        applyPopulationDamage(state, affected, cx, cy);
        destroyBuildings(state, affected);
        applyTerrainChange(state, affected);

        // Zdarzenie wizualne
        GameEvent visual;
        visual.type = GameEventType::ExplosionVisual;
        visual.x = cx;
        visual.y = cy;
        visual.radius = static_cast<float>(radius);
        visual.tick = state.currentTick;
        state.pendingEvents.push_back(visual);
    }

    // ── Straty populacji ──────────────────────────────────────────────────────────

    void NukeSystem::applyPopulationDamage(GameState& state,
        const std::vector<uint32_t>& tileIds,
        float cx, float cy) {
        // Grupuj zniszczone kafelki per (prowincja, właściciel)
        // ZMIANA: Z unordered_map na mapę dla determinizmu pakietów przesyłanych do klientów
        std::map<uint64_t, std::vector<uint32_t>> groups;

        for (uint32_t tid : tileIds) {
            auto* tile = state.getTile(tid);
            if (!tile || tile->ownerId == 0) continue;
            uint64_t key = (static_cast<uint64_t>(tile->landProvinceId) << 32)
                | tile->ownerId;
            groups[key].push_back(tid);
        }

        for (auto& [key, tiles] : groups) {
            uint32_t provId = static_cast<uint32_t>(key >> 32);
            uint32_t eid = static_cast<uint32_t>(key & 0xFFFFFFFF);

            auto* share = state.getShare(provId, eid);
            if (!share) continue;

            // Straty z kafelków
            float popLoss = static_cast<float>(tiles.size()) * constants::BASE_PER_TILE;

            // Straty z miast w promieniu
            for (uint32_t bid : share->buildingIds) {
                auto* b = state.getBuilding(bid);
                if (!b || b->type != BuildingType::City) continue;
                float dx = b->topLeftX - cx;
                float dy = b->topLeftY - cy;

                // Unikamy sqrt, operujemy na kwadratach
                if ((dx * dx + dy * dy) <= static_cast<float>(
                    constants::NUKE_RADIUS_ATOMIC * constants::NUKE_RADIUS_ATOMIC)) {
                    int lvl = std::min((int)b->level, 5);
                    constexpr float CITY_CAP[6] = { 0, 1000, 2500, 5000, 8000, 12000 };
                    popLoss += CITY_CAP[lvl];
                }
            }

            // Normalizuj do % aktualnej populacji
            float total = share->population.workers + share->population.military;
            float maxCap = std::max(share->population.maxCap, 1.0f);
            float lossRatio = std::min(popLoss / maxCap, 1.0f);

            share->population.workers -= share->population.workers * lossRatio;
            share->population.military -= share->population.military * lossRatio;
            share->population.workers = std::max(0.0f, share->population.workers);
            share->population.military = std::max(0.0f, share->population.military);

            state.markShareDirty(provId, eid);
        }
    }

    // ── Niszczenie budynków ───────────────────────────────────────────────────────

    void NukeSystem::destroyBuildings(GameState& state,
        const std::vector<uint32_t>& tileIds) {
        for (uint32_t tid : tileIds) {
            auto* tile = state.getTile(tid);
            if (!tile || tile->buildingId == 0) continue;

            auto* b = state.getBuilding(tile->buildingId);
            if (!b) continue;

            // Usuń budynek z share (zabezpiecza przed "fantomami" pożerającymi zasoby/prąd)
            auto* share = state.getShare(b->landProvinceId, b->ownerId);
            if (share) {
                auto& bids = share->buildingIds;
                bids.erase(std::remove(bids.begin(), bids.end(), b->id), bids.end());

                // Oznacz udział jako brudny, aby klient zobaczył, że budynków ubyło
                state.markShareDirty(b->landProvinceId, b->ownerId);
            }

            b->isActive = false;
            b->isPowered = false;

            // ZMIANA: Całkowicie wymazujemy budynek poprzez wyzerowanie mu ownerId.
            // Dzięki temu przestanie generować statystyki i zapychać inne systemy logiki.
            b->ownerId = 0;

            tile->buildingId = 0;

            state.markBuildingDirty(b->id);
            state.markTileDirty(tid);
        }
    }

    // ── Zmiana terenu na fallout ──────────────────────────────────────────────────

    void NukeSystem::applyTerrainChange(GameState& state,
        const std::vector<uint32_t>& tileIds) {
        for (uint32_t tid : tileIds) {
            auto* tile = state.getTile(tid);
            if (!tile) continue;

            // Zmień terrain na fallout-wariant
            tile->terrain = toFallout(tile->terrain);

            // Usuń właściciela — kafelek staje się wildernessem
            if (tile->ownerId != 0) {
                auto* share = state.getShare(tile->landProvinceId, tile->ownerId);
                if (share) {
                    auto& ids = share->ownedTileIds;
                    ids.erase(std::remove(ids.begin(), ids.end(), tid), ids.end());
                    state.markShareDirty(tile->landProvinceId, tile->ownerId);
                }
                tile->ownerId = 0;
            }

            state.markTileDirty(tid);
        }
    }

    // ── Kafelki w promieniu ───────────────────────────────────────────────────────

    std::vector<uint32_t> NukeSystem::getLandTilesInRadius(const GameState& state,
        float cx, float cy,
        int radius) const {
        std::vector<uint32_t> result;
        float r2 = static_cast<float>(radius * radius);

        for (const auto& tile : state.tiles) {
            if (tile.landProvinceId == 0) continue; // woda
            float dx = tile.x - cx;
            float dy = tile.y - cy;
            if (dx * dx + dy * dy <= r2) result.push_back(tile.id);
        }
        return result;
    }

    // ── Inputy ────────────────────────────────────────────────────────────────────

    void NukeSystem::handleLaunchMissileInput(GameState& state,
        const PlayerInput& input) {
        auto* silo = state.getBuilding(input.unitId);
        if (!silo || silo->type != BuildingType::MissileSilo) return;
        if (silo->ownerId != input.entityId) return;
        if (!silo->isActive) return;

        auto* entity = state.getEntity(input.entityId);
        if (!entity) return;

        if (entity->uranium < constants::URANIUM_COST_HYDROGEN) return;
        if (entity->gold < 18000) return;

        entity->uranium -= constants::URANIUM_COST_HYDROGEN;
        entity->gold -= 18000;

        MissileFlight missile;
        missile.id = state.nextUnitId();
        missile.ownerId = input.entityId;
        missile.currentX = static_cast<float>(silo->topLeftX);
        missile.currentY = static_cast<float>(silo->topLeftY);
        missile.targetX = static_cast<float>(input.tileX);
        missile.targetY = static_cast<float>(input.tileY);

        state.missiles.push_back(std::move(missile));
        state.markEntityDirty(input.entityId);
    }

    void NukeSystem::handleNuclearModeInput(GameState& state,
        const PlayerInput& input) {
        auto* b = state.getBuilding(input.unitId);
        if (!b || b->type != BuildingType::NuclearPlant) return;
        if (b->ownerId != input.entityId) return;

        b->nuclearMode = static_cast<NuclearPlantMode>(
            static_cast<uint8_t>(input.floatParam));
        state.markBuildingDirty(b->id);
    }

} // namespace gs::server