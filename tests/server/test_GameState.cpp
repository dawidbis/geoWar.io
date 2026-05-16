#include <gtest/gtest.h>
#include "server/sim/core/GameState.hpp"

using namespace gs::server;

TEST(GameStateTest, IdGeneratorsWorkCorrectly) {
    GameState state;

    // Sprawdzamy czy generatory startuj¹ od 1 i inkrementuj¹ poprawnie
    EXPECT_EQ(state.nextBuildingId(), 1);
    EXPECT_EQ(state.nextBuildingId(), 2);

    EXPECT_EQ(state.nextUnitId(), 1);
    EXPECT_EQ(state.nextAttackId(), 1);
}

TEST(GameStateTest, GetEntityReturnsCorrectPointer) {
    GameState state;

    Entity e1; e1.id = 1; e1.name = "Player 1";
    Entity e2; e2.id = 2; e2.name = "Player 2";

    state.entities.push_back(e1);
    state.entities.push_back(e2);

    // Szukamy istniej¹cej encji
    auto* found = state.getEntity(2);
    ASSERT_NE(found, nullptr); // Upewniamy siê, ¿e wskaŸnik nie jest nullem
    EXPECT_EQ(found->name, "Player 2");

    // Szukamy nieistniej¹cej encji
    auto* notFound = state.getEntity(99);
    EXPECT_EQ(notFound, nullptr);
}