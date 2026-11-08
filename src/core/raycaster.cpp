// raycaster.cpp — Raycaster.java 的 C++ 移植。
#include "raycaster.h"
#include <algorithm>
#include <cmath>

static const FB* wallTexFor(Textures& t, int cell, Game& g) {
    switch (cell) {
        case lm::CELL_PLASTER: return &t.walls[Textures::T_PLASTER];
        case lm::CELL_TILE:    return &t.walls[Textures::T_TILE];
        case lm::CELL_BLOOD:   return &t.walls[Textures::T_BLOOD];
        case lm::CELL_DOOR:    return &t.walls[Textures::T_DOOR];
        case lm::CELL_METAL:   return &t.walls[Textures::T_METAL];
        case lm::CELL_STAIR:   return &t.walls[Textures::T_STAIRS];
        case lm::CELL_GATE:
            if (g.boards >= 2) return &t.walls[Textures::T_GATE_2];
            if (g.boards == 1) return &t.walls[Textures::T_GATE_1];
            return &t.walls[Textures::T_GATE_CARD];
        default:               return &t.walls[Textures::T_PLASTER];
    }
}

static const FB* spriteTexFor(Textures& t, int type) {
    switch (type) {
        case Sprite::TYPE_KEY:     return &t.sprites[Textures::S_KEY];
        case Sprite::TYPE_CARD:    return &t.sprites[Textures::S_CARD];
        case Sprite::TYPE_TORCH:   return &t.sprites[Textures::S_TORCH];
        case Sprite::TYPE_BKEY:    return &t.sprites[Textures::S_KEY];
        case Sprite::TYPE_CROWBAR: return &t.sprites[Textures::S_CROWBAR];
        case Sprite::TYPE_BED:     return &t.sprites[Textures::S_BED];
        case Sprite::TYPE_GHOST:   return &t.sprites[Textures::S_GHOST];
        default:                   return nullptr;
    }
}

static float doorOpen(Game& g, int floor, int x, int y, int cell) {
    if (cell == lm::CELL_GATE) return g.gateOpenness;
    auto it = g.lm.doors.find(LevelMap::key(floor, x, y));
    return it == g.lm.doors.end() ? 0.f : it->second.openness;
}

float Raycaster::brightness(float camAbs, float dist, bool flash, bool b1,
                            float flicker, bool side) {
    double amb = b1 ? 0.05 : 0.42;
    double fade = 1.25 / (1.0 + (double)dist * (double)dist * 0.055);
    double b = amb * fade * 2.0;
    if (flash) {
        double cone = std::max(0.0, 1.0 - (double)camAbs * 1.85);
        cone *= cone;
        b += cone * (1.55 / (1.0 + (double)dist * 0.62)) * (double)flicker;
    }
    if (side) b *= 0.78;
    float r = (float)b;
    return std::max(0.f, std::min(1.f, r));
}

