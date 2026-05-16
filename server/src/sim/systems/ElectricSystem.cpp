#include "server/sim/systems/ElectricSystem.hpp"

#include <algorithm>
#include <queue>
#include <set> // ZMIANA: używamy std::set do bezpieczeństwa BFS

namespace gs::server {

    // Produkcja per poziom (MW)
    static constexpr float COAL_OUTPUT[4] = { 0, 100, 250, 500 };
    static constexpr float NUCLEAR_OUTPUT[4] = { 0, 500, 1200, 2500 };

    // Zużycie per typ i poziom (MW)
    static constexpr float POWER_DEMAND[13][6] = {
        //  0    1    2    3    4    5
            {0,   5,  10,  18,  30,  50},   // City
            {0,   8,  18,  35,  60, 100},   // Factory
            {0,   0,   0,   0,   0,   0},   // CoalPlant (producer)
            {0,   0,   0,   0,   0,   0},   // NuclearPlant (producer)
            {0,  10,  20,  40,   0,   0},   // MilitaryBase
            {0,   0,   0,   0,   0,   0},   // DefensePost
            {0,  15,  30,  60,   0,   0},   // AntiMissile
            {0,   8,  18,  35,   0,   0},   // AntiAir
            {0,  25,   0,   0,   0,   0},   // MissileSilo
            {0,  12,  25,  50,   0,   0},   // Port
            {0,  15,  30,  60,   0,   0},   // Airport
            {0,   0,   0,   0,   0,   0},   // Road (no power)
            {0,   0,   0,   0,   0,   0},   // LogisticsHub (no power)
    };

    // Priorytety wyłączania (wyższy = ważniejszy)
    static constexpr int ELECTRIC_PRIORITY[13] = {
        100, // City
         55, // Factory
          0, // CoalPlant
          0, // NuclearPlant
         70, // MilitaryBase
          0, // DefensePost
         90, // AntiMissile
         95, // AntiAir
         80, // MissileSilo
         60, // Port
         65, // Airport
          0, // Road
          0, // LogisticsHub
    };

    void ElectricSystem::tick(GameState& state) {
        for (auto& input : state.pendingInputs) {
            if (input.type == InputType::Build)
                handleCableInput(state, input);
        }

        for (auto& grid : state.electricGrids) {
            balanceGrid(state, grid);
        }

        // Budynki bez sieci — wyłączone (poza autonomicznymi)
        for (auto& b : state.buildings) {
            if (b.electricGridId == 0) {
                bool selfPowered = (b.type == BuildingType::CoalPlant
                    || b.type == BuildingType::NuclearPlant
                    || b.type == BuildingType::Road
                    || b.type == BuildingType::LogisticsHub
                    || b.type == BuildingType::DefensePost);

                bool newPowered = selfPowered;
                bool newActive = selfPowered && b.constructionProgress >= 1.0f;

                // ZMIANA: Zapisujemy brudny stan TYLKO gdy zmieniło się zasilanie!
                if (b.isPowered != newPowered || b.isActive != newActive) {
                    b.isPowered = newPowered;
                    b.isActive = newActive;
                    state.markBuildingDirty(b.id);
                }
            }
        }

        // Produkcja uranu
        tickUraniumProduction(state);
    }

    // ── Balansowanie sieci ────────────────────────────────────────────────────────

    void ElectricSystem::balanceGrid(GameState& state, ElectricGrid& grid) {
        grid.totalProductionMW = 0.0f;
        grid.totalDemandMW = 0.0f;

        // Policz produkcję
        for (uint32_t bid : grid.buildingIds) {
            auto* b = state.getBuilding(bid);
            if (!b || b->constructionProgress < 1.0f) continue;
            if (isProducer(*b)) {
                grid.totalProductionMW += computeOutput(state, *b);
            }
        }

        // Zbierz konsumentów
        std::vector<Building*> consumers;
        for (uint32_t bid : grid.buildingIds) {
            auto* b = state.getBuilding(bid);
            if (!b || b->constructionProgress < 1.0f) continue;
            if (!isProducer(*b) && powerDemand(*b) > 0.0f)
                consumers.push_back(b);
        }

        // ZMIANA: Sortowanie GWARANTUJĄCE determinizm!
        std::sort(consumers.begin(), consumers.end(),
            [this](const Building* a, const Building* b) {
                int pa = getPriority(a->type);
                int pb = getPriority(b->type);
                if (pa != pb) return pa > pb;

                // TIE-BREAKER: Jeśli priorytet jest równy, zawsze pierwszy 
                // włącza się budynek zbudowany wcześniej (mniejsze ID)
                return a->id < b->id;
            });

        // Przydzielaj moc
        float remaining = grid.totalProductionMW;
        for (auto* b : consumers) {
            float demand = powerDemand(*b);
            grid.totalDemandMW += demand;

            bool getsPower = (remaining >= demand);
            if (getsPower) remaining -= demand;

            bool newActive = getsPower && b->constructionProgress >= 1.0f;

            // ZMIANA: Wysyłaj w sieć TYLKO, jeśli na obiekcie faktycznie mignął prąd
            if (b->isPowered != getsPower || b->isActive != newActive) {
                b->isPowered = getsPower;
                b->isActive = newActive;
                state.markBuildingDirty(b->id);
            }
        }

        // Elektrownie zawsze active
        for (uint32_t bid : grid.buildingIds) {
            auto* b = state.getBuilding(bid);
            if (!b) continue;
            if (isProducer(*b) && b->constructionProgress >= 1.0f) {
                if (!b->isPowered || !b->isActive) {
                    b->isPowered = true;
                    b->isActive = true;
                    state.markBuildingDirty(b->id);
                }
            }
        }
    }

