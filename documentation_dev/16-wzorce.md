# 16 — Wzorce projektowe

## 1. System pattern (ECS-like)

Logika gry podzielona na **niezależne systemy**, każdy operuje na `GameState`. Brak dziedziczenia między systemami. Łatwe testowanie izolowane.

```
GameState = dane (structs of arrays)
System    = funkcja tick(GameState&)
```

Zalety: systemy wymienialne, testowalne jednostkowo, łatwe profilowanie per system.

## 2. Authoritative Server / Dumb Client

Klient nie wykonuje żadnej logiki symulacji. Wysyła tylko komendy, odbiera stan. Eliminuje desynchronizację i cheating.

```
Klient: event → PlayerInput → serwer
Serwer: PlayerInput → GameState → GameStateDelta → klient
```

## 3. Delta Compression

Zamiast wysyłać pełny `GameState` co tick (zbyt duże), wysyłamy tylko **zmienione pola** (`GameStateDelta`). Dirty tracking przez `dirtyTiles`, `dirtyShares`, `dirtyUnits`.

## 4. Command pattern (PlayerInput)

Każda akcja gracza to komenda (`PlayerInput`) z typem i parametrami. Serwer kolejkuje komendy i przetwarza je na początku ticku.

```cpp
struct PlayerInput {
    uint32_t    entityId;
    InputType   type;       // Attack, Build, Diplomacy, Debug...
    // payload zależy od type — union lub variant
};
```

## 5. Observer (GameEvent)

Systemy komunikują się przez `state.pendingEvents` zamiast bezpośrednich wywołań. `AirSystem` wrzuca `AtomicBombDetonation` — `NukeSystem` go odbiera i przetwarza.

Zaleta: systemy nie znają się nawzajem, brak cyklicznych zależności.

## 6. Throttled Decision Making (boty)

Boty nie podejmują decyzji co tick — używają interwałów per kategoria decyzji. Imituje "czas myślenia" i redukuje obliczenia.

## 7. Object Pool (jednostki bojowe)

Okręty, samoloty, transporty alokowane w `std::vector` w `GameState`. Zniszczone jednostki oznaczane `health <= 0` i usuwane przez `std::erase_if` na końcu tick() systemu. Unikamy fragmentacji pamięci.

```cpp
// Na końcu NavalSystem::tick():
std::erase_if(state.warships, [](const Warship& w) { return w.health <= 0; });
```
