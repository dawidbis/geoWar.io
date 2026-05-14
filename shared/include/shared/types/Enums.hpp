#pragma once
#include <cstdint>

namespace gs {

enum class TerrainType : uint8_t {
    Plains            = 0,
    Highlands         = 1,
    Mountains         = 2,
    ShallowWater      = 3,
    DeepWater         = 4,
    FalloutPlains     = 5,
    FalloutHighlands  = 6,
    FalloutMountains  = 7,
};

enum class BuildingType : uint8_t {
    City              = 0,
    Factory           = 1,
    CoalPlant         = 2,
    NuclearPlant      = 3,
    MilitaryBase      = 4,
    DefensePost       = 5,
    AntiMissile       = 6,
    AntiAir           = 7,
    MissileSilo       = 8,
    Port              = 9,
    Airport           = 10,
    Road              = 11,
    LogisticsHub      = 12,
};

enum class UnitType : uint8_t {
    Warship           = 0,
    Carrier           = 1,
    SeaCargo          = 2,
    Fighter           = 3,
    Bomber            = 4,
    AirCargo          = 5,
    Missile           = 6,  // rakieta wodorowa w locie
};

enum class RelationFlag : uint8_t {
    AtWar             = 1 << 0,
    BilateralAlly     = 1 << 1,
    BilateralTrade    = 1 << 2,
    Embargo           = 1 << 3,
    BorderClosed      = 1 << 4,
};

enum class PactType : uint8_t {
    Military          = 0,
    Economic          = 1,
};

enum class AutomationLevel : uint8_t {
    Manual            = 0,
    Assisted          = 1,
    FullAuto          = 2,
};

enum class NuclearPlantMode : uint8_t {
    Civilian          = 0,
    Mixed             = 1,
    Military          = 2,
};

enum class AttackStatus : uint8_t {
    Active            = 0,
    Retreating        = 1,
    Finished          = 2,
};

enum class CarrierState : uint8_t {
    Stationed         = 0,
    EnRoute           = 1,
    Sinking           = 2,
};

enum class BotLevel : uint8_t {
    Trivial           = 0,
    Easy              = 1,
    Medium            = 2,
    Hard              = 3,
    Nightmare         = 4,
};

} // namespace gs
