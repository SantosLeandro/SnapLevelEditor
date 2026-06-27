#include "Room.h"
#include <algorithm>
#include <stdexcept>

Room::Room(std::string name, int width, int height, int tileSize)
    : m_name(std::move(name))
    , m_width(width)
    , m_height(height)
    , m_tileSize(tileSize)
{
}

Layer* Room::addLayer(std::string name) {
    auto layer = std::make_unique<Layer>(std::move(name), m_width, m_height);
    layer->setZOrder((int)m_layers.size());
    Layer* ptr = layer.get();
    m_layers.push_back(std::move(layer));
    return ptr;
}

Layer* Room::insertLayer(int index, std::string name) {
    if (index < 0 || index > (int)m_layers.size())
        throw std::out_of_range("Layer index out of range");

    auto layer = std::make_unique<Layer>(std::move(name), m_width, m_height);
    layer->setZOrder(index);
    Layer* ptr = layer.get();
    m_layers.insert(m_layers.begin() + index, std::move(layer));

    for (int i = index + 1; i < (int)m_layers.size(); ++i)
        m_layers[i]->setZOrder(i);

    return ptr;
}

void Room::removeLayer(int index) {
    if (index < 0 || index >= (int)m_layers.size())
        throw std::out_of_range("Layer index out of range");
    m_layers.erase(m_layers.begin() + index);

    for (int i = index; i < (int)m_layers.size(); ++i)
        m_layers[i]->setZOrder(i);
}

bool Room::removeLayerByName(const std::string& name) {
    for (int i = 0; i < (int)m_layers.size(); ++i) {
        if (m_layers[i]->name() == name) {
            removeLayer(i);
            return true;
        }
    }
    return false;
}

void Room::moveLayer(int fromIndex, int toIndex) {
    if (fromIndex < 0 || fromIndex >= (int)m_layers.size())
        throw std::out_of_range("Source layer index out of range");
    if (toIndex < 0 || toIndex >= (int)m_layers.size())
        throw std::out_of_range("Target layer index out of range");
    if (fromIndex == toIndex) return;

    auto layer = std::move(m_layers[fromIndex]);
    m_layers.erase(m_layers.begin() + fromIndex);
    m_layers.insert(m_layers.begin() + toIndex, std::move(layer));

    for (int i = 0; i < (int)m_layers.size(); ++i)
        m_layers[i]->setZOrder(i);
}

Layer* Room::layer(int index) {
    if (index < 0 || index >= (int)m_layers.size()) return nullptr;
    return m_layers[index].get();
}

const Layer* Room::layer(int index) const {
    if (index < 0 || index >= (int)m_layers.size()) return nullptr;
    return m_layers[index].get();
}

Layer* Room::layerByName(const std::string& name) {
    for (auto& l : m_layers) {
        if (l->name() == name) return l.get();
    }
    return nullptr;
}

const Layer* Room::layerByName(const std::string& name) const {
    for (const auto& l : m_layers) {
        if (l->name() == name) return l.get();
    }
    return nullptr;
}

void Room::resize(int newWidth, int newHeight) {
    m_width = newWidth;
    m_height = newHeight;
    for (auto& l : m_layers) {
        l->resize(newWidth, newHeight);
    }
}
