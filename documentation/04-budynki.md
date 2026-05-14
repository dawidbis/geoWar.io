# 04 — Budynki

## Lista budynków

| Budynek | Levelowanie | Wymaga prądu? | Wymagania specjalne |
|---|---|---|---|
| **Miasto** | Tak (1–5) | Tak | — |
| **Fabryka** | Tak (1–5) | Tak | — |
| **Elektrownia węglowa** | Tak (1–3) | NIE (produkuje) | — |
| **Elektrownia atomowa** | Tak (1–3) | NIE (produkuje) | — |
| **Baza wojskowa** | Tak (1–3) | Tak | — |
| **Punkt umocnień** | NIE | NIE | — |
| **Obrona przeciwrakietowa** | Tak (1–3) | Tak | — |
| **Obrona przeciwlotnicza** | Tak (1–3) | Tak | — |
| **Silos rakietowy** | NIE | Tak | — |
| **Port** | Tak (1–3) | Tak | Wymaga sąsiedztwa ShallowWater |
| **Lotnisko** | Tak (1–3) | Tak | Preferowane plains |
| **Droga / Tory** | Tak (1–3) | NIE | — |
| **Punkt logistyczny** | Tak (1–3) | NIE | — |

> **Lotniskowiec** nie jest budynkiem — to **jednostka** budowana w kolejce stoczni portu. Patrz sekcja niżej i [12-modul-morski.md](12-modul-morski.md).

## Miasto

| Poziom | Cap bonus | Koszt złota | Czas (ticki) | Pobór mocy |
|---|---|---|---|---|
| 1 | +1000 | 500 | 1000 | 5 |
| 2 | +2500 | 1500 | 2000 | 10 |
| 3 | +5000 | 4000 | 3500 | 18 |
| 4 | +8000 | 9000 | 5500 | 30 |
| 5 | +12000 | 20000 | 8000 | 50 |

Magazyn: 500 × level. Stolica: +25% mnożnik do bonusu capa (patrz [11-rozgrywka.md](11-rozgrywka.md)).

## Fabryka

| Poziom | Produkcja zaop. | Optimal workers | Koszt złota | Pobór mocy |
|---|---|---|---|---|
| 1 | 5/tick | 50 | 800 | 8 |
| 2 | 12/tick | 120 | 2000 | 18 |
| 3 | 25/tick | 250 | 5000 | 35 |
| 4 | 50/tick | 500 | 12000 | 60 |
| 5 | 100/tick | 1000 | 28000 | 100 |

Magazyn: 1000 × level. Zasięg lokalnego zaop.: 5 × level kafelków.

## Elektrownia węglowa

| Poziom | Produkcja mocy | Workers | Koszt złota |
|---|---|---|---|
| 1 | 100 MW | 30 | 600 |
| 2 | 250 MW | 70 | 1800 |
| 3 | 500 MW | 150 | 4500 |

## Elektrownia atomowa

| Poziom | Produkcja (Civilian) | Workers | Koszt złota | Uran/tick (Military) |
|---|---|---|---|---|
| 1 | 500 MW | 80 | 5000 | 0.01 |
| 2 | 1200 MW | 200 | 12000 | 0.025 |
| 3 | 2500 MW | 400 | 25000 | 0.05 |

Tryby: Civilian / Mixed / Military. Szczegóły w [03-zasoby.md](03-zasoby.md).

## Baza wojskowa

| Poziom | Magazyn zaop. | Zasięg lok. zaop. | Bonus dosyłu wojska | Koszt złota | Pobór mocy |
|---|---|---|---|---|---|
| 1 | 5000 | 30 | ×1.2 | 2000 | 10 |
| 2 | 12000 | 60 | ×1.5 | 5000 | 20 |
| 3 | 25000 | 100 | ×2.0 | 12000 | 40 |

## Punkt umocnień

- Brak poziomów. Koszt: 800 złota. Brak poboru mocy.
- Efekt: `defensePost(isInRange) = 5 if yes else 1` (zasięg 10 kafelków).
- Szczegóły w [05-walka.md](05-walka.md).

## Obrona przeciwrakietowa

Zestrzeliwuje **wyłącznie rakiety wodorowe**.

| Poziom | Zasięg | Szansa zestrzelenia | Koszt złota | Pobór mocy |
|---|---|---|---|---|
| 1 | 80 | 35% | 4000 | 15 |
| 2 | 150 | 60% | 10000 | 30 |
| 3 | 250 | 80% | 25000 | 60 |

## Obrona przeciwlotnicza

Zestrzeliwuje **wszystkie samoloty**. **Nie zestrzeliwuje rakiet.**

