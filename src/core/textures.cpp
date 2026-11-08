// textures.cpp — 程序化贴图（Textures.java 的 C++ 移植）。
#include "textures.h"
#include <cmath>

static FB makeTex(int w, int h) {
    FB f;
    f.resize(w, h);
    return f;
}

void Textures::init() {
    TexRng rng;
    walls[T_PLASTER] = makeTex(TS, TS);   plaster(walls[T_PLASTER], rng);
    walls[T_TILE] = makeTex(TS, TS);      tile(walls[T_TILE], rng);
    walls[T_BLOOD] = makeTex(TS, TS);     blood(walls[T_BLOOD], rng);
    walls[T_DOOR] = makeTex(TS, TS);      woodDoor(walls[T_DOOR], rng);
    walls[T_METAL] = makeTex(TS, TS);     metalDoor(walls[T_METAL], rng);
    walls[T_STAIRS] = makeTex(TS, TS);    stairs(walls[T_STAIRS]);
    walls[T_GATE_2] = makeTex(TS, TS);    gateBase(walls[T_GATE_2], rng);
    plank(walls[T_GATE_2], rng, 16); plank(walls[T_GATE_2], rng, 34);
    walls[T_GATE_1] = makeTex(TS, TS);    gateBase(walls[T_GATE_1], rng);
    plank(walls[T_GATE_1], rng, 34);
    walls[T_GATE_CARD] = makeTex(TS, TS); gateBase(walls[T_GATE_CARD], rng);
    cardReader(walls[T_GATE_CARD]);

    sprites[S_KEY] = makeTex(32, 32);     keySprite(sprites[S_KEY]);
    sprites[S_CARD] = makeTex(32, 32);    cardSprite(sprites[S_CARD]);
    sprites[S_TORCH] = makeTex(32, 32);   torchSprite(sprites[S_TORCH]);
    sprites[S_CROWBAR] = makeTex(32, 32); crowbarSprite(sprites[S_CROWBAR]);
    sprites[S_GHOST] = makeTex(48, 72);   ghostSprite(sprites[S_GHOST]);
    sprites[S_BED] = makeTex(64, 40);     bedSprite(sprites[S_BED]);
}

// ---------------------------------------------------------------------------
// 墙面
// ---------------------------------------------------------------------------

void Textures::plaster(FB& c, TexRng& rng) {
    c.fill(0, 0, 64, 64, Col::argb(0xFF39413C));
    // 噪点
    for (int i = 0; i < 90; i++) {
        Col cc = rng.nextBool() ? Col::argb(0x11363028) : Col::argb(0x11495044);
        float x = rng.nextFloat() * 64.f;
        float y = rng.nextFloat() * 64.f;
        c.fill((int)x, (int)y, 2, 2, cc);
    }
    // 竖向污渍
    for (int i = 0; i < 7; i++) {
        float x = rng.nextFloat() * 64.f;
        float w = 3.f + rng.nextFloat() * 4.f;
        c.fill((int)x, (int)(rng.nextFloat() * 10.f), (int)w, 64,
               Col::argb(0x14201812));
    }
    // 裂缝（折线）
    for (int i = 0; i < 2; i++) {
        float x = rng.nextFloat() * 64.f;
        float y = 0.f;
        for (int k = 0; k < 3; k++) {
            float nx = x + rng.nextFloat() * 10.f - 5.f;
            float ny = y + (k == 0 ? 14.f : (k == 1 ? 16.f : 18.f));
            c.drawLine(x, y, nx, ny, 1.f, Col::argb(0xFF232B27));
            x = nx; y = ny;
        }
    }
    // 踢脚线
    c.fill(0, 57, 64, 7, Col::argb(0xFF2C332F));
    c.fill(0, 57, 64, 1, Col::argb(0xFF4A544E));
}

void Textures::tile(FB& c, TexRng& rng) {
    plaster(c, rng);
    c.fill(0, 26, 64, 38, Col::argb(0xFF40544A));
    for (int y = 26; y <= 64; y += 12)
        c.drawLine(0, (float)y, 64, (float)y, 2, Col::argb(0xFF2A3A33));
    for (int x = 0; x <= 64; x += 16)
        c.drawLine((float)x, 26, (float)x, 64, 2, Col::argb(0xFF2A3A33));
    c.fill(0, 26, 64, 2, Col::argb(0xFF54685C));
}

