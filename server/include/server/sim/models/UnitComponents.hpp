#pragma once
#include <shared/types/Enums.hpp>

#include <cstdint>
#include <vector>
#include <set>

namespace gs::server {

    struct Warship {
        uint32_t      id{ 0 };
        uint32_t      ownerId{ 0 };
        uint32_t      homeBaseId{ 0 };
        HomeBaseType  homeBaseType{ HomeBaseType::Port };
        float         x{ 0.0f };
        float         y{ 0.0f };
        float         health{ 100.0f };
        float         maxHealth{ 100.0f };
        WarshipState  state{ WarshipState::Patrolling };
        uint32_t      targetId{ 0 };
    };

    struct Carrier {
        uint32_t      id{ 0 };
        uint32_t      ownerId{ 0 };
        uint32_t      seaProvinceId{ 0 };
        uint32_t      targetSeaProvinceId{ 0 };
        float         visualX{ 0.0f };
        float         visualY{ 0.0f };
        float         health{ 300.0f };
        float         maxHealth{ 300.0f };
        CarrierState  state{ CarrierState::Stationed };
        uint8_t       level{ 1 };
        std::vector<uint32_t> escortWarshipIds;
    };

    struct SeaCargo {
        uint32_t  id{ 0 };
        uint32_t  ownerId{ 0 };
        uint32_t  homePortId{ 0 };
        uint32_t  destinationPortId{ 0 };
        float     x{ 0.0f };
        float     y{ 0.0f };
        float     health{ 10.0f };
        float     value{ 0.0f };
        bool      isReturning{ false };
        std::vector<std::pair<float, float>> route;
    };

    struct Fighter {
        uint32_t      id{ 0 };
        uint32_t      ownerId{ 0 };
        uint32_t      homeBaseId{ 0 };
        HomeBaseType  homeBaseType{ HomeBaseType::Airport };
        float         x{ 0.0f };
        float         y{ 0.0f };
        float         health{ 80.0f };
        float         maxHealth{ 80.0f };
        FighterState  state{ FighterState::AtBase };
        uint32_t      targetId{ 0 };
        uint32_t      escortTargetId{ 0 };
    };

    struct Bomber {
        uint32_t  id{ 0 };
        uint32_t  ownerId{ 0 };
        uint32_t  homeAirportId{ 0 };
        float     x{ 0.0f };
        float     y{ 0.0f };
        float     health{ 60.0f };
        uint16_t  targetX{ 0 };
        uint16_t  targetY{ 0 };
        bool      hasReleased{ false };
    };

    struct AirCargo {
        uint32_t  id{ 0 };
        uint32_t  ownerId{ 0 };
        uint32_t  homeAirportId{ 0 };
        uint32_t  destinationAirportId{ 0 };
        float     x{ 0.0f };
        float     y{ 0.0f };
        float     health{ 30.0f };
        float     value{ 0.0f };
        bool      isReturning{ false };
    };

    struct MissileFlight {
        uint32_t  id{ 0 };
        uint32_t  ownerId{ 0 };
        float     currentX{ 0.0f };
        float     currentY{ 0.0f };
        float     targetX{ 0.0f };
        float     targetY{ 0.0f };
        bool      intercepted{ false };

        // ZMIANA na std::set, co pozwala na bezpieczn¹ i deterministyczn¹ iteracjê
        std::set<uint32_t> triedInterceptors;
    };

} // namespace gs::server