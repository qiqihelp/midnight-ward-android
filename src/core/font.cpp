// font.cpp — SDF 字体渲染实现。
//
// 图集文件布局（scripts/gen_font_sdf.py 生成，小端）：
//   头 20 字节:
//     "MWF1" | count u32 | atlasW u16 | atlasH u16 | ascent i16 | descent i16
//     | spread u8 | nominalPx u16 | reserved u8
//   count × 20 字节字形记录:
//     cp u32 | x u16 | y u16 | w u16 | h u16 | left i16 | top i16 | advQ4 u16 | pad u16
//   atlasW × atlasH 字节 A8 距离场（128 = 边缘，值域线性映射距离 ±spread px）
#include "font.h"
#include <algorithm>
#include <cmath>
#include <cstring>

int utf8Decode(const char* s, uint32_t& cp) {
    unsigned char c = (unsigned char)s[0];
    if (c < 0x80) { cp = c; return 1; }
    if ((c & 0xE0) == 0xC0 && (s[1] & 0xC0) == 0x80) {
        cp = ((uint32_t)(c & 0x1F) << 6) | (uint32_t)(s[1] & 0x3F);
        return 2;
    }
    if ((c & 0xF0) == 0xE0 && (s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80) {
        cp = ((uint32_t)(c & 0x0F) << 12) | ((uint32_t)(s[1] & 0x3F) << 6)
           | (uint32_t)(s[2] & 0x3F);
        return 3;
    }
    if ((c & 0xF8) == 0xF0 && (s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80
        && (s[3] & 0xC0) == 0x80) {
        cp = ((uint32_t)(c & 0x07) << 18) | ((uint32_t)(s[1] & 0x3F) << 12)
           | ((uint32_t)(s[2] & 0x3F) << 6) | (uint32_t)(s[3] & 0x3F);
        return 4;
    }
    cp = 0xFFFD;
    return 1;
}

int utf8Len(uint32_t cp) {
    if (cp < 0x80) return 1;
    if (cp < 0x800) return 2;
    if (cp < 0x10000) return 3;
    return 4;
}

static uint32_t rdU32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16)
         | ((uint32_t)p[3] << 24);
}
static uint16_t rdU16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }

bool Font::load(const uint8_t* data, size_t size) {
    ok_ = false;
    glyphs_.clear();
    atlas_.clear();
    if (!data || size < 20) return false;
    if (std::memcmp(data, "MWF1", 4) != 0) return false;
    uint32_t count = rdU32(data + 4);
    atlasW_ = rdU16(data + 8);
    atlasH_ = rdU16(data + 10);
    ascent_ = (int16_t)rdU16(data + 12);
    descent_ = (int16_t)rdU16(data + 14);
    spread_ = data[16];
    nominalPx_ = rdU16(data + 17);
    if (count == 0 || count > 20000 || atlasW_ == 0 || atlasH_ == 0
        || nominalPx_ == 0 || spread_ == 0) return false;
    size_t o = 20;
    size_t glyphBytes = (size_t)count * 20;
    size_t atlasBytes = (size_t)atlasW_ * atlasH_;
    if (o + glyphBytes + atlasBytes != size) return false;

    glyphs_.resize(count);
    for (uint32_t i = 0; i < count; i++) {
        const uint8_t* p = data + o + (size_t)i * 20;
        Glyph& g = glyphs_[i];
        g.cp = rdU32(p);
        g.x = rdU16(p + 4);
        g.y = rdU16(p + 6);
        g.w = rdU16(p + 8);
        g.h = rdU16(p + 10);
        g.left = (int16_t)rdU16(p + 12);
        g.top = (int16_t)rdU16(p + 14);
        g.advQ4 = rdU16(p + 16);
    }
    // 字形记录必须按 cp 升序（二分查找依赖）；同时校验图集范围
    for (size_t i = 1; i < glyphs_.size(); i++) {
        if (glyphs_[i].cp <= glyphs_[i - 1].cp) return false;
    }
    for (const Glyph& g : glyphs_) {
        if ((size_t)g.x + g.w > atlasW_ || (size_t)g.y + g.h > atlasH_) return false;
    }
    atlas_.assign(data + o + glyphBytes, data + o + glyphBytes + atlasBytes);
    ok_ = true;
    return true;
}

