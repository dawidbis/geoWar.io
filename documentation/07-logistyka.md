# 07 — Logistyka

## Wprowadzenie

System logistyczny obejmuje:

1. **Połączenia logistyczne** między budynkami (drogi/tory).
2. **Transport populacji** (workers/military) między prowincjami.
3. **Transport zaopatrzenia** między budynkami.
4. **Wizualizacja transportów** na mapie.

Wszystkie wewnętrzne transporty (populacja, zaopatrzenie) są **wirtualne** — to przepływy zasobów. Wartości aktualizują się **na ticku**, klient płynnie interpoluje pozycje ikonek.

> **Wyjątek:** transporty morskie i lotnicze są **fizyczne** — to oddzielne systemy w [12-modul-morski.md](12-modul-morski.md) i [13-modul-powietrzny.md](13-modul-powietrzny.md).

## Graf logistyczny

Każdy budynek zdolny do logistyki jest węzłem grafu. Każda droga/tory dotykająca dwóch budynków tworzy krawędź.

```cpp
struct LogisticsGraph {
    vector<uint32_t> nodes;
    vector<pair<uint32_t, uint32_t>> edges;
    map<uint32_t, RouteInfo> routesCache;
};
```

**Budynki będące węzłami:**

- Miasto
- Fabryka
- Baza wojskowa
- Punkt logistyczny
- Port (jako węzeł lądowy — z portem łączy się również infrastruktura logistyczna)
- Lotnisko (jw.)

**Reguły grafu:**

1. Droga/tory musi dotykać budynku (przylegać kafelkiem).
2. Dwa budynki mogą być połączone wieloma trasami → wybierana najkrótsza/najszybsza dla danego transportu.
3. **Drogi przez terytorium innego gracza są niedozwolone** dla wewnętrznej logistyki gracza — tranzyt populacji/zaopatrzenia przez cudze kafelki **nie działa**.

> ❗ **Kluczowa decyzja designerska:** żaden pakt nie pozwala na fizyczny tranzyt populacji przez terytorium sojusznika. Populacja może być **przekazana** (transfer w pakcie gospodarczym), ale nie da się "przeprowadzić" jej własnym korytarzem przez cudze kafelki. Drogi/tory są **przerywane** na granicy gracza.

## Transport populacji wewnętrzny

### Wymagania

1. Istnieje **fizyczny korytarz** dróg/torów przez prowincje **gracza** (i tylko gracza) między źródłem a celem.
2. Gracz ma wystarczającą populację w prowincji-źródle.

### Zlecanie ręczne

```python
def request_population_transport(from_prov, to_prov, population_type, amount):
    if not has_internal_route(from_prov, to_prov):
        return Error("Brak własnego korytarza")
    if from_prov.share.population[population_type] < amount:
        return Error("Niewystarczająca populacja")

    from_prov.share.population[population_type] -= amount

    transport = PopulationTransport(
        type = population_type,
        amount = amount,
        from_id = from_prov.id,
        to_id = to_prov.id,
        route = compute_route(from_prov, to_prov),
        startTick = current_tick,
        arrivalTick = current_tick + compute_travel_time(route),
    )
    register_transport(transport)
```

### Automatyczne (Assisted/FullAuto)

- System wykrywa sub-prowincje frontu z niedostatkiem wojska.
- Generuje transporty z głębi (nadwyżki) do frontu.
- Priorytet: prowincje atakowane > atakujące > inne.

### Czas transportu

```python
def compute_travel_time(route):
    total = 0
    for segment in route.segments:
        base = 0.5  # ticki/kafelek (propozycja)
        speed_mult = ROAD_SPEED_MULT[segment.road_level]   # 1.0 / 1.8 / 3.0
        logistics_mult = LOGISTICS_HUB_BONUS(segment)      # ×1.3 / 1.7 / 2.5 jeśli przepływa przez hub
        total += segment.length_in_tiles * base / (speed_mult * logistics_mult)
    return total
```

Dodatkowo: **bazy wojskowe** w prowincji-celu przyspieszają dostarczanie populacji wojskowej (mnożnik 1.2/1.5/2.0).

### Wyładunek

Po dotarciu:

