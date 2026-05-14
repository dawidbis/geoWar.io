# Grand Strategy 2D — Dokumentacja designu

> **Status:** Wersja robocza · Obejmuje rozgrywkę lądową, morską i powietrzną

## O grze

Wieloosobowa, sieciowa gra typu **grand strategy 2D** łącząca uproszczone, "ciśnieniowe" zarządzanie populacją i walką znane z [openfront.io](https://openfront.io) z systemem prowincji i logistyki inspirowanym **Hearts of Iron IV**.

Gracz nie steruje pojedynczymi jednostkami w skali taktycznej. Zamiast tego zarządza:

- **populacją** prowincji (robotnicy / wojsko),
- **zasobami** (złoto, zaopatrzenie, elektryczność, uran),
- **infrastrukturą** (budynki, drogi/tory, sieć elektryczna, porty, lotniska),
- **dyplomacją** (sojusze bilateralne i multilateralne, pakty handlowe, unie celne, sprzedaż prowincji),
- **operacjami morskimi i powietrznymi** (okręty wojenne, myśliwce, bombowce, handel morski i lotniczy).

Walka lądowa jest wynikiem **ciśnienia populacji wojskowej** na granicy między prowincjami. Operacje morskie i powietrzne — autonomiczne jednostki bojowe — patrolują wokół swoich baz i reagują na zagrożenia.

## Kluczowe założenia techniczne (kontekst designu)

| Parametr | Wartość |
|---|---|
| Typ gry | Sieciowa, lobby-based |
| Liczba graczy w lobby | 20–100 |
| Tickrate serwera | **10 Hz** (1 tick = 100 ms) |
| Najmniejsza jednostka mapy | Kafelek 2×2 px |
| Mapa | Z lądami i wodami (oceany, morza, jeziora) |
| Warunek zwycięstwa | Posiadanie ≥ 80% możliwego do zdobycia terytorium **lub** kapitulacja po stracie stolicy |

> Wszystkie obliczenia (przyrost populacji, walka, handel, sieć elektryczna, zaopatrzenie) wykonywane są **na ticku**. Transporty widoczne na mapie (handlowe lądowe, ludność, zaopatrzenie) mają charakter **wyłącznie wizualizacji** — nie są fizycznymi jednostkami, tylko reprezentacją przepływów. **Jednostki bojowe (okręty, myśliwce, bombowce) i transporty morskie/lotnicze są fizyczne** — istnieją w stanie gry, mogą być zniszczone lub przejęte.

## Spis treści

| # | Dokument | Opis |
|---|---|---|
| 01 | [Mapa i prowincje](docs/01-mapa-i-prowincje.md) | Kafelki, prowincje, terreny lądowe i wodne, współwłasność |
| 02 | [Populacja](docs/02-populacja.md) | Podział, przyrost, automatyzacja, transport |
| 03 | [Zasoby](docs/03-zasoby.md) | Złoto, zaopatrzenie, elektryczność (sieć), uran |
| 04 | [Budynki](docs/04-budynki.md) | Lista, efekty, koszty, poziomy, wymagania |
| 05 | [Walka lądowa](docs/05-walka.md) | Atak, modyfikatory, wzory, zaopatrzenie, aneksja, multi-attack |
| 06 | [Handel](docs/06-handel.md) | Transporty lądowe, morskie, lotnicze; pakty handlowe |
| 07 | [Logistyka](docs/07-logistyka.md) | Połączenia, transport ludności i zaopatrzenia |
| 08 | [Dyplomacja](docs/08-dyplomacja.md) | Sojusze, pakty multilateralne, sprzedaż prowincji, embargo |
| 09 | [Broń strategiczna](docs/09-bron-strategiczna.md) | Uran, bomba atomowa (bombowiec), wodorowa (silos), obrony |
| 10 | [Boty](docs/10-boty.md) | Poziomy AI, zachowania |
| 11 | [Rozgrywka](docs/11-rozgrywka.md) | Start gry, stolica, warunek zwycięstwa, lobby |
| 12 | [Moduł morski](docs/12-modul-morski.md) | Porty, okręty wojenne, piractwo, handel morski |
| 13 | [Moduł powietrzny](docs/13-modul-powietrzny.md) | Lotniska, myśliwce, bombowce, obrona przeciwlotnicza |

## Konwencje używane w dokumentacji

- **TBD** — wartość/mechanika do ustalenia. Tam gdzie się dało, podana jest **propozycja** do akceptacji/korekty.
- **Wzory** podawane są w notacji matematycznej oraz pseudokodzie (Python-like) dla jednoznaczności.
- **Tick** = pojedynczy krok symulacji serwera (100 ms).
- **Encja** = gracz, bot lub neutralny aktor (np. nation).

## Inspiracje i zapożyczenia

- **openfront.io** — globalna populacja, "ciśnieniowa" walka, system aneksji, wzory na efficiency/speed ataku.
- **Hearts of Iron IV** — prowincje jako jednostka logistyczna, sieci logistyczne, zaopatrzenie, ulepszenia infrastruktury.

Niektóre wzory (np. atak efficiency, speed, przyrost populacji) są bezpośrednio adaptowane z openfront.io z modyfikacjami dla naszego systemu prowincji. Są zaznaczone w odpowiednich miejscach.
