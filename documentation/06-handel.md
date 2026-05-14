# 06 — Handel

## Filozofia

Handel generuje złoto — to drugie (po bazowym przychodzie) główne źródło bogacenia. **Nie wymaga populacji** (jest abstrakcyjny — transporty są wirtualne dla handlu lądowego; fizyczne dla morskiego/lotniczego, ale i tak nie potrzebują populacji do działania).

Gra ma **trzy kanały handlowe**, każdy z własną dynamiką:

| Kanał | Generator | Cel | Trasa | Fizyczność |
|---|---|---|---|---|
| **Lądowy** | Fabryka | Miasta podłączone drogami | BFS po grafie miast | Wirtualny (wizualizacja) |
| **Morski** | Port | Inny port | A* po kafelkach wodnych | **Fizyczny** (statki cargo) |
| **Lotniczy** | Lotnisko | Inne lotnisko | Linia prosta | **Fizyczny** (samolot cargo) |

## Hierarchia dochodowości

Od najniższego do najwyższego:

1. **Handel wewnętrzny** (między własnymi miastami/fabrykami) — niski.
2. **Handel międzygraczowy bez paktu** — średni.
3. **Handel z paktem handlowym** — wysoki.
4. **Handel w pakcie gospodarczym (unia celna)** — najwyższy.

## Handel lądowy

### Mechanika

Każda **fabryka** generuje transporty handlowe kursujące między fabryką a podłączonymi miastami przez sieć dróg/torów. Po dotarciu transport **generuje złoto** dla właściciela fabryki.

```
goldEarned = baseTransportValue * distanceFactor * partnerTypeFactor * roadLevelFactor * unionFactor
```

### Reguły zasięgu (zależne od poziomu fabryki)

| Poziom fabryki | Maks. hopów od miasta startowego |
|---|---|
| 1 | 1 (fabryka → miasto → 1 hop dalej) |
| 2 | 2 |
| 3 | 3 |
| 4 | 4 |
| 5 | brak limitu |

"Hop" = przejście do kolejnego miasta-węzła połączonego drogą.

### Wyznaczanie tras

```python
def compute_land_trade_routes(factory):
    maxHops = FACTORY_MAX_HOPS[factory.level]
    initialCity = nearest_connected_city(factory)
    if not initialCity:
        return []

    routes = []
    queue = [(initialCity, [initialCity], 0)]
    visited = {initialCity.id}

    while queue:
        city, path, depth = queue.pop(0)
        routes.append(path)
        if depth >= maxHops:
            continue
        for neighbor in connected_cities_via_roads(city):
            if neighbor.id not in visited:
                visited.add(neighbor.id)
                queue.append((neighbor, path + [neighbor], depth + 1))

    return routes
```

### Generowanie transportów

Fabryka co N ticków emituje transport (propozycja: co 100 ticków = 10 s).

- Transport jest **wirtualny** — widoczny na drogach, ale nie jest obiektem fizycznym.
- Wybiera trasę najdłuższą dostępną (preferencja dochodowości).
- Wraca do fabryki po dotarciu.
- Po powrocie generuje złoto.

### Wpływ poziomu drogi

| Poziom | Czas cyklu | Mnożnik wartości |
|---|---|---|
| 1 (droga) | bazowy | ×1.0 |
| 2 (tory) | ×0.55 | ×1.3 |
| 3 (tory szybkie) | ×0.33 | ×1.6 |

## Handel morski

### Mechanika

Każdy **port** generuje **fizyczne** transporty cargo morskie kursujące do portów innych graczy (preferencyjnie z paktu gospodarczego, potem neutralnych).

Szczegóły w [12-modul-morski.md](12-modul-morski.md). Najważniejsze tu:

- Wartość bazowa: `baseSeaTransportValue = 25` (propozycja, vs 10 lądowego).
- Transport jest **fizyczny** — może być piratowany lub zniszczony przez wrogie okręty.
- Zniszczenie = strata cyklu handlowego.
- Piractwo = utrata cyklu + pirat zarabia 50% wartości.