void Textures::blood(FB& c, TexRng& rng) {
    plaster(c, rng);
    c.fillCircle(22, 18, 7, Col::argb(0xFF4C120F));
    c.fillCircle(31, 24, 4, Col::argb(0xFF4C120F));
    c.fillCircle(14, 30, 5, Col::argb(0xFF4C120F));
    c.fillCircle(24, 52, 6, Col::argb(0xFF4C120F));
    for (int i = 0; i < 5; i++) {
        float x = 12.f + rng.nextFloat() * 22.f;
        float y0 = 24.f + rng.nextFloat() * 8.f;
        float y1 = 44.f + rng.nextFloat() * 18.f;
        c.fill((int)x, (int)y0, 2, (int)(y1 - y0), Col::argb(0xFF3D0E0B));
    }
}

void Textures::woodDoor(FB& c, TexRng& rng) {
    c.fill(0, 0, 64, 64, Col::argb(0xFF4B3826));
    // 木纹
    for (int i = 0; i < 26; i++) {
        float x = rng.nextFloat() * 64.f;
        c.fill((int)x, 0, 1, 64, Col::argb(0x33241A10));
    }
    // 边框
    c.strokeRect(2, 2, 60, 60, 3, Col::argb(0xFF2E2117));
    // 门板嵌线
    c.strokeRect(10, 10, 44, 18, 2, Col::argb(0xFF2E2117));
    c.strokeRect(10, 34, 44, 22, 2, Col::argb(0xFF2E2117));
    // 门牌（302）
    c.fill(24, 4, 16, 6, Col::argb(0xFFD8D4C4));
    microText(c, "302", 27, 5, Col::argb(0xFF222222));
    // 黄铜把手
    c.fillCircle(56, 34, 3, Col::argb(0xFFC9A227));
}

void Textures::metalDoor(FB& c, TexRng& rng) {
    c.fill(0, 0, 64, 64, Col::argb(0xFF3E444A));
    // 拉丝
    for (int y = 0; y < 64; y += 3) {
        Col cc = rng.nextBool() ? Col::argb(0x11333B41) : Col::argb(0x11505860);
        c.fill(0, y, 64, 1, cc);
    }
    // 铆钉
    for (int x = 6; x < 64; x += 13) {
        c.fillCircle((float)x, 5, 1.5f, Col::argb(0xFF5A646C));
        c.fillCircle((float)x, 59, 1.5f, Col::argb(0xFF5A646C));
    }
    // 锁箱
    c.fill(22, 24, 20, 20, Col::argb(0xFF262B30));
    c.strokeRect(22, 24, 20, 20, 2, Col::argb(0xFF12161A));
    c.fillCircle(32, 32, 2.5f, Col::argb(0xFFC9A227));
    c.fill(31, 32, 2, 6, Col::argb(0xFFC9A227));
    // B1 字样
    microText(c, "B1", 29, 10, Col::argb(0xFFB8BFC6));
}

void Textures::stairs(FB& c) {
    c.fill(0, 0, 64, 64, Col::argb(0xFF101318));
    // 侧壁（梯形）
    float s1[] = {0, 0, 8, 10, 8, 62, 0, 64};
    c.fillPoly(s1, 4, Col::argb(0xFF1A1F26));
    float s2[] = {64, 0, 56, 10, 56, 62, 64, 64};
    c.fillPoly(s2, 4, Col::argb(0xFF1A1F26));
    // 逐级变暗的台阶
    for (int i = 0; i < 6; i++) {
        int v = 34 + i * 5;
        c.fill(8 + i * 3, 12 + i * 8, (56 - i * 3) - (8 + i * 3), 8,
               Col((uint8_t)v, (uint8_t)(v + 2), (uint8_t)(v + 5)));
    }
    // 向下箭头
    float a[] = {32, 2, 27, 8, 37, 8};
    c.fillPoly(a, 3, Col::argb(0xFFC9C6B8));
    c.fill(31, 2, 2, 4, Col::argb(0xFFC9C6B8));
}

