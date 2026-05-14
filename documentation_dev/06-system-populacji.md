# 06 — System populacji (PopulationSystem)

## Odpowiedzialność

Oblicza przyrost populacji per gracz per prowincja, aktualizuje `maxCap`, zarządza `workerSplitRatio` w trybie FullAuto.

## tick()

```cpp
void PopulationSystem::tick(GameState& state) {
    for (auto& prov : state.landProvinces) {
        for (auto& [entityId, share] : prov.playerShares) {
            // 1. Aktualizuj maxCap
            share.population.maxCap = share.computeMaxCap(state);

            // 2. Tryb FullAuto — dostosuj workerSplitRatio
            if (share.automationLevel == AutomationLevel::FullAuto) {
                adjustWorkerSplit(state, share, prov);
            }

            // 3. Przyrost
            float current = share.population.workers + share.population.military;
            float cap     = share.population.maxCap;
            if (cap <= 0.0f) continue;

            float toAdd = growthFormula(current, cap);

            share.population.workers  += toAdd * share.workerSplitRatio;
            share.population.military += toAdd * (1.0f - share.workerSplitRatio);

            // 4. Koszt nadmiaru populacji
            if (current > cap) {
                float excess = current - cap;
                share.supplyStored -= excess * 0.01f;
            }

            state.dirtyShares.insert(makeShareKey(prov.id, entityId));
        }
    }
}
```

## Wzór przyrostu (z openfront.io)

```cpp
float PopulationSystem::growthFormula(float current, float cap) {
    if (cap <= 0.0f) return 0.0f;
    return (10.0f + std::pow(current, 0.73f) / 4.0f)
           * (1.0f - current / cap);
}
```

## Obliczanie maxCap

```cpp
float PlayerProvinceShare::computeMaxCap(const GameState& state) const {
    // Baza: kafelki × BASE_PER_TILE
    float base = static_cast<float>(ownedTileIds.size()) * constants::BASE_PER_TILE;

    // Budynki — miasta dodają bonus
    float cityBonus = 0.0f;
    bool  isCapital = false;
    for (uint32_t bid : buildingIds) {
        auto& b = state.buildings[bid];
        if (!b.isActive) continue;
        if (b.type == BuildingType::City) {
            float bonus = CITY_CAP_BONUS[b.level];
            if (b.isCapital) {
                bonus *= 1.25f;
                isCapital = true;
            }
            cityBonus += bonus;
        }
    }

    float total = base + cityBonus;

    // Bonus dominacji (>50% kafelków prowincji)
    auto& prov = state.landProvinces[provinceId];
    if (hasDominance(prov)) {
        total *= 1.10f;
    }

    return total;
}
```

## Zależności

- `GameState` — playerShares, buildings
- `SupplySystem` — koszt nadmiaru populacji pobierany z supplyStored
