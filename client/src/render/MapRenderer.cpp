#include "client/render/MapRenderer.hpp"
#include <cmath>

namespace gs::client {

// Kolory graczy — muszą odpowiadać UIRenderer
static const sf::Color PLAYER_COLORS_SF[] = {
    {80,  112, 212},  // niebieski
    {216,  90,  48},  // pomarańczowy
    { 95, 200, 165},  // zielony
    {212,  85, 125},  // różowy
    {237, 200,  38},  // żółty
    {136,  85, 180},  // fioletowy
    { 50, 185, 220},  // cyan
    {204, 140,  56},  // złoty
    { 80,  80,  80},  // szary (wilderness/woda)
};

void MapRenderer::init(sf::RenderWindow& window) {
    auto size = window.getSize();
    mapAreaW_ = static_cast<float>(size.x) - 200.0f - 180.0f;
    mapAreaH_ = static_cast<float>(size.y) - 30.0f;
}

void MapRenderer::render(sf::RenderWindow& window,
                          const ClientState& state) {
    auto size = window.getSize();
    mapAreaW_ = static_cast<float>(size.x) - 200.0f - 180.0f;
    mapAreaH_ = static_cast<float>(size.y) - 30.0f;

    // Clip do obszaru mapy
    sf::View mapView(sf::FloatRect(mapAreaX_, mapAreaY_,
                                   mapAreaW_, mapAreaH_));
    mapView.setViewport({
        mapAreaX_ / size.x,
        mapAreaY_ / size.y,
        mapAreaW_ / size.x,
        mapAreaH_ / size.y
    });
    window.setView(mapView);

    // Tło
    sf::RectangleShape bg({mapAreaW_, mapAreaH_});
    bg.setPosition(mapAreaX_, mapAreaY_);
    bg.setFillColor({20, 24, 36});
    window.draw(bg);

    // Odśwież vertex array gdy tick się zmienił
    if (state.currentTick != lastTick_ || vertices_.getVertexCount() == 0) {
        updateVertexArray(state);
        lastTick_ = state.currentTick;
    }

    // Zastosuj kamerę — transformacja
    sf::Transform transform;
    transform.translate(offsetX_, offsetY_);
    transform.scale(zoom_, zoom_);
    window.draw(vertices_, transform);

    // Podświetl zaznaczoną prowincję
    if (state.selectedProvinceId != 0) {
        for (const auto& tile : state.tiles) {
            if (tile.landProvinceId != state.selectedProvinceId) continue;

            float px = mapAreaX_ + offsetX_ + tile.x * tileSize_ * zoom_;
            float py = mapAreaY_ + offsetY_ + tile.y * tileSize_ * zoom_;
            float ts = tileSize_ * zoom_;

            sf::RectangleShape rect({ts, ts});
            rect.setPosition(px, py);
            rect.setFillColor({255, 255, 255, 30});
            rect.setOutlineColor({255, 255, 255, 80});
            rect.setOutlineThickness(0.5f);
            window.draw(rect);
        }
    }

    // Minimap
    float mmW = 80, mmH = 60;
    float mmX = mapAreaX_ + mapAreaW_ - mmW - 8;
    float mmY = mapAreaY_ + mapAreaH_ - mmH - 8;

    sf::RectangleShape mmBg({mmW, mmH});
    mmBg.setPosition(mmX, mmY);
    mmBg.setFillColor({0, 0, 0, 160});
    mmBg.setOutlineColor({80, 80, 80});
    mmBg.setOutlineThickness(0.5f);
    window.draw(mmBg);

    // Kafelki na minimapie
    if (state.mapWidth > 0 && state.mapHeight > 0) {
        float mmTx = mmW / state.mapWidth;
        float mmTy = mmH / state.mapHeight;
        sf::VertexArray mmVerts(sf::Quads,
            state.tiles.size() * 4);
        size_t vi = 0;
        for (const auto& tile : state.tiles) {
            sf::Color col = playerColor(tile.ownerId);
            if (tile.landProvinceId == 0) col = {0, 15, 40};

            float tx = mmX + tile.x * mmTx;
            float ty = mmY + tile.y * mmTy;
            mmVerts[vi+0] = {{tx,       ty},       col};
            mmVerts[vi+1] = {{tx+mmTx,  ty},       col};
            mmVerts[vi+2] = {{tx+mmTx,  ty+mmTy},  col};
            mmVerts[vi+3] = {{tx,       ty+mmTy},  col};
            vi += 4;
        }
        window.draw(mmVerts);
    }

    // Przywróć domyślny widok
    window.setView(window.getDefaultView());
}

void MapRenderer::updateVertexArray(const ClientState& state) {
    vertices_.clear();
    vertices_.resize(state.tiles.size() * 4);

    size_t i = 0;
    for (const auto& tile : state.tiles) {
        sf::Color col = tileColor(tile, state.myEntityId);

        float px = mapAreaX_ + tile.x * tileSize_;
        float py = mapAreaY_ + tile.y * tileSize_;
        float ts = tileSize_;

        vertices_[i+0] = {{px,    py},    col};
        vertices_[i+1] = {{px+ts, py},    col};
        vertices_[i+2] = {{px+ts, py+ts}, col};
        vertices_[i+3] = {{px,    py+ts}, col};
        i += 4;
    }
}

sf::Color MapRenderer::tileColor(const ClientTile& tile,
                                  uint32_t myEntityId) const {
    // Woda
    if (tile.landProvinceId == 0) {
        if (tile.terrain == TerrainType::ShallowWater) return {0, 60, 120};
        return {0, 20, 60};
    }
    // Niczyje
    if (tile.ownerId == 0) {
        return terrainBaseColor(tile.terrain);
    }
    // Własne terytorium — jaśniejszy kolor gracza
    sf::Color base = playerColor(tile.ownerId);
    if (tile.ownerId == myEntityId) {
        // Własne — nieco jaśniejsze
        return {
            static_cast<uint8_t>(std::min(255, base.r + 40)),
            static_cast<uint8_t>(std::min(255, base.g + 40)),
            static_cast<uint8_t>(std::min(255, base.b + 40))
        };
    }
    return base;
}

sf::Color MapRenderer::playerColor(uint32_t entityId) const {
    if (entityId == 0) return {50, 50, 50};
    return PLAYER_COLORS_SF[(entityId - 1) % 8];
}

sf::Color MapRenderer::terrainBaseColor(TerrainType t) const {
    switch (t) {
        case TerrainType::Plains:           return {60, 80, 45};
        case TerrainType::Highlands:        return {80, 90, 55};
        case TerrainType::Mountains:        return {90, 85, 80};
        case TerrainType::FalloutPlains:    return {60, 55, 30};
        case TerrainType::FalloutHighlands: return {70, 60, 35};
        case TerrainType::FalloutMountains: return {80, 70, 50};
        default: return {40, 40, 40};
    }
}

void MapRenderer::handleEvent(const sf::Event& event,
                               ClientState& state,
                               sf::RenderWindow& window) {
    if (event.type == sf::Event::MouseWheelScrolled) {
        float factor = event.mouseWheelScroll.delta > 0 ? 1.15f : 0.87f;
        zoom_ = std::max(0.5f, std::min(zoom_ * factor, 16.0f));
    }

    if (event.type == sf::Event::MouseButtonPressed &&
        event.mouseButton.button == sf::Mouse::Middle) {
        dragging_    = true;
        dragStart_   = {event.mouseButton.x, event.mouseButton.y};
        dragOffsetX_ = offsetX_;
        dragOffsetY_ = offsetY_;
    }
    if (event.type == sf::Event::MouseButtonReleased &&
        event.mouseButton.button == sf::Mouse::Middle) {
        dragging_ = false;
    }
    if (event.type == sf::Event::MouseMoved && dragging_) {
        offsetX_ = dragOffsetX_ + (event.mouseMove.x - dragStart_.x);
        offsetY_ = dragOffsetY_ + (event.mouseMove.y - dragStart_.y);
    }

    // Kliknięcie lewym → zaznacz prowincję
    if (event.type == sf::Event::MouseButtonPressed &&
        event.mouseButton.button == sf::Mouse::Left) {

        float mx = event.mouseButton.x;
        float my = event.mouseButton.y;

        // Sprawdź czy w obszarze mapy
        if (mx < mapAreaX_ || mx > mapAreaX_ + mapAreaW_) return;
        if (my < mapAreaY_ || my > mapAreaY_ + mapAreaH_) return;

        // Przelicz na współrzędne kafelka
        float wx = (mx - mapAreaX_ - offsetX_) / (tileSize_ * zoom_);
        float wy = (my - mapAreaY_ - offsetY_) / (tileSize_ * zoom_);

        int tx = static_cast<int>(wx);
        int ty = static_cast<int>(wy);

        // Znajdź kafelek o tych współrzędnych
        for (const auto& tile : state.tiles) {
            if (tile.x == tx && tile.y == ty) {
                state.selectedProvinceId = tile.landProvinceId;
                break;
            }
        }
    }
}

} // namespace gs::client