void Textures::gateBase(FB& c, TexRng& rng) {
    c.fill(0, 0, 64, 64, Col::argb(0xFF2F3B33));
    for (int i = 0; i < 20; i++) {
        float x = rng.nextFloat() * 64.f;
        c.fill((int)x, 0, 1, 64, Col::argb(0x22000000));
    }
    c.fill(0, 6, 64, 5, Col::argb(0xFF26302A));
    c.fill(0, 53, 64, 5, Col::argb(0xFF26302A));
    c.fill(30, 0, 4, 64, Col::argb(0xFF101613));
}

void Textures::plank(FB& c, TexRng& rng, int y) {
    c.fill(1, y, 62, 13, Col::argb(0xFF7A5C39));
    for (int i = 0; i < 8; i++) {
        float x = rng.nextFloat() * 60.f + 2;
        c.fill((int)x, y + 2, 1, 9, Col::argb(0x305A4026));
    }
    c.strokeRect(1, y, 62, 13, 2, Col::argb(0xFF4A3620));
    c.fillCircle(7, y + 6, 2, Col::argb(0xFF3A3A3A));
    c.fillCircle(57, y + 6, 2, Col::argb(0xFF3A3A3A));
}

void Textures::cardReader(FB& c) {
    c.fill(42, 22, 19, 20, Col::argb(0xFF1E2126));
    c.strokeRect(42, 22, 19, 20, 2, Col::argb(0xFF0E1114));
    c.fillCircle(51, 28, 2.5f, Col(0xFF, 0x44, 0x44)); // 红灯
    c.fill(46, 34, 11, 3, Col::argb(0xFF0A0C0E));      // 卡槽
}

// ---------------------------------------------------------------------------
// 精灵
// ---------------------------------------------------------------------------

void Textures::keySprite(FB& c) {
    // 光晕
    c.fillCircle(16, 16, 13, Col::argb(0x44E8C860));
    // 圆环（沿圆周密集铺点，宽 3px）
    for (int i = 0; i < 32; i++) {
        float a = (float)i / 32.f * 6.2831853f;
        c.fillCircle(10 + std::cos(a) * 5.f, 16 + std::sin(a) * 5.f, 1.8f,
                     Col::argb(0xFFC9A227));
    }
    // 杆 + 齿
    c.fill(14, 14, 13, 4, Col::argb(0xFFC9A227));
    c.fill(22, 18, 3, 5, Col::argb(0xFFC9A227));
    c.fill(26, 18, 2, 6, Col::argb(0xFFC9A227));
    c.fill(14, 14, 13, 1, Col::argb(0xFFE8D48A)); // 高光
}

void Textures::cardSprite(FB& c) {
    c.fillRoundRect(2, 6, 28, 20, 3, Col::argb(0x33FFFFFF));
    c.fillRoundRect(4, 8, 24, 16, 3, Col::argb(0xFFE8E8E0));
    c.fill(4, 8, 24, 5, Col::argb(0xFFB03A3A));
    c.fill(7, 16, 6, 4, Col::argb(0xFFC9A227));
    c.fill(15, 16, 10, 1, Col::argb(0xFF888888));
    c.fill(15, 19, 10, 1, Col::argb(0xFF888888));
}

void Textures::torchSprite(FB& c) {
    c.fillCircle(8, 16, 9, Col::argb(0x55F5E9A8));
    c.fill(10, 12, 20, 8, Col::argb(0xFF6E777D));
    c.fill(14, 12, 2, 8, Col::argb(0xFF4A5257));
    c.fill(20, 12, 2, 8, Col::argb(0xFF4A5257));
    c.fill(4, 10, 6, 12, Col::argb(0xFF3B4247));
    c.fillCircle(7, 16, 3.5f, Col::argb(0xFFF5E9A8));
}

