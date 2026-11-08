// levelmap.cpp — LevelMap.java 的 C++ 移植。
#include "levelmap.h"
#include "game.h"
#include <cmath>
#include <cstdio>

// 图例：'#' 涂料墙  '=' 瓷砖墙  '%' 血迹墙
//       'D' 木门  'B' 金属门  'G' 大门(木板/读卡器)  'S' 楼梯门  '.' 地板
static const char* MAP_F2[] = {
    "########################", // 0
    "#......#........#......#", // 1  病房A(起点) / 病房B / 病房C
    "#......%........#......#", // 2
    "#......#........%......#", // 3
    "#......#........#......#", // 4
    "#......#........#......#", // 5
    "#......#........#......#", // 6
    "###D#######D#######D####", // 7  房门 x3 / x11 / x19
    "#......................#", // 8  走廊
    "#......................#", // 9
    "#......................#", // 10
    "####D########D######S###", // 11 值班室门x4 / 杂物间门x13 / 楼梯x20
    "#.........#............#", // 12 值班室 x1-9 / 杂物间 x11-22
    "#.........#............#", // 13
    "#.........#............#", // 14
    "#.........#............#", // 15
    "#.........#............#", // 16
    "########################", // 17
};
static const char* MAP_F1[] = {
    "########################", // 0
    "#........#......#......#", // 1  药房 x1-8 / 院长办公室 x10-15 / 楼梯间 x17-22
    "#........#......#......#", // 2
    "#........#......#......#", // 3
    "#........#......#......#", // 4
    "#........#......#......#", // 5
    "#........#......#......#", // 6
    "####D#########D####S####", // 7  药房门x4 / 办公室门x14 / 下楼楼梯x19
    "#......................#", // 8  大厅
    "#......................#", // 9
    "#......................#", // 10
    "#....=......=..........#", // 11 立柱
    "#......................B", // 12 通往地下室的铁门 x23
    "#......................#", // 13
    "#....=......=..........#", // 14 立柱
    "#......................#", // 15
    "#......................#", // 16
    "###########G############", // 17 大门（木板×2 + 读卡器）x11
};
static const char* MAP_B1[] = {
    "####################", // 0
    "#..................#", // 1
    "#...====...====....#", // 2
    "#...#........%.....#", // 3
    "#...#........#.....#", // 4
    "#...#........#.....#", // 5
    "#...====...====....#", // 6
    "B..................#", // 7  回程铁门 x0
    "#..................#", // 8
    "#...====...====....#", // 9
    "#...#........#.....#", // 10
    "#...#........D.....#", // 11 撬棍间木门 x13
    "#...====...====....#", // 12
    "################%###", // 13
};

LevelMap::LevelMap() {
    parse();
    // 合法性校验失败属于程序数据错误——直接终止（构造期错误，非运行期）
    if (!validate()) {
        std::fprintf(stderr, "LevelMap validate failed!\n");
        // 不抛异常：保留数据但主循环外层会再次检查
    }
}

std::string LevelMap::key(int floor, int x, int y) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%d:%d:%d", floor, x, y);
    return std::string(buf);
}

void LevelMap::parse() {
    grid.assign(lm::FLOOR_COUNT, {});
    for (int f = 0; f < lm::FLOOR_COUNT; f++) {
        int rows = (f == lm::F2) ? (int)(sizeof(MAP_F2) / sizeof(MAP_F2[0]))
                : (f == lm::F1) ? (int)(sizeof(MAP_F1) / sizeof(MAP_F1[0]))
                                 : (int)(sizeof(MAP_B1) / sizeof(MAP_B1[0]));
        grid[f].assign(rows, {});
        for (int y = 0; y < rows; y++) {
            const char* r = (f == lm::F2) ? MAP_F2[y]
                         : (f == lm::F1) ? MAP_F1[y]
                                          : MAP_B1[y];
            int len = (int)std::string(r).size();
            grid[f][y].assign(len, lm::CELL_FLOOR);
            for (int x = 0; x < len; x++) {
                char ch = r[x];
                int cell;
                switch (ch) {
                    case '#': cell = lm::CELL_PLASTER; break;
                    case '=': cell = lm::CELL_TILE; break;
                    case '%': cell = lm::CELL_BLOOD; break;
                    case 'D': cell = lm::CELL_DOOR; break;
                    case 'B': cell = lm::CELL_METAL; break;
                    case 'G': cell = lm::CELL_GATE; break;
                    case 'S': cell = lm::CELL_STAIR; break;
                    default:  cell = lm::CELL_FLOOR; break;
                }
                grid[f][y][x] = cell;
                if (cell == lm::CELL_DOOR || cell == lm::CELL_METAL
                    || cell == lm::CELL_STAIR) {
                    lm::DoorState d;
                    d.floor = f; d.x = x; d.y = y; d.cellType = cell;
                    if (f == lm::F2 && x == 3 && y == 7) d.lock = lm::LOCK_ROOM;
                    if (f == lm::F1 && x == 23 && y == 12) d.lock = lm::LOCK_BASEMENT;
                    doors[key(f, x, y)] = d;
                }
            }
        }
    }

    using lm::Transition;
    const float PI = 3.14159265f;
    transitions[key(lm::F2, 20, 11)] = Transition{lm::F1, 19.5f, 8.8f, PI / 2};
    transitions[key(lm::F1, 19, 7)]  = Transition{lm::F2, 20.5f, 10.2f, -PI / 2};
    transitions[key(lm::F1, 23, 12)] = Transition{lm::B1, 1.9f, 7.5f, 0.f};
    transitions[key(lm::B1, 0, 7)]   = Transition{lm::F1, 22.0f, 12.5f, PI};

    spawn.floor = lm::F2;
    spawn.x = 3.5f;
    spawn.y = 3.2f;
    spawn.angle = PI / 2;
}

