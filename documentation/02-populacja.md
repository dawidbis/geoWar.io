# 02 — Populacja

## Filozofia

Populacja jest **liczona osobno per gracz, per prowincja**. Skoro prowincje mogą być współdzielone (patrz [01-mapa-i-prowincje.md](01-mapa-i-prowincje.md)), każdy gracz z kafelkami w prowincji ma w niej swoją **niezależną sub-populację**.

## Struktura danych

```cpp
struct ProvincePopulation {
    float workers;          // robotnicy gracza w tej prowincji
    float military;         // wojsko gracza w tej prowincji
    float maxCap;           // limit (z kafelków gracza + jego miast + bonus dominacji)
    float workerSplitRatio; // [0.0–1.0] — % nowej populacji idącej na robotników
};
```

`ProvincePopulation` jest częścią `PlayerProvinceShare` (patrz [01-mapa-i-prowincje.md](01-mapa-i-prowincje.md)).

## Kategorie populacji

| Kategoria | Funkcja |
|---|---|
| **Robotnicy** | Praca w fabrykach (zaopatrzenie), elektrowniach (prąd), wpływ na szybkość budowy. |
| **Wojsko** | Atak i obrona. Pobierane przy wysyłaniu ataku. Ginie definitywnie. |

**Handel nie wymaga populacji** — odbywa się automatycznie na infrastrukturze.

> ❗ **Gracz NIE może ręcznie przesuwać** istniejących mieszkańców między rolami. Tylko **nowo dochodząca populacja** trafia do roli wg `workerSplitRatio`.

### Wpływ robotników na produkcję budynku

Każdy budynek produkcyjny (fabryka, elektrownia) ma:

- `requiredWorkers` — minimum
- `optimalWorkers` — 100% wydajności
- `maxUsefulWorkers` — powyżej dalsze przydzielanie nic nie daje

**Wzór efektywności:**

```python
def efficiency(workers, required, optimal, max_useful):
    if workers < required:
        return workers / required * 0.5
    elif workers <= optimal:
        return 0.5 + 0.5 * (workers - required) / (optimal - required)
    elif workers <= max_useful:
        return 1.0 + 0.2 * (workers - optimal) / (max_useful - optimal)
    else:
        return 1.2  # cap
```

Robotnicy są **automatycznie alokowani** do budynków w prowincji wg priorytetów (TBD: gracz może modyfikować priorytety per budynek w UI).

### Wpływ robotników na szybkość budowy

```python
def construction_speed_multiplier(workers_assigned, required):
    return clamp(workers_assigned / required, 0.5, 1.5)
```

Propozycja: domyślnie `required = 100` na budynek w budowie.

## Przyrost populacji

Wzór z openfront.io, stosowany **per gracz, per prowincja**, każdy tick:

$$
toAdd = \left(10 + \frac{\text{currentPopulation}^{0.73}}{4}\right) \cdot \left(1 - \frac{\text{currentPopulation}}{\text{maxCap}}\right)
$$

W postaci znormalizowanej (r = currentPop / maxCap, M = maxCap):

$$
f(r) = \left(10 + \frac{(M \cdot r)^{0.73}}{4}\right) \cdot (1 - r)
$$

**Pseudokod (per gracz, per prowincja, per tick):**

```python
def tick_population_growth(share):
    current = share.population.workers + share.population.military
    cap = share.population.maxCap

    if cap <= 0:
        return  # martwa sub-prowincja

    if current >= cap:
        toAdd = max(0.0, (10 + (current ** 0.73) / 4) * (1 - current / cap))
    else:
        toAdd = (10 + (current ** 0.73) / 4) * (1 - current / cap)

    share.population.workers  += toAdd * share.population.workerSplitRatio
    share.population.military += toAdd * (1 - share.population.workerSplitRatio)
```

> ❗ Straty wojskowe w walce **nie są kompensowane bezpośrednio** — wojsko ginie, a nowi mieszkańcy dochodzą wg `workerSplitRatio` jak zwykle.

## Maksymalny cap populacji w prowincji (per gracz)

