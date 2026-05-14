# 01 — Mapa i prowincje

## Struktura hierarchiczna

```
Mapa
 ├── Prowincje lądowe (zmienny rozmiar)
 │    └── Kafelki lądowe (2×2 px)
 └── Prowincje morskie (DUŻO większe od lądowych)
      └── Kafelki wodne (2×2 px)
```

### Kafelek (Tile)

Najmniejsza jednostka terenu.

| Pole | Typ | Opis |
|---|---|---|
| `id` | `uint32` | Globalne ID |
| `x, y` | `uint16` | Pozycja |
| `landProvinceId` | `uint32` | ID prowincji lądowej (0 jeśli wodny) |
| `seaProvinceId` | `uint32` | ID prowincji morskiej (0 jeśli lądowy) |
| `terrain` | `TerrainType` | Patrz niżej |
| `ownerId` | `uint32` | ID encji kontrolującej kafelek (0 = niczyje / wody zawsze 0) |
| `isFallout` | `bool` | Czy jest skażony |
| `buildingId` | `uint32?` | Opcjonalne |

**Typy terenu:**

```cpp
enum class TerrainType : uint8_t {
    Plains            = 0,
    Highlands         = 1,
    Mountains         = 2,
    ShallowWater      = 3,  // wody przybrzeżne — można budować port
    DeepWater         = 4,  // ocean — tylko ruch jednostek
    // Fallout-mapowane warianty (powstają po bombach)
    FalloutPlains     = 5,
    FalloutHighlands  = 6,
    FalloutMountains  = 7,
};
```

Typy fallout-* są **wynikiem uderzenia jądrowego** — terrain bazowy zostaje zamieniony na swój fallout-wariant. Szczegóły w [09-bron-strategiczna.md](09-bron-strategiczna.md).

**Reguły dla wód:**

- **ShallowWater** — pas wzdłuż wybrzeża. Można budować **porty**.
- **DeepWater** — otwarte oceany. Tylko ruch jednostek.
- **Kafelki wodne należą do prowincji morskiej**, ale są **zawsze neutralne** (`ownerId = 0`).
- **Kontrola morza** = obecność lotniskowca + okrętów w danej prowincji morskiej, nie własność kafelków.

## Prowincja lądowa (LandProvince)

Klasyczna jednostka logistyczna. Składa się wyłącznie z kafelków lądowych.

| Pole | Typ | Opis |
|---|---|---|
| `id` | `uint32` | Globalne ID |
| `name` | `string` | Nazwa |
| `tileIds` | `vector<uint32>` | Lista kafelków lądowych |
| `playerShares` | `map<entityId, PlayerProvinceShare>` | Per gracz: jego sub-prowincja |
| `neighborLandProvinces` | `vector<ProvinceAdjacency>` | Sąsiednie prowincje lądowe |
| `coastalSeaProvinceIds` | `vector<uint32>` | Prowincje morskie graniczące |

```cpp
struct PlayerProvinceShare {
    uint32_t entityId;
    vector<uint32_t> ownedTileIds;
    ProvincePopulation population;
    vector<uint32_t> buildingIds;
    float supplyStored;
    float workerSplitRatio;
    AutomationLevel automationLevel;
};
```

## Prowincja morska (SeaProvince)

**Znacznie większa** od lądowej. Służy do:

- Grupowania kafelków wodnych dla pathfindingu.
- **Przypisania lotniskowców** — każdy lotniskowiec jest "zakotwiczony" w jednej prowincji morskiej.
- Ułatwienia rozgrywki morskiej (nie trzeba klikać na konkretne kafelki).

| Pole | Typ | Opis |
|---|---|---|
| `id` | `uint32` | Globalne ID |
| `name` | `string` | Nazwa (np. "Morze Środkowe") |
| `tileIds` | `vector<uint32>` | Lista kafelków wodnych |
| `centerX, centerY` | `float` | Środek geometryczny — punkt referencyjny dla lotniskowca |
| `neighborSeaProvinces` | `vector<uint32>` | Sąsiednie prowincje morskie |
| `coastalLandProvinceIds` | `vector<uint32>` | Lądowe prowincje graniczące (porty) |
| `terrainComposition` | `{Shallow: float, Deep: float}` | Statystyki |

**Reguły:**

- **Prowincje morskie są ZAWSZE neutralne** — nikt ich nie "posiada".
- Służą wyłącznie jako **kontener** dla wód i punkt zaczepienia dla lotniskowców.
- **Skala:** prowincja morska jest **5–15× większa** od typowej prowincji lądowej (TBD do tuningu). Mapa może mieć tylko kilka prowincji morskich.
- **Sąsiedztwo morskie:** dwie prowincje morskie są sąsiadami jeśli graniczą kafelkami lub są połączone cieśniną.

> 💡 Lotniskowiec przypisany do prowincji morskiej M **pływa wizualnie** wokół `(centerX, centerY)`, ale jego efektywna "pozycja" (środek strefy patrolu samolotów/okrętów) jest właśnie tym punktem centralnym — niezależnie od bieżącej animowanej pozycji statku. Patrz [12-modul-morski.md](12-modul-morski.md).

## ❗ Współwłasność prowincji lądowej

