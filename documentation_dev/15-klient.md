# 15 — Klient (architektura)

## ClientConnection

Asynchroniczne połączenie TCP z serwerem, używa tego samego wzorca `readHeader` / `readPayload` co serwer.

```cpp
class ClientConnection {
public:
    ClientConnection(boost::asio::io_context& ioc);
    void connect(const std::string& host, uint16_t port);
    void send(Message msg);
    void setOnMessage(std::function<void(Message)> cb);

private:
    void readHeader();
    void readPayload();

    boost::asio::ip::tcp::socket    socket_;
    std::function<void(Message)>    onMessage_;
    // kolejka zapisu jak w Session
};
```

## ClientState

Lokalny snapshot stanu gry. Aktualizowany przez aplikowanie `GameStateDelta`. Tylko do odczytu przez `Renderer`.

```cpp
class ClientState {
public:
    void applyFull (const GameStateFull&  full);
    void applyDelta(const GameStateDelta& delta);

    // Dostęp do danych (read-only dla Renderera)
    const std::vector<Tile>&         tiles()    const;
    const std::vector<LandProvince>& provinces() const;
    // ...

private:
    // Lokalny stan — odzwierciedla ostatni znany stan serwera
    std::vector<Tile>           tiles_;
    std::vector<LandProvince>   landProvinces_;
    // ... reszta GameState
    uint32_t                    lastTick_{0};
};
```

## Renderer

```cpp
class Renderer {
public:
    Renderer(sf::RenderWindow& window, const ClientState& state);
    void draw();

private:
    MapRenderer     mapRenderer_;
    UnitRenderer    unitRenderer_;
    UIRenderer      uiRenderer_;
};
```

## DebugPanel (GS_DEBUG_MODE)

```cpp
#ifdef GS_DEBUG_MODE
class DebugPanel {
public:
    void draw(const ClientState& state, ClientConnection& conn);

private:
    void drawTickControl(ClientConnection& conn);
    void drawEntityInspector(const ClientState& state);
    void drawProvinceInspector(const ClientState& state);
    void drawSystemStats(const ClientState& state);
};
#endif
```
