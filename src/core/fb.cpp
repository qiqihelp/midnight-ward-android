// fb.cpp — 软件帧缓冲与 2D 基元实现。
#include "fb.h"
#include <algorithm>
#include <cmath>
#include <cstring>

void FB::resize(int W, int H) {
    if (W <= 0 || H <= 0) { w = h = 0; px.clear(); return; }
    if (W == w && H == h) return;
    w = W; h = H;
    px.assign((size_t)w * h * 4, 0);
}

void FB::clear(Col c) {
    if (!valid()) return;
    if (c.a == 255) {
        uint32_t v;
        uint8_t* p = (uint8_t*)&v;
        p[0] = c.r; p[1] = c.g; p[2] = c.b; p[3] = 255;
        size_t n = (size_t)w * h;
        uint32_t* dst = (uint32_t*)px.data();
        for (size_t i = 0; i < n; i++) dst[i] = v;
    } else {
        fill(0, 0, w, h, c);
    }
}

void FB::blendPixelA(int x, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if ((unsigned)x >= (unsigned)w || (unsigned)y >= (unsigned)h || a == 0) return;
    uint8_t* p = row(y) + x * 4;
    if (a == 255) { p[0] = r; p[1] = g; p[2] = b; p[3] = 255; return; }
    unsigned inv = 255u - a;
    p[0] = (uint8_t)((r * a + p[0] * inv + 127) / 255);
    p[1] = (uint8_t)((g * a + p[1] * inv + 127) / 255);
    p[2] = (uint8_t)((b * a + p[2] * inv + 127) / 255);
    p[3] = (uint8_t)std::min(255, (int)p[3] + (int)(a * (255 - p[3]) + 127) / 255);
}

void FB::blendPixel(int x, int y, Col c) { blendPixelA(x, y, c.r, c.g, c.b, c.a); }

void FB::fill(int x, int y, int w_, int h_, Col c) {
    if (!valid() || w_ <= 0 || h_ <= 0) return;
    int x0 = std::max(0, x), y0 = std::max(0, y);
    int x1 = std::min(w, x + w_), y1 = std::min(h, y + h_);
    if (x1 <= x0 || y1 <= y0) return;
    if (c.a == 255) {
        uint32_t v;
        uint8_t* p = (uint8_t*)&v;
        p[0] = c.r; p[1] = c.g; p[2] = c.b; p[3] = 255;
        for (int yy = y0; yy < y1; yy++) {
            uint32_t* dst = (uint32_t*)row(yy);
            for (int xx = x0; xx < x1; xx++) dst[xx] = v;
        }
    } else {
        for (int yy = y0; yy < y1; yy++)
            for (int xx = x0; xx < x1; xx++) blendPixelA(xx, yy, c.r, c.g, c.b, c.a);
    }
}

void FB::fillVGrad(int x, int y, int w_, int h_, Col top, Col bottom) {
    if (!valid() || w_ <= 0 || h_ <= 0) return;
    int x0 = std::max(0, x), y0 = std::max(0, y);
    int x1 = std::min(w, x + w_), y1 = std::min(h, y + h_);
    if (x1 <= x0 || y1 <= y0) return;
    for (int yy = y0; yy < y1; yy++) {
        float t = (h_ <= 1) ? 0.f : (float)(yy - y) / (float)(h_ - 1);
        t = std::max(0.f, std::min(1.f, t));
        Col c((uint8_t)(top.r + (bottom.r - top.r) * t),
              (uint8_t)(top.g + (bottom.g - top.g) * t),
              (uint8_t)(top.b + (bottom.b - top.b) * t));
        uint8_t* p = row(yy);
        for (int xx = x0; xx < x1; xx++) {
            size_t o = (size_t)xx * 4;
            p[o] = c.r; p[o + 1] = c.g; p[o + 2] = c.b; p[o + 3] = 255;
        }
    }
}

void FB::strokeRect(int x, int y, int w_, int h_, float thickness, Col c) {
    if (w_ <= 0 || h_ <= 0) return;
    int t = std::max(1, (int)(thickness + 0.5f));
    fill(x, y, w_, t, c);
    fill(x, y + h_ - t, w_, t, c);
    fill(x, y + t, t, std::max(0, h_ - 2 * t), c);
    fill(x + w_ - t, y + t, t, std::max(0, h_ - 2 * t), c);
}

