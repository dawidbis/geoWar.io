#include "server/sim/systems/PopulationSystem.hpp"
#include <shared/sim/Constants.hpp>

#include <algorithm>
#include <cmath>

namespace gs::server {

    // Bonusy cap dla poziomów miast
    static constexpr float CITY_CAP_BONUS[6] = {
        0.0f, 1000.0f, 2500.0f, 5000.0f, 8000.0f, 12000.0f
    };

    void PopulationSystem::tick(GameState& state) {
        // Przetwórz inputy
        for (auto& input : state.pendingInputs) {
            if (input.type == InputType::SetWorkerSplit)
                handleWorkerSplitInput(state, input);
            else if (input.type == InputType::SetAutomation)
                handleAutomationInput(state, input);
        }

        // Przyrost per gracz per prowincja
        for (auto& prov : state.landProvinces) {
            for (auto& [eid, share] : prov.playerShares) {

                float oldCap = share.population.maxCap;
                float oldSplit = share.workerSplitRatio;
                float oldSupply = share.supplyStored;

                // 1. Aktualizuj maxCap
                share.population.maxCap = computeMaxCap(state, share);

                // 2. FullAuto — dostosuj workerSplitRatio
                if (share.automationLevel == AutomationLevel::FullAuto) {
                    adjustWorkerSplit(state, prov, share);
                }

                // 3. Przyrost
                float current = share.population.workers + share.population.military;
                float toAdd = growthFormula(current, share.population.maxCap);

                share.population.workers += toAdd * share.workerSplitRatio;
                share.population.military += toAdd * (1.0f - share.workerSplitRatio);

                // 4. Koszt nadmiaru populacji
                if (current > share.population.maxCap && share.population.maxCap > 0.0f) {
                    float excess = current - share.population.maxCap;
                    share.supplyStored = std::max(0.0f, share.supplyStored - excess * 0.01f);
                }

                // ZMIANA: Ograniczamy spam sieciowy!
                // Wysyłamy paczkę do klientów TYLKO jeśli zmieniły się wartości decydujące
                // o krzywej rośnięcia (cap/split) lub jeśli spaliło się zaopatrzenie.
                // Przyrost "toAdd" klient kalkuluje sam na podstawie tych samych zmiennych!
                if (std::abs(share.population.maxCap - oldCap) > 1.0f ||
                    std::abs(share.workerSplitRatio - oldSplit) > 0.01f ||
                    std::abs(share.supplyStored - oldSupply) > 1.0f) {
                    state.markShareDirty(prov.id, eid);
                }
            }
        }

        // Sprawdź utratę stolicy
        checkCapitalLoss(state);
    }

    // ── Wzór przyrostu ────────────────────────────────────────────────────────────

    float PopulationSystem::growthFormula(float current, float cap) const {
        if (cap <= 0.0f) return 0.0f;
        float ratio = current / cap;
        if (ratio >= 1.0f) return 0.0f;

        float c = std::max(0.0f, current);

        // ZMIANA: Zastępujemy niedeterministyczne std::pow(c, 0.73f) 
        // gwarantowanym matematycznie std::sqrt(c * std::sqrt(c)), co daje c^0.75.
        // Unikamy dzięki temu rozjazdu symulacji między różnymi systemami operacyjnymi!
        float powerApprox = std::sqrt(c * std::sqrt(c));

        return (10.0f + powerApprox / 4.0f) * (1.0f - ratio);
    }

    // ── Obliczanie maxCap ─────────────────────────────────────────────────────────

    float PopulationSystem::computeMaxCap(const GameState& state,
        const PlayerProvinceShare& share) const {
        // Baza z kafelków
        float cap = static_cast<float>(share.ownedTileIds.size())
            * constants::BASE_PER_TILE;

        // Bonus z miast
        float cityBonus = 0.0f;
        float capitalMult = 1.0f;
        for (uint32_t bid : share.buildingIds) {
            const auto* b = const_cast<GameState&>(state).getBuilding(bid);
            if (!b || !b->isActive) continue;
            if (b->type == BuildingType::City) {
                float bonus = CITY_CAP_BONUS[std::min((int)b->level, 5)];
                if (b->isCapital) capitalMult = 1.25f;
                cityBonus += bonus;
            }
        }
        cap += cityBonus * capitalMult;

        // Bonus dominacji
        auto* prov = const_cast<GameState&>(state).getLandProvince(share.provinceId);
        if (prov && share.hasDominance(prov->tileIds.size()))
            cap *= 1.10f;

        return cap;
    }