void Raycaster::render(FB& c, Game& g, Textures& t, float bobY, float flicker) {
    int w = c.w, h = c.h;
    if (w <= 0 || h <= 0) return;
    if ((int)zbuf_.size() < w) zbuf_.assign(w, 1e9f);

    Player& p = g.player;
    float dirX = std::cos(p.angle);
    float dirY = std::sin(p.angle);
    float planeLen = 0.72f; // ~72° 视场角
    float planeX = -dirY * planeLen;
    float planeY = dirX * planeLen;

    bool b1 = p.floor == lm::B1;
    bool flash = p.flashlightOn && p.hasFlashlight;

    // 天花板 / 地板（B1 更黑）
    c.fill(0, 0, w, h / 2, b1 ? palette::CEIL_B1 : palette::CEIL_NORMAL);
    c.fill(0, h / 2, w, h - h / 2, b1 ? palette::FLOOR_B1 : palette::FLOOR_NORMAL);

    const auto& gmap = g.lm.grid[p.floor];
    int gmaph = (int)gmap.size();

    // ---------------- 墙面（逐列 DDA） ----------------
    for (int x = 0; x < w; x++) {
        float cameraX = 2.f * x / w - 1.f;
        double rayX = dirX + planeX * cameraX;
        double rayY = dirY + planeY * cameraX;

        int mapX = (int)std::floor(p.x);
        int mapY = (int)std::floor(p.y);
        double deltaX = (std::abs(rayX) < 1e-9) ? 1e30 : std::abs(1.0 / rayX);
        double deltaY = (std::abs(rayY) < 1e-9) ? 1e30 : std::abs(1.0 / rayY);
        int stepX = rayX < 0 ? -1 : 1;
        int stepY = rayY < 0 ? -1 : 1;
        double sdX = rayX < 0 ? (p.x - mapX) * deltaX : (mapX + 1.0 - p.x) * deltaX;
        double sdY = rayY < 0 ? (p.y - mapY) * deltaY : (mapY + 1.0 - p.y) * deltaY;

        double perp = 0.0;
        double wallX = 0.0;
        int side = 0;
        const FB* tex = nullptr;
        bool hit = false;

        for (int iter = 0; iter < 96 && !hit; iter++) {
            if (sdX < sdY) {
                sdX += deltaX;
                mapX += stepX;
                side = 0;
            } else {
                sdY += deltaY;
                mapY += stepY;
                side = 1;
            }
            if (mapX < 0 || mapY < 0 || mapY >= gmaph
                || mapX >= (int)gmap[mapY].size()) {
                break;
            }
            int cell = gmap[mapY][mapX];
            if (cell == lm::CELL_FLOOR) continue;
            bool thin = cell == lm::CELL_DOOR || cell == lm::CELL_METAL
                      || cell == lm::CELL_STAIR || cell == lm::CELL_GATE;
            if (thin) {
                // 薄门：与格子中平面求交，openness 为滑开量
                double tt = 0, coord = 0;
                if (side == 0) {
                    if (std::abs(rayX) < 1e-9) continue;
                    tt = (mapX + 0.5 - p.x) / rayX;
                    double hy = p.y + tt * rayY;
                    if (tt <= 0 || (int)std::floor(hy) != mapY) continue;
                    coord = hy - mapY;
                } else {
                    if (std::abs(rayY) < 1e-9) continue;
                    tt = (mapY + 0.5 - p.y) / rayY;
                    double hx = p.x + tt * rayX;
                    if (tt <= 0 || (int)std::floor(hx) != mapX) continue;
                    coord = hx - mapX;
                }
                double open = doorOpen(g, p.floor, mapX, mapY, cell);
                if (coord >= open) {
                    perp = tt;
                    wallX = coord - open; // 纹理随门滑动
                    hit = true;
                    tex = wallTexFor(t, cell, g);
                }
                // 否则门已滑开该部分，继续 DDA
            } else {
                perp = (side == 0) ? sdX - deltaX : sdY - deltaY;
                double hx = (side == 0) ? p.y + perp * rayY : p.x + perp * rayX;
                wallX = hx - std::floor(hx);
                hit = true;
                tex = wallTexFor(t, cell, g);
            }
        }

        if (!hit || !tex) {
            zbuf_[x] = 1e9f;
            continue;
        }
        if (perp < 0.02) perp = 0.02;
        zbuf_[x] = (float)perp;

        int lineH = (int)(h / perp);
        int midY = h / 2 + (int)bobY;
        int y0 = midY - lineH / 2;
        int cy0 = std::max(0, y0);
        int cy1 = std::min(h, y0 + lineH);
        if (cy1 <= cy0) continue;
        int texX = (int)(wallX * (Textures::TS - 1));
        if (texX < 0) texX = 0;
        else if (texX > Textures::TS - 1) texX = Textures::TS - 1;

        // 竖列贴图（最近邻）+ 黑暗系数
        float b = brightness(std::abs(cameraX), (float)perp, flash, b1, flicker,
                             side == 1);
        int dark = (int)((1.f - std::min(1.f, b)) * 255.f);
        if (dark > 255) dark = 255;
        int colSpan = cy1 - cy0;
        for (int yy = 0; yy < colSpan; yy++) {
            int ty = yy * Textures::TS / colSpan;
            if (ty < 0) ty = 0;
            if (ty > Textures::TS - 1) ty = Textures::TS - 1;
            const uint8_t* s = tex->row(ty) + (size_t)texX * 4;
            uint8_t* d = c.row(cy0 + yy) + (size_t)x * 4;
            if (dark >= 255) { d[0] = d[1] = d[2] = 0; d[3] = 255; }
            else if (dark <= 0) { d[0] = s[0]; d[1] = s[1]; d[2] = s[2]; d[3] = 255; }
            else {
                unsigned inv = 255u - (unsigned)dark;
                d[0] = (uint8_t)((s[0] * inv) / 255);
                d[1] = (uint8_t)((s[1] * inv) / 255);
                d[2] = (uint8_t)((s[2] * inv) / 255);
                d[3] = 255;
            }
        }
    }

    // ---------------- 精灵（远 → 近，zbuffer 裁剪） ----------------
    struct Item { float d2; const Sprite* s; };
    std::vector<Item> list;
    for (const auto& s : g.sprites) {
        if (!s.alive || s.floor != p.floor) continue;
        float dx = s.x - p.x, dy = s.y - p.y;
        list.push_back({dx * dx + dy * dy, &s});
    }
    if (g.ghost && g.ghost->floor == p.floor) {
        float dx = g.ghost->x - p.x, dy = g.ghost->y - p.y;
        list.push_back({dx * dx + dy * dy, g.ghost.get()});
    }
    std::sort(list.begin(), list.end(),
              [](const Item& a, const Item& b) { return a.d2 > b.d2; });

    float invDet = 1.f / (planeX * dirY - dirX * planeY);

    for (const Item& item : list) {
        const Sprite& s = *item.s;
        float dx = s.x - p.x, dy = s.y - p.y;
        float tX = invDet * (dirY * dx - dirX * dy);
        float tY = invDet * (-planeY * dx + planeX * dy); // 深度
        if (tY <= 0.12f) continue;
        float sx = (w / 2.f) * (1.f + tX / tY);
        float scale = h / tY;
        int size = (int)(scale * s.height);
        int width = (int)(size * s.aspect);
        if (size < 2 || width < 2) continue;
        float floorLine = h / 2.f + (h / 2.f) / tY + bobY;
        float bobItem = s.type == Sprite::TYPE_GHOST
            ? 0.f
            : std::sin(g.timePlayed * 5.f + s.x * 3.f) * scale * 0.02f;
        float drawEnd = floorLine - bobItem;
        float drawStart = drawEnd - size;
        int top = std::max(0, (int)drawStart);
        int bot = std::min(h, (int)drawEnd);
        if (bot <= top) continue;
        const FB* bt = spriteTexFor(t, s.type);
        if (!bt) continue;
        int bw = bt->w, bh = bt->h;
        int x0 = (int)(sx - width / 2.f);
        int x1 = x0 + width;
        float camAbs = std::abs(tX / tY);
        float b = brightness(camAbs, tY, flash, b1, flicker, false);
        float vis = s.type == Sprite::TYPE_GHOST ? (0.55f + 0.45f * b)
                                                  : (0.22f + 0.78f * b);
        int alpha = (int)(255.f * std::min(1.f, s.alpha) * vis);
        if (alpha <= 6) continue;
        if (alpha > 255) alpha = 255;
        int spanY = bot - top;
        for (int x = std::max(0, x0); x < std::min(w, x1); x++) {
            if (tY >= zbuf_[x]) continue;
            int tx = (x - x0) * bw / width;
            if (tx < 0) tx = 0;
            else if (tx >= bw) tx = bw - 1;
            for (int yy = 0; yy < spanY; yy++) {
                int ty = yy * bh / spanY;
                if (ty < 0) ty = 0;
                else if (ty >= bh) ty = bh - 1;
                const uint8_t* sp = bt->row(ty) + (size_t)tx * 4;
                unsigned sa = ((unsigned)sp[3] * (unsigned)alpha) / 255;
                if (sa == 0) continue;
                uint8_t* d = c.row(top + yy) + (size_t)x * 4;
                if (sa >= 255) {
                    d[0] = sp[0]; d[1] = sp[1]; d[2] = sp[2]; d[3] = 255;
                } else {
                    unsigned inv = 255 - sa;
                    d[0] = (uint8_t)((sp[0] * sa + d[0] * inv) / 255);
                    d[1] = (uint8_t)((sp[1] * sa + d[1] * inv) / 255);
                    d[2] = (uint8_t)((sp[2] * sa + d[2] * inv) / 255);
                    d[3] = (uint8_t)std::min(255, d[3] + (int)(sa * (255 - d[3]) / 255));
                }
            }
        }
    }
}