const Glyph* Font::find_(uint32_t cp) const {
    size_t lo = 0, hi = glyphs_.size();
    while (lo < hi) {
        size_t mid = (lo + hi) / 2;
        if (glyphs_[mid].cp < cp) lo = mid + 1;
        else if (glyphs_[mid].cp > cp) hi = mid;
        else return &glyphs_[mid];
    }
    return nullptr;
}

float Font::measure(const char* utf8, float size, float spacing) const {
    if (!utf8 || !ok_) return 0.f;
    float scale = size / nominal();
    float w = 0.f;
    const char* p = utf8;
    while (*p) {
        uint32_t cp;
        p += utf8Decode(p, cp);
        const Glyph* g = find_(cp);
        if (g) w += (g->advQ4 / 16.f) * scale;
        else w += nominal() * 0.5f * scale;
        w += spacing;
    }
    return w;
}

float Font::draw(FB& fb, const char* utf8, float x, float baselineY, float size,
                 Col color, float alpha, float spacing) const {
    if (!utf8 || !ok_) return x;
    float scale = size / nominal();
    if (scale <= 0.f) return x;
    float alphaF = std::max(0.f, std::min(1.f, alpha));
    if (alphaF <= 0.f) return x;
    const float edgeSoft = 1.2f; // 目标空间边缘过渡宽度（px）

    const char* p = utf8;
    while (*p) {
        uint32_t cp;
        p += utf8Decode(p, cp);
        const Glyph* g = find_(cp);
        if (!g || g->w == 0 || g->h == 0) {
            x += nominal() * 0.5f * scale + spacing;
            continue;
        }
        float dw = g->w * scale, dh = g->h * scale;
        float dx = x + g->left * scale;
        float dyTop = baselineY - g->top * scale;
        int ix0 = std::max(0, (int)std::floor(dx));
        int iy0 = std::max(0, (int)std::floor(dyTop));
        int ix1 = std::min(fb.w, (int)std::ceil(dx + dw));
        int iy1 = std::min(fb.h, (int)std::ceil(dyTop + dh));
        if (ix1 <= ix0 || iy1 <= iy0) { x += (g->advQ4 / 16.f) * scale + spacing; continue; }
        float invS = 1.f / scale;
        int gw = g->w, gh = g->h;
        const uint8_t* atl = atlas_.data();

        for (int yy = iy0; yy < iy1; yy++) {
            for (int xx = ix0; xx < ix1; xx++) {
                // 目标像素中心 → 图集浮点坐标（双线性采样）
                float u = (xx + 0.5f - dx) * invS - 0.5f;
                float v = (yy + 0.5f - dyTop) * invS - 0.5f;
                u = std::max(0.f, std::min((float)(gw - 1), u));
                v = std::max(0.f, std::min((float)(gh - 1), v));
                int ui = (int)u, vi = (int)v;
                float fu = u - ui, fv = v - vi;
                const uint8_t* b00 = atl + (size_t)(g->y + vi) * atlasW_ + (g->x + ui);
                const uint8_t* b01 = b00 + (ui + 1 < gw ? 1 : 0);
                const uint8_t* b10 = b00 + (vi + 1 < gh ? atlasW_ : 0);
                const uint8_t* b11 = b10 + (ui + 1 < gw ? 1 : 0);
                float val = (*b00 * (1 - fu) + *b01 * fu) * (1 - fv)
                          + (*b10 * (1 - fu) + *b11 * fu) * fv;
                float dNom = (val / 255.f - 0.5f) * 2.f * spread_; // 名义尺度距离 px
                float dDst = dNom * scale;
                float cov = 0.5f + dDst / edgeSoft;
                if (cov <= 0.f) continue;
                if (cov > 1.f) cov = 1.f;
                fb.blendPixelA(xx, yy, color.r, color.g, color.b,
                               (uint8_t)(alphaF * cov * 255.f));
            }
        }
        x += (g->advQ4 / 16.f) * scale + spacing;
    }
    return x;
}

float Font::drawCentered(FB& fb, const char* utf8, float cx, float baselineY,
                         float size, Col color, float alpha, float spacing) const {
    float w = measure(utf8, size, spacing);
    return draw(fb, utf8, cx - w * 0.5f, baselineY, size, color, alpha, spacing);
}
