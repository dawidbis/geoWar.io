# 08 — Dyplomacja

## Filozofia

Dyplomacja w grze obejmuje:

- **Relacje bilateralne** (między dwoma graczami) — wojna, rozejm, sojusz, pakt handlowy.
- **Pakty multilateralne** (3+ graczy) — pakt militarny ("NATO") i pakt gospodarczy ("UE").
- **Transakcje** — sprzedaż prowincji, eksport prądu, transfer populacji.
- **Embargo** i zamknięte granice (jednostronne ograniczenia).

## Relacje bilateralne

Każda para gracz–gracz ma **stan relacji**:

```cpp
enum class RelationFlag : uint8_t {
    AtWar         = 1 << 0,   // wzajemna wojna
    BilateralAlly = 1 << 1,   // bilateralny sojusz (rzadziej — gracze zwykle wstępują do paktów)
    BilateralTrade= 1 << 2,   // bilateralny pakt handlowy
    Embargo       = 1 << 3,   // jednostronne
    BorderClosed  = 1 << 4,   // jednostronne
};

struct Relation {
    uint32_t playerA, playerB;
    uint8_t flagsAB;     // od A → B (one-way: embargo, border closed)
    uint8_t flagsBA;     // od B → A
    uint8_t flagsBoth;   // wzajemne (war, ally, trade)
};
```

> Bilateralne sojusze/pakty istnieją jako **podstawowa forma** — pakty multilateralne to "nadbudowa" pozwalająca na większe sojusze.

### Wojna

**Wzajemna.** Po deklaracji można zakończyć tylko **traktatem pokojowym** (obustronna zgoda).

**Efekty:**
- Możliwość ataków na siebie nawzajem.
- **Automatyczne zerwanie** wszystkich pozostałych umów z tym graczem (sojusz bilateralny, pakt handlowy bilateralny). **Pakty multilateralne** — patrz niżej.
- **Wzajemne embargo** automatycznie.

**Deklaracja wojny:**
- Bez sojuszu/paktu → natychmiastowa, bez kary.
- Z sojusznikiem (bilateralnym lub w pakcie militarnym) → **debuff zdrajcy**.

### Sojusz bilateralny

Rzadziej używana opcja — większość graczy wstępuje do paktów multilateralnych.

**Efekty:**
- Brak możliwości ataku na sojusznika.
- Możliwość **przekazania populacji wojskowej** sojusznikowi (jako transfer, nie marsz).
- **NIE daje** dostępu do sieci elektrycznej ani lepszego handlu — to są efekty paktu **gospodarczego**.
- **NIE pozwala** na marsz wojsk przez terytorium sojusznika.

### Pakt handlowy bilateralny

**Efekty:**
- Lepsze stawki handlu (patrz [06-handel.md](06-handel.md)) — `partnerTypeFactor = 2.0`.
- Cło spada do 5%.
- **NIE daje** swobody darmowych transferów (te są tylko dla unii celnej / paktów gospodarczych multilateralnych).

## Pakty multilateralne

Pakt to **struktura skupiająca 3+ graczy** o wspólnych zobowiązaniach. Są **dwa typy**:

```cpp
enum class PactType : uint8_t {
    Military,    // "NATO"
    Economic,    // "UE" / unia celna
};

struct Pact {
    uint32_t id;
    PactType type;
    string name;             // np. "Sojusz Północy"
    vector<uint32_t> memberIds;
    uint32_t founderId;
    uint32_t createdTick;
};
```

### Ograniczenia uczestnictwa

| Pakt | Limit per gracz |
|---|---|
| **Pakt militarny** | **1** — gracz może być tylko w jednym sojuszu wojskowym |
| **Pakt gospodarczy** | **1** — gracz może być tylko w jednej unii celnej |

> Gracz **może być jednocześnie w jednym pakcie militarnym i jednym pakcie gospodarczym** (np. być w "NATO" i w "UE"). Nie muszą się pokrywać członkostwem.

### Tworzenie paktu

1. Gracz wybiera typ paktu (militarny / gospodarczy), nadaje nazwę.
2. Koszt założenia: TBD (propozycja: 5000 złota).
3. Założyciel jest pierwszym członkiem.
4. Inni gracze mogą **prosić o przyjęcie** lub być **zaproszeni**.

### Dołączanie nowego członka

**Wymaga 100% głosów na TAK** wszystkich obecnych członków paktu.

