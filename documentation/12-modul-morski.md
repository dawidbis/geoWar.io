# 12 — Moduł morski

## Wprowadzenie

Moduł morski obejmuje:

- **Port** — budynek przybrzeżny, stocznia dla okrętów i lotniskowców.
- **Okręt wojenny** — autonomiczna jednostka bojowa.
- **Lotniskowiec** — mobilna baza morska ("pływający port + lotnisko"), przypisana do prowincji morskiej.
- **Transport handlowy morski** — fizyczne statki cargo kursujące między portami.
- **Piractwo** — okręty przechwytują wrogie/neutralne transporty.

> ❗ **Zasada izolacji domen:** interakcja ląd–morze **nie istnieje**. Okręty i lotniskowce atakują **wyłącznie inne jednostki morskie**. Port może zostać zniszczony przez atak lądowy (to budynek w prowincji lądowej), ale okręt nie może strzelać do portu. Bomby i rakiety mogą trafić okręty jeśli są w promieniu eksplozji.

## Prowincje morskie — podsumowanie

Patrz [01-mapa-i-prowincje.md](01-mapa-i-prowincje.md). Najważniejsze dla modułu morskiego:

- Prowincje morskie są **zawsze neutralne** — nikt ich nie posiada.
- **Każdy lotniskowiec jest przypisany do jednej prowincji morskiej** — to jego "baza".
- Punkt referencyjny lotniskowca = **środek prowincji morskiej** (`centerX, centerY`) — wszystkie zasięgi (okrętów, samolotów) liczone od tego punktu.
- Pływanie lotniskowca wokół centrum to **tylko wizualizacja** — mechanicznie jest "punktem".

## Port

### Charakterystyka

Port jest **budynkiem przybrzeżnym** — lądowa część stoi w prowincji lądowej, dok na ShallowWater.

| Poziom | Koszt złota | Generacja transportów (co N tick) | Maks. okrętów | Pobór mocy |
|---|---|---|---|---|
| 1 | 2500 | 200 ticków | 2 | 12 |
| 2 | 6000 | 130 ticków | 4 | 25 |
| 3 | 15000 | 80 ticków | 6 | 50 |

Port jest **stocznią** — tu buduje się zarówno okręty wojenne jak i lotniskowce.

### Generowanie transportów handlowych morskich

Port co N ticków emituje fizyczny transport handlowy morski, który:

1. Wybiera cel — port innego gracza (priorytet: pakt gospodarczy > pakt handlowy > neutralny; nigdy wróg).
2. Wykonuje pathfinding A* po kafelkach wodnych.
3. Płynie z prędkością `seaTransportSpeed` (TBD: 0.5 kafelka/tick).
4. Po dotarciu generuje złoto wg wzoru:

```
goldEarned = baseSeaTransportValue * distanceFactor * partnerTypeFactor * portLevelFactor
baseSeaTransportValue = 25 (propozycja)
```

## Okręt wojenny (Warship)

### Produkcja

- Budowany w **porcie** (kolejka produkcji).
- Koszt: **3000 złota + 50 zaopatrzenia**, czas budowy 1500 ticków.
- **Home base** domyślnie = port, w którym powstał.
- Gracz może **zmienić home base** na lotniskowiec → okręt odpływa do lotniskowca i patroluje wokół niego.

### Autonomia (state machine)

```
AT_BASE (port lub lotniskowiec, regeneracja)
    ↓ wykryto cel w zasięgu detekcji
ENGAGING → RETURNING → AT_BASE
```

### Parametry

| Parametr | Wartość (propozycja) |
|---|---|
| Health | 100 |
| Damage/tick (w zasięgu) | 5 |
| Zasięg uzbrojenia | 8 kafelków |
| Promień patrolu (od home base) | 60 × level portu / lotniskowca |
| Promień detekcji | 25 kafelków |
| Próg ucieczki | health < 30% → powrót do base |
| Regeneracja w base | 1 HP/tick |

### Eskorta lotniskowca

Gracz może **zmienić home base** okrętu z portu na lotniskowiec:

1. Klik na okręt → "Zmień bazę" → wskaż lotniskowiec.
2. Okręt płynie do pozycji lotniskowca (pathfinding po wodzie).
3. Od tej chwili **patroluje wokół lotniskowca** (traktuje go jak mobilny port).
4. Gdy lotniskowiec się porusza (zmiana prowincji morskiej) — okręt podąża za nim.

