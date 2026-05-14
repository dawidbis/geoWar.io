# 13 — Moduł powietrzny

## Wprowadzenie

Moduł powietrzny obejmuje:

- **Lotnisko** — budynek lądowy bazujący samoloty i generujący cargo.
- **Lotniskowiec** — może też bazować samoloty (patrz [12-modul-morski.md](12-modul-morski.md)).
- **Myśliwiec** — autonomiczna jednostka obronna.
- **Bombowiec** — jednorazowy, niesie bombę atomową.
- **Transport cargo lotniczy** — fizyczny samolot handlowy.
- **Obrona przeciwlotnicza** — zestrzeliwuje wszystkie samoloty.

## Lotnisko

### Charakterystyka

| Pole | Wartość |
|---|---|
| Powierzchnia | 10×10 |
| Wymaga prądu | TAK |
| Można levelować | TAK (1–3) |

| Poziom | Koszt złota | Cargo (co N tick) | Maks. myśliwców | Maks. bombowców | Pobór mocy |
|---|---|---|---|---|---|
| 1 | 3000 | 300 ticków | 3 | 1 | 15 |
| 2 | 8000 | 180 ticków | 6 | 2 | 30 |
| 3 | 18000 | 110 ticków | 10 | 4 | 60 |

### Strefa patrolu

| Poziom | Promień (kafelki) |
|---|---|
| 1 | 80 |
| 2 | 140 |
| 3 | 220 |

Wszystkie zasięgi liczone od centrum lotniska. Dla **lotniskowca** — od centrum prowincji morskiej.

## Myśliwiec (Fighter)

### Produkcja

- Budowany w **lotnisku** lub **lotniskowcu**.
- Koszt: **1500 złota + 20 zaopatrzenia**, czas: 800 ticków.
- Home base = lotnisko lub lotniskowiec, w którym powstał (może być zmieniony).

### Autonomia (state machine)

```
AT_BASE
    ↓ wykryto wrogi samolot w strefie patrolu
ENGAGING
    ↓ cel zniszczony / poza strefą / niski health
RETURNING → AT_BASE
```

Domyślnie myśliwce **patrolują** (auto-reagują na wszystko w strefie). Zasięg patrolu = zasięg bazy.

### Eskorta bombowca

- Trigger: bombowiec startuje z tej samej bazy → wolne myśliwce dołączają.
- Eskorta aktywna nad własnym lub pakt-mate terytorium.
- Na granicy sojuszniczego terytorium — myśliwce zawracają.

### Walka powietrzna

| Jednostka | Health | Damage/tick | Speed | Zasięg uzbrojenia |
|---|---|---|---|---|
| Myśliwiec | 80 | 4 (vs myśliwiec) | 2 kafelki/tick | 5 kafelków |
| Myśliwiec vs Bombowiec | 80 | 6 | 2 | 5 |
| Myśliwiec vs Cargo | 80 | 8 | 2 | 5 |
| Bombowiec | 60 | 0 (bezbronny) | 1.5 | — |
| Cargo | 30 | 0 | 1.2 | — |

```python
def tick_air_combat(fighter, target):
    if distance(fighter, target) <= AIR_WEAPONS_RANGE:
        fighter.health -= AIR_DAMAGE_FROM(target)
        target.health -= AIR_DAMAGE_FROM(fighter)
        if fighter.health <= 0: fighter.state = SHOT_DOWN
        if target.health <= 0: target.state = SHOT_DOWN
    else:
        move_toward(fighter, target)
```

## Bombowiec (Bomber)

### Charakterystyka

Jednorazowa jednostka do zrzucenia **bomby atomowej** (patrz [09-bron-strategiczna.md](09-bron-strategiczna.md)).

| Parametr | Wartość |
|---|---|
| Health | 60 |
| Uzbrojenie | Brak (bezbronny) |
| Speed | 1.5 kafelki/tick |
| Może być zestrzelony | TAK (myśliwce + AA) |

### Misja