1. Kandydat klika "Aplikuj o członkostwo" lub jest **zaproszony** przez członka paktu.
2. **Każdy obecny członek** głosuje (TAK/NIE/WSTRZYMANIE).
3. Głosowanie trwa **maksymalnie 1500 ticków** (~2.5 min real-time) — TBD.
4. Jeśli **wszyscy obecni członkowie zagłosowali TAK** — kandydat dołącza.
5. Jeśli choć jeden NIE — aplikacja odrzucona.
6. Jeśli upłynął czas a ktoś nie zagłosował → traktowany jako NIE (TBD: lub jako wstrzymanie? Propozycja: NIE — bezpieczniejsze).

```python
def process_pact_application(pact, candidate):
    votes = collect_votes(pact.memberIds, deadline=current_tick + 1500)
    if all(v == YES for v in votes.values()):
        pact.memberIds.append(candidate.id)
        notify_all_members(pact, f"{candidate.name} dołączył do paktu")
    else:
        notify_candidate(candidate, "Aplikacja odrzucona")
```

### Opuszczanie paktu

Gracz może **dobrowolnie opuścić** pakt w dowolnym momencie:

- **Bez debuffa** jeśli żaden członek paktu **nie jest w aktywnej wojnie** z kimś, kto wkrótce mógłby być atakowany przez wychodzącego.
- **Z debuffem zdrajcy** jeśli opuszcza pakt, w którym ktoś jest w aktywnej wojnie i wychodzący zaraz potem atakuje członka paktu lub jego sojusznika. (Heurystyka — TBD szczegóły wykrywania.)
- **Prostsze rule (propozycja v1):** opuszczenie paktu militarnego = debuff zdrajcy zawsze, jeśli w obrębie ostatnich 500 ticków po opuszczeniu zostanie wypowiedziana wojna członkowi paktu. Inaczej brak kary.

### Wykluczanie członka

Pakt może **wykluczyć członka** — wymaga **100% głosów** na TAK wszystkich pozostałych członków (poza wykluczanym).

- Wykluczony NIE dostaje debuffa zdrajcy.
- Może aplikować ponownie po N ticków (TBD: 3000 ticków = 5 min).

### Efekty paktu militarnego

Wszyscy członkowie automatycznie:

- **Nie mogą się wzajemnie atakować** (UI blokuje).
- Mogą **przekazywać sobie populację wojskową** bez kosztów (transfer).
- W walce mają **wspólne "main territory"** dla celu aneksji (patrz [05-walka.md](05-walka.md)) — kafelki sojusznika z paktu są "przyjazne" w obliczaniu spójności.
- **Debuff zdrajcy** aplikuje się gdy ktoś atakuje swojego pakt-mate lub opuści pakt podczas konfliktu (patrz [05-walka.md](05-walka.md) — wartość 0.8).

### Efekty paktu gospodarczego (unii celnej)

Wszyscy członkowie automatycznie:

- **Handel** między nimi: `partnerTypeFactor = 2.5`, cło 0%.
- **Darmowy transfer prądu** (po włączeniu eksportu w panelu energetycznym — patrz [03-zasoby.md](03-zasoby.md)).
- **Darmowy transfer populacji** (workers i military, jeśli istnieje ruta logistyczna lądowa lub statek/samolot transportowy).
- **Trasy handlowe** mogą przepływać przez terytorium członków paktu bez ceł.

### Pakt gospodarczy a brak swobody marszu

> ❗ **Ważne:** Pakt **gospodarczy ani militarny nie pozwala** na marsz wojsk przez cudze terytorium. Sojusznicy mogą sobie **przekazać** populację (transfer), ale nie da się przeprowadzić "korytarzem".

> **Wyjątek techniczny:** transport populacji w ramach paktu gospodarczego między dwiema swoimi prowincjami **może** odbywać się przez terytorium pakt-mate (tranzyt) — TBD.

> Propozycja: na v1 trzymamy się prostej zasady: **transport populacji** wymaga **własnego ciągłego korytarza terytorialnego** (od źródła do celu). Pakty pozwalają tylko na **transfer** — populacja "znika" u dawcy i "pojawia się" u biorcy w ramach jego prowincji-celu, z pewnym opóźnieniem zależnym od dystansu.