void FB::fillCircle(float cx, float cy, float r, Col c) {
    if (r <= 0 || !valid()) return;
    int x0 = std::max(0, (int)std::floor(cx - r)), x1 = std::min(w, (int)std::ceil(cx + r));
    int y0 = std::max(0, (int)std::floor(cy - r)), y1 = std::min(h, (int)std::ceil(cy + r));
    float r2 = r * r;
    for (int y = y0; y < y1; y++) {
        for (int x = x0; x < x1; x++) {
            float dx = x + 0.5f - cx, dy = y + 0.5f - cy;
            if (dx * dx + dy * dy <= r2) blendPixel(x, y, c);
        }
    }
}

void FB::strokeCircle(float cx, float cy, float r, float thickness, Col c) {
    if (r <= 0 || thickness <= 0 || !valid()) return;
    float outer = r + thickness * 0.5f + 1.f;
    int x0 = std::max(0, (int)std::floor(cx - outer));
    int x1 = std::min(w, (int)std::ceil(cx + outer));
    int y0 = std::max(0, (int)std::floor(cy - outer));
    int y1 = std::min(h, (int)std::ceil(cy + outer));
    float half = thickness * 0.5f;
    for (int y = y0; y < y1; y++) {
        for (int x = x0; x < x1; x++) {
            float dx = x + 0.5f - cx, dy = y + 0.5f - cy;
            float d = std::sqrt(dx * dx + dy * dy) - r;
            if (d <= -half - 0.5f) blendPixel(x, y, c);
            else if (d < half + 0.5f) {
                float cov = std::max(0.f, std::min(1.f, half + 0.5f - std::abs(d)));
                uint8_t a = (uint8_t)(c.a * cov);
                if (a > 0) blendPixelA(x, y, c.r, c.g, c.b, a);
            }
        }
    }
}

// 圆角矩形 SDF
static inline float sdRoundRect(float px, float py, float cx, float cy,
                                float hw, float hh, float rad) {
    float dx = std::abs(px - cx) - (hw - rad);
    float dy = std::abs(py - cy) - (hh - rad);
    float ax = std::max(dx, 0.f), ay = std::max(dy, 0.f);
    float out = std::sqrt(ax * ax + ay * ay);
    float in = std::min(std::max(dx, dy), 0.f);
    return out + in - rad;
}

void FB::fillRoundRect(float x, float y, float w_, float h_, float radius, Col c) {
    if (w_ <= 0 || h_ <= 0 || !valid()) return;
    if (radius > std::min(w_, h_) * 0.5f) radius = std::min(w_, h_) * 0.5f;
    float cx = x + w_ * 0.5f, cy = y + h_ * 0.5f, hw = w_ * 0.5f, hh = h_ * 0.5f;
    int x0 = std::max(0, (int)std::floor(x)), x1 = std::min(w, (int)std::ceil(x + w_));
    int y0 = std::max(0, (int)std::floor(y)), y1 = std::min(h, (int)std::ceil(y + h_));
    for (int yy = y0; yy < y1; yy++) {
        for (int xx = x0; xx < x1; xx++) {
            float d = sdRoundRect(xx + 0.5f, yy + 0.5f, cx, cy, hw, hh, radius);
            if (d <= -0.5f) blendPixel(xx, yy, c);
            else if (d < 0.5f) {
                uint8_t a = (uint8_t)(c.a * (0.5f - d));
                blendPixelA(xx, yy, c.r, c.g, c.b, a);
            }
        }
    }
}

void FB::strokeRoundRect(float x, float y, float w_, float h_, float radius,
                         float thickness, Col c) {
    if (w_ <= 0 || h_ <= 0 || !valid()) return;
    if (radius > std::min(w_, h_) * 0.5f) radius = std::min(w_, h_) * 0.5f;
    float cx = x + w_ * 0.5f, cy = y + h_ * 0.5f, hw = w_ * 0.5f, hh = h_ * 0.5f;
    int x0 = std::max(0, (int)std::floor(x) - 2), x1 = std::min(w, (int)std::ceil(x + w_) + 2);
    int y0 = std::max(0, (int)std::floor(y) - 2), y1 = std::min(h, (int)std::ceil(y + h_) + 2);
    for (int yy = y0; yy < y1; yy++) {
        for (int xx = x0; xx < x1; xx++) {
            float d = std::abs(sdRoundRect(xx + 0.5f, yy + 0.5f, cx, cy, hw, hh, radius));
            if (d <= thickness * 0.5f - 0.5f) blendPixel(xx, yy, c);
            else if (d < thickness * 0.5f + 0.5f) {
                uint8_t a = (uint8_t)(c.a * (thickness * 0.5f + 0.5f - d));
                blendPixelA(xx, yy, c.r, c.g, c.b, a);
            }
        }
    }
}

