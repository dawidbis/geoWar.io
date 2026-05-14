# Architektura projektu

## Struktura folderów

```
grand_strategy/
│
├── CMakeLists.txt                  # root — definiuje projekt, FetchContent, subdirectory
├── cmake/
│   ├── FetchDependencies.cmake     # wszystkie FetchContent_Declare
│   └── CompilerOptions.cmake       # flagi C++20, warnings, sanitizery
│
├── shared/                         # statyczna biblioteka — wspólna dla serwera i klienta
│   ├── CMakeLists.txt
│   ├── include/
│   │   └── shared/
│   │       ├── types/
│   │       │   ├── Tile.hpp
│   │       │   ├── Province.hpp
│   │       │   ├── SeaProvince.hpp
│   │       │   ├── Entity.hpp
│   │       │   ├── Building.hpp
│   │       │   ├── Unit.hpp           # okręt, samolot, lotniskowiec
│   │       │   ├── Attack.hpp
│   │       │   ├── Diplomacy.hpp
│   │       │   └── Enums.hpp          # TerrainType, BuildingType, RelationFlag...
│   │       ├── net/
│   │       │   ├── Message.hpp        # nagłówek + payload, serializacja
│   │       │   ├── MessageTypes.hpp   # enum MessageType (wszystkie typy wiadomości)
│   │       │   └── Serializer.hpp     # encode/decode — używa std::span / std::vector<uint8_t>
│   │       ├── sim/
│   │       │   └── Constants.hpp      # BASE_PER_TILE, SUPPLY_PER_SOLDIER, TICKRATE itd.
│   │       └── util/
│   │           ├── FixedPoint.hpp     # opcjonalnie — deterministyczna arytmetyka
│   │           └── IdGenerator.hpp
│   └── src/
│       ├── net/
│       │   ├── Message.cpp
│       │   └── Serializer.cpp
│       └── util/
│           └── IdGenerator.cpp
│
├── server/
│   ├── CMakeLists.txt
│   ├── include/
│   │   └── server/
│   │       ├── net/
│   │       │   ├── Server.hpp         # acceptor, zarządza sesjami
│   │       │   ├── Session.hpp        # jedna połączona sesja TCP
│   │       │   └── LobbyManager.hpp   # jedno aktywne lobby (v1)
│   │       ├── sim/
│   │       │   ├── GameState.hpp      # pełny stan gry (mapa, encje, ataki...)
│   │       │   ├── GameLoop.hpp       # tick loop, 10 Hz, steady_timer
│   │       │   ├── WorldGen.hpp       # generacja mapy (Voronoi, heightmap)
│   │       │   ├── systems/           # logika per-system, wywoływana z GameLoop
│   │       │   │   ├── CombatSystem.hpp
│   │       │   │   ├── PopulationSystem.hpp
│   │       │   │   ├── SupplySystem.hpp
│   │       │   │   ├── ElectricSystem.hpp
│   │       │   │   ├── TradeSystem.hpp
│   │       │   │   ├── NavalSystem.hpp
│   │       │   │   ├── AirSystem.hpp
│   │       │   │   ├── NukeSystem.hpp
│   │       │   │   └── DiplomacySystem.hpp
│   │       │   └── AnnexationSystem.hpp
│   │       └── bot/
│   │           ├── BotController.hpp
│   │           └── BotLevel.hpp
│   └── src/
│       ├── main.cpp
│       ├── net/
│       │   ├── Server.cpp
│       │   ├── Session.cpp
│       │   └── LobbyManager.cpp
│       ├── sim/
│       │   ├── GameState.cpp
│       │   ├── GameLoop.cpp
│       │   ├── WorldGen.cpp
│       │   └── systems/
│       │       ├── CombatSystem.cpp
│       │       ├── PopulationSystem.cpp
│       │       ├── SupplySystem.cpp
│       │       ├── ElectricSystem.cpp
│       │       ├── TradeSystem.cpp
│       │       ├── NavalSystem.cpp
│       │       ├── AirSystem.cpp
│       │       ├── NukeSystem.cpp
│       │       └── DiplomacySystem.cpp
│       └── bot/
│           ├── BotController.cpp
│           └── BotLevel.cpp
│
├── client/
│   ├── CMakeLists.txt
│   ├── include/
│   │   └── client/
│   │       ├── net/
│   │       │   └── ClientConnection.hpp   # TCP połączenie do serwera
│   │       ├── render/
│   │       │   ├── Renderer.hpp           # główna klasa renderująca (SFML)
│   │       │   ├── MapRenderer.hpp
│   │       │   ├── UnitRenderer.hpp
│   │       │   └── UIRenderer.hpp         # Dear ImGui
│   │       ├── state/
│   │       │   └── ClientState.hpp        # lokalny snapshot stanu gry (tylko do odczytu)
│   │       └── debug/
│   │           └── DebugPanel.hpp         # tryb dev: tick-by-tick, podgląd wszystkich danych
│   └── src/
│       ├── main.cpp
│       ├── net/
│       │   └── ClientConnection.cpp
│       ├── render/
│       │   ├── Renderer.cpp
│       │   ├── MapRenderer.cpp
│       │   ├── UnitRenderer.cpp
│       │   └── UIRenderer.cpp
│       ├── state/
│       │   └── ClientState.cpp
│       └── debug/
│           └── DebugPanel.cpp
│
└── tests/
    ├── CMakeLists.txt
    ├── server/
    │   ├── test_combat.cpp
    │   ├── test_population.cpp
    │   ├── test_supply.cpp
    │   ├── test_electric.cpp
    │   ├── test_annexation.cpp
    │   └── test_worldgen.cpp
    ├── client/
    │   └── test_serializer_roundtrip.cpp  # serialize → deserialize → porównaj
    └── shared/
        ├── test_message.cpp
        └── test_serializer.cpp
```

## Zależności

| Moduł | Zależności |
|---|---|
| shared | (brak zewnętrznych) |
| server | shared, Boost (asio, system) |
| client | shared, SFML, Dear ImGui |
| tests | shared, server (jako lib), client (jako lib), GTest |

## Przepływ danych (uproszczony)

```
[Klient]                          [Serwer]
  │                                  │
  │── PlayerInputMsg ──────────────► │
  │                                  │  tick() co 100ms
  │                                  │  ├─ CombatSystem
  │                                  │  ├─ PopulationSystem
  │                                  │  └─ ... (wszystkie systemy)
  │                                  │
  │◄── GameStateDeltaMsg ──────────  │
  │                                  │
ClientState.apply(delta)             │
Renderer.draw(clientState)           │
```

## Tryb dev/debug

Klient kompilowany z `-DGRAND_STRATEGY_DEBUG=ON` uaktywnia `DebugPanel`:
- podgląd pełnego stanu gry (wszystkich prowincji, populacji, zasobów)
- ręczne iterowanie ticków (wysłanie `DebugStepMsg` do serwera)
- konfiguracja serwera w runtime (zmiana tickrate, poziom logowania)
- serwer w trybie debug odpowiada na `DebugStepMsg` i zatrzymuje `steady_timer`
```
