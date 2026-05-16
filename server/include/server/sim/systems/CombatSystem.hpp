#pragma once
#include "server/sim/core/GameState.hpp"

namespace gs::server {

    /// System walki lądowej.
    /// Przetwarza wszystkie aktywne ataki co tick:
    ///   - wybiera kafelek frontu
    ///   - oblicza straty atakującego i obrońcy
    ///   - przesuwa front (capture)
    ///   - sprawdza aneksję odciętych grup
    ///   - obsługuje odwrót
    class CombatSystem {
    public:
        void tick(GameState& state);

    private:
        // ── Główna logika ataku ───────────────────────────────────────────────────
        void tickAttack(GameState& state, Attack& attack);
        void tickRetreat(GameState& state, Attack& attack);

        // ── Wybór kafelka frontu ──────────────────────────────────────────────────
        uint32_t pickFrontTile(const GameState& state, const Attack& attack) const;
        bool     bordersAttacker(const GameState& state, uint32_t tileId,
            uint32_t attackerId) const;

        // ── Wzory walki ───────────────────────────────────────────────────────────
        float computeAttackerLoss(const GameState& state, const Attack& attack,
            const Tile& tile) const;
        float computeDefenderLoss(const GameState& state, const Attack& attack,
            const Tile& tile) const;
        float computeCaptureSpeed(const GameState& state, const Attack& attack,
            const Tile& tile) const;

        // Modyfikatory terenu
        float terrainDef(TerrainType t) const;
        float terrainSpeed(TerrainType t) const;

        // Modyfikatory zależne od wielkości graczy
        float largeAttackerBuff(const GameState& state, uint32_t attackerId) const;
        float largeDefenderDebuff(const GameState& state, uint32_t defenderId) const;

        // Modyfikatory specjalne
        float supplyEfficiencyMod(const Attack& attack) const;
        float defensePostMod(const GameState& state, const Tile& tile) const;
        float traitorDebuff(const GameState& state, uint32_t entityId) const;

        // ── Capture i aneksja ─────────────────────────────────────────────────────
        void captureTile(GameState& state, uint32_t tileId, uint32_t attackerId);
        void checkAnnexation(GameState& state, uint32_t provinceId, uint32_t attackerId);
        bool hasPathToMainTerritory(const GameState& state, uint32_t entityId,
            const std::vector<uint32_t>& group) const;

        // ── Helpers ───────────────────────────────────────────────────────────────
        uint32_t countEntityTiles(const GameState& state, uint32_t entityId) const;
        void     returnTroopsToSource(GameState& state, const Attack& attack,
            float troops);
        void     returnSupply(GameState& state, const Attack& attack, float supply);

        // Inputy — tworzenie ataków z PlayerInput
        void handleAttackInput(GameState& state, const PlayerInput& input);
        void handleMultiAttackInput(GameState& state, const PlayerInput& input);
        void handleRetreatInput(GameState& state, const PlayerInput& input);
    };

} // namespace gs::server