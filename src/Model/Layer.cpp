#include "Layer.h"
#include <algorithm>
#include <stdexcept>
#include <cassert>

Layer::Layer(std::string name, int width, int height)
    : m_name(std::move(name))
    , m_width(width)
    , m_height(height)
    , m_tiles(width * height)
{
}

int Layer::index(int x, int y) const {
    return y * m_width + x;
}

void Layer::checkCoords(int x, int y) const {
    if (x < 0 || x >= m_width || y < 0 || y >= m_height) {
        throw std::out_of_range("Tile coordinates out of bounds: " +
                                std::to_string(x) + "," + std::to_string(y) +
                                " for layer size " + std::to_string(m_width) +
                                "x" + std::to_string(m_height));
    }
}

Tile& Layer::tile(int x, int y) {
    checkCoords(x, y);
    return m_tiles[index(x, y)];
}

const Tile& Layer::tile(int x, int y) const {
    checkCoords(x, y);
    return m_tiles[index(x, y)];
}

void Layer::setTile(int x, int y, int tileId) {
    checkCoords(x, y);
    m_tiles[index(x, y)].id = tileId;
}

void Layer::setTile(int x, int y, Tile t) {
    checkCoords(x, y);
    m_tiles[index(x, y)] = t;
}

int Layer::tileId(int x, int y) const {
    checkCoords(x, y);
    return m_tiles[index(x, y)].id;
}

bool Layer::isEmpty(int x, int y) const {
    checkCoords(x, y);
    return m_tiles[index(x, y)].isEmpty();
}

void Layer::resize(int newWidth, int newHeight) {
    if (newWidth == m_width && newHeight == m_height)
        return;

    std::vector<Tile> newTiles(newWidth * newHeight, Tile{});

    int copyW = std::min(m_width, newWidth);
    int copyH = std::min(m_height, newHeight);

    for (int y = 0; y < copyH; ++y) {
        for (int x = 0; x < copyW; ++x) {
            newTiles[y * newWidth + x] = m_tiles[y * m_width + x];
        }
    }

    m_tiles = std::move(newTiles);
    m_width = newWidth;
    m_height = newHeight;
}

void Layer::fill(int tileId) {
    for (auto& t : m_tiles) {
        t.id = tileId;
    }
}

void Layer::fill(int x, int y, int w, int h, int tileId) {
    int xEnd = std::min(x + w, m_width);
    int yEnd = std::min(y + h, m_height);
    for (int row = y; row < yEnd; ++row) {
        for (int col = x; col < xEnd; ++col) {
            m_tiles[index(col, row)].id = tileId;
        }
    }
}

void Layer::clear() {
    fill(-1);
}

void Layer::clearArea(int x, int y, int w, int h) {
    fill(x, y, w, h, -1);
}

void Layer::floodFill(int startX, int startY, int newTileId) {
    int oldTileId = tileId(startX, startY);
    if (oldTileId == newTileId) return;

    struct Pos { int x, y; };
    std::vector<Pos> stack;
    stack.push_back({startX, startY});

    while (!stack.empty()) {
        Pos p = stack.back();
        stack.pop_back();

        if (p.x < 0 || p.x >= m_width || p.y < 0 || p.y >= m_height)
            continue;
        if (m_tiles[index(p.x, p.y)].id != oldTileId)
            continue;

        m_tiles[index(p.x, p.y)].id = newTileId;

        stack.push_back({p.x + 1, p.y});
        stack.push_back({p.x - 1, p.y});
        stack.push_back({p.x, p.y + 1});
        stack.push_back({p.x, p.y - 1});
    }
}

GameObject& Layer::addObject(const GameObject& obj) {
    m_objects.push_back(obj);
    return m_objects.back();
}

void Layer::removeObject(int64_t id) {
    auto it = std::find_if(m_objects.begin(), m_objects.end(),
        [id](const GameObject& o) { return o.id == id; });
    if (it != m_objects.end()) {
        m_objects.erase(it);
    }
}

GameObject* Layer::object(int64_t id) {
    auto it = std::find_if(m_objects.begin(), m_objects.end(),
        [id](const GameObject& o) { return o.id == id; });
    return it != m_objects.end() ? &*it : nullptr;
}

const GameObject* Layer::object(int64_t id) const {
    auto it = std::find_if(m_objects.begin(), m_objects.end(),
        [id](const GameObject& o) { return o.id == id; });
    return it != m_objects.end() ? &*it : nullptr;
}
