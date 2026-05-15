#pragma once
#include <cstdint>

namespace gs {

    // ── Teren ─────────────────────────────────────────────────────────────────────

    enum class TerrainType : uint8_t {
        Plains = 0,
        Highlands = 1,
        Mountains = 2,
        ShallowWater = 3,
        DeepWater = 4,
        FalloutPlains = 5,
        FalloutHighlands = 6,
        FalloutMountains = 7,
    };

    inline TerrainType toFallout(TerrainType t) {
        switch (t) {
        case TerrainType::Plains:    return TerrainType::FalloutPlains;
        case TerrainType::Highlands: return TerrainType::FalloutHighlands;
        case TerrainType::Mountains: return TerrainType::FalloutMountains;
        default:                     return t; // już fallout lub woda
        }
    }

    inline TerrainType fromFallout(TerrainType t) {
        switch (t) {
        case TerrainType::FalloutPlains:    return TerrainType::Plains;
        case TerrainType::FalloutHighlands: return TerrainType::Highlands;
        case TerrainType::FalloutMountains: return TerrainType::Mountains;
        default:                            return t;
        }
    }

    inline bool isFalloutTerrain(TerrainType t) {
        return t == TerrainType::FalloutPlains
            || t == TerrainType::FalloutHighlands
            || t == TerrainType::FalloutMountains;
    }

    inline bool isWater(TerrainType t) {
        return t == TerrainType::ShallowWater || t == TerrainType::DeepWater;
    }

    // ── Budynki ───────────────────────────────────────────────────────────────────

    enum class BuildingType : uint8_t {
        City = 0,
        Factory = 1,
        CoalPlant = 2,
        NuclearPlant = 3,
        MilitaryBase = 4,
        DefensePost = 5,
        AntiMissile = 6,
        AntiAir = 7,
        MissileSilo = 8,
        Port = 9,
        Airport = 10,
        Road = 11,
        LogisticsHub = 12,
    };

    // ── Jednostki ─────────────────────────────────────────────────────────────────

    enum class UnitType : uint8_t {
        Warship = 0,
        Carrier = 1,
        SeaCargo = 2,
        Fighter = 3,
        Bomber = 4,
        AirCargo = 5,
        Missile = 6,
    };

    enum class HomeBaseType : uint8_t {
        Port = 0,
        Carrier = 1,
        Airport = 2,
    };

    enum class WarshipState : uint8_t {
        Patrolling = 0,
        Engaging = 1,
        Returning = 2,
        Resupply = 3,
        MovingToBase = 4,
    };

    enum class FighterState : uint8_t {
        AtBase = 0,
        Patrolling = 1,
        Engaging = 2,
        Returning = 3,
        Escorting = 4,
        ShotDown = 5,
    };

    enum class CarrierState : uint8_t {
        Stationed = 0,
        EnRoute = 1,
        Sinking = 2,
    };

    // ── Dyplomacja ────────────────────────────────────────────────────────────────

    enum class RelationFlag : uint8_t {
        AtWar = 1 << 0,
        BilateralAlly = 1 << 1,
        BilateralTrade = 1 << 2,
        Embargo = 1 << 3,
        BorderClosed = 1 << 4,
    };

    enum class PactType : uint8_t {
        Military = 0,
        Economic = 1,
    };

    enum class VoteResult : uint8_t {
        Pending = 0,
        Yes = 1,
        No = 2,
    };

    // ── Walka ─────────────────────────────────────────────────────────────────────

    enum class AttackStatus : uint8_t {
        Active = 0,
        Retreating = 1,
        Finished = 2,
    };

    // ── Populacja / automatyzacja ─────────────────────────────────────────────────

    enum class AutomationLevel : uint8_t {
        Manual = 0,
        Assisted = 1,
        FullAuto = 2,
    };

    // ── Budynki specjalne ─────────────────────────────────────────────────────────

    enum class NuclearPlantMode : uint8_t {
        Civilian = 0,
        Mixed = 1,
        Military = 2,
    };

    // ── Encje ─────────────────────────────────────────────────────────────────────

    enum class EntityType : uint8_t {
        Human = 0,
        Bot = 1,
        Disconnected = 2,
    };

    enum class BotLevel : uint8_t {
        Trivial = 0,
        Easy = 1,
        Medium = 2,
        Hard = 3,
        Nightmare = 4,
    };

    // ── Gra ───────────────────────────────────────────────────────────────────────

    enum class GamePhase : uint8_t {
        Lobby = 0,
        Playing = 1,
        Finished = 2,
    };

    enum class GameEventType : uint8_t {
        PlayerJoined = 0,
        PlayerEliminated = 1,
        WarDeclared = 2,
        PactFormed = 3,
        AtomicBombDetonation = 4,
        HydrogenDetonation = 5,
        ExplosionVisual = 6,
        PiracyIncident = 7,
        VictoryAchieved = 8,
    };

    // ── Inputy gracza ─────────────────────────────────────────────────────────────

    enum class InputType : uint8_t {
        Attack = 0,
        MultiAttack = 1,
        Retreat = 2,
        Build = 3,
        UpgradeBuilding = 4,
        DestroyBuilding = 5,
        SetWorkerSplit = 6,
        SetAutomation = 7,
        DeclareWar = 8,
        ProposePact = 9,
        PactVote = 10,
        LeavePact = 11,
        SellProvince = 12,
        TransferPopulation = 13,
        SetNuclearMode = 14,
        BuildUnit = 15,
        MoveCarrier = 16,
        SetUnitHomeBase = 17,
        LaunchMissile = 18,
        LaunchBomber = 19,
    };

} // namespace gs