package com.midnight.ward;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * 三层地图（二楼病房区 / 一楼大厅 / 地下室）+ 门锁 + 楼层传送 + 物品布置。
 *
 * 图例：
 *   '#' 涂料墙   '=' 瓷砖墙(墙裙)   '%' 血迹墙
 *   'D' 木门     'B' 金属门          'G' 大门(带木板/读卡器)
 *   'S' 楼梯门   '.' 地板
 */
public class LevelMap {

    public static final int F2 = 0, F1 = 1, B1 = 2;

    public static final int CELL_FLOOR = 0;
    public static final int CELL_PLASTER = 1;
    public static final int CELL_TILE = 2;
    public static final int CELL_BLOOD = 3;
    public static final int CELL_DOOR = 4;
    public static final int CELL_METAL = 5;
    public static final int CELL_GATE = 6;
    public static final int CELL_STAIR = 7;

    public static final int LOCK_NONE = 0;
    public static final int LOCK_ROOM = 1;      // 病房钥匙
    public static final int LOCK_BASEMENT = 2;  // 地下室钥匙

    /** 门/楼梯的动态状态。 */
    public static final class DoorState {
        public int floor, x, y, cellType;
        public float openness;       // 0 关 … 1 全开
        public boolean opening;
        public int lock = LOCK_NONE;

        /** 楼梯与地下室的回程铁门：靠近自动打开。 */
        public boolean isAutoOpen() {
            return cellType == CELL_STAIR || (floor == B1 && cellType == CELL_METAL);
        }
    }

    /** 楼层传送点。 */
    public static final class Transition {
        public final int floor;
        public final float x, y, angle;

        public Transition(int floor, float x, float y, float angle) {
            this.floor = floor;
            this.x = x;
            this.y = y;
            this.angle = angle;
        }
    }

    /** 出生点。 */
    public static final class Spawn {
        public int floor;
        public float x, y, angle;
    }

