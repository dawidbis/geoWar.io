# 11 — Rozgrywka

## Lobby

### Tworzenie i konfiguracja

Gracz tworzy lobby i ustawia:

| Parametr | Zakres | Domyślnie |
|---|---|---|
| Liczba graczy | 20–100 | 40 |
| Tryb mapy | Losowa (seed) / Ręczny seed | Losowa |
| Boty (ilość i poziom) | 0–(maxPlayers-1) | Wypełnia puste miejsca botem Medium |
| Czas rozgrywki (opcjonalny limit) | wyłączony / 30–120 min | wyłączony |

### Dołączanie

Gracz dołącza przez kod lobby lub przez browsing listy publicznych. Po dołączeniu widzi mapę w trybie "preview" (bez interakcji, widoczne prowincje i potencjalne spawny).

### Start

Po kliknięciu "Start" przez hosta (gdy wystarczająca liczba graczy gotowa):

1. Serwer generuje mapę (deterministycznie z seeda).
2. Losowanie spawn pointów dla wszystkich graczy (w tym botów).
3. Graczom przydzielane są prowincje startowe.
4. Odliczanie 5 sekund → gra startuje.

## Spawn gracza

### Przydzielana prowincja startowa

Każdy gracz startuje z **jedną prowincją** + **stolicą** w niej.

- **Spawn przybrzeżny (~70%)** — prowincja lądowa granicząca z prowincją morską.
- **Spawn lądowy (~30%)** — prowincja w głębi lądu.

Algorytm: wybieramy prowincje możliwie **odległe od siebie** (max-min distance spread), żeby gracze nie startowali stłoczeni.

### Zasoby startowe

| Zasób | Wartość startowa |
|---|---|
| Złoto | 1000 |
| Zaopatrzenie | 2000 |
| Elektryczność | 50 MW (wbudowane w stolicę — działa bez sieci) |
| Robotnicy | 200 |
| Wojsko | 100 |
| `workerSplitRatio` | 0.6 (domyślne) |

Startowe 50 MW elektryczności to **wbudowany generator stolicy** — działa bez podłączenia do sieci. Pozwala startować bez budowania elektrowni od zera.

### Stolica startowa

Każdy gracz startuje z **miastem oznaczonym jako stolica** w swojej prowincji startowej. Stolica:

- Jest domyślnie **miastem poziomu 1** z bonusem capa ×1.25.
- Wyróżniona wizualnie na mapie.
- Podlega mechanice kapitulacji (patrz niżej).
- Można ją **przenieść** na inne własne miasto (za koszt i z czasem — TBD: 5000 złota, 1000 ticków).

## Stolica — mechanika

### Mnożnik produkcji

Stolica daje **+25% mnożnik** do `capBonus` miasta, w którym się znajduje:

```python
def capital_multiplier(city, isCapital):
    return city.capBonus * 1.25 if isCapital else city.capBonus
```

### Utrata stolicy = kapitulacja

> ❗ **Utrata stolicy jest natychmiastową przegraną.** Gdy ostatni kafelek zawierający stolicę przechodzi pod kontrolę wroga lub wybucha bomba jądrowa w promieniu stolicy — gracz przegrywa.

```python
def check_capital_loss(playerId):
    capital = get_capital(playerId)
    if capital.tile.ownerId != playerId:
        trigger_capitulation(playerId)

def trigger_capitulation(playerId):
    # Wszystkie kafelki i budynki gracza stają się neutralne / do przejęcia
    for tile in get_all_tiles(playerId):
        tile.ownerId = 0  # wilderness
    # Gracz wyrzucony z gry
    broadcast_player_eliminated(playerId)
```

> Kafelki gracza po kapitulacji stają się **wildernessem** — do przejęcia przez każdego. Nie są automatycznie przekazywane agresorowi.

### Przeniesienie stolicy

- Klik na własne miasto → "Ustaw jako stolicę".
- Koszt: TBD (propozycja: 5000 złota).
- Stary bonus stolicy przenosi się na nowe miasto.

## Warunek zwycięstwa

### Kryterium 80% terytorium

Zwycięstwo gdy gracz (lub pakt militarny) kontroluje **≥ 80% wszystkich kafelków lądowych mapy** (licząc kafelki posiadane przez wszystkich graczy — bez falloutu, który jest pustkowiem).