    // ── Automatyczne dostosowanie workerSplitRatio ────────────────────────────────

    void PopulationSystem::adjustWorkerSplit(GameState& state,
        const LandProvince& prov,
        PlayerProvinceShare& share) const {
        bool isFront = false;
        for (const auto& adj : prov.neighbors) {
            auto* neighbor = state.getLandProvince(adj.neighborProvinceId);
            if (!neighbor) continue;
            for (auto& [eid, nShare] : neighbor->playerShares) {
                if (eid != share.entityId && state.isAtWar(share.entityId, eid)) {
                    isFront = true;
                    break;
                }
            }
            if (isFront) break;
        }

        float totalWorkers = share.population.workers;
        float neededWorkers = 0.0f;
        for (uint32_t bid : share.buildingIds) {
            auto* b = state.getBuilding(bid);
            if (!b || !b->isActive) continue;
            if (b->type == BuildingType::Factory || b->type == BuildingType::CoalPlant
                || b->type == BuildingType::NuclearPlant) {
                neededWorkers += 50.0f * b->level;
            }
        }

        if (isFront) {
            share.workerSplitRatio = 0.3f;
        }
        else if (totalWorkers < neededWorkers * 0.5f) {
            share.workerSplitRatio = 0.9f;
        }
        else {
            share.workerSplitRatio = 0.6f;
        }
    }

    // ── Utrata stolicy → kapitulacja ──────────────────────────────────────────────

    void PopulationSystem::checkCapitalLoss(GameState& state) const {
        for (auto& entity : state.entities) {
            if (entity.isEliminated) continue;
            if (entity.capitalBuildingId == 0) continue;

            auto* capital = state.getBuilding(entity.capitalBuildingId);
            if (!capital) continue;

            bool capitalLost = false;
            for (const auto& tile : state.tiles) {
                if (tile.buildingId == entity.capitalBuildingId) {
                    if (tile.ownerId != entity.id) {
                        capitalLost = true;
                    }
                    break;
                }
            }

            if (capitalLost) {
                entity.isEliminated = true;
                state.markEntityDirty(entity.id);

                // Kafelki gracza stają się wildernessem
                for (auto& tile : state.tiles) {
                    if (tile.ownerId == entity.id) {
                        tile.ownerId = 0;
                        state.markTileDirty(tile.id);
                    }
                }

                // ZMIANA: Zamiast "erase" uciekającego z radaru delty serwera, 
                // zerujemy zasoby i flagujemy jako brudne. Klient odbierze zera 
                // i zorientuje się, że ten gracz zniknął z mapy.
                for (auto& prov : state.landProvinces) {
                    auto* share = prov.getShare(entity.id);
                    if (share) {
                        share->population.workers = 0.0f;
                        share->population.military = 0.0f;
                        share->supplyStored = 0.0f;
                        share->ownedTileIds.clear();
                        share->buildingIds.clear();
                        state.markShareDirty(prov.id, entity.id);
                    }
                }

                GameEvent evt;
                evt.type = GameEventType::PlayerEliminated;
                evt.entityId = entity.id;
                evt.tick = state.currentTick;
                state.pendingEvents.push_back(evt);
            }
        }
    }

    // ── Inputy ────────────────────────────────────────────────────────────────────

    void PopulationSystem::handleWorkerSplitInput(GameState& state,
        const PlayerInput& input) const {
        auto* share = state.getShare(input.provinceId, input.entityId);
        if (!share) return;
        share->workerSplitRatio = std::clamp(input.floatParam, 0.0f, 1.0f);

        // Ustawienie manualne wyłącza automatyzację
        share->automationLevel = AutomationLevel::Manual;
        state.markShareDirty(input.provinceId, input.entityId);
    }

    void PopulationSystem::handleAutomationInput(GameState& state,
        const PlayerInput& input) const {
        auto* share = state.getShare(input.provinceId, input.entityId);
        if (!share) return;
        share->automationLevel = static_cast<AutomationLevel>(
            static_cast<uint8_t>(input.floatParam));
        state.markShareDirty(input.provinceId, input.entityId);
    }

} // namespace gs::server