# 05 — Walka lądowa

## Filozofia

Walka **nie wymaga sterowania jednostkami**. Jest wynikiem **ciśnienia populacji wojskowej** na granicy między prowincjami. Gracz decyduje:

1. **Z których prowincji** atakuje.
2. **Które prowincje** atakuje.
3. **Jaki procent** swojej dostępnej populacji wojskowej wysłać.

## Sąsiedztwo dla ataku

Gracz A "graniczy z prowincją X" jeśli kontroluje co najmniej jeden kafelek lądowy graniczący (4-kier.) z kafelkiem prowincji X należącym do wroga lub wildernessem (w tym fallout-wilderness).

## Inicjacja ataku — pojedynczy

```python
def initiate_attack(playerId, sourceProvId, targetProvId, percentMilitary):
    sourceShare = get_share(sourceProvId, playerId)
    if not borders_target(sourceShare, targetProvId, playerId):
        return Error("Brak granicy")

    troopsToSend = sourceShare.population.military * percentMilitary
    sourceShare.population.military -= troopsToSend

    needSupply = troopsToSend * SUPPLY_PER_SOLDIER
    supplyTaken = min(sourceShare.supplyStored, needSupply)
    remove_supply_from_share(sourceShare, supplyTaken)

    attack = Attack(
        attackerId = playerId,
        sourceProvinceId = sourceProvId,
        targetProvinceId = targetProvId,
        troops = troopsToSend,
        supply = supplyTaken,
        startTick = current_tick,
    )
    register_attack(attack)
```

## Multi-attack (Ctrl+klik)

### Logika podziału (Wariant A)

> **Każda prowincja-źródło wysyła X% swojej populacji wojskowej proporcjonalnie do wszystkich celów, z którymi graniczy.**

```python
def initiate_multi_attack(playerId, sourceProvIds, targetProvIds, percentMilitary):
    for srcId in sourceProvIds:
        srcShare = get_share(srcId, playerId)
        reachable = [t for t in targetProvIds if borders(srcId, t, playerId)]
        if not reachable:
            continue  # źródło ignorowane — nie graniczy z żadnym celem

        totalToSend = srcShare.population.military * percentMilitary
        perTarget = totalToSend / len(reachable)

        for tId in reachable:
            initiate_attack_internal(playerId, srcId, tId, perTarget)
```

Wszystkie wynikowe ataki startują w tym samym ticku.

## Mechanika walki — kafelek po kafelku

```python
def tick_attack(attack):
    frontTile = pick_next_front_tile(attack)
    if frontTile is None:
        finalize_attack(attack)
        return

    tileOwnerId = frontTile.ownerId

    attackerLoss  = compute_attacker_loss(attack, frontTile, tileOwnerId)
    defenderLoss  = compute_defender_loss(attack, frontTile, tileOwnerId)
    captureSpeed  = compute_capture_speed(attack, frontTile, tileOwnerId)

    attack.capturedTileProgress += 1.0 / captureSpeed
    consume_supply(attack, frontTile)

    if attack.capturedTileProgress >= 1.0:
        capture_tile(frontTile, attack.attackerId)
        attack.troops -= attackerLoss
        if tileOwnerId != 0:
            get_share(frontTile.landProvinceId, tileOwnerId).population.military -= defenderLoss
        attack.capturedTileProgress = 0.0
        check_annexation(frontTile.landProvinceId, attack.attackerId)

    if attack.troops <= 0:
        finalize_attack(attack)
```

### Kafelek fallout jako wilderness

> ❗ Kafelek fallout-* **jest wildernessem** (`ownerId == 0`) — nie ma obrońcy, ale jego terrain daje **znacznie wyższy opór** niż zwykły wilderness.

## Wzory walki

### Modyfikatory terenu

```python
def terrainDef(terrain):
    return {
        Plains:           0.8,
        Highlands:        5.0,
        Mountains:        1.2,
        FalloutPlains:    3.0,   # trudny do przejścia przez skażenie
        FalloutHighlands: 8.0,   # bardzo trudny
        FalloutMountains: 4.0,   # TBD — może trudniejszy niż Mountains?
    }[terrain]

def terrainSpeed(terrain):
    return {
        Plains:           16.5,
        Highlands:        20.0,
        Mountains:        25.0,
        FalloutPlains:    35.0,  # wolne przesuwanie przez pustkowie
        FalloutHighlands: 50.0,
        FalloutMountains: 45.0,
    }[terrain]
```

> Wartości fallout-* są propozycjami — kluczowa zasada: **fallout-plains jest trudniejszy niż zwykłe Mountains**, bo to połączenie trudności terenu i skażenia.

### Strata atakującego (walka z żywym obrońcą)

$$
\text{attackerTroopLoss} = 0.8 \cdot terrainDef \cdot defensePost \cdot botDebuff \cdot largeAttackerBuff \cdot largeDefenderDebuff \cdot traitorDebuff \cdot clampedTroopRatio \cdot \frac{1}{supplyEfficiencyMod}
$$

