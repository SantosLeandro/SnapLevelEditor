#pragma once

#include <string>
#include <vector>
#include <memory>
#include "Layer.h"

class Room {
public:
    Room() = default;
    Room(std::string name, int width, int height, int tileSize = 32);

    Layer* addLayer(std::string name);
    Layer* insertLayer(int index, std::string name);
    void removeLayer(int index);
    bool removeLayerByName(const std::string& name);
    void moveLayer(int fromIndex, int toIndex);

    Layer* layer(int index);
    const Layer* layer(int index) const;
    Layer* layerByName(const std::string& name);
    const Layer* layerByName(const std::string& name) const;
    int layerCount() const { return (int)m_layers.size(); }

    const std::string& name() const { return m_name; }
    void setName(const std::string& name) { m_name = name; }

    int width() const { return m_width; }
    int height() const { return m_height; }
    void resize(int newWidth, int newHeight);

    int tileSize() const { return m_tileSize; }
    void setTileSize(int tileSize) { m_tileSize = tileSize; }

    int pixelWidth() const { return m_width * m_tileSize; }
    int pixelHeight() const { return m_height * m_tileSize; }

    int worldX() const { return m_worldX; }
    int worldY() const { return m_worldY; }
    void setWorldX(int x) { m_worldX = x; }
    void setWorldY(int y) { m_worldY = y; }

private:
    std::string m_name;
    int m_width = 20;
    int m_height = 15;
    int m_tileSize = 32;
    int m_worldX = 0;
    int m_worldY = 0;
    std::vector<std::unique_ptr<Layer>> m_layers;
};
