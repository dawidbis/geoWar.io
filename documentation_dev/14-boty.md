# 14 — Boty (BotController)

## Architektura

`BotController` to komponent wywoływany z `GameLoop` po wszystkich systemach, ale przed generowaniem delty. Każdy bot ma własny `BotController` z poziomem trudności.

```cpp
class BotController {
public:
    BotController(uint32_t entityId, BotLevel level);
    void tick(GameState& state);

private:
    uint32_t    entityId_;
    BotLevel    level_;

    // Throttling — decyzje co N ticków
    void        tickAttack    (GameState& state, uint32_t currentTick);
    void        tickBuild     (GameState& state, uint32_t currentTick);
    void        tickDiplomacy (GameState& state, uint32_t currentTick);
    void        tickNaval     (GameState& state, uint32_t currentTick);
    void        tickAir       (GameState& state, uint32_t currentTick);
    void        tickNuke      (GameState& state, uint32_t currentTick);

    float       computeThreat (const GameState& state, uint32_t targetId) const;
};
```

## Throttling decyzji

```cpp
void BotController::tick(GameState& state) {
    uint32_t t = state.currentTick;

    if (t % 50  == 0) tickAttack   (state, t);
    if (t % 100 == 0) tickBuild    (state, t);
    if (t % 500 == 0) tickDiplomacy(state, t);
    if (t % 80  == 0) tickNaval    (state, t);
    if (t % 80  == 0) tickAir      (state, t);
    if (t % 200 == 0) tickNuke     (state, t);
}
```

## Threat assessment

```cpp
float BotController::computeThreat(const GameState& state, uint32_t targetId) const {
    auto& me     = state.entities[entityId_];
    auto& target = state.entities[targetId];

    float myTiles  = countTiles(state, entityId_);
    float tgtTiles = countTiles(state, targetId);
    float myMil    = sumMilitary(state, entityId_);
    float tgtMil   = sumMilitary(state, targetId);

    float milRatio  = tgtMil  / std::max(myMil,  1.0f);
    float terrRatio = tgtTiles / std::max(myTiles, 1.0f);
    bool  isBorder  = isBorderNeighbor(state, entityId_, targetId);

    return milRatio * 0.5f + terrRatio * 0.3f + (isBorder ? 0.2f : 0.0f);
}
```

## Zależności

- `GameState` — wszystkie dane (tylko odczyt + generowanie PlayerInput)
- Systemy — bot generuje `PlayerInput` tak samo jak gracz-człowiek
