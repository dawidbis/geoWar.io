# 10 — System morski (NavalSystem)

## Odpowiedzialność

Porusza okręty wojenne i lotniskowce, obsługuje walkę morską, piractwo, ruch lotniskowców między prowincjami morskimi.

## tick()

```cpp
void NavalSystem::tick(GameState& state) {
    tickWarships(state);
    tickCarriers(state);
    tickSeaCargos(state);
}
```

## Okręty wojenne — state machine

```cpp
void NavalSystem::tickWarships(GameState& state) {
    for (auto& ship : state.warships) {
        switch (ship.state) {
            case WarshipState::Patrolling:
                tickPatrol(state, ship);
                break;
            case WarshipState::Engaging:
                tickEngage(state, ship);
                break;
            case WarshipState::Returning:
                tickReturn(state, ship);
                break;
            case WarshipState::Resupply:
                tickResupply(state, ship);
                break;
        }
    }
}

void NavalSystem::tickPatrol(GameState& state, Warship& ship) {
    // Szukaj celów w zasięgu detekcji (25 kafelków)
    auto target = findNearestTarget(state, ship, WARSHIP_DETECT_RANGE);
    if (target) {
        ship.targetId = target->id;
        ship.state    = WarshipState::Engaging;
        return;
    }
    // Patrol wokół home base
    moveTowardPatrolPoint(state, ship);
}

void NavalSystem::tickEngage(GameState& state, Warship& ship) {
    auto* target = getUnit(state, ship.targetId);
    if (!target || target->health <= 0) {
        ship.state    = WarshipState::Returning;
        ship.targetId = 0;
        return;
    }
    float dist = distance(ship.x, ship.y, target->x, target->y);
    if (dist <= WARSHIP_WEAPONS_RANGE) {
        // Wymiana obrażeń
        target->health -= WARSHIP_DAMAGE_PER_TICK;
        ship.health    -= WARSHIP_DAMAGE_PER_TICK;
        if (ship.health < ship.maxHealth * 0.3f) {
            ship.state = WarshipState::Returning;
        }
    } else {
        moveToward(ship, target->x, target->y, WARSHIP_SPEED);
    }
}
```

## Lotniskowiec — ruch między prowincjami

```cpp
void NavalSystem::tickCarriers(GameState& state) {
    for (auto& carrier : state.carriers) {
        if (carrier.state != CarrierState::EnRoute) continue;

        auto& targetProv = state.seaProvinces[carrier.targetSeaProvinceId];
        float dist = distance(carrier.visualX, carrier.visualY,
                              targetProv.centerX, targetProv.centerY);

        if (dist < CARRIER_SPEED) {
            // Dotarł
            carrier.visualX       = targetProv.centerX;
            carrier.visualY       = targetProv.centerY;
            carrier.seaProvinceId = carrier.targetSeaProvinceId;
            carrier.state         = CarrierState::Stationed;
            state.dirtyUnits.insert(carrier.id);
        } else {
            // Przesuń w kierunku celu
            moveToward(carrier.visualX, carrier.visualY,
                       targetProv.centerX, targetProv.centerY, CARRIER_SPEED);
            state.dirtyUnits.insert(carrier.id);
        }
    }
}
```

## Piractwo

```cpp
void NavalSystem::checkPiracy(GameState& state, Warship& ship, SeaCargo& cargo) {
    if (distance(ship.x, ship.y, cargo.x, cargo.y) > WARSHIP_WEAPONS_RANGE)
        return;

    auto relation = state.getRelation(ship.ownerId, cargo.ownerId);
    if (relation & RelationFlag::AtWar) {
        // Zniszcz transport wroga
        cargo.health = 0;
        auto& entity = state.entities[ship.ownerId];
        entity.gold += static_cast<int64_t>(cargo.value * 0.2f);
    } else {
        // Piractwo na neutralnym — incydent dyplomatyczny
        cargo.health = 0;
        auto& entity = state.entities[ship.ownerId];
        entity.gold += static_cast<int64_t>(cargo.value * 0.5f);
        // Dodaj zdarzenie dyplomatyczne
        state.pendingEvents.push_back(makePiracyEvent(ship.ownerId, cargo.ownerId));
    }
}
```

## Zasada izolacji ląd–morze

```cpp
// NavalSystem NIE modyfikuje nic poza:
// - pozycjami jednostek morskich
// - health jednostek morskich
// - gold encji (piractwo)
// - pendingEvents
//
// Nigdy nie modyfikuje: tiles, landProvinces, buildings, attacks
```

## Zależności

- `GameState` — warships, carriers, seaCargos, seaProvinces
- `TradeSystem` — generuje SeaCargo które NavalSystem może niszczyć
