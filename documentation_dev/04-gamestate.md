# 04 — Stan gry (GameState)

## Filozofia

`GameState` to **jedyna kopia prawdy** po stronie serwera. Zawiera kompletny stan symulacji — mapę, encje, jednostki, dyplomację, zdarzenia. Każdy system czyta i modyfikuje `GameState` podczas `tick()`.

## Główna struktura

```cpp
// server/include/server/sim/GameState.hpp

struct GameState {
    // ── Meta ──────────────────────────────────────────────────────────────
    uint32_t    currentTick{0};
    uint64_t    mapSeed{0};
    GamePhase   phase{GamePhase::Lobby};  // Lobby, Playing, Finished

    // ── Mapa ──────────────────────────────────────────────────────────────
    uint16_t                        mapWidth{0};
    uint16_t                        mapHeight{0};
    std::vector<Tile>               tiles;          // indexed by tileId
    std::vector<LandProvince>       landProvinces;  // indexed by provinceId
    std::vector<SeaProvince>        seaProvinces;   // indexed by seaProvinceId

    // ── Encje (gracze + boty) ─────────────────────────────────────────────
    std::vector<Entity>             entities;       // indexed by entityId

    // ── Jednostki bojowe ──────────────────────────────────────────────────
    std::vector<Warship>            warships;
    std::vector<Carrier>            carriers;
    std::vector<Fighter>            fighters;
    std::vector<Bomber>             bombers;
    std::vector<AirCargo>           airCargos;
    std::vector<SeaCargo>           seaCargos;
    std::vector<MissileFlight>      missiles;

    // ── Ataki lądowe ──────────────────────────────────────────────────────
    std::vector<Attack>             attacks;

    // ── Dyplomacja ────────────────────────────────────────────────────────
    std::vector<Relation>           relations;      // per para graczy
    std::vector<Pact>               pacts;

    // ── Zdarzenia (per tick, wysyłane w delta) ────────────────────────────
    std::vector<GameEvent>          pendingEvents;

    // ── Dirty tracking (do generowania delty) ────────────────────────────
    std::unordered_set<uint32_t>    dirtyTiles;
    std::unordered_set<uint32_t>    dirtyUnits;
    // klucz: (provinceId << 32 | entityId)
    std::unordered_set<uint64_t>    dirtyShares;

    void clearDirtyFlags();

    // ── Pomocnicze lookups ────────────────────────────────────────────────
    // Zwraca PlayerProvinceShare gracza w danej prowincji (lub nullptr)
    PlayerProvinceShare* getShare(uint32_t provinceId, uint32_t entityId);

    // Sprawdza czy encja A i B są w tym samym pakcie militarnym
    bool inSameMilitaryPact(uint32_t entityA, uint32_t entityB) const;

    // Zwraca aktualny stan relacji między dwoma encjami
    RelationFlag getRelation(uint32_t a, uint32_t b) const;
};
```

## Tile (kafelek)

```cpp
struct Tile {
    uint32_t    id;
    uint16_t    x, y;
    uint32_t    landProvinceId{0};   // 0 jeśli wodny
    uint32_t    seaProvinceId{0};    // 0 jeśli lądowy
    TerrainType terrain;
    uint32_t    ownerId{0};          // 0 = niczyje / woda zawsze 0
    bool        isFallout{false};
    uint32_t    buildingId{0};       // 0 = brak budynku
};
```

## LandProvince

```cpp
struct LandProvince {
    uint32_t                    id;
    std::string                 name;
    std::vector<uint32_t>       tileIds;
    // Klucz: entityId
    std::unordered_map<uint32_t, PlayerProvinceShare> playerShares;
    std::vector<ProvinceAdjacency>  neighborProvinces;
    std::vector<uint32_t>           coastalSeaProvinceIds;
};

struct ProvinceAdjacency {
    uint32_t neighborProvinceId;
    uint32_t sharedBorderTilesCount;
};
```

## PlayerProvinceShare

Reprezentuje "sub-prowincję" gracza — jego kafelki, populację i infrastrukturę w danej prowincji lądowej.

```cpp
struct PlayerProvinceShare {
    uint32_t    entityId;
    uint32_t    provinceId;

    std::vector<uint32_t>   ownedTileIds;
    std::vector<uint32_t>   buildingIds;

    ProvincePopulation      population;
    float                   supplyStored{0.0f};
    float                   workerSplitRatio{0.6f};
    AutomationLevel         automationLevel{AutomationLevel::FullAuto};

    // Computed per tick przez ElectricSystem
    float                   powerAvailableMW{0.0f};

    // Pomocnicze
    float computeMaxCap(const GameState& state) const;
    bool  hasDominance(const LandProvince& prov) const;
};

struct ProvincePopulation {
    float   workers{0.0f};
    float   military{0.0f};
    float   maxCap{0.0f};   // aktualizowane przez PopulationSystem
};
```