void Textures::crowbarSprite(FB& c) {
    // 橇棍：曲线以折线逼近（Java quadTo）
    float pts[26];
    int n = 0;
    float x = 5, y = 28; // moveTo(5,28)
    auto quadTo = [&](float cx, float cy, float ex, float ey) {
        for (int i = 1; i <= 6; i++) {
            float t = (float)i / 6.f;
            float u = 1 - t;
            float px = u * u * x + 2 * u * t * cx + t * t * ex;
            float py = u * u * y + 2 * u * t * cy + t * t * ey;
            pts[n * 2] = px; pts[n * 2 + 1] = py; n++;
        }
        x = ex; y = ey;
    };
    quadTo(18, 24, 25, 10);
    quadTo(29, 4, 27, 12);
    for (int i = 0; i < n - 1; i++)
        c.drawLine(pts[i * 2], pts[i * 2 + 1], pts[i * 2 + 2], pts[i * 2 + 3],
                   5, Col::argb(0xFF8A2F27));
    for (int i = 0; i < n - 1; i++)
        c.drawLine(pts[i * 2], pts[i * 2 + 1], pts[i * 2 + 2], pts[i * 2 + 3],
                   2, Col::argb(0xFFC05248));
}

void Textures::ghostSprite(FB& c) {
    // 白裙（三角）
    float dress[] = {24, 6, 6, 62, 42, 62};
    c.fillPoly(dress, 3, Col::argb(0xFFE9E9E2));
    c.fillCircle(24, 12, 9, Col::argb(0xFFE9E9E2));
    // 长发
    float hair[] = {14, 12, 24, -2, 34, 12, 37, 26, 31, 14, 29, 40,
                    23, 16, 18, 36, 14, 14};
    c.fillPoly(hair, 9, Col::argb(0xFF0C0C10));
    // 双眼
    c.fillCircle(20, 12, 2, Col(0, 0, 0, 255));
    c.fillCircle(28, 12, 2, Col(0, 0, 0, 255));
}

void Textures::bedSprite(FB& c) {
    // 影子
    c.fill(6, 32, 52, 6, Col::argb(0x22000000));
    // 床头板
    c.fill(6, 6, 5, 22, Col::argb(0xFF2E3335));
    // 床架
    c.fill(8, 18, 52, 8, Col::argb(0xFF3A3F3D));
    // 床垫
    c.fill(10, 12, 48, 8, Col::argb(0xFFC9C6B8));
    // 枕头
    c.fillRoundRect(12, 8, 14, 7, 3, Col::argb(0xFFE4E0D2));
    // 脚轮
    c.fillCircle(16, 30, 3, Col::argb(0xFF191D20));
    c.fillCircle(52, 30, 3, Col::argb(0xFF191D20));
}

// 3×5 微型像素字（数字 + B）
void Textures::microText(FB& c, const char* s, int x, int y, Col col) {
    static const uint8_t G[12][5] = {
        // 0
        {0b111, 0b101, 0b101, 0b101, 0b111},
        // 1
        {0b010, 0b110, 0b010, 0b010, 0b111},
        // 2
        {0b111, 0b001, 0b111, 0b100, 0b111},
        // 3
        {0b111, 0b001, 0b111, 0b001, 0b111},
        // 4
        {0b101, 0b101, 0b111, 0b001, 0b001},
        // 5
        {0b111, 0b100, 0b111, 0b001, 0b111},
        // 6
        {0b111, 0b100, 0b111, 0b101, 0b111},
        // 7
        {0b111, 0b001, 0b001, 0b001, 0b001},
        // 8
        {0b111, 0b101, 0b111, 0b101, 0b111},
        // 9
        {0b111, 0b101, 0b111, 0b001, 0b111},
        // B
        {0b110, 0b101, 0b110, 0b101, 0b111},
        // (unused)
        {0, 0, 0, 0, 0},
    };
    int ox = x;
    for (const char* p = s; *p; p++) {
        int idx = -1;
        if (*p >= '0' && *p <= '9') idx = *p - '0';
        else if (*p == 'B') idx = 10;
        if (idx >= 0) {
            for (int r = 0; r < 5; r++)
                for (int b = 0; b < 3; b++)
                    if (G[idx][r] & (4 >> b)) c.fill(ox + b, y + r, 1, 1, col);
        }
        ox += 4;
    }
}
