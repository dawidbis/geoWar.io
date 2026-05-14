# 05 — System walki (CombatSystem)

## Odpowiedzialność

`CombatSystem` przetwarza wszystkie aktywne ataki lądowe co tick. Dla każdego ataku: wybiera następny kafelek do przejęcia, oblicza straty, przesuwa front, sprawdza aneksję.

## Implementacja tick()

```cpp
void CombatSystem::tick(GameState& state) {
    for (auto& attack : state.attacks) {
        if (attack.status == AttackStatus::Retreating) {
            tickRetreat(state, attack);
        } else if (attack.status == AttackStatus::Active) {
            tickAttack(state, attack);
        }
    }
    // Usuń zakończone ataki
    std::erase_if(state.attacks, [](const Attack& a) {
        return a.status == AttackStatus::Finished;
    });
}
```

## Wybór kafelka frontu

```cpp
uint32_t CombatSystem::pickFrontTile(const GameState& state, const Attack& attack) {
    // Kandydaci: kafelki targetProvince graniczące z kafelkami atakującego
    std::vector<uint32_t> candidates;

    auto& targetProv = state.landProvinces[attack.targetProvinceId];
    for (uint32_t tileId : targetProv.tileIds) {
        auto& tile = state.tiles[tileId];
        if (tile.ownerId == attack.attackerId) continue;  // już nasze
        if (!bordersAttacker(state, tileId, attack.attackerId)) continue;
        candidates.push_back(tileId);
    }

    if (candidates.empty()) return 0;  // brak frontu

    // Wybierz kafelek z najniższym kosztem przejęcia
    // (terrain × defensePost × defender_troops_ratio)
    return *std::min_element(candidates.begin(), candidates.end(),
        [&](uint32_t a, uint32_t b) {
            return captureScore(state, attack, a) < captureScore(state, attack, b);
        });
}
```

## Wzory — strata atakującego

```cpp
float CombatSystem::computeAttackerLoss(const GameState& state,
                                         const Attack& attack,
                                         const Tile& tile) {
    if (tile.ownerId == 0) {
        // Wilderness (w tym fallout)
        return (terrainDef(tile.terrain) + falloutDef(state, tile))
               / attackerTypeBuff(state, attack.attackerId);
    }

    // Wrogi gracz
    float def        = terrainDef(tile.terrain);
    float post       = defensePostMod(state, tile);
    float botD       = botDebuff(state, tile.ownerId);
    float largA      = largeAttackerBuff(state, attack.attackerId);
    float largD      = largeDefenderDebuff(state, tile.ownerId);
    float traitor    = traitorDebuff(state, tile.ownerId);
    float ratio      = clampedTroopRatio(state, attack, tile.ownerId);
    float supplyMod  = supplyEfficiencyMod(attack);

    return 0.8f * def * post * botD * largA * largD * traitor * ratio
           * (1.0f / supplyMod);
}
```

## Modyfikatory terenu

```cpp
float CombatSystem::terrainDef(TerrainType t) {
    switch (t) {
        case TerrainType::Plains:           return 0.8f;
        case TerrainType::Highlands:        return 5.0f;
        case TerrainType::Mountains:        return 1.2f;
        case TerrainType::FalloutPlains:    return 3.0f;
        case TerrainType::FalloutHighlands: return 8.0f;
        case TerrainType::FalloutMountains: return 4.0f;
        default:                            return 1.0f;
    }
}

float CombatSystem::terrainSpeed(TerrainType t) {
    switch (t) {
        case TerrainType::Plains:           return 16.5f;
        case TerrainType::Highlands:        return 20.0f;
        case TerrainType::Mountains:        return 25.0f;
        case TerrainType::FalloutPlains:    return 35.0f;
        case TerrainType::FalloutHighlands: return 50.0f;
        case TerrainType::FalloutMountains: return 45.0f;
        default:                            return 20.0f;
    }
}
```

## Przejęcie kafelka i aneksja

```cpp
void CombatSystem::captureTile(GameState& state, uint32_t tileId,
                                uint32_t attackerId) {
    auto& tile = state.tiles[tileId];
    uint32_t prevOwner = tile.ownerId;

    tile.ownerId = attackerId;
    // Fallout znika po zajęciu
    if (tile.isFallout) {
        tile.isFallout = false;
        tile.terrain = baseTerrain(tile.terrain);
    }
    state.dirtyTiles.insert(tileId);

    // Aktualizuj PlayerProvinceShare
    if (prevOwner != 0) {
        auto* defShare = state.getShare(tile.landProvinceId, prevOwner);
        if (defShare) {
            std::erase(defShare->ownedTileIds, tileId);
        }
    }
    auto* atkShare = state.getShare(tile.landProvinceId, attackerId);
    if (atkShare) {
        atkShare->ownedTileIds.push_back(tileId);
    }

    // Sprawdź aneksję odciętych grup
    checkAnnexation(state, tile.landProvinceId, attackerId);
}
```

## Odwrót

```cpp
void CombatSystem::tickRetreat(GameState& state, Attack& attack) {
    if (state.currentTick >= attack.retreatEndTick) {
        float survivors = attack.troops * (1.0f - constants::RETREAT_TROOP_LOSS);
        returnTroopsToSource(state, attack, survivors);
        returnSupply(state, attack, attack.supply * constants::RETREAT_SUPPLY_BACK);
        attack.status = AttackStatus::Finished;
    }
}
```

## Multi-attack

`PlayerInput` z typem `MultiAttack` zawiera listę par (sourceProvId, targetProvId) + procent wojska. `CombatSystem` tworzy osobny `Attack` dla każdej pary:

```cpp
void CombatSystem::handleMultiAttack(GameState& state,
                                      const MultiAttackInput& input) {
    for (auto& [srcId, tgtId] : input.pairs) {
        float troops = getShare(state, srcId, input.entityId)->population.military
                       * input.percentMilitary;
        createAttack(state, input.entityId, srcId, tgtId, troops);
    }
}
```

## Zależności

- `GameState` — tiles, landProvinces, attacks, entities
- `SupplySystem` — supplyEfficiencyMod zależy od stanu zaopatrzenia
