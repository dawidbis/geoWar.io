# 10 — Boty (AI)

## Poziomy trudności

| Poziom | Opis |
|---|---|
| **Trivial** | Pasywny, praktycznie nie atakuje |
| **Easy** | Ekspanduje na wilderness, rzadko atakuje graczy |
| **Medium** | Aktywny, buduje infrastrukturę, atakuje słabszych |
| **Hard** | Pełna strategia, moduł morski, lotniczy, broń atomowa |
| **Nightmare** | Hard + agresywna dyplomacja, pakty, koordynacja |

## Architektura decydowania

Bot wykonuje decyzje w **throttlowanych interwałach** — nie per tick, ale co N ticków dla każdej kategorii:

```cpp
struct BotDecisionSchedule {
    uint32_t attackInterval   = 50;    // co 50 ticków = 5 s
    uint32_t buildInterval    = 100;   // co 10 s
    uint32_t diplomacyInterval = 500;  // co 50 s
    uint32_t strategyInterval = 1000;  // co 100 s (1.67 min)
    uint32_t navalInterval    = 80;    // co 8 s
    uint32_t airInterval      = 80;    // co 8 s
};
```

Każda decyzja jest **niezależna** — bot może podjąć decyzję budowlaną niezależnie od decyzji ataku.

## Threat Level

Bot oblicza `threatLevel` per gracz/encja:

```python
def compute_threat(bot, target):
    militaryRatio = target.totalMilitary / max(bot.totalMilitary, 1)
    territoryRatio = target.totalTiles / max(bot.totalTiles, 1)
    isBorderNeighbor = any(borders(bot, target, prov) for prov in bot.provinces)
    return militaryRatio * 0.5 + territoryRatio * 0.3 + isBorderNeighbor * 0.2
```

## Zachowania per poziom

### Trivial

- `workerSplitRatio = 0.9` — prawie wszystko na robotników.
- Ekspanduje na wilderness jeśli graniczy, nigdy nie atakuje graczy.
- Nie buduje nic poza miastami.
- Brak dyplomacji.
- Brak modułu morskiego i lotniczego.

### Easy

- `workerSplitRatio = 0.7`.
- Atakuje wilderness i **tylko graczy z bardzo małym terytorium**.
- Buduje fabryki i drogi.
- Brak dyplomacji aktywnej (akceptuje propozycje, nie inicjuje).
- Brak modułu morskiego i lotniczego.

### Medium

- `workerSplitRatio = 0.6`.
- Atakuje słabszych sąsiadów (threatLevel < 0.8).
- Buduje pełną infrastrukturę lądową.
- Może **proponować bilateralne sojusze** i pakty handlowe.
- Podstawowy moduł morski (buduje port, kilka okrętów jeśli przybrzeżny).
- Brak modułu powietrznego.

### Hard

- `workerSplitRatio = 0.55`.
- Agresywna ekspansja, wielokierunkowe ataki.
- Pełna infrastruktura lądowa + morska + powietrzna.
- **Może aplikować do paktów multilateralnych** (militarnych i gospodarczych).
- **Buduje i używa bomby atomowe** — targetuje stolice i skupiska populacji.
- **Buduje lotniskowce** jeśli ma port lvl 2+.
- Rozbudowane threat assessment.

### Nightmare

- Wszystko z Hard, plus:
- **Aktywnie inicjuje pakty multilateralne** (zakłada je i rekrutuje inne boty/graczy).
- **Koordynuje ataki** z sojusznikami (jeśli w pakcie militarnym).
- **Atakuje natychmiast** po rozbiciu paktu wroga (okno okazji).
- Buduje rakiety wodorowe.
- Threat Level recalculowany co 200 ticków (częściej niż inne).

## Logika lądowa

### Wybór celów ataku

```python
def choose_attack_targets(bot):
    candidates = []
    for prov in bot.borderProvinces:
        for neighbor in prov.neighborProvinces:
            if is_wilderness(neighbor):
                candidates.append((neighbor, priority=10))
            elif is_enemy(bot, neighbor.owner) and threatLevel(bot, neighbor.owner) < 1.2:
                candidates.append((neighbor, priority=5))
    # Sortuj po priorytecie i bliskości stolicy wroga
    return sorted(candidates, key=lambda x: -x.priority)
```

### Przyrost wojska

Bot dynamicznie dostosowuje `workerSplitRatio`:

```python
def adjust_worker_split(bot):
    if bot.isUnderAttack:
        return 0.3  # więcej wojska
    if bot.hasLargeMilitary and not bot.isExpanding:
        return 0.8  # więcej robotników
    return bot.defaultWorkerSplit
```

### Zaopatrzenie

Bot zawsze utrzymuje magazyny na poziomie > 30% pojemności — jeśli spada, przerywa ataki i czeka na uzupełnienie.

## Logika morska (Medium+)