    // ---------------- 二楼：病房区 ----------------
    private static final String[] MAP_F2 = {
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

    // ---------------- 一楼：大厅（大门 + 药房 + 院长办公室 + 楼梯 + B1铁门） ----------------
    private static final String[] MAP_F1 = {
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

    // ---------------- 地下室 B1：漆黑的储物区 ----------------
    private static final String[] MAP_B1 = {
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

    public final int[][][] grid = new int[3][][];
    public final Map<String, DoorState> doors = new HashMap<>();
    public final Map<String, Transition> transitions = new HashMap<>();
    public final Spawn spawn = new Spawn();

    public LevelMap() {
        parse();
        validate();
    }

    public static String key(int floor, int x, int y) {
        return floor + ":" + x + ":" + y;
    }

    private void parse() {
        String[][] maps = {MAP_F2, MAP_F1, MAP_B1};
        for (int f = 0; f < 3; f++) {
            String[] m = maps[f];
            int[][] g = new int[m.length][];
            for (int y = 0; y < m.length; y++) {
                g[y] = new int[m[y].length()];
                for (int x = 0; x < m[y].length(); x++) {
                    char ch = m[y].charAt(x);
                    int cell;
                    switch (ch) {
                        case '#':
                            cell = CELL_PLASTER;
                            break;
                        case '=':
                            cell = CELL_TILE;
                            break;
                        case '%':
                            cell = CELL_BLOOD;
                            break;
                        case 'D':
                            cell = CELL_DOOR;
                            break;
                        case 'B':
                            cell = CELL_METAL;
                            break;
                        case 'G':
                            cell = CELL_GATE;
                            break;
                        case 'S':
                            cell = CELL_STAIR;
                            break;
                        default:
                            cell = CELL_FLOOR;
                            break;
                    }
                    g[y][x] = cell;
                    if (cell == CELL_DOOR || cell == CELL_METAL || cell == CELL_STAIR) {
                        DoorState d = new DoorState();
                        d.floor = f;
                        d.x = x;
                        d.y = y;
                        d.cellType = cell;
                        // 特定门上锁
                        if (f == F2 && x == 3 && y == 7) {
                            d.lock = LOCK_ROOM;
                        }
                        if (f == F1 && x == 23 && y == 12) {
                            d.lock = LOCK_BASEMENT;
                        }
                        doors.put(key(f, x, y), d);
                    }
                }
            }
            grid[f] = g;
        }

        // 楼层互转（走到楼梯门格里触发）
        transitions.put(key(F2, 20, 11), new Transition(F1, 19.5f, 8.8f, (float) (Math.PI / 2)));
        transitions.put(key(F1, 19, 7), new Transition(F2, 20.5f, 10.2f, (float) (-Math.PI / 2)));
        transitions.put(key(F1, 23, 12), new Transition(B1, 1.9f, 7.5f, 0f));
        transitions.put(key(B1, 0, 7), new Transition(F1, 22.0f, 12.5f, (float) Math.PI));

        // 出生点：二楼病房 A，面向房门
        spawn.floor = F2;
        spawn.x = 3.5f;
        spawn.y = 3.2f;
        spawn.angle = (float) (Math.PI / 2);
    }

    private static final int[][] ITEM_DEFS = {
            // {floor, floor(x), floor(y), type}
            {F2, 2, 5, Sprite.TYPE_KEY},      // 病房钥匙（房内桌上）
            {F2, 4, 1, Sprite.TYPE_BED},      // 病床（陈设）
            {F2, 5, 14, Sprite.TYPE_CARD},    // 房卡（二楼值班室）
            {F1, 4, 3, Sprite.TYPE_TORCH},    // 手电筒（一楼药房）
            {F1, 12, 3, Sprite.TYPE_BKEY},    // 地下室钥匙（院长办公室）
            {B1, 16, 11, Sprite.TYPE_CROWBAR},// 撬棍（地下室深处）
    };

    /** 每局重新生成物品（拾取会消耗 alive 标记）。 */
    public List<Sprite> spawnItems() {
        List<Sprite> list = new ArrayList<>();
        for (int[] d : ITEM_DEFS) {
            list.add(new Sprite(d[0], d[1] + 0.5f, d[2] + 0.5f, d[3]));
        }
        return list;
    }

    /** 碰撞判定：坐标 fx,fy 处是否被墙/关门挡住。 */
    public boolean blocked(Game g, int floor, float fx, float fy) {
        int cx = (int) Math.floor(fx);
        int cy = (int) Math.floor(fy);
        int[][] gmap = grid[floor];
        if (cy < 0 || cy >= gmap.length || cx < 0 || cx >= gmap[cy].length) {
            return true;
        }
        int cell = gmap[cy][cx];
        if (cell == CELL_FLOOR) {
            return false;
        }
        if (cell == CELL_DOOR || cell == CELL_METAL || cell == CELL_STAIR) {
            DoorState d = doors.get(key(floor, cx, cy));
            return d == null || d.openness < 0.75f;
        }
        if (cell == CELL_GATE) {
            return !g.gateOpen || g.gateOpenness < 0.75f;
        }
        return true;
    }

    public int cellAt(int floor, int cx, int cy) {
        int[][] gmap = grid[floor];
        if (cy < 0 || cy >= gmap.length || cx < 0 || cx >= gmap[cy].length) {
            return CELL_PLASTER;
        }
        return gmap[cy][cx];
    }

    /** 地图合法性自检（本地测试/开发期保障）。 */
    private void validate() {
        for (int f = 0; f < 3; f++) {
            String[] m = f == F2 ? MAP_F2 : (f == F1 ? MAP_F1 : MAP_B1);
            int len = m[0].length();
            for (String row : m) {
                if (row.length() != len) {
                    throw new IllegalStateException("地图行宽不一致 floor=" + f + " row=" + row);
                }
            }
            int[][] g = grid[f];
            int h = g.length;
            // 边界必须非地板
            for (int x = 0; x < len; x++) {
                if (g[0][x] == CELL_FLOOR || g[h - 1][x] == CELL_FLOOR) {
                    throw new IllegalStateException("地图上下边界漏墙 floor=" + f);
                }
            }
            for (int y = 0; y < h; y++) {
                if (g[y][0] == CELL_FLOOR || g[y][len - 1] == CELL_FLOOR) {
                    throw new IllegalStateException("地图左右边界漏墙 floor=" + f);
                }
            }
        }
        // 物品必须落在地板上
        for (int[] d : ITEM_DEFS) {
            if (cellAt(d[0], d[1], d[2]) != CELL_FLOOR) {
                throw new IllegalStateException("物品摆在了非地板格 floor=" + d[0]
                        + " (" + d[1] + "," + d[2] + ")");
            }
        }
        // 传送源必须是门格，目的地必须是地板
        for (Map.Entry<String, Transition> e : transitions.entrySet()) {
            String[] p = e.getKey().split(":");
            int f = Integer.parseInt(p[0]);
            int x = Integer.parseInt(p[1]);
            int y = Integer.parseInt(p[2]);
            int cell = cellAt(f, x, y);
            if (cell != CELL_DOOR && cell != CELL_METAL && cell != CELL_STAIR) {
                throw new IllegalStateException("传送源不是门格: " + e.getKey());
            }
            Transition t = e.getValue();
            if (cellAt(t.floor, (int) Math.floor(t.x), (int) Math.floor(t.y)) != CELL_FLOOR) {
                throw new IllegalStateException("传送目的地不是地板: " + e.getKey());
            }
        }
        // 出生点必须在地板上
        if (cellAt(spawn.floor, (int) Math.floor(spawn.x), (int) Math.floor(spawn.y)) != CELL_FLOOR) {
            throw new IllegalStateException("出生点不在地板上");
        }
    }
}
