#include "World.h"
#include <algorithm>
#include <stdexcept>

World::World(std::string name)
    : m_name(std::move(name))
{
}

Room* World::addRoom(std::string name) {
    auto room = std::make_unique<Room>(std::move(name), 20, 15, m_defaultTileSize);
    Room* ptr = room.get();
    m_rooms.push_back(std::move(room));
    return ptr;
}

Room* World::addRoom(std::unique_ptr<Room> room) {
    Room* ptr = room.get();
    m_rooms.push_back(std::move(room));
    return ptr;
}

void World::removeRoom(int index) {
    if (index < 0 || index >= (int)m_rooms.size())
        throw std::out_of_range("Room index out of range");
    m_rooms.erase(m_rooms.begin() + index);
}

bool World::removeRoomByName(const std::string& name) {
    for (int i = 0; i < (int)m_rooms.size(); ++i) {
        if (m_rooms[i]->name() == name) {
            removeRoom(i);
            return true;
        }
    }
    return false;
}

Room* World::room(int index) {
    if (index < 0 || index >= (int)m_rooms.size()) return nullptr;
    return m_rooms[index].get();
}

const Room* World::room(int index) const {
    if (index < 0 || index >= (int)m_rooms.size()) return nullptr;
    return m_rooms[index].get();
}

Room* World::roomByName(const std::string& name) {
    for (auto& r : m_rooms) {
        if (r->name() == name) return r.get();
    }
    return nullptr;
}

const Room* World::roomByName(const std::string& name) const {
    for (const auto& r : m_rooms) {
        if (r->name() == name) return r.get();
    }
    return nullptr;
}
