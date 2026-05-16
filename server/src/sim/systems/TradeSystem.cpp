#include "server/sim/systems/TradeSystem.hpp"
#include <shared/sim/Constants.hpp>

#include <algorithm>
#include <cmath>

namespace gs::server {

    static constexpr uint32_t FACTORY_TRADE_INTERVAL[6] = { 0, 100, 80, 60, 50, 40 };
    static constexpr uint32_t PORT_TRADE_INTERVAL[4] = { 0, 200, 130, 80 };
    static constexpr uint32_t AIRPORT_TRADE_INTERVAL[4] = { 0, 300, 180, 110 };

    static constexpr float FACTORY_LEVEL_FACTOR[6] = { 0, 1.0f, 1.2f, 1.5f, 1.9f, 2.4f };
    static constexpr float PORT_LEVEL_FACTOR[4] = { 0, 1.0f, 1.3f, 1.7f };
    static constexpr float AIRPORT_LEVEL_FACTOR[4] = { 0, 1.0f, 1.3f, 1.7f };

    void TradeSystem::tick(GameState& state) {
        uint32_t t = state.currentTick;

        for (auto& b : state.buildings) {
            if (!b.isActive) continue;
            int lvl = std::min((int)b.level, 5);

            if (b.type == BuildingType::Factory) {
                int flvl = std::min(lvl, 5);
                if (t % FACTORY_TRADE_INTERVAL[flvl] == 0)
                    generateLandTrade(state, b);
            }
            else if (b.type == BuildingType::Port) {
                int plvl = std::min(lvl, 3);
                if (t % PORT_TRADE_INTERVAL[plvl] == 0)
                    generateSeaTrade(state, b);
            }
            else if (b.type == BuildingType::Airport) {
                int alvl = std::min(lvl, 3);
                if (t % AIRPORT_TRADE_INTERVAL[alvl] == 0)
                    generateAirTrade(state, b);
            }
        }

        processDeliveries(state);
    }

    // ── Handel lądowy ─────────────────────────────────────────────────────────────

    void TradeSystem::generateLandTrade(GameState& state, Building& factory) {
        uint32_t bestCityId = 0;
        float    bestScore = -1.0f;

        for (auto& b : state.buildings) {
            if (b.type != BuildingType::City) continue;
            if (!b.isActive) continue;
            if (state.isAtWar(factory.ownerId, b.ownerId)) continue;

            // ZMIANA: Szybki całkowitoliczbowy (integer) dystans Manhattan
            int distInt = std::abs(b.topLeftX - factory.topLeftX)
                + std::abs(b.topLeftY - factory.topLeftY);
            float dist = static_cast<float>(distInt);

            float factor = partnerTypeFactor(state, factory.ownerId, b.ownerId);
            float score = dist * factor;

            // ZMIANA: Tie-breaker. Jeśli remisy, mniejsze ID wygrywa zawsze. 100% determinizmu.
            if (score > bestScore || (score == bestScore && b.id < bestCityId)) {
                bestScore = score;
                bestCityId = b.id;
            }
        }

        if (bestCityId == 0) return;

        auto* dest = state.getBuilding(bestCityId);
        if (!dest) return;

        int distInt = std::abs(dest->topLeftX - factory.topLeftX)
            + std::abs(dest->topLeftY - factory.topLeftY);
        float dist = static_cast<float>(distInt);

        int   lvl = std::min((int)factory.level, 5);
        float gold = computeGold(state, factory.ownerId, dest->ownerId,
            dist, 10.0f, FACTORY_LEVEL_FACTOR[lvl]);

        distributeGold(state, factory.ownerId, dest->ownerId, gold);
    }

    // ── Handel morski ─────────────────────────────────────────────────────────────

    void TradeSystem::generateSeaTrade(GameState& state, Building& port) {
        uint32_t bestPortId = 0;
        float    bestScore = -1.0f;

        for (auto& b : state.buildings) {
            if (b.id == port.id) continue;
            if (b.type != BuildingType::Port) continue;
            if (!b.isActive) continue;
            if (state.isAtWar(port.ownerId, b.ownerId)) continue;

            int distInt = std::abs(b.topLeftX - port.topLeftX)
                + std::abs(b.topLeftY - port.topLeftY);
            float dist = static_cast<float>(distInt);

            float factor = partnerTypeFactor(state, port.ownerId, b.ownerId);
            float score = dist * factor;

            if (score > bestScore || (score == bestScore && b.id < bestPortId)) {
                bestScore = score;
                bestPortId = b.id;
            }
        }

        if (bestPortId == 0) return;

        auto* dest = state.getBuilding(bestPortId);
        if (!dest) return;

        int distInt = std::abs(dest->topLeftX - port.topLeftX)
            + std::abs(dest->topLeftY - port.topLeftY);
        float dist = static_cast<float>(distInt);

        int   lvl = std::min((int)port.level, 3);
        float gold = computeGold(state, port.ownerId, dest->ownerId,
            dist, constants::BASE_SEA_TRADE_VALUE,
            PORT_LEVEL_FACTOR[lvl]);

        SeaCargo cargo;
        cargo.id = state.nextUnitId();
        cargo.ownerId = port.ownerId;
        cargo.homePortId = port.id;
        cargo.destinationPortId = dest->id;
        cargo.x = static_cast<float>(port.topLeftX);
        cargo.y = static_cast<float>(port.topLeftY);
        cargo.value = gold;
        cargo.health = 10.0f;
        state.seaCargos.push_back(std::move(cargo));
    }

    // ── Handel lotniczy ───────────────────────────────────────────────────────────

