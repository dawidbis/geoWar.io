#pragma once
#include <client/state/ClientState.hpp>
#include <SFML/Graphics.hpp>

namespace gs::client {

/// Renderuje mapę kafelków na kanwas SFML.
/// Obszar mapy = środek okna między lewym a prawym panelem ImGui.
class MapRenderer {
public:
    void init(sf::RenderWindow& window);

    void render(sf::RenderWindow& window, const ClientState& state);

    /// Obsługa zdarzeń myszy (zaznaczanie prowincji, scroll zoom)
    void handleEvent(const sf::Event& event, ClientState& state,
                     sf::RenderWindow& window);

private:
    void updateVertexArray(const ClientState& state);

    sf::Color tileColor(const ClientTile& tile, uint32_t myEntityId) const;
    sf::Color playerColor(uint32_t entityId) const;
    sf::Color terrainBaseColor(TerrainType t) const;

    // Kamera
    float offsetX_{0}, offsetY_{0};
    float zoom_{1.0f};
    bool  dragging_{false};
    sf::Vector2i dragStart_;
    float dragOffsetX_, dragOffsetY_;

    // Rozmiar kafelka na ekranie
    float tileSize_{4.0f};

    // Obszar mapy na ekranie (między panelami ImGui)
    float mapAreaX_{200}, mapAreaY_{30};
    float mapAreaW_{0},   mapAreaH_{0};

    // Vertex array dla efektywnego renderowania
    sf::VertexArray vertices_{sf::Quads};

    uint32_t lastTick_{0}; // odswieżaj tylko gdy zmieniło się
};

} // namespace gs::client
