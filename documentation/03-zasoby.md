# 03 — Zasoby

## Lista zasobów

| Zasób | Skala | Producent | Konsumenci |
|---|---|---|---|
| **Złoto** | Globalnie (per gracz) | Handel, bazowy przychód, eksport prądu | Budynki, jednostki bojowe, głowice |
| **Zaopatrzenie** | Per budynek (sumowane per gracz, per prowincja) | Fabryki | Walka, nadmiar populacji, produkcja jednostek |
| **Elektryczność** | Globalnie (per sieć elektryczna) | Elektrownie | Większość budynków |
| **Uran** | Globalnie (per gracz) | Elektrownie atomowe (Mixed/Military) | Bomby atomowe, rakiety wodorowe |

## Złoto

Globalny zasób gracza. **Nie jest przypisany do prowincji.**

### Źródła

1. **Bazowy przychód** — każda encja: `+2 złota/tick` (propozycja).
2. **Handel lądowy** — fabryki generują transporty (patrz [06-handel.md](06-handel.md)).
3. **Handel morski** — porty generują transporty morskie (patrz [12-modul-morski.md](12-modul-morski.md)).
4. **Handel lotniczy** — lotniska generują cargo (patrz [13-modul-powietrzny.md](13-modul-powietrzny.md)).
5. **Eksport prądu sojusznikowi** (sprzedaż, jeśli nie pakt gospodarczy).
6. **Piractwo morskie** — okręty wojenne przejmujące neutralne/wrogie transporty.
7. **Sprzedaż prowincji** innym graczom (patrz [08-dyplomacja.md](08-dyplomacja.md)).

### Wydatki

- Budowa i ulepszanie budynków.
- Produkcja jednostek bojowych (okręty, myśliwce, bombowce).
- Produkcja głowic (atomowe, wodorowe).
- Zakup prądu od sojusznika (jeśli poza paktem gospodarczym).
- Tworzenie paktów (5000 złota — TBD).

### Struktura

```cpp
struct EntityGold {
    int64_t amount;
    int32_t lastTickIncome;  // dla UI
};
```

> Trzymamy jako `int64_t` dla determinizmu (brak błędów zaokrągleń).

## Zaopatrzenie

Najbardziej rozproszony zasób w grze. **Nie ma globalnej puli** — istnieje **suma zaopatrzenia w budynkach gracza w danej prowincji**, plus mechanizmy transportu i wykorzystywania.

### Źródła

**Tylko fabryki.**

```
supplyPerTick = baseSupplyOutput[level] * efficiency(workers) * (1 if isPowered else 0)
```

| Poziom | `baseSupplyOutput` | Optimal workers |
|---|---|---|
| 1 | 5/tick | 50 |
| 2 | 12/tick | 120 |
| 3 | 25/tick | 250 |
| 4 | 50/tick | 500 |
| 5 | 100/tick | 1000 |

### Magazynowanie

| Budynek | Pojemność |
|---|---|
| Fabryka | 1000 × level |
| Baza wojskowa | 5000 × level |
| Punkt logistyczny | 2000 × level |
| Miasto | 500 × level |
| Inne | 0 |

**Zaopatrzenie sub-prowincji** = suma magazynów wszystkich własnych budynków gracza w tej prowincji.

### Lokalne źródła zaopatrzenia (zasięg)

Budynki dające wojsku **lokalny pull** poza granicami prowincji:

| Budynek | Zasięg (kafelki) |
|---|---|
| Baza wojskowa | 30 × level |
| Punkt logistyczny | 15 × level |
| Fabryka | 5 × level |

### "Plecak" wojska atakującego

Przy wysłaniu ataku wojsko zabiera ze sobą zapas:

```
backpackCapacity = attackingTroops * SUPPLY_PER_SOLDIER  # propozycja: 0.5
```

W walce zużycie:

```
supplyConsumptionPerTick = activeCombatTroops * COMBAT_SUPPLY_RATE  # propozycja: 0.05/tick na żołnierza
```

Brak zaopatrzenia → niska efektywność (`supplyEfficiencyMod` → 0.1).

### Transport zaopatrzenia (pull-based)

Patrz [07-logistyka.md](07-logistyka.md). Krótko: prowincja w deficycie żąda zaopatrzenia z najbliższych źródeł — system automatycznie wybiera trasę i tworzy wirtualne transporty.

## Elektryczność

**Globalna w obrębie sieci elektrycznej** — jedna z kluczowych mechanik.

### Sieć jako graf

