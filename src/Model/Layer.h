#pragma once

#include <string>
#include <vector>
#include <memory>
#include "Tile.h"
#include "GameObject.h"

class Layer {
public:
    Layer() = default;
    Layer(std::string name, int width, int height);

    Tile& tile(int x, int y);
    const Tile& tile(int x, int y) const;
    void setTile(int x, int y, int tileId);
    void setTile(int x, int y, Tile t);
    int tileId(int x, int y) const;
    bool isEmpty(int x, int y) const;

    void resize(int newWidth, int newHeight);
    void fill(int tileId);
    void fill(int x, int y, int w, int h, int tileId);
    void clear();
    void clearArea(int x, int y, int w, int h);
    void floodFill(int x, int y, int newTileId);

    GameObject& addObject(const GameObject& obj);
    void removeObject(int64_t id);
    GameObject* object(int64_t id);
    const GameObject* object(int64_t id) const;

    const std::string& name() const { return m_name; }
    void setName(const std::string& name) { m_name = name; }

    const std::string& tileset() const { return m_tileset; }
    void setTileset(const std::string &path) { m_tileset = path; }

    int width() const { return m_width; }
    int height() const { return m_height; }

    bool visible() const { return m_visible; }
    void setVisible(bool v) { m_visible = v; }

    bool locked() const { return m_locked; }
    void setLocked(bool l) { m_locked = l; }

    float opacity() const { return m_opacity; }
    void setOpacity(float o) { m_opacity = o; }

    int zOrder() const { return m_zOrder; }
    void setZOrder(int z) { m_zOrder = z; }

    int tileCount() const { return (int)m_tiles.size(); }
    const Tile* tileData() const { return m_tiles.data(); }
    Tile* tileData() { return m_tiles.data(); }

    const std::vector<GameObject>& objects() const { return m_objects; }
    std::vector<GameObject>& objects() { return m_objects; }

private:
    std::string m_name;
    int m_width = 0;
    int m_height = 0;
    std::vector<Tile> m_tiles;
    std::vector<GameObject> m_objects;
    bool m_visible = true;
    bool m_locked = false;
    float m_opacity = 1.0f;
    int m_zOrder = 0;
    std::string m_tileset;

    int index(int x, int y) const;
    void checkCoords(int x, int y) const;
};
