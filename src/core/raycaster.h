// raycaster.h — 光线投射伪 3D 渲染器（Raycaster.java 的 C++ 移植）。
// Wolfenstein 3D 风格：逐列 DDA + 薄门滑开 + billboard 精灵 + zbuffer
// + 手电筒锥形照明 / 距离衰减 / 侧面变暗。全部渲染到低分辨率 FB。
#pragma once
#include "fb.h"
#include "game.h"
#include "textures.h"
#include <vector>

class Raycaster {
public:
    // 渲染到低分辨率缓冲 c（宽约 300px），bobY 为行走上下浮动，flicker 为火苗抖动
    void render(FB& c, Game& g, Textures& t, float bobY, float flicker);

private:
    std::vector<float> zbuf_;
    static float brightness(float camAbs, float dist, bool flash, bool b1,
                            float flicker, bool side);
};
