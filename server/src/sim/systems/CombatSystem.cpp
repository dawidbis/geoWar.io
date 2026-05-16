#include "server/sim/systems/CombatSystem.hpp"
#include <shared/sim/Constants.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

namespace gs::server {

    void CombatSystem::tick(GameState& state) {
        // Zabezpieczenie przed mutacją w trakcie przetwarzania inputów.
        // Jeśli MultiAttack tworzy nowe ataki, chcemy, żeby zostały przetworzone dopiero
        // w następnej iteracji pętli poniżej.
        for (const auto& input : state.pendingInputs) {
            switch (input.type) {
            case InputType::Attack:      handleAttackInput(state, input); break;
            case InputType::MultiAttack: handleMultiAttackInput(state, input); break;
            case InputType::Retreat:     handleRetreatInput(state, input); break;
            default: break;
            }
        }

        // Pamiętaj: Jeśli jakikolwiek system doda nowy obiekt do std::deque w trakcie TEJ pętli,
        // iteratory ulegną inwalidacji (crash!). 
        // Na szczęście tickAttack modyfikuje stan ataku (i kafelków), ale nie tworzy NOWYCH ataków.
        // Możemy bezpiecznie iterować po indeksach, by w ogóle pozbyć się problemu z iteratorami:
        size_t attackCount = state.attacks.size();
        for (size_t i = 0; i < attackCount; ++i) {
            auto& attack = state.attacks[i];

            if (attack.status == AttackStatus::Active) {
                tickAttack(state, attack);
            }
            else if (attack.status == AttackStatus::Retreating) {
                tickRetreat(state, attack);
            }
        }

        // Usuń zakończone ataki
        std::erase_if(state.attacks, [](const Attack& a) {
            return a.status == AttackStatus::Finished;
            });
    }

    // ── Tick ataku ────────────────────────────────────────────────────────────────

    void CombatSystem::tickAttack(GameState& state, Attack& attack) {
        if (attack.troops <= 0.0f) {
            attack.status = AttackStatus::Finished;
            return;
        }

        // TODO: Wybierz kafelek frontu
        uint32_t frontTileId = pickFrontTile(state, attack);
        if (frontTileId == 0) {
            // Brak frontu — prowincja w pełni zdobyta lub odcięta
            attack.status = AttackStatus::Finished;
            return;
        }

        auto* tile = state.getTile(frontTileId);
        if (!tile) { attack.status = AttackStatus::Finished; return; }

        // TODO: Oblicz straty i prędkość przejęcia
        // float attackerLoss  = computeAttackerLoss (state, attack, *tile);
        // float defenderLoss  = computeDefenderLoss (state, attack, *tile);
        // float captureSpeed  = computeCaptureSpeed (state, attack, *tile);

        // Placeholder — uproszczone wartości do czasu implementacji wzorów
        float captureSpeed = 20.0f; // ticki na kafelek
        float attackerLoss = 0.1f;
        float defenderLoss = 0.1f;

        attack.capturedTileProgress += 1.0f / captureSpeed;

        // TODO: Konsumpcja zaopatrzenia z plecaka
        // consumeSupply(state, attack);

        if (attack.capturedTileProgress >= 1.0f) {
            // Zadaj straty obrońcy
            if (tile->ownerId != 0) {
                auto* defShare = state.getShare(tile->landProvinceId, tile->ownerId);
                if (defShare) {
                    defShare->population.military =
                        std::max(0.0f, defShare->population.military - defenderLoss);
                    state.markShareDirty(tile->landProvinceId, tile->ownerId);
                }
            }

            // Zadaj straty atakującemu
            attack.troops = std::max(0.0f, attack.troops - attackerLoss);

            // Przejmij kafelek
            captureTile(state, frontTileId, attack.attackerId);

            attack.capturedTileProgress = 0.0f;
            attack.currentFrontTileId = 0;
        }
    }

    // ── Tick odwrotu ──────────────────────────────────────────────────────────────

    void CombatSystem::tickRetreat(GameState& state, Attack& attack) {
        if (state.currentTick < attack.retreatEndTick) return;

        float survivors = attack.troops * (1.0f - constants::RETREAT_TROOP_LOSS);
        float supplyBack = attack.supply * constants::RETREAT_SUPPLY_BACK;

        returnTroopsToSource(state, attack, survivors);
        returnSupply(state, attack, supplyBack);

        attack.status = AttackStatus::Finished;
    }