### Wybór celu

Algorytm dobiera **port docelowy** wg:

1. Port w pakcie gospodarczym (najwyższa wartość).
2. Port w pakcie handlowym bilateralnym.
3. Neutralny port (ale tylko jeśli brak embargo/wojny).
4. **Nigdy** port wroga (nie kursuje).

Pathfinding A* po kafelkach wodnych. Wynik cache'owany per para portów.

## Handel lotniczy

### Mechanika

Każde **lotnisko** generuje **fizyczne** transporty cargo lotnicze kursujące do innych lotnisk po **linii prostej**.

Szczegóły w [13-modul-powietrzny.md](13-modul-powietrzny.md). Najważniejsze tu:

- Wartość bazowa: `baseAirCargoValue = 30` (propozycja, najwyższa).
- Bonus szybkości — krótsze cykle = większy dochód/min.
- Cargo może być **zestrzelone** (myśliwce wroga, obrona przeciwlotnicza).
- Zniszczenie = utrata cyklu, **brak piractwa** (nic do przejęcia w powietrzu).

### Wybór celu

Identycznie jak morski — priorytet pakt-mate gospodarczy → handlowy → neutralny.

Brak pathfindingu — zawsze linia prosta. Ryzyko zestrzelenia rośnie z dystansem (więcej obron po drodze).

## Mnożniki dochodu — wspólne

### partnerTypeFactor

| Relacja z odbiorcą | partnerTypeFactor |
|---|---|
| Własne miasto/port/lotnisko | 1.0 |
| Neutralny (bez paktu) | 1.5 |
| Pakt handlowy bilateralny | 2.0 |
| Unia celna / pakt gospodarczy | 2.5 |

### Cło

| Relacja | Cło (% wartości dla właściciela celu) |
|---|---|
| Własne (wewnętrzne) | 0% |
| Neutralny | 20% |
| Pakt handlowy | 5% |
| Unia celna | 0% |

```python
def distribute_gold(transport):
    gold = compute_gold_earned(transport)
    if transport.destinationOwnerId == transport.factoryOwnerId:
        # Własny handel
        give_gold(transport.factoryOwnerId, gold)
    else:
        tariff = TARIFF_RATES[relation_type(transport.factoryOwnerId, transport.destinationOwnerId)]
        give_gold(transport.factoryOwnerId, gold * (1 - tariff))
        give_gold(transport.destinationOwnerId, gold * tariff)
```

### unionFactor

Dodatkowy ×1.2 dla tras pełnie w obrębie unii celnej (wszystkie miasta/porty/lotniska po trasie należą do członków paktu gospodarczego).

## Embargo i wojna

- **Wojna** automatycznie zatrzymuje handel między stronami. Transporty już w drodze **giną** (handel morski/lotniczy) lub nie generują dochodu (lądowy).
- **Embargo** (jednostronne) — transporty embargującej strony nie kursują, ale druga strona może (jeśli sama nie nałożyła embarga).
- **Zamknięte granice** — handel lądowy zostaje przerwany dla tranzytu.

## Otwarte pytania / TBD

- [ ] Tuning wszystkich wartości
- [ ] Czy fabryka może mieć **preferowane miasta-cele** (whitelist)? *(propozycja: NIE — automat, ale gracz może "blacklistować" cele)*
- [ ] Co się dzieje z lądowym transportem "w drodze" przy zmianie statusu wojny? *(propozycja: ginie, fabryka traci cykl)*
- [ ] Czy handel morski preferuje krótsze/dłuższe trasy? *(propozycja: najwyższy ROI = dystans × partnerType / czas)*
- [ ] Czy wartości cła powinny być **negocjowalne**? *(propozycja: NIE w v1, stałe stawki)*
- [ ] Czy gracz może w pakcie gospodarczym **ustawić priorytet** swoich transportów (do innych pakt-mate czy do siebie)? *(propozycja: NIE — automat decyduje optymalnie)*
