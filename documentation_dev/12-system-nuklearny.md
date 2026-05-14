# 12 — System nuklearny (NukeSystem)

## Odpowiedzialność

Przetwarza eksplozje bomb atomowych i rakiet wodorowych, aplikuje fallout, natychmiastowe straty populacji, zniszczenie budynków. Obsługuje też obronę przeciwrakietową.

## tick()

```cpp
void NukeSystem::tick(GameState& state) {
    // 1. Ruch rakiet balistycznych
    tickMissiles(state);

    // 2. Przetwórz zdarzenia eksplozji (od bombowców + rakiet które dotarły)
    for (auto& event : state.pendingEvents) {
        if (event.type == GameEventType::AtomicBombDetonation) {
            detonate(state, event.x, event.y, constants::NUKE_RADIUS_ATOMIC);
        } else if (event.type == GameEventType::HydrogenDetonation) {
            detonate(state, event.x, event.y, constants::NUKE_RADIUS_HYDROGEN);
        }
    }
    // Usuń przetworzone zdarzenia nuklearne z listy
    std::erase_if(state.pendingEvents, isNukeEvent);
}
```

## Obrona przeciwrakietowa

```cpp
void NukeSystem::tickMissiles(GameState& state) {
    for (auto& missile : state.missiles) {
        // Sprawdź interceptory na trajektorii
        if (!missile.intercepted) {
            tryIntercept(state, missile);
        }
        if (missile.intercepted) continue;

        // Przesuń rakietę
        float dist = distance(missile.currentX, missile.currentY,
                              missile.targetX,  missile.targetY);
        if (dist < MISSILE_SPEED) {
            // Dotarła — eksplozja
            state.pendingEvents.push_back(
                makeHydrogenEvent(missile.targetX, missile.targetY, missile.ownerId)
            );
            missile.health = 0;
        } else {
            moveStraight(missile, MISSILE_SPEED);
        }
    }
    std::erase_if(state.missiles, [](const MissileFlight& m) {
        return m.health <= 0;
    });
}

void NukeSystem::tryIntercept(GameState& state, MissileFlight& missile) {
    for (auto& b : state.buildings) {
        if (b.type != BuildingType::AntiMissile || !b.isActive) continue;
        if (missile.interceptedBy.count(b.id)) continue;  // już próbował

        if (isOnTrajectory(missile, b, ANTI_MISSILE_RANGE[b.level])) {
            missile.interceptedBy.insert(b.id);
            if (randomChance(ANTI_MISSILE_CHANCE[b.level])) {
                missile.intercepted = true;
                return;
            }
        }
    }
}
```

## Eksplozja — pełny flow

```cpp
void NukeSystem::detonate(GameState& state, uint16_t cx, uint16_t cy, int radius) {
    auto affectedTiles = getLandTilesInRadius(state, cx, cy, radius);

    // 1. Natychmiastowe straty populacji (zanim kafelki zmienią właściciela)
    applyPopulationDamage(state, affectedTiles, cx, cy, radius);

    // 2. Zniszczenie budynków
    for (uint32_t tileId : affectedTiles) {
        if (state.tiles[tileId].buildingId != 0) {
            destroyBuilding(state, state.tiles[tileId].buildingId);
        }
    }

    // 3. Kafelki → wilderness + fallout terrain
    for (uint32_t tileId : affectedTiles) {
        auto& tile  = state.tiles[tileId];
        tile.terrain = toFalloutTerrain(tile.terrain);
        tile.isFallout = true;
        tile.ownerId   = 0;
        // Usuń z PlayerProvinceShare poprzedniego właściciela
        removeFromShare(state, tileId);
        state.dirtyTiles.insert(tileId);
    }

    // 4. Rebuild grafów
    state.electricSystem->rebuildGrids(state);
    state.logisticsSystem->rebuildRoutes(state);

    // 5. Zdarzenie widoczne dla graczy
    state.pendingEvents.push_back(makeExplosionVisualEvent(cx, cy, radius));
}
```

## Straty populacji

```cpp
void NukeSystem::applyPopulationDamage(GameState& state,
                                        const std::vector<uint32_t>& affectedTiles,
                                        uint16_t cx, uint16_t cy, int radius) {
    // Grupuj zniszczone kafelki per prowincja per właściciel
    std::map<std::pair<uint32_t,uint32_t>, std::vector<uint32_t>> byOwner;
    for (uint32_t tid : affectedTiles) {
        auto& t = state.tiles[tid];
        if (t.ownerId == 0) continue;
        byOwner[{t.landProvinceId, t.ownerId}].push_back(tid);
    }

    for (auto& [key, tiles] : byOwner) {
        auto [provId, entityId] = key;
        auto* share = state.getShare(provId, entityId);
        if (!share) continue;

        // Straty z kafelków
        float popLoss = static_cast<float>(tiles.size()) * constants::BASE_PER_TILE;

        // Straty z miast w promieniu
        for (uint32_t bid : share->buildingIds) {
            auto& b = state.buildings[bid];
            if (b.type == BuildingType::City && isInRadius(b, cx, cy, radius)) {
                popLoss += CITY_CAP_BONUS[b.level];
            }
        }

        // Normalizuj do procentu populacji
        float total    = share->population.workers + share->population.military;
        float maxCap   = share->population.maxCap;
        float lossRatio = std::min(popLoss / std::max(maxCap, 1.0f), 1.0f);

        share->population.workers  -= share->population.workers  * lossRatio;
        share->population.military -= share->population.military * lossRatio;
        state.dirtyShares.insert(makeShareKey(provId, entityId));
    }
}
```

## Zależności

- `GameState` — tiles, buildings, playerShares, missiles, pendingEvents
- `ElectricSystem` — rebuild po zniszczeniu węzłów sieci
- `AirSystem` — generuje zdarzenia AtomicBombDetonation
