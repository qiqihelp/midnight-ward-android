// levelmap.h — 三层地图 + 门锁 + 楼层传送（LevelMap.java 的 C++ 移植）。
#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

class Game;

namespace lm {

constexpr int F2 = 0, F1 = 1, B1 = 2;
constexpr int FLOOR_COUNT = 3;

constexpr int CELL_FLOOR  = 0;
constexpr int CELL_PLASTER = 1;
constexpr int CELL_TILE   = 2;
constexpr int CELL_BLOOD  = 3;
constexpr int CELL_DOOR   = 4;
constexpr int CELL_METAL  = 5;
constexpr int CELL_GATE   = 6;
constexpr int CELL_STAIR  = 7;

constexpr int LOCK_NONE     = 0;
constexpr int LOCK_ROOM     = 1; // 病房钥匙
constexpr int LOCK_BASEMENT = 2; // 地下室钥匙

struct DoorState {
    int floor = 0, x = 0, y = 0, cellType = 0;
    float openness = 0.f;   // 0 关 … 1 全开
    bool opening = false;
    int lock = LOCK_NONE;
    bool isAutoOpen() const {
        return cellType == CELL_STAIR || (floor == B1 && cellType == CELL_METAL);
    }
};

struct Transition {
    int floor;
    float x, y, angle;
};

struct Spawn {
    int floor = 0;
    float x = 0, y = 0, angle = 0;
};

} // namespace lm

class LevelMap {
public:
    // grid[floor][y][x]
    std::vector<std::vector<std::vector<int>>> grid;
    std::unordered_map<std::string, lm::DoorState> doors;
    std::unordered_map<std::string, lm::Transition> transitions;
    lm::Spawn spawn;

    LevelMap(); // 构造时 parse + validate

    static std::string key(int floor, int x, int y);
    int cellAt(int floor, int cx, int cy) const;
    bool blocked(Game* g, int floor, float fx, float fy) const;
    bool checkValid() const { return validate(); } // 供测试 / 启动自检

private:
    void parse();
    bool validate() const; // 返回是否合法（合法图必须恒为 true）
};
