#include "server/sim/systems/SupplySystem.hpp"
#include <shared/sim/Constants.hpp>

#include <algorithm>
#include <cmath>
#include <map>

namespace gs::server {

    // Produkcja zaopatrzenia per poziom fabryki (per tick)
    static constexpr float FACTORY_OUTPUT[6] = { 0,  5, 12, 25,  50, 100 };
    static constexpr float FACTORY_CAP[6] = { 0, 1000, 2000, 4000, 8000, 16000 };
    static constexpr float FACTORY_OPTIMAL[6] = { 0,  50, 120, 250, 500, 1000 };

    // Pojemność magazynów
    static constexpr float BASE_CAP[6] = { 0, 500, 1000, 2000, 4000, 8000 };
    static constexpr float MILBASE_CAP[6] = { 0, 5000, 12000, 25000, 0, 0 };
    static constexpr float LOGHUB_CAP[6] = { 0, 2000, 5000, 10000, 0, 0 };

    void SupplySystem::tick(GameState& state) {
        // ZMIANA: Snapshot stanu początkowego magazynów.
        // Używamy std::map, by zachować 100% determinizmu w iteracji przy porównaniach.
        std::map<uint64_t, float> oldSupply;
        for (const auto& prov : state.landProvinces) {
            for (const auto& [eid, share] : prov.playerShares) {
                uint64_t key = (static_cast<uint64_t>(prov.id) << 32) | eid;
                oldSupply[key] = share.supplyStored;
            }
        }

        // Wykonujemy całą ciągłą symulację logistyki po cichu
        produceSupply(state);
        consumeCombatSupply(state);
        distributeSupply(state);

        // ZMIANA: Na koniec porównujemy i brudzimy TYLKO te udziały, 
        // gdzie zaszła zauważalna zmiana. Setki malutkich transferów między 
        // sąsiadami znikną z transferu sieciowego!
        for (const auto& prov : state.landProvinces) {
            for (const auto& [eid, share] : prov.playerShares) {
                uint64_t key = (static_cast<uint64_t>(prov.id) << 32) | eid;
                float old = oldSupply[key];

                // Próg 1.0f wystarczy, by klient dostawał gładkie update'y,
                // ale nie był spamowany zmianami rzędu 0.001 z powodu podziału robotników.
                if (std::abs(share.supplyStored - old) > 1.0f) {
                    state.markShareDirty(prov.id, eid);
                }
            }
        }
    }

    // ── Produkcja ─────────────────────────────────────────────────────────────────

    void SupplySystem::produceSupply(GameState& state) {
        for (auto& b : state.buildings) {
            if (b.type != BuildingType::Factory) continue;
            if (!b.isActive) continue;

            auto* share = state.getShare(b.landProvinceId, b.ownerId);
            if (!share) continue;

            float workers = getWorkersAssigned(state, b);
            int   lvlIdx = std::min((int)b.level, 5);

            float eff = workerEfficiency(workers, lvlIdx);
            float output = FACTORY_OUTPUT[lvlIdx] * eff;

            if (output > 0.0f) {
                // ZMIANA: Od razu ładujemy produkcję do wspólnego magazynu prowincji
                // i zderzamy to z limitem maksymalnej pojemności.
                float maxCap = computeTotalCapacity(state, *share);
                share->supplyStored = std::min(share->supplyStored + output, maxCap);

                // Usunięto b.supplyStored i markBuildingDirty - budynek nie musi nic wiedzieć.
            }
        }
    }

    // ── Konsumpcja przez ataki ────────────────────────────────────────────────────

    void SupplySystem::consumeCombatSupply(GameState& state) {
        for (auto& attack : state.attacks) {
            if (attack.status != AttackStatus::Active) continue;

            float consume = attack.troops * constants::COMBAT_SUPPLY_RATE;
            if (attack.supply >= consume) {
                attack.supply -= consume;
            }
            else {
                // Plecak pusty — próbuj pobrać lokalnie z prowincji frontowej
                float deficit = consume - attack.supply;
                attack.supply = 0.0f;
                tryPullLocalSupply(state, attack, deficit);
            }
        }
    }