void FB::drawLine(float x0, float y0, float x1, float y1, float thickness, Col c) {
    if (!valid()) return;
    float minx = std::min(x0, x1) - thickness, maxx = std::max(x0, x1) + thickness;
    float miny = std::min(y0, y1) - thickness, maxy = std::max(y0, y1) + thickness;
    int x0i = std::max(0, (int)std::floor(minx)), x1i = std::min(w, (int)std::ceil(maxx));
    int y0i = std::max(0, (int)std::floor(miny)), y1i = std::min(h, (int)std::ceil(maxy));
    float dx = x1 - x0, dy = y1 - y0;
    float len2 = dx * dx + dy * dy;
    float half = thickness * 0.5f;
    for (int yy = y0i; yy < y1i; yy++) {
        for (int xx = x0i; xx < x1i; xx++) {
            float px = xx + 0.5f, py = yy + 0.5f;
            float t = (len2 > 0) ? ((px - x0) * dx + (py - y0) * dy) / len2 : 0.f;
            t = std::max(0.f, std::min(1.f, t));
            float ex = x0 + dx * t - px, ey = y0 + dy * t - py;
            float d = std::sqrt(ex * ex + ey * ey) - half;
            if (d <= -0.5f) blendPixel(xx, yy, c);
            else if (d < 0.5f) {
                uint8_t a = (uint8_t)(c.a * (0.5f - d));
                blendPixelA(xx, yy, c.r, c.g, c.b, a);
            }
        }
    }
}

void FB::fillPoly(const float* pts, int n, Col c) {
    if (!valid() || n < 3) return;
    float miny = pts[1], maxy = pts[1];
    for (int i = 1; i < n; i++) {
        miny = std::min(miny, pts[i * 2 + 1]);
        maxy = std::max(maxy, pts[i * 2 + 1]);
    }
    int y0 = std::max(0, (int)std::floor(miny)), y1 = std::min(h, (int)std::ceil(maxy));
    float xs[64];
    for (int y = y0; y < y1; y++) {
        float sy = y + 0.5f;
        int cnt = 0;
        for (int i = 0; i < n && cnt < 62; i++) {
            int j = (i + 1) % n;
            float ay = pts[i * 2 + 1], by = pts[j * 2 + 1];
            if ((ay <= sy && by > sy) || (by <= sy && ay > sy)) {
                float t = (sy - ay) / (by - ay);
                xs[cnt++] = pts[i * 2] + t * (pts[j * 2] - pts[i * 2]);
            }
        }
        for (int i = 1; i < cnt; i++) { // 插入排序
            float v = xs[i]; int k = i - 1;
            while (k >= 0 && xs[k] > v) { xs[k + 1] = xs[k]; k--; }
            xs[k + 1] = v;
        }
        for (int i = 0; i + 1 < cnt; i += 2) {
            int a = std::max(0, (int)std::ceil(xs[i] - 0.5f));
            int b = std::min(w - 1, (int)std::floor(xs[i + 1] - 0.5f));
            for (int x = a; x <= b; x++) blendPixel(x, y, c);
        }
    }
}

void FB::blitScaled(const FB& src, int dx, int dy, int dw, int dh) {
    if (!valid() || !src.valid() || dw <= 0 || dh <= 0) return;
    int x0 = std::max(0, dx), y0 = std::max(0, dy);
    int x1 = std::min(w, dx + dw), y1 = std::min(h, dy + dh);
    for (int yy = y0; yy < y1; yy++) {
        int sy = (yy - dy) * src.h / dh;
        if (sy < 0) sy = 0; else if (sy >= src.h) sy = src.h - 1;
        const uint8_t* srow = src.row(sy);
        uint8_t* drow = row(yy);
        for (int xx = x0; xx < x1; xx++) {
            int sx = (xx - dx) * src.w / dw;
            if (sx < 0) sx = 0; else if (sx >= src.w) sx = src.w - 1;
            const uint8_t* s = srow + (size_t)sx * 4;
            uint8_t* d = drow + (size_t)xx * 4;
            d[0] = s[0]; d[1] = s[1]; d[2] = s[2]; d[3] = 255;
        }
    }
}

