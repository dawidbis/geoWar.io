# 11 — System powietrzny (AirSystem)

## Odpowiedzialność

Porusza myśliwce, bombowce i cargo lotnicze. Obsługuje walkę powietrzną, obronę przeciwlotniczą, eskorty bombowców.

## tick()

```cpp
void AirSystem::tick(GameState& state) {
    tickFighters(state);
    tickBombers(state);
    tickAirCargos(state);
    tickAntiAir(state);  // AA strzela do wszystkiego w zasięgu
}
```

## Myśliwce — detekcja i angażowanie

```cpp
void AirSystem::tickFighters(GameState& state) {
    for (auto& fighter : state.fighters) {
        if (fighter.state == FighterState::AtBase) {
            // Sprawdź czy jest wróg w strefie patrolu
            auto enemy = findEnemyInPatrolZone(state, fighter);
            if (enemy) {
                fighter.state    = FighterState::Engaging;
                fighter.targetId = enemy->id;
            }
        } else if (fighter.state == FighterState::Engaging) {
            auto* target = getAirUnit(state, fighter.targetId);
            if (!target || target->health <= 0) {
                fighter.state = FighterState::Returning;
                continue;
            }
            float dist = distance(fighter.x, fighter.y, target->x, target->y);
            if (dist <= AIR_WEAPONS_RANGE) {
                float dmg = AIR_DAMAGE_VS[fighter.type][target->type];
                target->health -= dmg;
                fighter.health -= getDamageFrom(*target);
                if (fighter.health <= 0) fighter.state = FighterState::ShotDown;
            } else {
                moveToward(fighter, *target, FIGHTER_SPEED);
            }
        } else if (fighter.state == FighterState::Returning) {
            moveTowardBase(state, fighter);
        }
    }
}
```

## Bombowiec — misja

```cpp
void AirSystem::tickBombers(GameState& state) {
    for (auto& bomber : state.bombers) {
        if (bomber.hasReleased) {
            // Wraca do bazy po zrzucie
            moveTowardBase(state, bomber);
            continue;
        }
        // Leć do celu
        float dist = distance(bomber.x, bomber.y,
                              bomber.targetX, bomber.targetY);
        if (dist < BOMBER_SPEED) {
            // Dotarł — zrzuć bombę
            bomber.hasReleased = true;
            state.pendingEvents.push_back(
                makeAtomicBombEvent(bomber.targetX, bomber.targetY, bomber.ownerId)
            );
            // NukeSystem wykona eksplozję w tym samym ticku (wywołany po AirSystem)
        } else {
            moveStraight(bomber, BOMBER_SPEED);
        }
    }
}
```

## Obrona przeciwlotnicza

```cpp
void AirSystem::tickAntiAir(GameState& state) {
    for (auto& b : state.buildings) {
        if (b.type != BuildingType::AntiAir || !b.isActive) continue;

        for (auto& fighter : state.fighters) {
            if (fighter.ownerId == b.ownerId) continue;
            if (distance(b.x, b.y, fighter.x, fighter.y) > AA_RANGE[b.level])
                continue;
            if (randomChance(AA_CHANCE_PER_TICK[b.level])) {
                fighter.health = 0;
                fighter.state  = FighterState::ShotDown;
            }
        }
        // Analogicznie dla bombers i airCargos
    }
}
```

## Zależności

- `GameState` — fighters, bombers, airCargos, buildings
- `NukeSystem` — AirSystem generuje zdarzenie bomby, NukeSystem je przetwarza
