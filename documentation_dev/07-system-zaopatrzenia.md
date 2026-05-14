# 07 — System zaopatrzenia (SupplySystem)

## Odpowiedzialność

Produkuje zaopatrzenie w fabrykach, dystrybuuje je pull-based między prowincjami, zarządza "plecakami" atakujących.

## tick()

```cpp
void SupplySystem::tick(GameState& state) {
    // 1. Produkcja — fabryki generują zaopatrzenie
    for (auto& b : state.buildings) {
        if (b.type != BuildingType::Factory || !b.isActive) continue;
        auto* share = state.getShare(b.landProvinceId, b.ownerId);
        if (!share) continue;
        float workers = getWorkersAssigned(state, b);
        float output  = FACTORY_OUTPUT[b.level] * workerEfficiency(workers, b.level);
        float cap     = FACTORY_SUPPLY_CAP[b.level];
        share->supplyStored = std::min(share->supplyStored + output, cap);
    }

    // 2. Konsumpcja — aktywne ataki zużywają z plecaka
    for (auto& attack : state.attacks) {
        if (attack.status != AttackStatus::Active) continue;
        float consume = attack.troops * constants::COMBAT_SUPPLY_RATE;
        attack.supply = std::max(0.0f, attack.supply - consume);
        // Jeśli plecak pusty — próbuj dobrać lokalnie
        if (attack.supply <= 0.0f) {
            tryPullLocalSupply(state, attack);
        }
    }

    // 3. Pull-based dystrybucja między prowincjami
    distributeSupply(state);
}
```

## Pull-based dystrybucja

```cpp
void SupplySystem::distributeSupply(GameState& state) {
    for (auto& prov : state.landProvinces) {
        for (auto& [entityId, share] : prov.playerShares) {
            float capacity = computeTotalCapacity(state, share);
            if (share.supplyStored >= capacity * 0.2f) continue;  // nie potrzeba

            float needed = capacity * 0.8f - share.supplyStored;
            auto sources = findSupplySources(state, share, entityId);

            for (auto& src : sources) {
                float transfer = std::min(src.available, needed);
                if (transfer <= 0.0f) break;
                src.share->supplyStored -= transfer;
                share.supplyStored      += transfer;
                needed                  -= transfer;
            }
        }
    }
}
```

## Zależności

- `GameState` — buildings, attacks, playerShares
- `CombatSystem` — supplyEfficiencyMod używany w wzorach walki