| Poziom | Zasięg | Szansa/tick | Koszt złota | Pobór mocy |
|---|---|---|---|---|
| 1 | 30 | 8% | 2000 | 8 |
| 2 | 50 | 15% | 5000 | 18 |
| 3 | 80 | 25% | 12000 | 35 |

## Silos rakietowy

- Brak poziomów. Koszt: 6000 złota. Pobór mocy: 25.
- Maks. 1 rakieta. Cooldown: 3000 ticków.
- Szczegóły w [09-bron-strategiczna.md](09-bron-strategiczna.md).

## Port

**Budynek przybrzeżny.** Stocznia dla okrętów, lotniskowców i transportów morskich.

| Poziom | Koszt złota | Generacja transportów (co N tick) | Maks. okrętów w kolejce | Pobór mocy |
|---|---|---|---|---|
| 1 | 2500 | 200 | 2 | 12 |
| 2 | 6000 | 130 | 4 | 25 |
| 3 | 15000 | 80 | 6 | 50 |

**Wymagania:** sąsiedztwo ShallowWater.

### Stocznia — kolejka produkcji portu

W panelu portu gracz może kolejkować budowę:

| Jednostka | Koszt złota | Koszt zaop. | Czas (ticki) | Wymagania |
|---|---|---|---|---|
| Okręt wojenny | 3000 | 50 | 1500 | Port lvl 1+ |
| Lotniskowiec | 20000 | 500 | 4000 | Port lvl 2+ |

Po zbudowaniu okręt/lotniskowiec pojawia się przy doku i czeka na rozkazy gracza.

Szczegóły w [12-modul-morski.md](12-modul-morski.md).

## Lotnisko

**Hangar i pas startowy.** Baza dla samolotów bojowych i cargo.

| Poziom | Koszt złota | Cargo (co N tick) | Maks. myśliwców | Maks. bombowców | Pobór mocy |
|---|---|---|---|---|---|
| 1 | 3000 | 300 | 3 | 1 | 15 |
| 2 | 8000 | 180 | 6 | 2 | 30 |
| 3 | 18000 | 110 | 10 | 4 | 60 |

Strefa patrolu: 80/140/220 kafelków. Szczegóły w [13-modul-powietrzny.md](13-modul-powietrzny.md).

## Droga / Tory

| Poziom | Typ | Mnożnik szybkości | Koszt / kafelek |
|---|---|---|---|
| 1 | Droga | ×1.0 | 5 |
| 2 | Tory | ×1.8 | 20 |
| 3 | Tory szybkie | ×3.0 | 60 |

Nie wymagają prądu na żadnym poziomie.

## Punkt logistyczny

| Poziom | Magazyn | Bonus przepustowości | Koszt złota |
|---|---|---|---|
| 1 | 2000 | ×1.3 | 1500 |
| 2 | 5000 | ×1.7 | 4000 |
| 3 | 10000 | ×2.5 | 10000 |

Brak poboru mocy. Zasięg lokalnego zaop.: 15 × level.

## Zasady budowy — wspólne

1. Gracz kontroluje **wszystkie kafelki** zajmowane przez budynek.
2. Kafelki **wolne** (brak innych budynków).
3. Wystarczająco złota.
4. **Nie można budować na terytorium sojusznika** — nawet w pakcie.
5. **Nie można budować na kafelkach fallout** — dopóki isFallout = true.

### Zniszczenie budynku przez bombę

Budynki na kafelkach w promieniu rażenia bomby jądrowej są **natychmiastowo niszczone**. Kafelki stają się pustkowiem-fallout. Brak możliwości budowy dopóki ktoś kafelka nie odzyska (i nie wyczyści falloutu).

### Anulowanie / burzenie

- Anulowanie w budowie: zwrot **50% kosztu**.
- Burzenie gotowego: zwrot **20% kosztu**.

## Sieć elektryczna

Gracz ręcznie rysuje kable między węzłami (max 50 kafelków dystansu). Sieć może obejmować wiele prowincji. Eksport prądu do sojusznika z paktu gospodarczego — darmowy po włączeniu w panelu. Szczegóły w [03-zasoby.md](03-zasoby.md).

## Otwarte pytania / TBD

- [ ] Wszystkie wartości tabel — tuning po testach
- [ ] Limit budynków per prowincja? *(propozycja: NIE, przestrzeń ogranicza naturalnie)*
- [ ] Wymagania portu dla lotniskowca *(propozycja: lvl 2+)*
- [ ] Maksymalny zasięg kabla elektrycznego *(propozycja: 50 kafelków)*
- [ ] Czas budowy ulepszeń *(propozycja: ~70% czasu nowego budynku)*
