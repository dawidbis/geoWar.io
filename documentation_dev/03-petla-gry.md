# 03 — Pętla gry i tick

## GameLoop

`GameLoop` odpowiada za wywoływanie wszystkich systemów w ustalonej kolejności co 100ms (10 Hz). Używa `boost::asio::steady_timer`.

```cpp
// server/include/server/sim/GameLoop.hpp

class GameLoop {
public:
    explicit GameLoop(boost::asio::io_context& ioc,
                      GameState& state,
                      SessionBroadcaster& broadcaster);

    void start();
    void stop();

    // Tryb debug — wykonaj dokładnie N ticków i zatrzymaj timer
    void stepTicks(uint32_t n);
    void setTickrate(uint32_t hz);

private:
    void scheduleTick();
    void tick();

    boost::asio::steady_timer   timer_;
    GameState&                  state_;
    SessionBroadcaster&         broadcaster_;

    uint32_t    tickrateHz_{10};
    bool        running_{false};
    bool        debugPaused_{false};
    uint32_t    debugStepsRemaining_{0};

    // Systemy — wywoływane w kolejności w tick()
    CombatSystem        combatSys_;
    PopulationSystem    populationSys_;
    SupplySystem        supplySys_;
    ElectricSystem      electricSys_;
    TradeSystem         tradeSys_;
    NavalSystem         navalSys_;
    AirSystem           airSys_;
    NukeSystem          nukeSys_;
    DiplomacySystem     diplomacySys_;
};
```

## Implementacja tick loop

```cpp
void GameLoop::scheduleTick() {
    if (!running_) return;

    auto interval = std::chrono::milliseconds(1000 / tickrateHz_);
    timer_.expires_after(interval);
    timer_.async_wait([this](boost::system::error_code ec) {
        if (ec) return;
        tick();
        // Tryb debug — jeśli stepTicks() wywołane, odliczaj
        if (debugPaused_) {
            if (debugStepsRemaining_ > 0) {
                --debugStepsRemaining_;
                scheduleTick();
            }
            // else: czekaj na kolejne stepTicks()
        } else {
            scheduleTick();
        }
    });
}

void GameLoop::tick() {
    state_.currentTick++;

    // Kolejność systemów jest ważna — patrz niżej
    combatSys_    .tick(state_);
    populationSys_.tick(state_);
    supplySys_    .tick(state_);
    electricSys_  .tick(state_);
    tradeSys_     .tick(state_);
    navalSys_     .tick(state_);
    airSys_       .tick(state_);
    nukeSys_      .tick(state_);
    diplomacySys_ .tick(state_);

    checkVictoryConditions(state_);

    // Generuj delta i roześlij do wszystkich klientów
    auto delta = generateDelta(state_);
    broadcaster_.sendToAll(delta);

    // Reset "dirty flags" na GameState
    state_.clearDirtyFlags();
}
```

## Kolejność systemów — uzasadnienie

| Kolejność | System | Uzasadnienie |
|---|---|---|
| 1 | CombatSystem | Walka przed przyrostem — wojsko nie rośnie w trakcie walki w tym samym ticku |
| 2 | PopulationSystem | Przyrost po walce — odzwierciedla straty z poprzedniego kroku |
| 3 | SupplySystem | Dystrybucja zaop. po walce — priorytety frontowe aktualne |
| 4 | ElectricSystem | Bilans sieci po ewentualnych zniszczeniach (NukeSystem wywołany ostatnio w poprzednim ticku) |
| 5 | TradeSystem | Handel po ustaleniu stanu infrastruktury |
| 6 | NavalSystem | Ruch jednostek morskich |
| 7 | AirSystem | Ruch jednostek powietrznych |
| 8 | NukeSystem | Eksplozje na końcu — efekty widoczne dopiero od następnego ticku |
| 9 | DiplomacySystem | Dyplomacja na końcu — zmiany relacji wchodzą w życie w następnym ticku |

## Interfejs systemu

Każdy system implementuje ten sam interfejs:

```cpp
// Konwencja — każdy system to klasa z metodą tick()
class CombatSystem {
public:
    void tick(GameState& state);
    // opcjonalnie: void init(GameState& state);
};
```

Systemy **nie przechowują stanu** — wszystko jest w `GameState`. Dzięki temu łatwo testować każdy system izolowanie:

```cpp
// test_combat.cpp
TEST(CombatSystem, AttackerLosesWhenOutnumbered) {
    GameState state = makeTestState();
    // ustaw prowincje, wojsko, atak...
    CombatSystem sys;
    sys.tick(state);
    // sprawdź wynik...
}
```

## Tryb debug

### stepTicks (DebugStepMsg)

Klient wysyła `DebugStep` z liczbą ticków do wykonania. Serwer wykonuje dokładnie N ticków i zatrzymuje timer.

```cpp
void GameLoop::stepTicks(uint32_t n) {
    debugPaused_ = true;
    debugStepsRemaining_ = n;
    scheduleTick();  // uruchom pierwszy tick od razu
}
```

### setTickrate (DebugSetTickrateMsg)

```cpp
void GameLoop::setTickrate(uint32_t hz) {
    tickrateHz_ = std::clamp(hz, 1u, 60u);
    // Nowy tickrate aktywny od następnego scheduleTick()
}
```

### DebugQueryState

Serwer serializuje cały `GameState` do JSON (lub binarnie) i odsyła jako `DebugStateResponse`. Klient wyświetla w `DebugPanel` przez ImGui.

## Generowanie delty

```cpp
GameStateDelta GameLoop::generateDelta(const GameState& state) {
    GameStateDelta delta;
    delta.tick = state.currentTick;

    // Kafelki oznaczone jako dirty (zmieniony ownerId, terrain, isFallout)
    for (auto tileId : state.dirtyTiles) {
        delta.tileChanges.push_back(makeTileDelta(state.tiles[tileId]));
    }

    // PlayerProvinceShare z dirty populacją/zasobami
    for (auto& [key, share] : state.dirtyShares) {
        delta.shareChanges.push_back(makeShareDelta(share));
    }

    // Jednostki (nowe, zniszczone, przesunięte)
    for (auto unitId : state.dirtyUnits) {
        delta.unitChanges.push_back(makeUnitDelta(state.units[unitId]));
    }

    // Zdarzenia globalne (eksplozja, kapitulacja, dołączenie do paktu)
    delta.events = std::move(state.pendingEvents);

    return delta;
}
```

## Zależności

- `boost.asio` — `steady_timer`, `io_context`
- `GameState` — czytany i modyfikowany przez wszystkie systemy
- Wszystkie systemy — wywoływane sekwencyjnie w `tick()`