### Strata atakującego (wilderness, w tym fallout)

$$
\text{attackerTroopLoss} = \frac{terrainDef + falloutDef}{attackerTypeBuff}
$$

```python
def falloutDef(isTileFallout, terrain):
    if not isTileFallout:
        return 0
    # Globalna miara skażenia mapy
    falloutRatio = numAllFalloutTiles / numAllLandTiles
    return 5 - falloutRatio * 2
```

> Im więcej skażonej ziemi globalnie, tym mniejszy dodatkowy debuff (kafelki fallout stają się "normą"). Wzór bezpośrednio z openfront.io.

### Komponenty wspólne

```python
def terrainDef(terrain):
    # patrz tabela wyżej

def defensePost(isInRange):
    return 5 if isInRange else 1

def botDebuff(isDefenderBot):
    return 0.8 if isDefenderBot else 1

def largeAttackerBuff(attackerNumTilesOwned):
    if attackerNumTilesOwned < 100_000:
        return 1
    return (100_000 / attackerNumTilesOwned) ** 0.7

def largeDefenderDebuff(defenderNumTilesOwned):
    return 0.7 + 0.3 * (1 - 1 / (1 + exp(-log(2)/50_000 * (defenderNumTilesOwned - 150_000))))

def traitorDebuff(isDefenderTraitor):
    return 0.8 if isDefenderTraitor else 1

def clampedTroopRatio(defTroops, atkTroops):
    return clamp(defTroops / atkTroops, 0.6, 2.0)

def attackerTypeBuff(isBot):
    return 10 if isBot else 5

def supplyEfficiencyMod(attack):
    if attack.supply > 0:
        return clamp(attack.supply / max_backpack(attack), 0.1, 1.0)
    elif has_local_supply_source(attack):
        return 1.0
    else:
        return 0.1
```

### Szybkość przejmowania

```python
def terrainSpeed(terrain):
    # patrz tabela wyżej

def clampedSpeedAttackRatio(def_, atk):
    return clamp(def_ / (5 * atk), 0.2, 1.5)

def wildSpeed(troops, terrain, defensePost):
    base = 2000 * max(terrainSpeed(terrain) * defensePost * falloutSpeedMod, 10) / troops
    return clamp(base, 5, 100)

def falloutSpeedMod(isFallout):
    return 2.0 if isFallout else 1.0  # propozycja
```

## Obrona

Co może zrobić obrońca:

1. Wysłać **posiłki** z innych prowincji (transport logistyczny, patrz [07-logistyka.md](07-logistyka.md)).
2. Zmienić `workerSplitRatio`.
3. Otrzymać **transfer populacji** od sojusznika z paktu militarnego.
4. Wysłać **kontratak**.

## Odwrót (Retreat)

1. Gracz klika "Wycofaj" przy danym ataku.
2. Po **20 tickach** atak anulowany.
3. **25% pozostałych wojsk ginie**.
4. Reszta wraca do prowincji-źródła.
5. **50% zaopatrzenia z plecaka** wraca do najbliższego magazynu.

## Aneksja

Po każdym capture kafelka — sprawdzenie spójności obrońcy:

1. Grupy kafelków obrońcy bez połączenia (przez własne kafelki lub pakt-mate militarny) z głównym terytorium — zostają **natychmiastowo zaanektowane**.
2. **Aneksja fallout-kafelków:** fallout-kafelki po zajęciu zmieniają terrain z powrotem na bazowy (np. `FalloutPlains → Plains`) i `isFallout = false`.

```python
def check_annexation(landProvinceId, attackerId):
    for defender_share in get_province(landProvinceId).playerShares.values():
        if defender_share.entityId == attackerId:
            continue
        groups = find_connected_groups(defender_share.ownedTileIds)
        for group in groups:
            if not has_path_to_main_territory(defender_share.entityId, group):
                annex_tiles(group, attackerId)
                for tile in group:
                    if tile.isFallout:
                        tile.isFallout = False
                        tile.terrain = base_terrain(tile.terrain)
```

## Otwarte pytania / TBD

- [ ] Wartości `terrainDef` i `terrainSpeed` dla fallout-* *(propozycje podane wyżej — do tuningu)*
- [ ] `falloutSpeedMod` *(propozycja: ×2.0)*
- [ ] Czy aneksja kasuje fallout natychmiastowo czy po N tickach? *(decyzja: natychmiastowo przy zajęciu)*
- [ ] Czy bonus dominacji daje +10% straty atakującego? *(propozycja: TAK)*
- [ ] Heurystyka front tile selection — szczegółowy algorytm (TBD)
- [ ] Co się dzieje z transportem gdy prowincja-źródło zostaje przejęta? *(propozycja: atak trwa, ale bez możliwości pull supply)*