    // ── Wybór kafelka frontu ──────────────────────────────────────────────────────

    uint32_t CombatSystem::pickFrontTile(const GameState& state,
        const Attack& attack) const {
        auto* targetProv = const_cast<GameState&>(state)
            .getLandProvince(attack.targetProvinceId);
        if (!targetProv) return 0;

        for (uint32_t tileId : targetProv->tileIds) {
            auto* tile = const_cast<GameState&>(state).getTile(tileId);
            if (!tile) continue;
            if (tile->ownerId == attack.attackerId) continue;
            if (isWater(tile->terrain)) continue;
            if (bordersAttacker(state, tileId, attack.attackerId))
                return tileId;
        }
        return 0;
    }

    bool CombatSystem::bordersAttacker(const GameState& state, uint32_t tileId,
        uint32_t attackerId) const {
        (void)state; (void)tileId; (void)attackerId;
        return true;
    }

    // ── Wzory walki (TODO) ────────────────────────────────────────────────────────

    float CombatSystem::computeAttackerLoss(const GameState& state,
        const Attack& attack,
        const Tile& tile) const {
        (void)state; (void)attack; (void)tile;
        return 0.1f;
    }

    float CombatSystem::computeDefenderLoss(const GameState& state,
        const Attack& attack,
        const Tile& tile) const {
        (void)state; (void)attack; (void)tile;
        return 0.1f;
    }

    float CombatSystem::computeCaptureSpeed(const GameState& state,
        const Attack& attack,
        const Tile& tile) const {
        (void)state; (void)attack;
        return terrainSpeed(tile.terrain);
    }

    float CombatSystem::terrainDef(TerrainType t) const {
        switch (t) {
        case TerrainType::Plains:            return 0.8f;
        case TerrainType::Highlands:         return 5.0f;
        case TerrainType::Mountains:         return 1.2f;
        case TerrainType::FalloutPlains:     return 3.0f;
        case TerrainType::FalloutHighlands:  return 8.0f;
        case TerrainType::FalloutMountains:  return 4.0f;
        default:                             return 1.0f;
        }
    }

    float CombatSystem::terrainSpeed(TerrainType t) const {
        switch (t) {
        case TerrainType::Plains:            return 16.5f;
        case TerrainType::Highlands:         return 20.0f;
        case TerrainType::Mountains:         return 25.0f;
        case TerrainType::FalloutPlains:     return 35.0f;
        case TerrainType::FalloutHighlands:  return 50.0f;
        case TerrainType::FalloutMountains:  return 45.0f;
        default:                             return 20.0f;
        }
    }

    float CombatSystem::largeAttackerBuff(const GameState& state,
        uint32_t attackerId) const {
        (void)state; (void)attackerId;
        return 1.0f;
    }

    float CombatSystem::largeDefenderDebuff(const GameState& state,
        uint32_t defenderId) const {
        (void)state; (void)defenderId;
        return 1.0f;
    }

    float CombatSystem::supplyEfficiencyMod(const Attack& attack) const {
        if (attack.supply > 0.0f) return 1.0f;
        return 0.1f;
    }

    float CombatSystem::defensePostMod(const GameState& state,
        const Tile& tile) const {
        (void)state; (void)tile;
        return 1.0f;
    }

    float CombatSystem::traitorDebuff(const GameState& state,
        uint32_t entityId) const {
        auto* e = const_cast<GameState&>(state).getEntity(entityId);
        if (!e) return 1.0f;
        return (e->traitorDebuffUntilTick > state.currentTick) ? 0.8f : 1.0f;
    }

    // ── Capture i aneksja ─────────────────────────────────────────────────────────