```python
def transfer_population(donorPlayer, donorProv, receiverPlayer, receiverProv, type, amount):
    if not in_same_pact(donorPlayer, receiverPlayer, PactType.Economic):
        return Error("Transfer wymaga paktu gospodarczego")
    # Pobierz od dawcy
    donor_share = get_share(donorProv, donorPlayer.id)
    if donor_share.population[type] < amount:
        return Error("Niewystarczająca populacja")
    donor_share.population[type] -= amount

    # Uruchom "logistyczny" transfer (wirtualny — nie fizyczny korytarz)
    distance = manhattan_distance(donorProv.center, receiverProv.center)
    arrivalTick = current_tick + (distance / TRANSFER_SPEED)

    schedule_population_arrival(receiverPlayer.id, receiverProv.id, type, amount, arrivalTick)
```

## Sprzedaż prowincji

Gracze mogą sobie **oferować sprzedaż swojej części prowincji** (wszystkich swoich kafelków + budynków w danej prowincji).

### Mechanika

1. Gracz A klika na swoją część prowincji X → "Sprzedaj" → wybiera gracza B → ustawia **cenę w złocie**.
2. Oferta trafia do gracza B.
3. B może **akceptować, odrzucić lub kontrofertę** (TBD: kontroferty — propozycja: w v1 NIE, tylko akceptuj/odrzuć).
4. Jeśli akceptuje:
   - Gracz B płaci złoto graczowi A.
   - **Wszystkie kafelki A w prowincji X** stają się kafelkami B.
   - **Wszystkie budynki A w prowincji X** zmieniają właściciela na B.
   - **Cała populacja A w prowincji X** (workers + military) przenosi się jako populacja B (TBD: czy populacja "zostaje" jako ludność, czy część emigruje? Propozycja: zostaje w 100% — to terytorialny transfer).
   - Sieć elektryczna gracza A może się rozpaść jeśli sprzedane budynki były węzłami — wymagana rekompozycja grafu.

### Ograniczenia

- Można sprzedać tylko **całą swoją część prowincji** (nie fragmentami).
- Nie można sprzedać prowincji jeśli na niej **toczy się aktywny atak** na/z tej prowincji (TBD: lub po prostu zezwolić i atak się załamie?).
- Nie można sprzedać prowincji z **stolicą** (najpierw przenieś stolicę).

## Embargo

**Jednostronne**, nakładane przez gracza A na gracza B.

**Efekty:**
- Transporty handlowe A nie kursują do B.
- Nadal można otrzymywać towary od B (jeśli B nie nałożył embarga).
- Można nałożyć/zdjąć w dowolnym momencie.

## Zamknięte granice

**Jednostronne**, dotyczy:

- Tranzytu wirtualnych transportów handlowych.
- **Nie dotyczy** prób ataku (wojna ignoruje granice).

Granice **automatycznie otwarte** dla:
- Członków tego samego paktu gospodarczego.

## UI dyplomacji

Główny panel pokazuje:

- **Listę wszystkich znanych graczy/botów** z kolorem statusu (zielony=pakt, żółty=neutralny, czerwony=wojna).
- **Sekcję paktów** — w jakich paktach jestem, kto inny w nich jest.
- **Dostępne akcje per gracz**: zaproponuj sojusz, zaproponuj pakt handlowy, wypowiedz wojnę, zaproś do mojego paktu, zaoferuj sprzedaż prowincji, nałóż embargo.
- **Powiadomienia** o aplikacjach, propozycjach, wojnach.
- **Sekcję głosowań** — aktywne głosowania w moich paktach (przyjmowanie nowych członków).

## Otwarte pytania / TBD

- [ ] Czy istnieją **kontroferty** dla sprzedaży prowincji? *(propozycja: NIE w v1)*
- [ ] Czas głosowania w pakcie *(propozycja: 1500 ticków = 2.5 min)*
- [ ] Co stanie się gdy gracz w pakcie zaatakuje członka? *(propozycja: automatyczne wykluczenie + debuff zdrajcy)*
- [ ] Czas trwania debuffa zdrajcy *(propozycja: 3000 ticków = 5 min)*
- [ ] Czy są **gwarancje niepodległości** (asymetryczne)? *(propozycja: NIE w v1)*
- [ ] Czy traktat pokojowy może zawierać **warunki cesji**? *(propozycja: NIE w v1 — pokój jest "biały")*
- [ ] Tranzyt populacji przez kafelki pakt-mate *(propozycja: NIE; tylko transfer)*
- [ ] Cena tworzenia paktu *(propozycja: 5000 złota)*
- [ ] Czy bot Hard może dołączać do paktów? *(propozycja: TAK)*
- [ ] Wypisanie z paktu — kara *(propozycja: debuff zdrajcy gdy w obrębie 500 ticków od wypisania wybuchnie wojna z pakt-mate)*