1. Gracz wskazuje **kafelek docelowy** → bombowiec startuje.
2. Lot po linii prostej.
3. Eskorta myśliwców nad własnym/pakt-mate terytorium.
4. Może być zestrzelony przez wrogie myśliwce i AA po drodze.
5. Dotarcie do celu → eksplozja (patrz [09-bron-strategiczna.md](09-bron-strategiczna.md)).
6. Bombowiec zawsze ginie po zrzucie (jednorazowy).

> ❌ **Nie można odwołać** bombowca po starcie.

## Transport cargo lotniczy (Air Cargo)

Fizyczny samolot kursujący między lotniskami lub lotniskowcami po **linii prostej**.

| Parametr | Wartość |
|---|---|
| Health | 30 |
| Speed | 1.2 kafelki/tick |
| Trasa | Linia prosta (brak pathfindingu) |
| Wartość bazowa | 30 złota (propozycja) |

- Może być zestrzelony przez wrogie myśliwce lub AA.
- Zniszczenie = strata cyklu handlowego.
- **Brak piractwa** (nic fizycznego do przejęcia w powietrzu).

### Dobór celu cargo

Priorytet: pakt gospodarczy → pakt handlowy → neutralny. Nigdy wróg.

Cel może być **lotniskiem lądowym lub lotniskowcem** (jeśli lotniskowiec sojusznika/partnera).

## Obrona przeciwlotnicza (Anti-Aircraft)

Zestrzeliwuje **wszystkie samoloty**. **Nie zestrzeliwuje rakiet wodorowych** (od tego: obrona przeciwrakietowa).

| Poziom | Zasięg | Szansa/tick | Koszt złota | Pobór mocy |
|---|---|---|---|---|
| 1 | 30 | 8% | 2000 | 8 |
| 2 | 50 | 15% | 5000 | 18 |
| 3 | 80 | 25% | 12000 | 35 |

```python
def tick_aa_defense(aa, planes):
    for plane in planes:
        if not plane.is_hostile_to(aa.ownerId):
            continue
        if distance(aa, plane) <= aa.range:
            if random() < aa.shotDownChance:
                plane.health = 0
                plane.state = SHOT_DOWN
```

AA strzela **stale, bez cooldownu** w każdym ticku dla każdego wrogiego samolotu w zasięgu.

## Walka powietrzna — mechanika ogólna

- Myśliwce patrolują automatycznie wokół swojej bazy (lotnisko lub lotniskowiec).
- **Trigger startu:** wrogi samolot w strefie patrolu.
- **Spatial hashing** samolotów — detekcja O(1) per tick.
- **Nie walczą z jednostkami morskimi ani lądowymi** — tylko z innymi samolotami.

## Samoloty a lotniskowiec

Lotniskowiec działa jako baza dla samolotów identycznie jak lotnisko, z różnicą:

- Zasięg patrolu liczony od **centrum prowincji morskiej** lotniskowca.
- Samoloty bazowane na lotniskowcu mogą latać dalej w morze.
- Gdy lotniskowiec tonie — samoloty bez bazy **giną** (brak miejsca do lądowania).

## Otwarte pytania / TBD

- [ ] Promień patrolu lotniska *(propozycja: 80/140/220)*
- [ ] Zasięg uzbrojenia myśliwca *(propozycja: 5 kafelków)*
- [ ] Speed samolotów *(propozycja: 2 / 1.5 / 1.2)*
- [ ] Wartość bazowa cargo *(propozycja: 30)*
- [ ] Czy bombowiec leci prosto czy unika obrony? *(propozycja: prosto w v1)*
- [ ] Czy myśliwce eskortują nad wrogim terytorium? *(propozycja: NIE — zawracają na granicy)*
- [ ] Czy AA ma cooldown? *(propozycja: NIE)*
- [ ] Czy cargo z lotniskowca do lotniskowca generuje złoto? *(propozycja: TAK, jak normalne)*
- [ ] Czy samoloty z lotniskowca mogą atakować lotniskowiec wroga? *(propozycja: NIE — samoloty vs samoloty tylko)*