- Każda elektrownia może być rdzeniem sieci.
- Gracz **ręcznie podłącza** budynki rysując kabel (wizualizacja, nie zajmuje kafelków).
- Z podłączonych budynków można **ciągnąć sieć dalej**.
- **Sieć może obejmować wiele prowincji.**
- Dwie elektrownie w tej samej sieci → moce się sumują.

### Zasięg podłączenia

Od węzła do nowego budynku — max **50 kafelków po linii prostej** (TBD).

### Bilans sieci

```python
def tick_electric_grid(grid):
    total_power = sum(b.powerOutput for b in grid.producers if b.isActive)
    total_demand = sum(b.powerDemand for b in grid.consumers if b.isActive)

    if total_power >= total_demand:
        for b in grid.consumers:
            b.isPowered = True
    else:
        deficit = total_demand - total_power
        # Wyłącz budynki w odwrotnej kolejności priorytetów
        for b in sorted(grid.consumers, key=lambda x: x.electricPriority):
            if deficit <= 0:
                break
            b.isPowered = False
            deficit -= b.powerDemand
```

### Domyślne priorytety wyłączania

| Budynek | Priorytet (większy = ważniejszy) |
|---|---|
| Miasto | 100 |
| Obrona przeciwlotnicza | 95 |
| Obrona przeciwrakietowa | 90 |
| Silos rakietowy | 80 |
| Baza wojskowa | 70 |
| Lotnisko | 65 |
| Port | 60 |
| Fabryka | 55 |
| Inne | 50 |

Gracz może modyfikować priorytety w UI.

### Eksport prądu sojusznikowi

Gracz może udostępnić część swojej sieci elektrycznej sojusznikowi. **Wymaga:**

- Sojusznik w **pakcie gospodarczym** (unii celnej) → **darmowy transfer** prądu (jeśli włączone w panelu energetycznym).
- Sojusznik w **bilateralnym pakcie handlowym** → **sprzedaż prądu za złoto** (jeśli włączone).
- Brak paktu handlowego → **niedostępne**.

```cpp
struct ElectricExport {
    uint32_t exporterGridId;
    uint32_t consumerPlayerId;
    bool isFree;            // jeśli pakt gospodarczy
    float pricePerMWPerTick;// jeśli sprzedaż (propozycja: 0.5 złota/MW/tick)
    float maxMW;            // limit eksportu
};
```

**Mechanika:**

- Sojusznik tworzy "wirtualny link" od swojej sieci do sieci eksportera (konfiguracja w panelu).
- Konsument dolicza eksportowaną moc do swojej puli przy bilansowaniu.
- Eksporter musi mieć **nadwyżkę** — eksport jest **ograniczony** do `total_power - own_demand`.
- Jeśli eksporter sam ma deficyt → eksport **automatycznie się zamyka**.

> ❗ Eksport prądu **wymaga aktywacji w panelu** energetycznym. Domyślnie wyłączony. Nawet w pakcie gospodarczym musisz świadomie udostępnić sieć.

## Uran

Specjalny zasób — produkowany **tylko przez elektrownie atomowe**.

```
uraniumPerTick = baseUraniumOutput[level] * efficiency * powerActive * modeMultiplier
```

| Poziom elektrowni | Baza |
|---|---|
| 1 | 0.01/tick |
| 2 | 0.025/tick |
| 3 | 0.05/tick |

**Tryby pracy:**

| Tryb | Mnożnik mocy | Mnożnik uranu |
|---|---|---|
| Civilian | ×1.0 | ×0.0 |
| Mixed | ×0.7 | ×0.5 |
| Military | ×0.3 | ×1.0 |

**Wykorzystanie:**

- **Bomba atomowa** (na bombowiec): **50 uranu** + 4000 złota.
- **Rakieta wodorowa** (silos): **150 uranu** + 18000 złota.

Szczegóły w [09-bron-strategiczna.md](09-bron-strategiczna.md).

## Otwarte pytania / TBD

- [ ] Tuning bazowego przychodu złota *(propozycja: +2/tick)*
- [ ] Tuning produkcji zaopatrzenia i magazynów
- [ ] Cena prądu poza paktem gospodarczym *(propozycja: 0.5 złota/MW/tick)*
- [ ] Zasięg podłączenia elektrycznego *(propozycja: 50 kafelków)*
- [ ] Czy elektrownia węglowa potrzebuje "paliwa"? *(decyzja: NIE)*
- [ ] Czy fabryka potrzebuje "input" surowców? *(decyzja: NIE)*
- [ ] Eksport prądu — czy limity? *(propozycja: max 50% nadwyżki — gracz nie może wyeksportować wszystkiego naraz)*
