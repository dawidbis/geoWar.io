#include <gtest/gtest.h>
#include "server/sim/core/WorldGen.hpp"
#include "server/sim/core/GameState.hpp"

using namespace gs::server;

TEST(WorldGenTest, GeneratesCorrectMapDimensions) {
    GameState state;
    WorldGen generator;

    std::vector<LobbyPlayer> players = {
        {1, "Dawid", false},
        {2, "Bot_Easy", true}
    };

    // Generujemy mapê 50x50 z seedem 12345
    generator.generate(state, players, 12345, 50, 50);

    // 1. Sprawdzamy wymiary
    EXPECT_EQ(state.mapWidth, 50);
    EXPECT_EQ(state.mapHeight, 50);
    EXPECT_EQ(state.tiles.size(), 50 * 50);

    // 2. Sprawdzamy, czy wygenerowano prowincje l¹dowe
    EXPECT_GT(state.landProvinces.size(), 0);

    // 3. Sprawdzamy, czy gracze dostali udzia³y w prowincjach (Stolice)
    // Graczy by³o dwóch, wiêc powinni dostaæ swoje startowe kafelki
    bool hasPlayer1 = false;
    bool hasPlayer2 = false;

    for (const auto& prov : state.landProvinces) {
        if (prov.playerShares.count(1)) hasPlayer1 = true;
        if (prov.playerShares.count(2)) hasPlayer2 = true;
    }

    EXPECT_TRUE(hasPlayer1);
    EXPECT_TRUE(hasPlayer2);
}