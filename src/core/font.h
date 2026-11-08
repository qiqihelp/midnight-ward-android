// font.h — SDF（有向距离场）位图字体：单图集任意尺寸清晰渲染。
// 图集由 scripts/gen_font_sdf.py 生成（assets/font_sdf.bin）。
#pragma once
#include "fb.h"
#include <cstdint>
#include <string>
#include <vector>

struct Glyph {
    uint32_t cp;      // 码点
    uint16_t x, y;    // 图集内位置
    uint16_t w, h;    // 图集内尺寸
    int16_t left;     // 起点到字形左缘（px，名义尺度）
    int16_t top;      // 基线到字形顶缘（px，正为上）
    uint16_t advQ4;   // 步进 ×16
};

class Font {
public:
    bool load(const uint8_t* data, size_t size);
    bool ok() const { return ok_; }
    float nominal() const { return (float)nominalPx_; }   // 名义字号（图集基准）
    float ascent() const { return (float)ascent_; }
    float descent() const { return (float)descent_; }

    // 宽度测量（返回 px）
    float measure(const char* utf8, float size, float spacing = 0.f) const;
    // 在基线 y 处绘制，返回终点 x
    float draw(FB& fb, const char* utf8, float x, float baselineY, float size,
               Col color, float alpha = 1.f, float spacing = 0.f) const;
    // 水平居中绘制
    float drawCentered(FB& fb, const char* utf8, float cx, float baselineY,
                       float size, Col color, float alpha = 1.f, float spacing = 0.f) const;

private:
    bool ok_ = false;
    uint16_t atlasW_ = 0, atlasH_ = 0;
    int16_t ascent_ = 0, descent_ = 0;
    uint8_t spread_ = 12;
    uint16_t nominalPx_ = 64;
    std::vector<Glyph> glyphs_;      // 按 cp 升序
    std::vector<uint8_t> atlas_;     // A8
    const Glyph* find_(uint32_t cp) const;
};

// UTF-8 解码（返回字节步长；cp 为码点；非法序列回退 U+FFFD）
int utf8Decode(const char* s, uint32_t& cp);
int utf8Len(uint32_t cp);