```python
def compute_max_cap(share, province):
    base = BASE_PER_TILE * len(share.ownedTileIds)
    city_bonus = sum(c.capBonus for c in share.cities if c.isActive)
    capital_multiplier = 1.25 if any(c.isCapital for c in share.cities) else 1.0
    dominance_bonus = 1.10 if has_dominance(share, province) else 1.0
    return (base + city_bonus * capital_multiplier) * dominance_bonus
```

Gdzie:

- `BASE_PER_TILE = 5` (propozycja).
- `city.capBonus` — zależne od poziomu (1000 / 2500 / 5000 / 8000 / 12000 dla lvl 1–5).
- Bonus dominacji: ×1.10 jeśli gracz ma > 50% kafelków w prowincji.
- Mnożnik stolicy: ×1.25 do bonusu z miasta-stolicy.

## Podział worker / military

### Ustawianie

Gracz w **panelu prowincji** ustawia `workerSplitRatio` ∈ [0, 1].

- `0.0` = cała nowa populacja → wojsko
- `1.0` = cała → robotnicy
- `0.5` = po połowie

**Tryby ustawiania:**

1. Per prowincja — ręcznie.
2. Zaznaczenie wielu prowincji (Ctrl+klik) — ten sam ratio dla wszystkich.
3. Globalnie — wszystkie własne sub-prowincje.

### Tryby automatyzacji

```cpp
enum class AutomationLevel : uint8_t {
    Manual,    // gracz ustawia wszystko ręcznie
    Assisted,  // automat sugeruje wartości, gracz akceptuje
    FullAuto,  // automat samodzielnie dostosowuje
};
```

**Logika `FullAuto`:**

- Sub-prowincja na froncie (graniczy z wrogimi kafelkami) → `workerSplitRatio = 0.3`.
- Sub-prowincja w głębi (bezpieczna) → `workerSplitRatio = 0.7`.
- Niedobór robotników w fabrykach lokalnych → push w stronę 0.8.
- Niskie zaopatrzenie i jest fabryka → push w stronę 0.9.

Poziom automatyzacji ustawiany jest **per sub-prowincja** (z możliwością masowej zmiany).

## Nadmiar populacji (przekroczenie capa)

Populacja gracza w prowincji **może przekroczyć cap** w wyniku transportu lub transferu.

**Konsekwencje:**

1. Przyrost naturalny zatrzymuje się.
2. Każda nadmiarowa osoba kosztuje **0.01 zaopatrzenia/tick** (propozycja).
3. Brak zaopatrzenia → migracja lub śmierć (patrz [03-zasoby.md](03-zasoby.md)).

## Transport populacji między prowincjami

### Lądowy (przez własny korytarz)

Wymaga **fizycznego połączenia logistycznego** (drogi/tory) przez prowincje **gracza** (nie pakt-mate — pakt nie daje tranzytu!).

Gracz ręcznie zleca transport (lub `FullAuto` decyduje).

Szczegóły w [07-logistyka.md](07-logistyka.md).

### Transfer do sojusznika

W ramach **paktu gospodarczego** (unia celna) gracz może **transferować** swoją populację (workers lub military) do prowincji sojusznika:

- Pobranie u dawcy → "podróż wirtualna" → pojawienie się u biorcy po N ticków.
- **Nie ma fizycznego korytarza** — to transfer.
- **W pakcie gospodarczym: darmowy.**
- **Poza paktem gospodarczym: NIEDOSTĘPNE** (bilateralny pakt handlowy nie wystarcza — tylko unia celna).
- W bilateralnym sojuszu (poza paktem gosp.) — możliwe, ale **gracz B otrzymuje** populację jako swoją; nie ma swobody przemarszu.

Szczegóły transferu w [08-dyplomacja.md](08-dyplomacja.md).

## Otwarte pytania / TBD

- [ ] Dokładne wartości `BASE_PER_TILE` i bonusów miast
- [ ] Czy `workerSplitRatio` jest ciągły czy dyskretny? *(propozycja: ciągły, UI z krokiem 0.05)*
- [ ] Koszt zaopatrzenia za nadmiar *(propozycja: 0.01/osobę/tick)*
- [ ] Czas wirtualnego transferu (pakt gospodarczy) *(propozycja: bazowy 500 ticków + 5 × dystans/100 kafelków)*
- [ ] Czy populacja ma morale? *(propozycja: NIE)*