```python
def check_victory():
    total_land_tiles = count_all_owned_land_tiles()  # tylko kafelki z ownerId != 0

    for entity in all_entities:
        owned = count_tiles(entity)
        if owned / total_land_tiles >= 0.80:
            declare_winner(entity)
            return

    # Pakt militarny — zsumowane kafelki wszystkich członków
    for pact in military_pacts:
        combined = sum(count_tiles(m) for m in pact.members)
        if combined / total_land_tiles >= 0.80:
            declare_winner(pact)
            return
```

> Kafelki fallout (pustkowie) **nie są wliczane** do mianownika — zmniejszają "dostępne terytorium" i przybliżają wszystkich do progu 80%.

### Kryterium kapitulacji

Gracz przegrywa **natychmiastowo** po utracie stolicy (patrz wyżej). Jeśli zostanie tylko jeden gracz / jeden pakt militarny — wygrywa.

### Remis (TBD)

Jeśli gra osiągnie limit czasu (jeśli włączony):

- Wygrywa gracz z największą liczbą kafelków.
- Jeśli pakty — zsumowane jak wyżej.

## Tick serwera — kolejność operacji

Serwer wykonuje co tick (100 ms) następujące operacje **w ustalonej kolejności:**

```
1.  Przetwarzanie wejść graczy (komendy z kolejki)
2.  Tick walki lądowej (per atak)
3.  Tick jednostek morskich (ruch, walka, piractwo)
4.  Tick jednostek powietrznych (ruch, walka, AA)
5.  Tick rakiet balistycznych
6.  Sprawdzenie eksplozji (bomby, rakiety które dotarły)
7.  Przyrost populacji (per prowincja, per gracz)
8.  Tick produkcji zasobów (fabryki, elektrownie, uran)
9.  Tick handlu (generacja transportów, dostarczenia)
10. Tick logistyki (transport populacji i zaopatrzenia)
11. Tick produkcji budynków i jednostek
12. Sprawdzenie warunków zwycięstwa / kapitulacji
13. Generacja delta-stanu do wysłania klientom
```

> Kolejność determinuje zachowanie w edge casach. Np. atak lądowy jest rozwiązywany PRZED przyrostem populacji — wojsko nie "rośnie" w trakcie walki w tym samym ticku.

## AFK i rozłączenie

- Gracz rozłączony powyżej **300 ticków (30 s)** → bot przejmuje sterowanie na poziomie **Easy** (TBD: Medium?).
- Po reconnect gracz przejmuje z powrotem sterowanie bez utraty stanu.
- Gracz, który celowo opuści grę (klik "Wyjdź") → bot przejmuje na stałe (poziom Medium).
- Brak specjalnego "AFK debuffu" dla rozłączonego gracza — jego prowincje funkcjonują normalnie pod botem.

## UI — elementy główne

- **Mapa** z kolorami graczy, ikonkami budynków, frontem walki.
- **Panel prowincji** — klik na własną prowincję → populacja, zaopatrzenie, budynki, `workerSplitRatio`.
- **Panel dyplomacji** — lista graczy, relacje, pakty, głosowania.
- **Panel zasobów** — złoto, zaopatrzenie, prąd, uran (globalnie).
- **Minimap** z widokiem całości.
- **Logi zdarzeń** — ataki, kapitulacje, eksplozje, dołączenia do paktów.

## Otwarte pytania / TBD

- [ ] Dokładne wartości zasobów startowych *(propozycja jak wyżej)*
- [ ] Koszt przeniesienia stolicy *(propozycja: 5000 złota + 1000 ticków)*
- [ ] Czas do przejęcia przez bota AFK *(propozycja: 300 ticków = 30 s)*
- [ ] Poziom bota AFK *(propozycja: Easy)*
- [ ] Mechanika remisu *(propozycja: największa liczba kafelków)*
- [ ] Czy 80% liczy się bez falloutu w mianowniku? *(decyzja: TAK — fallout to pustkowie, nie "terytorium")*
- [ ] Czy pakt militarny może "wygrać" razem? *(propozycja: TAK — jeśli łączne 80%)*