```python
def set_warship_home_base(warship, newBase):
    warship.homeBaseType = newBase.type  # Port lub Carrier
    warship.homeBaseId = newBase.id
    warship.state = MOVING_TO_BASE
    warship.pathToBase = pathfind(warship.position, newBase.position)
```

> **Eskortowanie lotniskowca w marszu:** gdy gracz wysyła lotniskowiec do odległej prowincji morskiej, może przypisać grupę okrętów jako eskortę. Podróżują razem — okręty trzymają się blisko lotniskowca. Jeśli po drodze jest wrogi lotniskowiec lub armada, eskortujące okręty automatycznie angażują się w walkę, by chronić lotniskowiec.

## Lotniskowiec (Carrier)

### Filozofia

Lotniskowiec to **mobilna baza** dająca projekcję siły morskiej i powietrznej na dowolny ocean. Działa jak połączony "port + lotnisko" zakotwiczony w prowincji morskiej.

Mechanicznie lotniskowiec jest **punktem** w środku prowincji morskiej. Wizualnie — pływa po prowincji, ale wszystkie zasięgi i obliczenia wychodzą z centrum prowincji.

```cpp
struct Carrier {
    uint32_t id;
    uint32_t ownerId;
    uint32_t seaProvinceId;          // przypisana prowincja morska
    CarrierState state;              // Stationed, EnRoute, Sinking
    float health;
    float visualX, visualY;          // tylko do animacji — mechanicznie = centrum prowincji
    uint32_t targetSeaProvinceId;    // cel przy marszu
    vector<uint32_t> escortWarshipIds;  // okręty z homeBase = ten lotniskowiec
    // Parametry bazowe (jak port + lotnisko w jednym)
    uint8_t level;
};
```

### Produkcja

- Budowany w **porcie** (kolejka produkcji, tak jak okręt wojenny ale droższy).
- Koszt: **20000 złota + 500 zaopatrzenia**, czas budowy 4000 ticków (~7 min) (TBD).
- Wymaga portu **level 2+** (TBD).
- Po zbudowaniu stoi w porcie. Gracz klika "Wyślij do prowincji morskiej X" → lotniskowiec odpływa.

### Przypisanie do prowincji morskiej

```python
def assign_carrier_to_sea_province(carrier, seaProvinceId):
    if not is_reachable_by_water(carrier.position, seaProvinceId):
        return Error("Niedostępna prowincja morska")
    if get_carrier_in_province(seaProvinceId, carrier.ownerId):
        return Error("Gracz ma już lotniskowiec w tej prowincji")

    carrier.targetSeaProvinceId = seaProvinceId
    carrier.state = EN_ROUTE
    carrier.path = pathfind_to_province_center(carrier.position, seaProvinceId)
```

### Limit

**Jeden lotniskowiec per prowincja morska per gracz.** Brak globalnego limitu lotniskowców dla gracza — może mieć w różnych prowincjach.

### Ruch lotniskowca

- Klik prawym na prowincję morską → "Przenieś lotniskowiec X tutaj".
- Pathfinding po kafelkach wodnych z centrum starej prowincji do centrum nowej.
- Czas trwania zależny od dystansu i prędkości lotniskowca (TBD: 0.3 kafelka/tick — wolniejszy niż okręt).
- **Podczas marszu** eskortujące okręty podążają razem.
- **Podczas marszu** lotniskowiec **nadal działa** jako baza (samoloty mogą startować / lądować, okręty mogą patrolować) — tyle że centrum przesuwa się wraz z aktualną pozycją lotniskowca (TBD: czy centrum jest dynamiczne czy skacze dopiero po dotarciu? Propozycja: skacze po dotarciu — prościej implementacyjnie).

### Parametry jako baza

Lotniskowiec działa jak zintegrowany port + lotnisko o określonym poziomie:

| Poziom lotniskowca | Zasięg okrętów/samolotów | Maks. okrętów | Maks. samolotów | HP |
|---|---|---|---|---|
| 1 | 60 kafelki od centrum | 3 | 4 myśliwce / 1 bombowiec | 300 |
| 2 | 100 | 5 | 8 / 2 | 500 |
| 3 | 150 | 8 | 12 / 3 | 800 |

> Lotniskowiec nie generuje **transportów handlowych** — tylko port lądowy to robi.

### Rekrutacja jednostek z lotniskowca

Gracz może budować okręty i samoloty **bezpośrednio z lotniskowca**, tak jak z portu/lotniska. Kolejka produkcji działa identycznie. Nowe jednostki "pojawiają się" przy centrum prowincji morskiej.

### Zniszczenie lotniskowca

Lotniskowiec ma duże HP i jest celem dla:

