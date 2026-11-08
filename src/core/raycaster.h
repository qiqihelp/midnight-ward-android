// raycaster.h — 光线投射 3D 渲染器（Raycaster.java 的 C++ 演进版）。
// 逐列 DDA + 薄门滑开 + billboard 精灵 + zbuffer
// + 地板/天花板透视贴图（floor casting）
// + 手电筒椭圆锥形照明（可分离查找表）/ 距离雾 / 侧面变暗 / Bayer 抖动去 banding
// + 俯仰视角（地平线偏移）+ 屏幕暗角。全部软件渲染到内部 FB。
#pragma once
#include "fb.h"
#include "game.h"
#include "textures.h"
#include <vector>

class Raycaster {
public:
    // 渲染到内部低分辨率缓冲 c，bobY 为行走上下浮动（px），flicker 为灯光抖动
    void render(FB& c, Game& g, Textures& t, float bobY, float flicker);

private:
    std::vector<float> zbuf_;
    // 手电筒锥形衰减查找表（可分离：cone = coneX[x] * coneY[y]）
    std::vector<float> coneX_, coneY_;
    // 暗角查找表（vig = vigX[x] * vigY[y]）
    std::vector<float> vigX_, vigY_;
    void resizeTables(int w, int h);
    void applyVignette(FB& c);
    static float brightness(float camAbs, float dist, bool flash, bool b1,
                            float flicker, bool side);
};
