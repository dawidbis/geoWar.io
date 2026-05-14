# 09 — System handlu (TradeSystem)

## Odpowiedzialność

Generuje transporty handlowe (lądowe, morskie, lotnicze), oblicza dochód z handlu, dystrybuuje złoto między graczami.

## tick()

```cpp
void TradeSystem::tick(GameState& state) {
    // Lądowe — fabryki generują transporty
    for (auto& b : state.buildings) {
        if (b.type != BuildingType::Factory || !b.isActive) continue;
        if (state.currentTick % FACTORY_TRADE_INTERVAL[b.level] != 0) continue;
        generateLandTransport(state, b);
    }

    // Morskie — porty
    for (auto& b : state.buildings) {
        if (b.type != BuildingType::Port || !b.isActive) continue;
        if (state.currentTick % PORT_TRADE_INTERVAL[b.level] != 0) continue;
        generateSeaTransport(state, b);
    }

    // Lotnicze — lotniska
    for (auto& b : state.buildings) {
        if (b.type != BuildingType::Airport || !b.isActive) continue;
        if (state.currentTick % AIRPORT_TRADE_INTERVAL[b.level] != 0) continue;
        generateAirTransport(state, b);
    }

    // Dostarczenia — transporty które dotarły do celu
    processDeliveries(state);
}
```

## Obliczanie dochodu

```cpp
float TradeSystem::computeGold(const GameState& state,
                                uint32_t sourceEntityId,
                                uint32_t destEntityId,
                                float distance,
                                float baseValue,
                                float levelFactor) {
    float partnerFactor = getPartnerTypeFactor(state, sourceEntityId, destEntityId);
    float distFactor    = 1.0f + distance / 1000.0f;  // dłuższa trasa = więcej
    return baseValue * distFactor * partnerFactor * levelFactor;
}

float TradeSystem::getPartnerTypeFactor(const GameState& state,
                                         uint32_t a, uint32_t b) {
    if (a == b)                              return 1.0f;  // własny
    if (state.inSameEconomicPact(a, b))      return 2.5f;
    if (state.hasBilateralTrade(a, b))       return 2.0f;
    if (!state.isAtWar(a, b))               return 1.5f;
    return 0.0f;  // wrogowie — brak handlu
}
```

## Zależności

- `GameState` — buildings, entities (gold), relations, pacts
- `NavalSystem` — morskie transporty fizyczne (piractwo wykrywane w NavalSystem)
- `AirSystem` — lotnicze transporty fizyczne
