# 02 — Protokół sieciowy

## Format wiadomości TCP

Każda wiadomość składa się z **nagłówka (8 bajtów)** i **payloadu (N bajtów)**:

```
┌──────────────┬──────────────┬──────────────────┬─────────────────────┐
│  type        │ payloadSize  │    reserved      │      payload...     │
│  uint16_t    │  uint16_t    │    uint32_t      │    [payloadSize B]  │
│  2 bajty     │  2 bajty     │    4 bajty       │                     │
└──────────────┴──────────────┴──────────────────┴─────────────────────┘
  ◄────────────────── 8 bajtów nagłówka ──────────────────►
```

Wszystkie pola little-endian. `reserved` zarezerwowane na przyszłość (numer sekwencji, flagi).

```cpp
// shared/include/shared/net/Message.hpp
struct MessageHeader {
    MessageType type{};
    uint16_t    payloadSize{0};
    uint32_t    reserved{0};
};
static_assert(sizeof(MessageHeader) == 8);
```

## Typy wiadomości (MessageType)

```cpp
enum class MessageType : uint16_t {
    // Lobby
    ClientHello      = 0x0001,  // C→S: dołącz (nazwa gracza)
    ServerWelcome    = 0x0002,  // S→C: entityId + stan lobby
    LobbyState       = 0x0003,  // S→C: broadcast kto jest w lobby
    LobbyStartGame   = 0x0004,  // S→C: gra startuje

    // Gra
    PlayerInput      = 0x0010,  // C→S: komenda gracza
    GameStateFull    = 0x0020,  // S→C: pełny snapshot (connect/reconnect)
    GameStateDelta   = 0x0021,  // S→C: delta per tick

    // Debug
    DebugStep        = 0xD001,  // C→S: wykonaj N ticków
    DebugSetTickrate = 0xD002,  // C→S: zmień tickrate
    DebugQueryState  = 0xD003,  // C→S: żądaj pełnego stanu
    DebugStateResponse = 0xD004,// S→C: odpowiedź z pełnym stanem

    ErrorResponse    = 0xFFFF,
};
```

## Serializacja

Payload każdej wiadomości to binarnie spakowane dane. Serializacja ręczna przez `std::vector<uint8_t>` — bez zewnętrznych bibliotek (protobuf, flatbuffers) w v1.

```cpp
// shared/include/shared/net/Serializer.hpp

class Serializer {
public:
    // Zapis
    void writeU8 (uint8_t v);
    void writeU16(uint16_t v);
    void writeU32(uint32_t v);
    void writeU64(uint64_t v);
    void writeF32(float v);
    void writeStr(std::string_view s);  // uint16_t len + bajty

    std::vector<uint8_t> take();  // zwraca payload i resetuje

    // Odczyt
    uint8_t  readU8 ();
    uint16_t readU16();
    uint32_t readU32();
    uint64_t readU64();
    float    readF32();
    std::string readStr();

    bool hasData() const;

private:
    std::vector<uint8_t> buf_;
    size_t               readPos_{0};
};
```

Każdy typ wiadomości ma parę funkcji `encode`/`decode`:

```cpp
// Przykład dla PlayerInput
namespace gs::net {

struct PlayerInputPayload {
    uint32_t    entityId;
    InputType   inputType;   // enum: Attack, Build, Diplomacy...
    // + union z danymi zależnymi od typu
};

std::vector<uint8_t> encode(const PlayerInputPayload& p);
PlayerInputPayload   decode_player_input(std::span<const uint8_t> data);

} // namespace gs::net
```

## Odczyt asynchroniczny (Session)

Czytamy w dwóch krokach: najpierw nagłówek, potem payload.

```cpp
// server/src/net/Session.cpp

void Session::readHeader() {
    auto self = shared_from_this();
    boost::asio::async_read(
        socket_,
        boost::asio::buffer(&inHeader_, sizeof(MessageHeader)),
        [this, self](boost::system::error_code ec, size_t) {
            if (ec) { disconnect(); return; }
            readPayload();
        }
    );
}

void Session::readPayload() {
    inPayload_.resize(inHeader_.payloadSize);
    auto self = shared_from_this();
    boost::asio::async_read(
        socket_,
        boost::asio::buffer(inPayload_),
        [this, self](boost::system::error_code ec, size_t) {
            if (ec) { disconnect(); return; }
            handleMessage(inHeader_, inPayload_);
            readHeader();  // czekaj na kolejną
        }
    );
}
```

## Zapis asynchroniczny (kolejka wysyłania)

Używamy kolejki żeby nie wywoływać `async_write` współbieżnie (niezdefiniowane zachowanie).

```cpp
void Session::send(Message msg) {
    bool writeInProgress = !outQueue_.empty();
    outQueue_.push_back(std::move(msg));
    if (!writeInProgress) {
        doWrite();
    }
}

void Session::doWrite() {
    // Serializuj nagłówek + payload do jednego bufora
    auto& msg = outQueue_.front();
    outBuf_.clear();
    // ... append header bytes, then payload bytes

    auto self = shared_from_this();
    boost::asio::async_write(
        socket_,
        boost::asio::buffer(outBuf_),
        [this, self](boost::system::error_code ec, size_t) {
            if (ec) { disconnect(); return; }
            outQueue_.pop_front();
            if (!outQueue_.empty()) doWrite();
        }
    );
}
```

## GameStateDelta — format

Delta wysyłana co tick zawiera **tylko zmienione pola**. Klient aplikuje ją na swój lokalny `ClientState`.

```
GameStateDelta payload:
  uint32_t tick                        — numer ticka
  uint16_t numTileChanges              — ile kafelków się zmieniło
  [numTileChanges × TileDelta]
  uint16_t numShareChanges             — ile PlayerProvinceShare się zmieniło
  [numShareChanges × ShareDelta]
  uint16_t numUnitChanges              — ile jednostek (okręty, samoloty)
  [numUnitChanges × UnitDelta]
  uint16_t numEventCount               — zdarzenia (eksplozja, kapitulacja...)
  [numEventCount × EventMsg]
```

Zasada: jeśli nic się nie zmieniło w danej kategorii — `numXChanges = 0`, brak danych. Minimalizuje ruch sieciowy.

## Obsługa błędów połączenia

- Klient rozłączony → `Session` wykrywa błąd w `async_read` → wywołuje `LobbyManager::onDisconnect(entityId)`.
- Bot przejmuje sterowanie po 300 tickach AFK (patrz [14-boty.md](14-boty.md)).
- Reconnect → klient wysyła `ClientHello` z tym samym `entityId` → serwer wysyła `GameStateFull`.

## Zależności

- `shared` — typy wiadomości, serializacja
- `boost.asio` — async TCP (tylko serwer i klient, nie shared)