bool LevelMap::validate() const {
    for (int f = 0; f < lm::FLOOR_COUNT; f++) {
        const auto& g = grid[f];
        if (g.empty()) return false;
        size_t len = g[0].size();
        for (const auto& row : g)
            if (row.size() != len) return false;
        int h = (int)g.size();
        for (size_t x = 0; x < len; x++)
            if (g[0][x] == lm::CELL_FLOOR || g[h - 1][x] == lm::CELL_FLOOR) return false;
        for (int y = 0; y < h; y++)
            if (g[y][0] == lm::CELL_FLOOR || g[y][len - 1] == lm::CELL_FLOOR) return false;
    }
    // 物品落点（与 Java ITEM_DEFS 一致）
    static const int ITEM_DEFS[][3] = {
        {lm::F2, 2, 5}, {lm::F2, 4, 1}, {lm::F2, 5, 14},
        {lm::F1, 4, 3}, {lm::F1, 12, 3}, {lm::B1, 16, 11},
    };
    for (const auto& d : ITEM_DEFS)
        if (cellAt(d[0], d[1], d[2]) != lm::CELL_FLOOR) return false;
    // 字条落点（与 Game::reset 的 NOTE_DEFS 一致，取整为格）
    static const float NOTE_DEFS[][3] = {
        {lm::F2, 3.1f, 4.3f}, {lm::F2, 4.0f, 9.4f}, {lm::F2, 5.0f, 13.4f},
        {lm::F1, 12.0f, 8.6f}, {lm::F1, 13.5f, 4.8f}, {lm::B1, 5.0f, 7.5f},
        {lm::B1, 15.0f, 11.5f}, {lm::F1, 14.1f, 14.75f},
    };
    for (const auto& d : NOTE_DEFS)
        if (cellAt((int)d[0], (int)std::floor(d[1]), (int)std::floor(d[2]))
            != lm::CELL_FLOOR) return false;
    // 传送：源必须是门格，目的地必须是地板
    for (const auto& e : transitions) {
        // 解析 key "f:x:y"
        int f, x, y;
        if (std::sscanf(e.first.c_str(), "%d:%d:%d", &f, &x, &y) != 3) return false;
        int cell = cellAt(f, x, y);
        if (cell != lm::CELL_DOOR && cell != lm::CELL_METAL && cell != lm::CELL_STAIR)
            return false;
        const auto& t = e.second;
        if (t.floor < 0 || t.floor >= lm::FLOOR_COUNT) return false;
        if (cellAt(t.floor, (int)std::floor(t.x), (int)std::floor(t.y)) != lm::CELL_FLOOR)
            return false;
    }
    if (spawn.floor < 0 || spawn.floor >= lm::FLOOR_COUNT) return false;
    if (cellAt(spawn.floor, (int)std::floor(spawn.x), (int)std::floor(spawn.y))
        != lm::CELL_FLOOR) return false;
    return true;
}

int LevelMap::cellAt(int floor, int cx, int cy) const {
    if (floor < 0 || floor >= lm::FLOOR_COUNT) return lm::CELL_PLASTER;
    const auto& g = grid[floor];
    if (cy < 0 || cy >= (int)g.size()) return lm::CELL_PLASTER;
    const auto& row = g[cy];
    if (cx < 0 || cx >= (int)row.size()) return lm::CELL_PLASTER;
    return row[cx];
}

bool LevelMap::blocked(Game* g, int floor, float fx, float fy) const {
    int cx = (int)std::floor(fx);
    int cy = (int)std::floor(fy);
    int cell = cellAt(floor, cx, cy);
    if (cell == lm::CELL_FLOOR) return false;
    if (cell == lm::CELL_DOOR || cell == lm::CELL_METAL || cell == lm::CELL_STAIR) {
        auto it = doors.find(key(floor, cx, cy));
        if (it == doors.end()) return true;
        return it->second.openness < 0.75f;
    }
    if (cell == lm::CELL_GATE) {
        if (!g) return true;
        return !g->gateOpen || g->gateOpenness < 0.75f;
    }
    return true;
}
