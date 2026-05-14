# Dokumentacja techniczna — Grand Strategy 2D

## Spis treści

| # | Dokument | Opis |
|---|---|---|
| 01 | [Przegląd architektury](01-przeglad-architektury.md) | Warstwy systemu, przepływ danych, autoritative server |
| 02 | [Protokół sieciowy](02-protokol-sieciowy.md) | Format wiadomości TCP, serializacja, typy wiadomości |
| 03 | [Pętla gry i tick](03-petla-gry.md) | GameLoop, steady_timer, kolejność systemów, tryb debug |
| 04 | [Stan gry — GameState](04-gamestate.md) | Struktury danych, mapa, prowincje, encje, jednostki |
| 05 | [System walki](05-system-walki.md) | CombatSystem — wzory, kafelki, aneksja, multi-attack |
| 06 | [System populacji](06-system-populacji.md) | PopulationSystem — przyrost, workerSplit, automatyzacja |
| 07 | [System zaopatrzenia](07-system-zaopatrzenia.md) | SupplySystem — magazyny, plecak, pull-based dystrybucja |
| 08 | [System elektryczny](08-system-elektryczny.md) | ElectricSystem — graf sieci, bilansowanie, eksport |
| 09 | [System handlu](09-system-handlu.md) | TradeSystem — transporty lądowe, morskie, lotnicze |
| 10 | [System morski](10-system-morski.md) | NavalSystem — okręty, lotniskowiec, piractwo |
| 11 | [System powietrzny](11-system-powietrzny.md) | AirSystem — myśliwce, bombowce, AA |
| 12 | [System nuklearny](12-system-nuklearny.md) | NukeSystem — eksplozja, fallout, straty populacji |
| 13 | [System dyplomacji](13-system-dyplomacji.md) | DiplomacySystem — pakty, relacje, głosowania |
| 14 | [Boty — AI](14-boty.md) | BotController — poziomy, throttling, decyzje |
| 15 | [Klient — architektura](15-klient.md) | ClientConnection, ClientState, Renderer, DebugPanel |
| 16 | [Wzorce projektowe](16-wzorce.md) | Użyte wzorce — System, Observer, Command, Delta State |

## Konwencje w dokumentacji

- **Pseudokod** — Python-like, tylko dla ilustracji logiki. Implementacja w C++20.
- **TODO** — miejsca do uzupełnienia podczas implementacji.
- Nazwy typów C++ pisane są `monospace` (np. `GameState`, `uint32_t`).
- Każdy dokument kończy się sekcją **Zależności** (od jakich innych systemów zależy).