## SeaProvince

```cpp
struct SeaProvince {
    uint32_t                id;
    std::string             name;
    std::vector<uint32_t>   tileIds;
    float                   centerX, centerY;   // punkt referencyjny lotniskowca
    std::vector<uint32_t>   neighborSeaProvinceIds;
    std::vector<uint32_t>   coastalLandProvinceIds;
};
```

## Entity (gracz / bot)

```cpp
struct Entity {
    uint32_t    id;
    std::string name;
    EntityType  type;       // Human, Bot, Disconnected

    // Zasoby globalne
    int64_t     gold{1000};
    float       uranium{0.0f};

    // Stolica
    uint32_t    capitalBuildingId{0};  // 0 = brak stolicy (po kapitulacji)

    // Boty
    BotLevel    botLevel{BotLevel::Medium};

    // Stan połączenia
    uint32_t    disconnectedSinceTick{0};   // 0 = połączony
    bool        isEliminated{false};

    // Pakty
    uint32_t    militaryPactId{0};   // 0 = brak
    uint32_t    economicPactId{0};   // 0 = brak
};
```

## Building

```cpp
struct Building {
    uint32_t        id;
    BuildingType    type;
    uint8_t         level{1};

    uint32_t        ownerId;
    uint32_t        landProvinceId;
    uint16_t        topLeftX, topLeftY;
    uint8_t         sizeX, sizeY;

    float           constructionProgress{0.0f};  // 0–1
    bool            isPowered{false};
    bool            isActive{false};   // powered AND constructed AND not damaged

    float           supplyStored{0.0f};
    float           supplyCapacity{0.0f};

    uint32_t        electricGridId{0};
    int8_t          electricPriority{50};

    bool            isCapital{false};

    // Tylko dla NuclearPlant
    NuclearPlantMode nuclearMode{NuclearPlantMode::Civilian};
};
```

## Attack (atak lądowy)

```cpp
struct Attack {
    uint32_t    id;
    uint32_t    attackerId;
    uint32_t    sourceProvinceId;
    uint32_t    targetProvinceId;

    float       troops;
    float       supply;     // "plecak" zaopatrzeniowy
    uint32_t    startTick;

    float       capturedTileProgress{0.0f};
    AttackStatus status{AttackStatus::Active};
    uint32_t    retreatEndTick{0};
};
```

## Jednostki bojowe

```cpp
struct Warship {
    uint32_t    id;
    uint32_t    ownerId;
    uint32_t    homePortId;         // port lub lotniskowiec (homeBaseType)
    HomeBaseType homeBaseType;      // Port lub Carrier
    float       x, y;
    float       health{100.0f};
    WarshipState state;
    uint32_t    targetId{0};
};

struct Carrier {
    uint32_t    id;
    uint32_t    ownerId;
    uint32_t    seaProvinceId;
    float       visualX, visualY;   // tylko animacja
    float       health{300.0f};
    CarrierState state;
    uint32_t    targetSeaProvinceId{0};
    uint8_t     level{1};
    std::vector<uint32_t> escortWarshipIds;
};

struct Fighter {
    uint32_t    id;
    uint32_t    ownerId;
    uint32_t    homeBaseId;
    HomeBaseType homeBaseType;      // Airport lub Carrier
    float       x, y;
    float       health{80.0f};
    FighterState state;
    uint32_t    targetId{0};
};

struct Bomber {
    uint32_t    id;
    uint32_t    ownerId;
    uint32_t    homeAirportId;
    float       x, y;
    float       health{60.0f};
    uint16_t    targetX, targetY;
    bool        hasReleased{false};
};
```

## Dirty tracking

Systemy oznaczają zmienione elementy przez wstawienie ID do dirty setów:

```cpp
// Przykład w CombatSystem
void captureTile(GameState& state, uint32_t tileId, uint32_t newOwner) {
    state.tiles[tileId].ownerId = newOwner;
    state.dirtyTiles.insert(tileId);    // oznacz jako zmieniony
}
```

`GameLoop::generateDelta()` iteruje po dirty setach i pakuje zmiany do `GameStateDelta`.

## Zależności

- `shared` — typy danych, enumy, stałe
- Wszystkie systemy — czytają i modyfikują `GameState`
- `WorldGen` — wypełnia `GameState` przy starcie gry
