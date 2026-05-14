#pragma once
#include <cstdint>

namespace gs::constants {

// ── Tick ──────────────────────────────────────────────────────────────────────
inline constexpr uint32_t TICKRATE_HZ        = 10;
inline constexpr uint32_t TICK_MS            = 1000 / TICKRATE_HZ;   // 100 ms

// ── Mapa ──────────────────────────────────────────────────────────────────────
inline constexpr float    BASE_PER_TILE      = 5.0f;   // cap populacji per kafelek

// ── Populacja ─────────────────────────────────────────────────────────────────
inline constexpr float    DEFAULT_WORKER_SPLIT = 0.6f;

// ── Zaopatrzenie ──────────────────────────────────────────────────────────────
inline constexpr float    SUPPLY_PER_SOLDIER  = 0.5f;
inline constexpr float    COMBAT_SUPPLY_RATE  = 0.05f; // per żołnierz per tick

// ── Walka ─────────────────────────────────────────────────────────────────────
inline constexpr float    RETREAT_TICKS       = 20.0f;
inline constexpr float    RETREAT_TROOP_LOSS  = 0.25f;
inline constexpr float    RETREAT_SUPPLY_BACK = 0.50f;

// ── Start gracza ──────────────────────────────────────────────────────────────
inline constexpr int64_t  START_GOLD          = 1000;
inline constexpr float    START_SUPPLY        = 2000.0f;
inline constexpr float    START_POWER_MW      = 50.0f;
inline constexpr float    START_WORKERS       = 200.0f;
inline constexpr float    START_MILITARY      = 100.0f;

// ── Zwycięstwo ────────────────────────────────────────────────────────────────
inline constexpr float    VICTORY_THRESHOLD   = 0.80f; // 80% kafelków lądowych

// ── Sieć elektryczna ──────────────────────────────────────────────────────────
inline constexpr float    MAX_CABLE_DISTANCE  = 50.0f; // kafelki

// ── Broń strategiczna ────────────────────────────────────────────────────────
inline constexpr int32_t  NUKE_RADIUS_ATOMIC  = 20;
inline constexpr int32_t  NUKE_RADIUS_HYDROGEN= 45;
inline constexpr int32_t  URANIUM_COST_ATOMIC = 50;
inline constexpr int32_t  URANIUM_COST_HYDROGEN = 150;

// ── Morski ───────────────────────────────────────────────────────────────────
inline constexpr float    SEA_TRANSPORT_SPEED = 0.5f;  // kafelki/tick
inline constexpr float    BASE_SEA_TRADE_VALUE= 25.0f;

// ── Powietrzny ───────────────────────────────────────────────────────────────
inline constexpr float    BASE_AIR_TRADE_VALUE= 30.0f;

} // namespace gs::constants