    void SupplySystem::tryPullLocalSupply(GameState& state, Attack& attack,
        float needed) {
        auto* share = state.getShare(attack.sourceProvinceId, attack.attackerId);
        if (!share) return;

        float available = std::min(share->supplyStored, needed);
        if (available > 0.0f) {
            share->supplyStored -= available;
            attack.supply += available;
            // Usunięto markShareDirty (obsługiwane globalnie na końcu ticku)
        }
    }

    // ── Dystrybucja pull-based ────────────────────────────────────────────────────

    void SupplySystem::distributeSupply(GameState& state) {
        for (auto& prov : state.landProvinces) {
            for (auto& [eid, share] : prov.playerShares) {
                float capacity = computeTotalCapacity(state, share);
                if (capacity <= 0.0f) continue;

                float threshold = capacity * 0.20f; // próg 20%
                if (share.supplyStored >= threshold) continue;

                float needed = capacity * 0.80f - share.supplyStored;
                if (needed <= 0.0f) continue;

                // Szukaj źródeł w sąsiednich prowincjach tego samego gracza
                for (const auto& adj : prov.neighbors) {
                    if (needed <= 0.0f) break;

                    auto* srcShare = state.getShare(adj.neighborProvinceId, eid);
                    if (!srcShare) continue;

                    float srcCapacity = computeTotalCapacity(state, *srcShare);
                    float available = std::max(0.0f, srcShare->supplyStored - srcCapacity * 0.30f);
                    if (available <= 0.0f) continue;

                    float transfer = std::min(available, needed);
                    srcShare->supplyStored -= transfer;
                    share.supplyStored += transfer;
                    needed -= transfer;

                    // Usunięto markShareDirty (obsługiwane globalnie na końcu ticku)
                }
            }
        }
    }

    // ── Pomocnicze ────────────────────────────────────────────────────────────────

    float SupplySystem::workerEfficiency(float workers, int level) const {
        if (level < 1 || level > 5) return 0.0f;
        float required = FACTORY_OPTIMAL[level] * 0.2f;
        float optimal = FACTORY_OPTIMAL[level];
        float maxUseful = optimal * 2.0f;

        if (workers < required)    return workers / required * 0.5f;
        if (workers <= optimal)    return 0.5f + 0.5f * (workers - required) / (optimal - required);
        if (workers <= maxUseful)  return 1.0f + 0.2f * (workers - optimal) / (maxUseful - optimal);
        return 1.2f;
    }

    float SupplySystem::getWorkersAssigned(const GameState& state,
        const Building& b) const {
        auto* share = const_cast<GameState&>(state).getShare(b.landProvinceId, b.ownerId);
        if (!share) return 0.0f;

        size_t prodBuildings = 0;
        for (uint32_t bid : share->buildingIds) {
            auto* ob = const_cast<GameState&>(state).getBuilding(bid);
            if (ob && ob->isActive &&
                (ob->type == BuildingType::Factory ||
                    ob->type == BuildingType::CoalPlant ||
                    ob->type == BuildingType::NuclearPlant)) {
                ++prodBuildings;
            }
        }
        if (prodBuildings == 0) return 0.0f;
        return share->population.workers / static_cast<float>(prodBuildings);
    }

    float SupplySystem::computeTotalCapacity(const GameState& state,
        const PlayerProvinceShare& share) const {
        float cap = 0.0f;
        for (uint32_t bid : share.buildingIds) {
            auto* b = const_cast<GameState&>(state).getBuilding(bid);
            if (!b || !b->isActive) continue;
            int lvl = std::min((int)b->level, 5);
            switch (b->type) {
            case BuildingType::City:         cap += BASE_CAP[lvl];    break;
            case BuildingType::Factory:      cap += FACTORY_CAP[lvl]; break;
            case BuildingType::MilitaryBase: cap += MILBASE_CAP[std::min(lvl, 3)]; break;
            case BuildingType::LogisticsHub: cap += LOGHUB_CAP[std::min(lvl, 3)]; break;
            default: break;
            }
        }
        return cap > 0.0f ? cap : 500.0f; // minimum
    }

} // namespace gs::server