### Port

- Jeśli bot ma prowincję przybrzeżną → buduje port lvl 1 (Medium+), lvl 2+ (Hard+).
- Po wybudowaniu portu → buduje okręty wojenne do limitu.
- Medium: 2–3 okręty. Hard+: pełny limit.

### Lotniskowiec (Hard+)

- Jeśli bot ma port lvl 2+ i złoto > 25000 → buduje lotniskowiec.
- Po zbudowaniu → przypisuje do prowincji morskiej sąsiadującej z wrogiem lub kluczowej handlowo.
- Przypisuje 2–3 okręty jako eskortę.

### Decyzje morskie

```python
def naval_decision(bot):
    if bot.hasCarrier:
        # Sprawdź czy prowincja morska sąsiaduje z wrogiem
        threat_sea_provinces = find_contested_sea_provinces(bot)
        if threat_sea_provinces:
            move_carrier_to(bot.carrier, threat_sea_provinces[0])
        # Rekrutuj okręty z lotniskowca
        if bot.carrier.warshipCount < bot.carrier.maxWarships:
            build_warship_from_carrier(bot.carrier)
    elif bot.hasPort:
        # Buduj okręty do limitu portu
        if count_warships(bot) < port_warship_limit(bot.port):
            build_warship(bot.port)
```

## Logika powietrzna (Hard+)

### Lotnisko

- Hard+ buduje lotnisko w prowincji z dużą populacją lub blisko frontu.
- Po wybudowaniu → buduje myśliwce do limitu.

### Bomba atomowa (Hard+)

```python
def atomic_decision(bot):
    if bot.uranium >= 50 and bot.gold >= 4000:
        # Targetuj: stolica wroga z dużą populacją, duże miasto przy froncie
        target = find_best_nuke_target(bot)
        if target and distance_to_airport(bot, target) <= BOMBER_RANGE:
            build_atomic_bomb(bot.airport)
            queue_bomber_mission(bot.airport, target)
```

### Rakieta wodorowa (Nightmare)

```python
def hydrogen_decision(bot):
    if bot.uranium >= 150 and bot.gold >= 18000:
        target = find_best_hydrogen_target(bot)  # duże skupisko, stolica
        if target:
            build_hydrogen_missile(bot.silo)
            queue_missile_launch(bot.silo, target)
```

## Dyplomacja botów

### Easy/Medium — pasywna

- **Akceptuje** propozycje bilateralnych sojuszy i paktów handlowych od graczy.
- Nigdy nie inicjuje.
- Wypisuje się z sojuszu tylko jeśli sojusznik atakuje.

### Hard — aktywna bilateralna

- Może **proponować bilateralne** sojusze graczom z niskim threatLevel.
- **Aplikuje do istniejących paktów multilateralnych** (jeśli widzi że gracze są w pakcie i nie jest wrogiem żadnego z nich).
- Zawsze głosuje TAK na przyjęcie nowych botów do paktu jeśli threatLevel nowego bota < 0.5.

### Nightmare — inicjuje pakty

- **Zakłada pakty militarne i gospodarcze** jeśli ma złoto i sąsiadów o niskim threatLevel.
- Aktywnie **rekrutuje** graczy i inne boty do swoich paktów.
- **Rozgrywa dyplomację agresywnie**: obserwuje pakty wrogów, stara się je rozbijać przez propozycje bilateralne kierowane do ich członków.

```python
def diplomacy_decision(bot):
    if bot.level >= NIGHTMARE:
        if not bot.militaryPact:
            candidates = find_low_threat_neighbors(bot)
            if len(candidates) >= 2:
                create_military_pact(bot, candidates[:3])

        for enemy_pact in get_enemy_pacts(bot):
            weakest_member = min(enemy_pact.members, key=lambda m: m.totalTiles)
            if threatLevel(bot, weakest_member) < 0.6:
                propose_bilateral_alliance(bot, weakest_member)  # kusi do opuszczenia paktu
```

## Debuff bota w walce

Boty mają stały debuff dla gracza atakującego bota:

```python
def botDebuff(isDefenderBot):
    return 0.8 if isDefenderBot else 1
```

Oznacza to, że atakowanie botów jest łatwiejsze niż atakowanie graczy tej samej wielkości.

## Otwarte pytania / TBD

- [ ] Wartości throttlowania decyzji *(propozycje powyżej)*
- [ ] Dokładny próg "duże złoto" dla lotniskowca *(propozycja: > 25000)*
- [ ] Czy boty reagują na ataki nuklearne specjalną logiką? *(propozycja: NIE w v1 — po prostu kontynuują strategię)*
- [ ] Czy boty mogą transferować populację do sojuszników? *(propozycja: TAK dla Hard+)*
- [ ] Balans — docelowo boty nie powinny być gorsze od przyzwoitego gracza na Medium+ *(wymaga playtestów)*
