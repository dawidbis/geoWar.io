#pragma once
#include <cstdint>

namespace gs {

/// Każda wiadomość TCP zaczyna się od 4-bajtowego nagłówka:
///   [uint16_t type][uint16_t payloadSize]
/// Następnie payloadSize bajtów payload.

enum class MessageType : uint16_t {
    // ── Lobby ─────────────────────────────────────────────────────────────
    ClientHello         = 0x0001,  // C→S: dołącz do lobby (nazwa gracza)
    ServerWelcome       = 0x0002,  // S→C: przyznane entityId + aktualny stan lobby
    LobbyState          = 0x0003,  // S→C: broadcast — kto jest w lobby
    LobbyStartGame      = 0x0004,  // S→C: gra startuje, payload = GameStartInfo

    // ── Wejście gracza ────────────────────────────────────────────────────
    PlayerInput         = 0x0010,  // C→S: komenda gracza (attack, build, diplomacy…)

    // ── Stan gry ──────────────────────────────────────────────────────────
    GameStateFull       = 0x0020,  // S→C: pełny snapshot (przy connect/reconnect)
    GameStateDelta      = 0x0021,  // S→C: delta per tick (tylko zmienione pola)

    // ── Debug / Dev ───────────────────────────────────────────────────────
    DebugStep           = 0xD001,  // C→S: wykonaj N ticków (tryb debug)
    DebugSetTickrate    = 0xD002,  // C→S: zmień tickrate serwera
    DebugQueryState     = 0xD003,  // C→S: zażądaj pełnego stanu
    DebugStateResponse  = 0xD004,  // S→C: odpowiedź z pełnym stanem (JSON lub binary)

    // ── Błędy ─────────────────────────────────────────────────────────────
    ErrorResponse       = 0xFFFF,
};

} // namespace gs