    void TradeSystem::generateAirTrade(GameState& state, Building& airport) {
        uint32_t bestAirId = 0;
        float    bestScore = -1.0f;

        for (auto& b : state.buildings) {
            if (b.id == airport.id) continue;
            if (b.type != BuildingType::Airport) continue;
            if (!b.isActive) continue;
            if (state.isAtWar(airport.ownerId, b.ownerId)) continue;

            int distInt = std::abs(b.topLeftX - airport.topLeftX)
                + std::abs(b.topLeftY - airport.topLeftY);
            float dist = static_cast<float>(distInt);

            float factor = partnerTypeFactor(state, airport.ownerId, b.ownerId);
            float score = dist * factor;

            if (score > bestScore || (score == bestScore && b.id < bestAirId)) {
                bestScore = score;
                bestAirId = b.id;
            }
        }

        if (bestAirId == 0) return;

        auto* dest = state.getBuilding(bestAirId);
        if (!dest) return;

        int distInt = std::abs(dest->topLeftX - airport.topLeftX)
            + std::abs(dest->topLeftY - airport.topLeftY);
        float dist = static_cast<float>(distInt);

        int   lvl = std::min((int)airport.level, 3);
        float gold = computeGold(state, airport.ownerId, dest->ownerId,
            dist, constants::BASE_AIR_TRADE_VALUE,
            AIRPORT_LEVEL_FACTOR[lvl]);

        AirCargo cargo;
        cargo.id = state.nextUnitId();
        cargo.ownerId = airport.ownerId;
        cargo.homeAirportId = airport.id;
        cargo.destinationAirportId = dest->id;
        cargo.x = static_cast<float>(airport.topLeftX);
        cargo.y = static_cast<float>(airport.topLeftY);
        cargo.value = gold;
        cargo.health = 30.0f;
        state.airCargos.push_back(std::move(cargo));
    }

    // ── Dostarczenia (cargo które dotarło) ───────────────────────────────────────

    void TradeSystem::processDeliveries(GameState& state) {
        for (auto& cargo : state.seaCargos) {
            if (cargo.health <= 0.0f) continue;
            auto* dest = state.getBuilding(cargo.destinationPortId);
            if (!dest) continue;

            float dx = cargo.x - dest->topLeftX;
            float dy = cargo.y - dest->topLeftY;

            // ZMIANA: Porównujemy kwadraty, żeby zrzucić z procesora kosztowne sqrt()
            if ((dx * dx + dy * dy) < 4.0f) {
                distributeGold(state, cargo.ownerId, dest->ownerId, cargo.value);
                cargo.health = 0.0f;
            }
        }
        std::erase_if(state.seaCargos, [](const SeaCargo& c) { return c.health <= 0.0f; });

        for (auto& cargo : state.airCargos) {
            if (cargo.health <= 0.0f) continue;
            auto* dest = state.getBuilding(cargo.destinationAirportId);
            if (!dest) continue;

            float dx = cargo.x - dest->topLeftX;
            float dy = cargo.y - dest->topLeftY;

            if ((dx * dx + dy * dy) < 4.0f) {
                distributeGold(state, cargo.ownerId, dest->ownerId, cargo.value);
                cargo.health = 0.0f;
            }
        }
        std::erase_if(state.airCargos, [](const AirCargo& c) { return c.health <= 0.0f; });
    }

    // ── Helpers ───────────────────────────────────────────────────────────────────

    float TradeSystem::computeGold(const GameState& state,
        uint32_t srcEntity, uint32_t dstEntity,
        float distance, float baseValue,
        float levelFactor) const {
        float distFactor = 1.0f + distance / 1000.0f;
        float partnerFactor = partnerTypeFactor(state, srcEntity, dstEntity);
        return baseValue * distFactor * partnerFactor * levelFactor;
    }

    float TradeSystem::partnerTypeFactor(const GameState& state,
        uint32_t a, uint32_t b) const {
        if (a == b)                                        return 1.0f;
        if (state.inSameEconomicPact(a, b))               return 2.5f;
        const auto* rel = const_cast<GameState&>(state).getRelation(a, b);
        if (rel && rel->hasFlag(RelationFlag::BilateralTrade)) return 2.0f;
        if (state.isAtWar(a, b))                           return 0.0f;
        return 1.5f;
    }

    void TradeSystem::distributeGold(GameState& state,
        uint32_t srcEntity, uint32_t dstEntity,
        float gold) const {
        // Cło
        float tariff = 0.0f;
        if (srcEntity != dstEntity) {
            if (state.inSameEconomicPact(srcEntity, dstEntity))      tariff = 0.0f;
            else {
                auto* rel = state.getRelation(srcEntity, dstEntity);
                if (rel && rel->hasFlag(RelationFlag::BilateralTrade)) tariff = 0.05f;
                else                                                   tariff = 0.20f;
            }
        }

        int64_t goldInt = static_cast<int64_t>(gold);

        auto* src = state.getEntity(srcEntity);
        auto* dst = state.getEntity(dstEntity);

        // ZMIANA: Usuwamy state.markEntityDirty!
        // Symulacja działa po obu stronach (Lock-Step / State Sync).
        // Spamowanie sieci paczkami ze wszystkimi atrybutami gracza co ułamek 
        // sekundy tylko z powodu dostarczonego złota całkowicie zadusiłoby łącze.
        if (src) {
            src->gold += static_cast<int64_t>(goldInt * (1.0f - tariff));
        }
        if (dst && srcEntity != dstEntity) {
            dst->gold += static_cast<int64_t>(goldInt * tariff);
        }
    }

} // namespace gs::server