#pragma once

#include <string>
#include <vector>
#include <memory>
#include "Room.h"

class World {
public:
    World() = default;
    explicit World(std::string name);

    Room* addRoom(std::string name);
    Room* addRoom(std::unique_ptr<Room> room);
    void removeRoom(int index);
    bool removeRoomByName(const std::string& name);

    Room* room(int index);
    const Room* room(int index) const;
    Room* roomByName(const std::string& name);
    const Room* roomByName(const std::string& name) const;
    int roomCount() const { return (int)m_rooms.size(); }

    const std::string& name() const { return m_name; }
    void setName(const std::string& name) { m_name = name; }

    int defaultTileSize() const { return m_defaultTileSize; }
    void setDefaultTileSize(int size) { m_defaultTileSize = size; }

private:
    std::string m_name = "Untitled";
    int m_defaultTileSize = 32;
    std::vector<std::unique_ptr<Room>> m_rooms;
};
