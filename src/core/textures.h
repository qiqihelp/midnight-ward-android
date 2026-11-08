// textures.h — 程序化生成全部贴图（Textures.java 的 C++ 移植）。
// 墙面 9 张 64×64，精灵 6 张，运行时一次性生成，零素材文件。
#pragma once
#include "fb.h"

struct TexRng {
    uint32_t s = 7;
    float nextFloat() {
        s = s * 1664525u + 1013904223u;
        return (float)(s >> 8) / 16777216.f;
    }
    bool nextBool() { return nextFloat() < 0.5f; }
};

class Textures {
public:
    static constexpr int TS = 64; // 墙纹理边长

    // 墙面纹理索引
    static constexpr int T_PLASTER = 0; // 涂料墙
    static constexpr int T_TILE = 1;    // 瓷砖墙裙
    static constexpr int T_BLOOD = 2;   // 血迹墙
    static constexpr int T_DOOR = 3;    // 木门
    static constexpr int T_METAL = 4;   // 金属门（B1）
    static constexpr int T_STAIRS = 5;  // 楼梯门
    static constexpr int T_GATE_2 = 6;  // 大门：两根木板
    static constexpr int T_GATE_1 = 7;  // 大门：一根木板
    static constexpr int T_GATE_CARD = 8; // 大门：读卡器

    // 精灵纹理索引
    static constexpr int S_KEY = 0;
    static constexpr int S_CARD = 1;
    static constexpr int S_TORCH = 2;
    static constexpr int S_CROWBAR = 3;
    static constexpr int S_GHOST = 4;
    static constexpr int S_BED = 5;

    FB walls[9];
    FB sprites[6];

    void init();

private:
    void plaster(FB& c, TexRng& rng);
    void tile(FB& c, TexRng& rng);
    void blood(FB& c, TexRng& rng);
    void woodDoor(FB& c, TexRng& rng);
    void metalDoor(FB& c, TexRng& rng);
    void stairs(FB& c);
    void gateBase(FB& c, TexRng& rng);
    void plank(FB& c, TexRng& rng, int y);
    void cardReader(FB& c);

    void keySprite(FB& c);
    void cardSprite(FB& c);
    void torchSprite(FB& c);
    void crowbarSprite(FB& c);
    void ghostSprite(FB& c);
    void bedSprite(FB& c);

    static void microText(FB& c, const char* s, int x, int y, Col cIn); // 3×5 像素字
};