void FB::blitScaledBilinear(const FB& src, int dx, int dy, int dw, int dh) {
    if (!valid() || !src.valid() || dw <= 0 || dh <= 0) return;
    if (src.w < 2 || src.h < 2) { blitScaled(src, dx, dy, dw, dh); return; }
    int x0 = std::max(0, dx), y0 = std::max(0, dy);
    int x1 = std::min(w, dx + dw), y1 = std::min(h, dy + dh);
    float sxScale = (float)src.w / (float)dw;
    float syScale = (float)src.h / (float)dh;
    int sw = src.w, sh = src.h;
    for (int yy = y0; yy < y1; yy++) {
        // 目标像素中心 → 源浮点坐标
        float fy = (yy - dy + 0.5f) * syScale - 0.5f;
        if (fy < 0.f) fy = 0.f;
        if (fy > sh - 1.001f) fy = sh - 1.001f;
        int vy0 = (int)fy;
        int vy1 = vy0 + 1 < sh ? vy0 + 1 : sh - 1;
        float ty = fy - vy0;
        const uint8_t* r0 = src.row(vy0);
        const uint8_t* r1 = src.row(vy1);
        uint8_t* drow = row(yy);
        for (int xx = x0; xx < x1; xx++) {
            float fx = (xx - dx + 0.5f) * sxScale - 0.5f;
            if (fx < 0.f) fx = 0.f;
            if (fx > sw - 1.001f) fx = sw - 1.001f;
            int ux0 = (int)fx;
            int ux1 = ux0 + 1 < sw ? ux0 + 1 : sw - 1;
            float tx = fx - ux0;
            const uint8_t* a = r0 + (size_t)ux0 * 4;
            const uint8_t* b = r0 + (size_t)ux1 * 4;
            const uint8_t* c = r1 + (size_t)ux0 * 4;
            const uint8_t* d = r1 + (size_t)ux1 * 4;
            uint8_t* o = drow + (size_t)xx * 4;
            unsigned w00 = (unsigned)((1.f - tx) * (1.f - ty) * 256.f + 0.5f);
            unsigned w10 = (unsigned)(tx * (1.f - ty) * 256.f + 0.5f);
            unsigned w01 = (unsigned)((1.f - tx) * ty * 256.f + 0.5f);
            unsigned w11 = (unsigned)(tx * ty * 256.f + 0.5f);
            o[0] = (uint8_t)((a[0] * w00 + b[0] * w10 + c[0] * w01 + d[0] * w11) >> 8);
            o[1] = (uint8_t)((a[1] * w00 + b[1] * w10 + c[1] * w01 + d[1] * w11) >> 8);
            o[2] = (uint8_t)((a[2] * w00 + b[2] * w10 + c[2] * w01 + d[2] * w11) >> 8);
            o[3] = 255;
        }
    }
}

void FB::blit(const FB& src, int dx, int dy, int alpha255) {
    if (!valid() || !src.valid() || alpha255 <= 0) return;
    int x0 = std::max(0, dx), y0 = std::max(0, dy);
    int x1 = std::min(w, dx + src.w), y1 = std::min(h, dy + src.h);
    for (int yy = y0; yy < y1; yy++) {
        const uint8_t* srow = src.row(yy - dy);
        uint8_t* drow = row(yy);
        for (int xx = x0; xx < x1; xx++) {
            const uint8_t* s = srow + (size_t)(xx - dx) * 4;
            uint8_t* d = drow + (size_t)xx * 4;
            unsigned a = ((unsigned)s[3] * alpha255) / 255;
            if (a == 0) continue;
            if (a >= 255) { d[0] = s[0]; d[1] = s[1]; d[2] = s[2]; d[3] = 255; }
            else {
                unsigned inv = 255 - a;
                d[0] = (uint8_t)((s[0] * a + d[0] * inv + 127) / 255);
                d[1] = (uint8_t)((s[1] * a + d[1] * inv + 127) / 255);
                d[2] = (uint8_t)((s[2] * a + d[2] * inv + 127) / 255);
                d[3] = (uint8_t)std::min(255, d[3] + (int)(a * (255 - d[3]) + 127) / 255);
            }
        }
    }
}
