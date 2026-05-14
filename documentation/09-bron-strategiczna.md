# 09 — Broń strategiczna

## Wprowadzenie

| Broń | Dostarczenie | Obrona |
|---|---|---|
| **Bomba atomowa** | **Bombowiec** | Obrona przeciwlotnicza + myśliwce |
| **Rakieta wodorowa** | **Silos rakietowy** | Obrona przeciwrakietowa |

## Łańcuchy produkcji

```
ATOMOWA:
  Elektrownia atomowa (Military Mode) → uran
  + Lotnisko → Bombowiec z bombą atomową
             → Lot → zestrzelenie? → uderzenie

WODOROWA:
  Elektrownia atomowa (Military Mode) → uran
  + Silos rakietowy → Rakieta wodorowa
                    → Lot balistyczny → zestrzelenie? → uderzenie
```

## Produkcja ładunków

### Bomba atomowa

| Parametr | Wartość (propozycja) |
|---|---|
| Koszt uranu | 50 |
| Koszt złota | 4000 |
| Czas produkcji | 1500 ticków (~2.5 min) |
| Przechowywanie | Max 2 bomby per lotnisko |

### Rakieta wodorowa

| Parametr | Wartość (propozycja) |
|---|---|
| Koszt uranu | 150 |
| Koszt złota | 18000 |
| Czas produkcji | 3500 ticków (~6 min) |
| Przechowywanie | Max 1 rakieta per silos |

## Dostarczenie — bombowiec

Szczegóły w [13-modul-powietrzny.md](13-modul-powietrzny.md). Najważniejsze:

1. Bombowiec startuje z lotniska z **kafelkiem docelowym**.
2. Leci po linii prostej.
3. Może być zestrzelony przez wrogie myśliwce i obronę przeciwlotniczą.
4. Dotarcie do celu → eksplozja, bombowiec ginie.
5. **Nie można odwołać po starcie.**

## Dostarczenie — silos

1. Gracz klika na silos → "Wystrzel" → wskazuje **kafelek docelowy**.
2. Tworzony obiekt `MissileFlight`.
3. Rakieta leci ~100 ticków (10 s) do celu.
4. Obrona przeciwrakietowa próbuje zestrzelić podczas lotu.
5. **Nie można odwołać po starcie.**

## Eksplozja — mechanika

### Obszar rażenia

- **Bomba atomowa:** promień **20 kafelków** (TBD).
- **Rakieta wodorowa:** promień **45 kafelków** (TBD).

### Zmiana terenu

Każdy kafelek **lądowy** w promieniu rażenia:

```python
FALLOUT_TERRAIN_MAP = {
    TerrainType.Plains:    TerrainType.FalloutPlains,
    TerrainType.Highlands: TerrainType.FalloutHighlands,
    TerrainType.Mountains: TerrainType.FalloutMountains,
    # kafelki już w stanie fallout — zostają (lub biorą gorszy?)
    TerrainType.FalloutPlains:    TerrainType.FalloutPlains,
    TerrainType.FalloutHighlands: TerrainType.FalloutHighlands,
    TerrainType.FalloutMountains: TerrainType.FalloutMountains,
}

def apply_terrain_change(tile):
    if tile.terrain in FALLOUT_TERRAIN_MAP:
        tile.terrain = FALLOUT_TERRAIN_MAP[tile.terrain]
        tile.isFallout = True
```

**Kafelki wodne w promieniu** — nie zmieniają terenu (woda nie ma fallout-wariantu). Eksplozja nad wodą nadal zadaje obrażenia populacji w prowincjach lądowych w zasięgu.

### Natychmiastowe straty populacji

Uderzenie jądrowe **natychmiastowo niszczy część populacji** we wszystkich prowincjach lądowych dotkniętych wybuchem.

```python
def apply_nuke_population_damage(center, blastRadius):
    affectedTiles = get_land_tiles_in_radius(center, blastRadius)

    # Grupujemy po prowincji i po właścicielu
    per_province_per_player = defaultdict(lambda: defaultdict(list))
    for tile in affectedTiles:
        if tile.ownerId != 0:
            per_province_per_player[tile.landProvinceId][tile.ownerId].append(tile)

    for provId, players in per_province_per_player.items():
        for playerId, destroyed_tiles in players.items():
            share = get_share(provId, playerId)

            # Straty z samych kafelków
            pop_loss = len(destroyed_tiles) * BASE_PER_TILE

            # Dodatkowe straty z miast w zasięgu bomby
            for building in share.buildingIds:
                b = get_building(building)
                if b.type == BuildingType.City and b.isInRadius(center, blastRadius):
                    pop_loss += CITY_CAP_BONUS[b.level]

            # Znormalizuj — nie można stracić więcej niż ma prowincja
            total_pop = share.population.workers + share.population.military
            loss_ratio = min(pop_loss / max(share.population.maxCap, 1), 1.0)

            share.population.workers  -= share.population.workers  * loss_ratio
            share.population.military -= share.population.military * loss_ratio
```

> Strata populacji jest **proporcjonalna** — kafelki × BASE_PER_TILE + miasta × city_cap_bonus daje "bazowy impact", który następnie jest normalizowany do procentu aktualnej populacji. Dzięki temu bomba nie tworzy "ujemnej populacji".

