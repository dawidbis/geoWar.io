# 01 — Przegląd architektury

## Filozofia

Gra opiera się na modelu **authoritative server** — serwer jest jedynym źródłem prawdy. Klient jest "głupim rendererem": odbiera stan gry, rysuje go i wysyła komendy gracza. Nie wykonuje żadnej logiki symulacji.

```
┌─────────────────────────────────────────────────────┐
│                      SERWER                         │
│                                                     │
│  LobbyManager → GameLoop → [Systemy] → GameState   │
│                     ↑             ↓                 │
│               PlayerInput    StateDelta             │
└──────────────────────┬──────────────┬──────────────┘
                       │  TCP         │  TCP
┌──────────────────────┴──────────────┴──────────────┐
│                      KLIENT                         │
│                                                     │
│  ClientConnection → ClientState → Renderer          │
│       ↑                                ↓            │
│  PlayerInput ←──────── UI (ImGui/SFML) ←───────────│
└─────────────────────────────────────────────────────┘
```

## Warstwy systemu

### Shared (statyczna biblioteka)

Kod współdzielony przez serwer i klient. Kompiluje się raz, linkowany statycznie przez obie strony.

Zawiera:
- **Typy danych** — `Tile`, `LandProvince`, `SeaProvince`, `Entity`, `Building`, `Unit`, `Attack` itd.
- **Enumy** — `TerrainType`, `BuildingType`, `UnitType`, `RelationFlag`, `PactType` itd.
- **Protokół sieciowy** — `Message`, `MessageHeader`, `MessageType`, serializacja/deserializacja.
- **Stałe symulacji** — `Constants.hpp` (tickrate, BASE_PER_TILE, SUPPLY_PER_SOLDIER itd.)

Zasada: **shared nie zna boost ani SFML**. Zależy tylko od standardowej biblioteki C++20.

### Server

Wykonuje całą logikę symulacji. Zbudowany na `boost.asio` (TCP, async I/O, timer).

Główne komponenty:

```
Server
 └── LobbyManager          — zarządza jednym aktywnym lobby (v1)
      └── [gracze dołączają]
           ↓ gra startuje
      GameLoop              — tick co 100ms (steady_timer)
       ├── CombatSystem
       ├── PopulationSystem
       ├── SupplySystem
       ├── ElectricSystem
       ├── TradeSystem
       ├── NavalSystem
       ├── AirSystem
       ├── NukeSystem
       └── DiplomacySystem
```

### Client

Łączy się z serwerem przez TCP. Odbiera `GameStateDelta` co tick, aktualizuje lokalny `ClientState`, renderuje przez SFML + ImGui.

```
ClientConnection          — async read/write przez boost.asio
 ↓
ClientState               — lokalny snapshot stanu (read-only)
 ↓
Renderer
 ├── MapRenderer           — kafelki, prowincje, kolory graczy
 ├── UnitRenderer          — okręty, samoloty, transporty
 └── UIRenderer            — ImGui: panele, HUD, dyplomacja
      └── DebugPanel        — tylko przy GS_DEBUG_MODE=1
```

## Przepływ danych — jeden tick

```
[Tick N — serwer]

1. Odczytaj komendy graczy z kolejki (PlayerInputMsg)
2. Wykonaj systemy w kolejności:
   CombatSystem → PopulationSystem → SupplySystem →
   ElectricSystem → TradeSystem → NavalSystem →
   AirSystem → NukeSystem → DiplomacySystem
3. Sprawdź warunki zwycięstwa / kapitulacji
4. Wygeneruj GameStateDelta (tylko zmienione pola)
5. Wyślij delta do każdego klienta

[Klient — asynchronicznie]

1. Odbierz GameStateDelta
2. Zastosuj delta na ClientState
3. Następna klatka renderowania używa zaktualizowanego stanu
```

## Tryb singleplayer

Singleplayer = **serwer na localhost**. Klient łączy się przez TCP z `127.0.0.1:7777`. Z punktu widzenia kodu klient nie wie czy gra z ludźmi czy sam — różnica jest tylko w tym że serwer jest uruchomiony lokalnie.

```cpp
// W trybie singleplayer klient uruchamia serwer jako subprocess
// lub użytkownik uruchamia oba ręcznie (v1)
// TODO v2: klient automatycznie uruchamia gs_server.exe jako child process
```

## Tryb debug (GS_DEBUG_MODE=1)

Klient kompilowany z `-DGS_DEBUG_MODE=ON` uaktywnia `DebugPanel`:

- Podgląd pełnego `GameState` (wszystkie prowincje, populacje, zasoby).
- Ręczne iterowanie ticków — `DebugStepMsg` zatrzymuje `steady_timer` na serwerze.
- Zmiana tickrate w runtime — `DebugSetTickrateMsg`.
- Widok wszystkich parametrów wszystkich encji.

Serwer w trybie debug reaguje na `DebugStepMsg` — wykonuje dokładnie N ticków i czeka.

## Deterministyczność symulacji

Serwer jest jedynym źródłem symulacji — nie ma potrzeby deterministyczności po stronie klienta (klient tylko renderuje). Jednak serwer sam w sobie powinien być deterministyczny przy tym samym seedzie i tych samych inputach (ważne dla testów i debugowania).

Zasady:
- **Nie używamy `float` do krytycznych obliczeń** (walka, populacja) tam gdzie liczy się dokładność — rozważyć `int64_t` z skalowaniem lub `FixedPoint`.
- **Kolejność iteracji** po mapach i listach musi być deterministyczna — używamy `std::vector` zamiast `std::unordered_map` wszędzie gdzie iterujemy per tick.
- **Random** — `std::mt19937` z seedem z lobby, jeden per gra.

## Zależności między modułami

```
shared
  ↑ linkuje
server ──── boost.asio (sieć + timer)
client ──── SFML (grafika, okno)
            Dear ImGui (UI)
tests  ──── GTest
            server_lib (serwer bez main.cpp)
            client_lib (klient bez main.cpp)
```