    void CombatSystem::captureTile(GameState& state, uint32_t tileId,
        uint32_t attackerId) {
        auto* tile = state.getTile(tileId);
        if (!tile) return;

        uint32_t prevOwner = tile->ownerId;

        // Fallout znika po zajęciu
        if (isFalloutTerrain(tile->terrain)) {
            tile->terrain = fromFallout(tile->terrain);
            tile->buildingId = 0; // budynki zniszczone przez bombę
        }

        // Aktualizuj poprzedniego właściciela
        if (prevOwner != 0 && prevOwner != attackerId) {
            auto* defShare = state.getShare(tile->landProvinceId, prevOwner);
            if (defShare) {
                auto& ids = defShare->ownedTileIds;
                ids.erase(std::remove(ids.begin(), ids.end(), tileId), ids.end());
                state.markShareDirty(tile->landProvinceId, prevOwner);
            }
        }

        // Przypisz do atakującego
        tile->ownerId = attackerId;
        state.markTileDirty(tileId);

        auto* atkShare = state.getShare(tile->landProvinceId, attackerId);
        if (!atkShare) {
            // Utwórz nowy share jeśli to pierwszy kafelek atakującego w prowincji
            PlayerProvinceShare newShare;
            newShare.entityId = attackerId;
            newShare.provinceId = tile->landProvinceId;
            auto* prov = state.getLandProvince(tile->landProvinceId);
            if (prov) {
                prov->playerShares[attackerId] = std::move(newShare);
                atkShare = prov->getShare(attackerId);
            }
        }
        if (atkShare) {
            atkShare->ownedTileIds.push_back(tileId);
            state.markShareDirty(tile->landProvinceId, attackerId);
        }

        checkAnnexation(state, tile->landProvinceId, attackerId);
    }

    void CombatSystem::checkAnnexation(GameState& state, uint32_t provinceId,
        uint32_t attackerId) {
        (void)state; (void)provinceId; (void)attackerId;
    }

    bool CombatSystem::hasPathToMainTerritory(const GameState& state,
        uint32_t entityId,
        const std::vector<uint32_t>& group) const {
        (void)state; (void)entityId; (void)group;
        return true;
    }

    // ── Helpers ───────────────────────────────────────────────────────────────────

    uint32_t CombatSystem::countEntityTiles(const GameState& state,
        uint32_t entityId) const {
        uint32_t count = 0;
        for (const auto& tile : state.tiles) {
            if (tile.ownerId == entityId && tile.landProvinceId != 0) ++count;
        }
        return count;
    }

    void CombatSystem::returnTroopsToSource(GameState& state,
        const Attack& attack, float troops) {
        auto* share = state.getShare(attack.sourceProvinceId, attack.attackerId);
        if (share) {
            share->population.military += troops;
            state.markShareDirty(attack.sourceProvinceId, attack.attackerId);
        }
    }

    void CombatSystem::returnSupply(GameState& state, const Attack& attack,
        float supply) {
        auto* share = state.getShare(attack.sourceProvinceId, attack.attackerId);
        if (share) {
            share->supplyStored += supply;
            state.markShareDirty(attack.sourceProvinceId, attack.attackerId);
        }
    }

    // ── Obsługa inputów ───────────────────────────────────────────────────────────

    void CombatSystem::handleAttackInput(GameState& state,
        const PlayerInput& input) {
        auto* srcShare = state.getShare(input.provinceId, input.entityId);
        if (!srcShare) return;
        if (srcShare->population.military <= 0.0f) return;

        float troops = srcShare->population.military * input.floatParam;
        if (troops <= 0.0f) return;

        srcShare->population.military -= troops;
        state.markShareDirty(input.provinceId, input.entityId);

        float supply = std::min(srcShare->supplyStored,
            troops * constants::SUPPLY_PER_SOLDIER);
        srcShare->supplyStored -= supply;

        Attack attack;
        attack.id = state.nextAttackId();
        attack.attackerId = input.entityId;
        attack.sourceProvinceId = input.provinceId;
        attack.targetProvinceId = input.targetProvinceId;
        attack.troops = troops;
        attack.supply = supply;
        attack.startTick = state.currentTick;

        state.attacks.push_back(std::move(attack));
    }

    void CombatSystem::handleMultiAttackInput(GameState& state,
        const PlayerInput& input) {
        for (auto& [srcId, tgtId] : input.multiTargets) {
            PlayerInput single = input;
            single.type = InputType::Attack;
            single.provinceId = srcId;
            single.targetProvinceId = tgtId;
            single.multiTargets.clear();
            handleAttackInput(state, single);
        }
    }

    void CombatSystem::handleRetreatInput(GameState& state,
        const PlayerInput& input) {
        // Ponownie chronimy iteratory przed inwalidacją:
        for (size_t i = 0; i < state.attacks.size(); ++i) {
            auto& attack = state.attacks[i];
            if (attack.id != input.unitId) continue;
            if (attack.attackerId != input.entityId) continue;
            if (attack.status != AttackStatus::Active) continue;

            attack.status = AttackStatus::Retreating;
            attack.retreatEndTick = state.currentTick
                + static_cast<uint32_t>(constants::RETREAT_TICKS);
            break;
        }
    }

} // namespace gs::server