    // ── Rebuild grafu sieci ───────────────────────────────────────────────────────

    void ElectricSystem::rebuildGrids(GameState& state) {
        state.electricGrids.clear();
        for (auto& b : state.buildings) {
            b.electricGridId = 0;
        }

        std::vector<ElectricEdge> allEdges;
        uint32_t nextGridId = 1;

        // ZMIANA: Przechodzimy na std::set, aby wykluczyć hashowanie przy BFS.
        std::set<uint32_t> visited;

        for (auto& b : state.buildings) {
            if (visited.count(b.id)) continue;

            ElectricGrid grid;
            grid.id = nextGridId++;
            grid.ownerId = b.ownerId;

            std::queue<uint32_t> q;
            q.push(b.id);
            visited.insert(b.id);

            while (!q.empty()) {
                uint32_t curr = q.front(); q.pop();
                grid.buildingIds.push_back(curr);

                auto* cb = state.getBuilding(curr);
                if (cb) cb->electricGridId = grid.id;

                for (const auto& edge : allEdges) {
                    uint32_t neighbor = 0;
                    if (edge.buildingA == curr) neighbor = edge.buildingB;
                    if (edge.buildingB == curr) neighbor = edge.buildingA;

                    if (neighbor && !visited.count(neighbor)) {
                        visited.insert(neighbor);
                        q.push(neighbor);
                        grid.edges.push_back(edge);
                    }
                }
            }

            state.electricGrids.push_back(std::move(grid));
        }
    }

    // ── Produkcja uranu ───────────────────────────────────────────────────────────

    void ElectricSystem::tickUraniumProduction(GameState& state) {
        static constexpr float URANIUM_BASE[4] = { 0, 0.01f, 0.025f, 0.05f };
        static constexpr float MODE_MULT[3] = { 0.0f, 0.5f, 1.0f };

        for (auto& b : state.buildings) {
            if (b.type != BuildingType::NuclearPlant) continue;
            if (!b.isActive) continue;

            int lvl = std::min((int)b.level, 3);
            int mode = static_cast<int>(b.nuclearMode);

            float uraniumPerTick = URANIUM_BASE[lvl] * MODE_MULT[mode];
            if (uraniumPerTick <= 0.0f) continue;

            auto* entity = state.getEntity(b.ownerId);
            if (!entity) continue;

            entity->uranium += uraniumPerTick;

            // ZMIANA: Przestajemy spamować sieć dodawaniem 0.01 uranu per tick!
            // Podobnie jak ze złotem z dyplomacji, przychód pasywny klient wylicza sam.
            // Oznaczaj uran jako dirty TYLKO, gdy gracz użyje go do ataku (CombatSystem).
        }
    }

    // ── Helpers ───────────────────────────────────────────────────────────────────

    int ElectricSystem::getPriority(BuildingType type) const {
        int typeIdx = static_cast<int>(type);
        if (typeIdx < 0 || typeIdx >= 13) return 0;
        return ELECTRIC_PRIORITY[typeIdx];
    }

    bool ElectricSystem::isProducer(const Building& b) const {
        return b.type == BuildingType::CoalPlant
            || b.type == BuildingType::NuclearPlant;
    }

    float ElectricSystem::computeOutput(const GameState& state,
        const Building& b) const {
        int lvl = std::min((int)b.level, 3);
        if (b.type == BuildingType::CoalPlant)
            return COAL_OUTPUT[lvl];

        if (b.type == BuildingType::NuclearPlant) {
            float base = NUCLEAR_OUTPUT[lvl];
            switch (b.nuclearMode) {
            case NuclearPlantMode::Civilian: return base * 1.0f;
            case NuclearPlantMode::Mixed:    return base * 0.7f;
            case NuclearPlantMode::Military: return base * 0.3f;
            }
        }
        return 0.0f;
    }

    float ElectricSystem::powerDemand(const Building& b) const {
        int typeIdx = static_cast<int>(b.type);
        int lvl = std::min((int)b.level, 5);
        if (typeIdx < 0 || typeIdx >= 13) return 0.0f;
        return POWER_DEMAND[typeIdx][lvl];
    }

    void ElectricSystem::handleCableInput(GameState& state,
        const PlayerInput& input) {
        (void)state; (void)input;
    }

} // namespace gs::server