# 08 — System elektryczny (ElectricSystem)

## Odpowiedzialność

Zarządza grafem sieci elektrycznej każdego gracza. Co tick bilansuje produkcję i zapotrzebowanie, włącza/wyłącza budynki według priorytetów.

## Graf sieci

Sieć elektryczna to **graf nieskierowany** gdzie węzłami są budynki, a krawędziami kable.

```cpp
struct ElectricGrid {
    uint32_t                id;
    uint32_t                ownerId;
    std::vector<uint32_t>   buildingIds;   // wszystkie budynki w sieci
    std::vector<ElectricEdge> edges;       // kable między budynkami

    float totalProduction{0.0f};
    float totalDemand{0.0f};
};

struct ElectricEdge {
    uint32_t buildingA;
    uint32_t buildingB;
    float    lengthTiles;   // <= MAX_CABLE_DISTANCE (50)
};
```

## tick()

```cpp
void ElectricSystem::tick(GameState& state) {
    for (auto& grid : state.electricGrids) {
        // 1. Policz produkcję
        float production = 0.0f;
        for (uint32_t bid : grid.buildingIds) {
            auto& b = state.buildings[bid];
            if (isProducer(b) && b.constructionProgress >= 1.0f) {
                production += computeOutput(state, b);
            }
        }

        // 2. Policz zapotrzebowanie
        float demand = 0.0f;
        for (uint32_t bid : grid.buildingIds) {
            if (!isProducer(state.buildings[bid])) {
                demand += BUILDING_POWER_DEMAND[state.buildings[bid].type]
                                              [state.buildings[bid].level];
            }
        }

        // 3. Bilansowanie
        if (production >= demand) {
            // Wszystko działa
            for (uint32_t bid : grid.buildingIds) {
                state.buildings[bid].isPowered = true;
            }
        } else {
            // Wyłącz budynki o najniższym priorytecie
            balanceGrid(state, grid, production);
        }

        // 4. Aktualizuj isActive
        for (uint32_t bid : grid.buildingIds) {
            auto& b = state.buildings[bid];
            b.isActive = b.isPowered
                      && b.constructionProgress >= 1.0f
                      && !b.isDamaged;
        }
    }
}
```

## Balansowanie przy deficycie

```cpp
void ElectricSystem::balanceGrid(GameState& state, ElectricGrid& grid,
                                  float availablePower) {
    // Posortuj konsumentów według priorytetu (malejąco)
    auto consumers = getConsumers(state, grid);
    std::sort(consumers.begin(), consumers.end(),
        [&](uint32_t a, uint32_t b) {
            return state.buildings[a].electricPriority
                 > state.buildings[b].electricPriority;
        });

    float remaining = availablePower;
    for (uint32_t bid : consumers) {
        auto& b = state.buildings[bid];
        float demand = BUILDING_POWER_DEMAND[b.type][b.level];
        if (remaining >= demand) {
            b.isPowered = true;
            remaining  -= demand;
        } else {
            b.isPowered = false;
        }
    }
}
```

## Rebuild grafu

Wywoływane gdy zniszczony zostaje budynek-węzeł (np. przez bombę):

```cpp
void ElectricSystem::rebuildGrids(GameState& state) {
    state.electricGrids.clear();
    // BFS/DFS po kablach — tworzy spójne komponenty (osobne sieci)
    std::unordered_set<uint32_t> visited;
    for (auto& b : state.buildings) {
        if (visited.count(b.id)) continue;
        ElectricGrid grid;
        grid.ownerId = b.ownerId;
        bfsGrid(state, b.id, grid, visited);
        state.electricGrids.push_back(std::move(grid));
    }
}
```

## Zależności

- `GameState` — buildings, electricGrids
- `NukeSystem` — wywołuje `rebuildGrids()` po eksplozji
