#pragma once

struct Tile {
    int id = -1;
    bool operator==(const Tile& other) const { return id == other.id; }
    bool operator!=(const Tile& other) const { return id != other.id; }
    bool isEmpty() const { return id < 0; }
};
