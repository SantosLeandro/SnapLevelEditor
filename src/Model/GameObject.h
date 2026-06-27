#pragma once

#include <string>
#include <unordered_map>
#include <cstdint>

struct GameObject {
    int64_t id = 0;
    std::string name;
    std::string type;
    float x = 0.0f;
    float y = 0.0f;
    float width = 32.0f;
    float height = 32.0f;
    bool flipX = false;
    bool flipY = false;
    std::unordered_map<std::string, std::string> properties;

    static int64_t nextId() {
        return nextIdCounterRef()++;
    }
    static void resetIdCounter(int64_t next = 1) {
        static int64_t &counter = nextIdCounterRef();
        counter = next;
    }
    static void updateIdCounter(int64_t maxId) {
        static int64_t &counter = nextIdCounterRef();
        if (maxId >= counter) counter = maxId + 1;
    }
private:
    static int64_t& nextIdCounterRef() {
        static int64_t s_idCounter = 1;
        return s_idCounter;
    }
};