- **Wrogich okrętów wojennych** — w zasięgu uzbrojenia.
- **Bombowców / bomb atomowych / rakiet** — jeśli eksplozja jest w promieniu (traktowany jak duża jednostka morska).
- **Wrogich myśliwców** — nie atakują bezpośrednio lotniskowca (nie ma sensu; lotniskowiec nie latający). TBD: czy myśliwce mogą atakować lotniskowiec? Propozycja: NIE — tylko okręty.

```python
def tick_carrier_damage(carrier, attacker):
    carrier.health -= attacker.damage
    if carrier.health <= 0:
        sink_carrier(carrier)

def sink_carrier(carrier):
    # Wszystkie jednostki z home_base = ten lotniskowiec zostają bez bazy
    for warship in carrier.escortWarshipIds:
        warship.homeBaseType = HOMELESS
        warship.state = RETURNING_TO_NEAREST_PORT
    for plane in carrier.basedPlanes:
        plane.state = SHOT_DOWN  # samoloty bez bazy giną
    destroy_carrier(carrier)
    spawn_debris_visual(carrier.position)
```

> Gdy lotniskowiec tonie, **samoloty bez paliwa giną** (brak bazy), a **okręty** próbują dotrzeć do najbliższego własnego portu.

## Transport handlowy morski (Sea Trade Convoy)

Fizyczny statek kursujący między portami lądowymi. **Lotniskowce nie generują transportów** — tylko porty lądowe.

- Może być piratowany lub zniszczony.
- Zniszczenie = strata cyklu handlowego.
- Piractwo (50% wartości dla pirata; incydent dyplomatyczny jeśli neutralny).

Szczegóły w [06-handel.md](06-handel.md).

## Walka morska

### Okręt vs okręt / okręt vs transport / okręt vs lotniskowiec

```python
def tick_naval_combat(ship_a, ship_b):
    if distance(ship_a, ship_b) <= WEAPONS_RANGE:
        ship_a.health -= WEAPONS_DAMAGE
        ship_b.health -= WEAPONS_DAMAGE
        if ship_a.health <= 0: ship_a.state = SINKING
        if ship_b.health <= 0: ship_b.state = SINKING
    else:
        move_toward(ship_a, ship_b)
```

> ❗ Okręty atakują **wyłącznie inne jednostki morskie** (okręty, transporty, lotniskowce). Żadnej interakcji z lądem.

### Zderzenie eskorty z wrogą armadam

Gdy lotniskowiec A z eskortą płynie przez prowincję morską gdzie jest lotniskowiec B (wrogi):

1. Eskortujące okręty A automatycznie angażują okręty B w zasięgu.
2. Lotniskowiec A jest celem dla okrętów B (jeśli nie ma eskorty lub eskorta pokonana).
3. Myśliwce z lotniskowca A automatycznie startują do walki z myśliwcami B.

## Interakcja morze–ląd (brak)

> Dla jasności, lista **niedozwolonych interakcji**:
> - Okręt **nie może** atakować prowincji lądowej ani budynków.
> - Okręt **nie może** blokować portu (brak mechaniki "blokady portowej").
> - Okręt **nie może** lądować wojsk na wybrzeżu.
> - Port lądowy **nie może** strzelać do okrętów.
> - Populacja lądowa **nie jest zagrożona** przez okręty (tylko przez bomby/rakiety).

## Otwarte pytania / TBD

- [ ] Koszt i czas budowy lotniskowca *(propozycja: 20000 złota, 4000 ticków)*
- [ ] Prędkość lotniskowca *(propozycja: 0.3 kafelka/tick)*
- [ ] Czy centrum prowincji morskiej jest dynamiczne podczas marszu? *(propozycja: NIE — skacze po dotarciu)*
- [ ] Czy myśliwce mogą atakować lotniskowiec? *(propozycja: NIE)*
- [ ] Poziomy lotniskowca *(propozycja: 3 poziomy, jak wyżej)*
- [ ] Wymagania portu do budowy lotniskowca *(propozycja: port lvl 2+)*
- [ ] Czy baza okrętów/samolotów może być zmieniona "w locie"? *(propozycja: TAK — nowy pathfinding do nowej bazy)*
- [ ] Piractwo neutralnego statku — automatyczna deklaracja wojny czy incydent? *(propozycja: incydent widoczny w UI, decyzja właściciela transportu)*
- [ ] Czy okręty bez bazy (po zatopieniu lotniskowca) walczą dalej? *(propozycja: TAK — szukają nearest port, po drodze normalnie reagują na zagrożenia)*
