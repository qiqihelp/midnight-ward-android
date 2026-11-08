// fb.h — RGBA 帧缓冲 + 基础 2D 绘图基元（软件渲染，平台无关）。
#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>

struct Col { // 直通（非预乘）RGBA
    uint8_t r = 0, g = 0, b = 0, a = 255;
    constexpr Col() = default;
    constexpr Col(uint8_t R, uint8_t G, uint8_t B, uint8_t A = 255)
        : r(R), g(G), b(B), a(A) {}
    // 0xAARRGGBB（与 Java 版 0xFF39413C 写法一致，便于移植）
    static constexpr Col argb(uint32_t v) {
        return Col(uint8_t((v >> 16) & 0xFF), uint8_t((v >> 8) & 0xFF),
                   uint8_t(v & 0xFF), uint8_t((v >> 24) & 0xFF));
    }
    static constexpr Col rgb(uint32_t v) { // 0xRRGGBB
        return Col(uint8_t((v >> 16) & 0xFF), uint8_t((v >> 8) & 0xFF),
                   uint8_t(v & 0xFF), 255);
    }
};

struct FB {
    int w = 0, h = 0;
    std::vector<uint8_t> px; // 行主序 RGBA

    void resize(int W, int H);
    inline uint8_t* row(int y) { return px.data() + (size_t)y * w * 4; }
    inline const uint8_t* row(int y) const { return px.data() + (size_t)y * w * 4; }
    inline bool valid() const { return w > 0 && h > 0 && px.size() == (size_t)w * h * 4; }
    void clear(Col c);
    void fill(int x, int y, int w_, int h_, Col c);
    void fillVGrad(int x, int y, int w_, int h_, Col top, Col bottom);
    void blendPixel(int x, int y, Col c);
    void blendPixelA(int x, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t a);

    void strokeRect(int x, int y, int w_, int h_, float thickness, Col c);
    void fillCircle(float cx, float cy, float r, Col c);
    void strokeCircle(float cx, float cy, float r, float thickness, Col c);
    // SDF 方式的圆角矩形（填充 / 描边），对 HUD 按钮尺寸足够快
    void fillRoundRect(float x, float y, float w_, float h_, float radius, Col c);
    void strokeRoundRect(float x, float y, float w_, float h_, float radius,
                         float thickness, Col c);
    void drawLine(float x0, float y0, float x1, float y1, float thickness, Col c);
    // 凸/简单多边形（扫描线，奇偶规则）
    void fillPoly(const float* pts, int n, Col c);
    // 整数倍最近邻放大/平铺拷贝：把 src 拉伸绘制到 (dx,dy,dw,dh)
    void blitScaled(const FB& src, int dx, int dy, int dw, int dh);
    // 1:1 带透明度拷贝（纹理/精灵）
    void blit(const FB& src, int dx, int dy, int alpha255 = 255);
};

// 便捷颜色（游戏配色，与 Java 版一致）
namespace palette {
constexpr Col NIGHT_BG      = Col(4, 4, 4, 255);
constexpr Col CEIL_NORMAL   = Col(0x0A, 0x0D, 0x12, 255);
constexpr Col FLOOR_NORMAL  = Col(0x19, 0x15, 0x10, 255);
constexpr Col CEIL_B1       = Col(0x03, 0x03, 0x04, 255);
constexpr Col FLOOR_B1      = Col(0x05, 0x04, 0x04, 255);
constexpr Col TEXT_MAIN     = Col(0xE8, 0xE6, 0xD9, 255);
constexpr Col TEXT_DIM      = Col(0x8A, 0x8F, 0x8C, 255);
constexpr Col TEXT_SOFT     = Col(0xCF, 0xCF, 0xC6, 235);
constexpr Col TEXT_WARM     = Col(0xE8, 0xB1, 0x5C, 255);
constexpr Col TEXT_RED      = Col(0xCC, 0x44, 0x44, 255);
} // namespace palette