W tej grze **nie istnieje "główny właściciel" prowincji lądowej**.

Jeśli gracz A kontroluje choćby **jeden kafelek** w prowincji X, to dla A jest to **pełnoprawna prowincja z jednym kafelkiem**:

- A ma w niej własną sub-populację.
- A ma własny cap populacji.
- A może w niej budować na swoich kafelkach.
- A zarządza swoim `workerSplitRatio`, swoim zaopatrzeniem.

### Bonus dominacji (>50% kafelków)

Gracz kontrolujący **więcej niż połowę kafelków lądowych prowincji** otrzymuje:

- **+10% do capa populacji**,
- **+10% do produkcji budynków**.

```python
def get_dominance_bonus(province, playerId):
    totalTiles = len(province.tileIds)
    playerTiles = len(province.playerShares[playerId].ownedTileIds)
    if playerTiles > totalTiles / 2:
        return 1.10
    return 1.00
```

## Generacja mapy

**TBD — propozycja:**

1. Generacja heightmap (Perlin/Simplex noise).
2. Klasyfikacja terenu:
   - `height < seaLevel-deep` → DeepWater
   - `seaLevel-deep ≤ height < seaLevel` → ShallowWater
   - `seaLevel ≤ height < 0.3` → Plains
   - `0.3 ≤ height < 0.7` → Highlands
   - `height ≥ 0.7` → Mountains
3. **Wyznaczenie prowincji morskich** — flood-fill po kafelkach wodnych z bardzo dużymi celami (np. 5000–20000 kafelków na prowincję morską). Mniejsze "morza wewnętrzne" mogą być osobnymi prowincjami nawet jeśli mniejsze.
4. **Podział lądu na prowincje** algorytmem Voronoi (z perturbacją). Liczba prowincji lądowych proporcjonalna do liczby graczy (np. ~8–10× liczba graczy).
5. Wyznaczenie sąsiedztw (graph adjacency dla lądowych i morskich).
6. Wyznaczenie przybrzeżności (`coastalSeaProvinceIds` dla lądowych, `coastalLandProvinceIds` dla morskich).
7. Spawn pointy — preferencyjnie ~70% przybrzeżnych.

## Sąsiedztwo prowincji lądowych

```cpp
struct ProvinceAdjacency {
    uint32_t neighborProvinceId;
    uint32_t sharedBorderTilesCount;  // wpływa na rozmiar frontu w walce
};
```

**Prowincje rozdzielone wodą** — nie są sąsiadami lądowymi. Wymagana komunikacja przez moduł morski.

## Wpływ kafelków na rozgrywkę

- **Kontrolowany kafelek lądowy** → zwiększa cap populacji w prowincji (`base_per_tile = 5`).
- **Globalna liczba kafelków lądowych encji** → modyfikatory walki (patrz [05-walka.md](05-walka.md)).
- **Terreny** → wpływ na efficiency i speed ataku.
- **Kafelki zajęte budynkami** liczą się do bazowego capa populacji.
- **Kafelki wodne** nie liczą się do żadnych statystyk terytorialnych.
- **Kafelki fallout-***  zachowują się trudniej w walce — patrz [05-walka.md](05-walka.md) i [09-bron-strategiczna.md](09-bron-strategiczna.md).

## Wilderness (teren niczyj)

Kafelki lądowe bez właściciela (`ownerId == 0`). Mogą być przejęte przez dowolną encję. Walka z wildernessem — osobne wzory.

> 💡 **Po uderzeniu jądrowym** kafelki tracą właściciela i stają się wildernessem **z dodatkową flagą `isFallout`**, co dodatkowo utrudnia ich odzyskanie.

## Fallout

Kafelek po uderzeniu jądrowym:

1. Otrzymuje **terrain** = fallout-wariant odpowiedni do bazowego (`Plains → FalloutPlains` itd.).
2. Otrzymuje flagę `isFallout = true`.
3. **Traci właściciela** (`ownerId = 0`).
4. **Wszystkie budynki na nim są zniszczone**.
5. Może być **z powrotem zajęty** zwykłym atakiem na wilderness — ale jest to **znacznie trudniejsze** (patrz [05-walka.md](05-walka.md)).
6. **Po odzyskaniu** kafelka: `isFallout` znika, terrain wraca do bazowego (FalloutPlains → Plains itd.).

> ⚠️ Fallout **nie znika sam z siebie**. Znika tylko gdy ktoś zajmie kafelek. Decyzja designerska: tworzy strategiczne pustkowia, które stają się "blizną" mapy.

## Otwarte pytania / TBD

- [ ] Liczba prowincji lądowych per mapę *(propozycja: ~8–10× liczba graczy)*
- [ ] Skala prowincji morskich *(propozycja: 5000–20000 kafelków każda)*
- [ ] Procent graczy spawniących na wybrzeżu *(propozycja: ~70%)*
- [ ] Wartość bonusu dominacji *(propozycja: +10%)*
- [ ] Czy strefa wodna może mieć typy ("morze wewnętrzne" vs "ocean")? *(propozycja: NIE w v1)*
- [ ] Czy prowincje morskie mają nazwy generowane proceduralnie? *(propozycja: TAK, lista bazowych nazw + sufiks)*