```python
def deliver_population_transport(transport):
    target_share = get_share(transport.to_id, transport.owner)
    target_share.population[transport.type] += transport.amount
    # może przekroczyć cap → patrz 02-populacja.md
    unregister_transport(transport)
```

> ⚠️ Jeśli prowincja-cel została stracona — transport ginie (TBD: lub próbuje wrócić).

## Transfer populacji do pakt-mate gospodarczego

Patrz [08-dyplomacja.md](08-dyplomacja.md). Krótko: transfer **nie wymaga korytarza** — to wirtualne "przekazanie" populacji.

```python
def transfer_population_to_pact_member(donor_share, receiver_share, type, amount):
    if not in_economic_pact(donor.player, receiver.player):
        return Error("Transfer wymaga paktu gospodarczego")
    donor_share.population[type] -= amount
    # Wirtualna podróż — czas zależy od dystansu
    distance = manhattan(donor_prov.center, receiver_prov.center)
    arrivalTick = current_tick + 500 + 5 * (distance / 100)  # propozycja
    schedule_arrival(receiver_share, type, amount, arrivalTick)
```

## Transport zaopatrzenia (pull-based)

**Trigger:** sub-prowincja konsumuje zaopatrzenie i jej lokalna pula spada poniżej progu.

```python
def tick_supply_distribution(share):
    if share.totalSupply < SUPPLY_THRESHOLD_REQUEST:  # 20% pojemności
        sources = find_supply_sources(share)  # sortowane po koszcie trasy
        needed = SUPPLY_TARGET - share.totalSupply  # 80% pojemności
        for source in sources:
            transferred = min(source.availableSupply, needed)
            if transferred > 0:
                create_supply_transport(source, share, transferred)
                needed -= transferred
            if needed <= 0:
                break
```

### Wybór źródła

Sortowanie kandydatów (rosnąco po koszcie):

1. **W tej samej sub-prowincji** (inny własny budynek).
2. **W sąsiednich sub-prowincjach** tego samego gracza (przez korytarz dróg).
3. **W dalszych prowincjach gracza**, max 5 hopów (TBD).

```
costScore = travelTicks + supplyHopPenalty * numHops
```

### Lokalne zaopatrzenie (bez transportu)

Jeśli wojsko atakujące jest w **zasięgu** lokalnego źródła (baza wojskowa, punkt logistyczny, fabryka — patrz [03-zasoby.md](03-zasoby.md)) — pobiera **bezpośrednio**, bez transportu, w tym samym ticku.

## Wizualizacja transportów

```cpp
struct VisualTransport {
    uint32_t id;
    TransportType type;       // Trade, Population, Supply
    Path path;                // sekwencja kafelków
    float progressOnPath;     // 0.0 - 1.0
    float speedTilesPerTick;
    bool isReturning;
};
```

> Klient **interpoluje** pozycję między tickami (10 Hz). Serwer wysyła `progressOnPath` i `speedTilesPerTick`.

Maksymalna liczba widocznych transportów per kafelek drogi: ~5 (TBD), wyższe są agregowane wizualnie.

## Poziomy automatyzacji

Patrz [02-populacja.md](02-populacja.md). Krótko:

| Poziom | Zachowanie systemu |
|---|---|
| **Manual** | Gracz zleca transporty ręcznie. Zaopatrzenie zawsze auto. |
| **Assisted** | System sugeruje (popup), gracz akceptuje. |
| **FullAuto** | System samodzielnie zarządza. |

## Otwarte pytania / TBD

- [ ] Wartości progowe `SUPPLY_THRESHOLD_REQUEST` (20%), `SUPPLY_TARGET` (80%)
- [ ] `BASE_TICKS_PER_TILE = 0.5` — czy ok?
- [ ] Max liczba hopów dla pull-supply *(propozycja: 5)*
- [ ] Czy gracz może priorytetyzować źródła zaopatrzenia ręcznie? *(propozycja: NIE — system optymalizuje)*
- [ ] Co się dzieje z transportem gdy korytarz zostaje przerwany w połowie drogi? *(propozycja: transport "zatrzymuje się" na ostatnim własnym kafelku i czeka, max 1000 ticków; potem ginie)*
- [ ] Czas wirtualnego transferu pakt-mate gospodarczego — formuła OK?