### Niszczenie budynków

Wszystkie budynki gracza na kafelkach w promieniu rażenia zostają **zniszczone**:

```python
def apply_nuke_buildings(center, blastRadius):
    for tile in get_land_tiles_in_radius(center, blastRadius):
        if tile.buildingId:
            destroy_building(tile.buildingId)

    rebuild_logistics_graphs()
    rebuild_electric_grids()
```

### Zmiana własności — pustkowie

Po eksplozji każdy lądowy kafelek w promieniu:

```python
def apply_nuke_ownership(affectedTiles):
    for tile in affectedTiles:
        tile.ownerId = 0  # kafelek staje się wildernessem
```

> Kafelek staje się **neutralnym pustkowiem** z fallout terrainem. Poprzedni właściciel nie ma już do niego "praw". Może być zajęty przez kogokolwiek przez zwykły atak — ale jest to **trudniejsze** niż normalny wilderness (wyższy `terrainDef` i `terrainSpeed`).

### Pełny flow eksplozji

```python
def detonate(center, blastRadius):
    affectedLandTiles = get_land_tiles_in_radius(center, blastRadius)

    # 1. Natychmiastowe straty populacji (zanim kafelki zmienią właściciela)
    apply_nuke_population_damage(center, blastRadius)

    # 2. Zniszczenie budynków
    apply_nuke_buildings(center, blastRadius)

    # 3. Kafelki stają się pustkowiem
    apply_nuke_ownership(affectedLandTiles)

    # 4. Zmiana terenu na fallout-*
    for tile in affectedLandTiles:
        apply_terrain_change(tile)

    # 5. Przebudowa grafów
    rebuild_logistics_graphs()
    rebuild_electric_grids()

    # 6. Notyfikacja graczy
    broadcast_nuke_event(center, blastRadius)
```

## Obrona przeciwrakietowa (contra wodorowe)

| Poziom | Zasięg | Szansa zestrzelenia | Koszt złota | Pobór mocy |
|---|---|---|---|---|
| 1 | 80 | 35% | 4000 | 15 |
| 2 | 150 | 60% | 10000 | 30 |
| 3 | 250 | 80% | 25000 | 60 |

Multiplikatywna kumulacja wielu obron na trajektorii:

```
P(przeżycie) = ∏ (1 - P(intercept_i))
```

## Obrona przeciwlotnicza (contra samoloty)

Szczegóły w [13-modul-powietrzny.md](13-modul-powietrzny.md).

| Poziom | Zasięg | Szansa/tick | Koszt złota | Pobór mocy |
|---|---|---|---|---|
| 1 | 30 | 8% | 2000 | 8 |
| 2 | 50 | 15% | 5000 | 18 |
| 3 | 80 | 25% | 12000 | 35 |

## Fallout — pełna charakterystyka

| Aspekt | Zachowanie |
|---|---|
| **Własność** | Niczyje (ownerId = 0) — pustkowie |
| **Terrain dla walki** | FalloutPlains / FalloutHighlands / FalloutMountains |
| **terrainDef** | 3.0 / 8.0 / 4.0 (propozycje) |
| **terrainSpeed** | 35 / 50 / 45 (propozycje) |
| **Budynki** | Zniszczone, nie można budować dopóki isFallout |
| **Znikanie** | TYLKO przez zajęcie — po odzyskaniu terrain → bazowy, isFallout = false |
| **Globalny falloutRatio** | Wpływa na `falloutDef` w wzorach walki (im więcej falloutu globalnie, tym mniejszy dodatkowy debuff) |

> ⚠️ Fallout **nie znika sam z siebie**. Jedyna droga do oczyszczenia to zajęcie kafelka przez dowolną encję.

## Porównanie atomowej vs wodorowej

| Aspekt | Atomowa (bombowiec) | Wodorowa (silos) |
|---|---|---|
| Koszt uranu | 50 | 150 |
| Koszt złota | 4000 + lotnisko + bombowiec | 18000 + silos |
| Czas produkcji | ~2.5 min | ~6 min |
| Promień rażenia | 20 kafelków | 45 kafelków |
| Zestrzelenie | Myśliwce + AA | Obrona przeciwrakietowa |
| Infrastruktura | Lotnisko blisko celu (ryzyko!) | Silos gdziekolwiek |

## Otwarte pytania / TBD

- [ ] Promienie rażenia *(propozycja: 20 / 45 kafelków)*
- [ ] Wartości terrainDef/terrainSpeed dla fallout-* *(propozycje powyżej)*
- [ ] Czy strata populacji powinna być multiplicatywna czy addytywna per prowincja? *(decyzja: proporcjonalna — patrz wzór)*
- [ ] Cooldown silosu *(propozycja: 3000 ticków)*
- [ ] Czy eksplozja nad wodą (centrum promienia na wodzie) nadal zadaje obrażenia pobliskim prowincjom? *(propozycja: TAK, jeśli lądowe kafelki są w promieniu)*
- [ ] Czy lotniskowiec może zostać zniszczony przez bombę jądrową? *(propozycja: TAK — traktowany jak okręt z wysokim HP, eksplozja zadaje mu obrażenia jeśli jest w promieniu)*
