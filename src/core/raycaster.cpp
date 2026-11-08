// raycaster.cpp — 光线投射 3D 渲染器实现。
//
// 渲染管线（全部到低分辨率 FB，随后由 App 双线性放大上屏）：
//   1) 地板 / 天花板：逐行透视投射（floor casting），含灯盘发光标记
//   2) 墙面：逐列 DDA（含薄门滑开半平面判定），竖向双线性采样
//   3) 精灵：远→近 billboard，zbuffer 逐列裁剪
//   4) 屏幕暗角
// 光照：环境衰减 + 手电筒椭圆锥（coneX/coneY 可分离表）+ 距离雾
//      + 4×4 Bayer 抖动消除暗部色带。
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
        case Sprite::TYPE_NOTE:    return &t.sprites[Textures::S_NOTE];
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
        b += cone * (3.6 / (1.0 + (double)dist * 0.62)) * (double)flicker;
    }
    if (side) b *= 0.78;
    float r = (float)b;
    return std::max(0.f, std::min(1.f, r));
}

void Raycaster::resizeTables(int w, int h) {
    if ((int)zbuf_.size() < w) zbuf_.assign(w, 1e9f);
    if ((int)coneX_.size() != w) coneX_.assign(w, 0.f);
    if ((int)vigX_.size() != w) vigX_.assign(w, 0.f);
    if ((int)coneY_.size() != h) coneY_.assign(h, 0.f);
    if ((int)vigY_.size() != h) vigY_.assign(h, 0.f);
}

// 4×4 Bayer 抖动（加到亮度上，消除雾效暗部色带）
static const float BAYER[16] = {
    -0.0471f,  0.0353f, -0.0157f,  0.0510f,
     0.0667f, -0.0314f,  0.0588f, -0.0235f,
    -0.0196f,  0.0431f, -0.0392f,  0.0275f,
     0.0549f, -0.0275f,  0.0471f, -0.0118f,
};

void Raycaster::applyVignette(FB& c) {
    int w = c.w, h = c.h;
    const float* vx = vigX_.data();
    const float* vy = vigY_.data();
    for (int y = 0; y < h; y++) {
        uint8_t* row = c.row(y);
        float fy = vy[y];
        for (int x = 0; x < w; x++) {
            float v = vx[x] * fy;
            unsigned m = (unsigned)(v * 256.f + 0.5f);
            uint8_t* p = row + (size_t)x * 4;
            p[0] = (uint8_t)((p[0] * m) >> 8);
            p[1] = (uint8_t)((p[1] * m) >> 8);
            p[2] = (uint8_t)((p[2] * m) >> 8);
        }
    }
}

void Raycaster::render(FB& c, Game& g, Textures& t, float bobY, float flicker) {
    int w = c.w, h = c.h;
    if (w <= 0 || h <= 0) return;
    resizeTables(w, h);

    Player& p = g.player;
    float dirX = std::cos(p.angle);
    float dirY = std::sin(p.angle);
    float planeLen = 0.72f; // ~72° 视场角
    float planeX = -dirY * planeLen;
    float planeY = dirX * planeLen;

    bool b1 = p.floor == lm::B1;
    bool flash = p.flashlightOn && p.hasFlashlight;

    // 地平线：俯仰视角 + 行走浮动（钳制，保证地板/天花板各行均有意义）
    float horizon = h * 0.5f + p.pitch * h + bobY;
    if (horizon < h * 0.18f) horizon = h * 0.18f;
    if (horizon > h * 0.82f) horizon = h * 0.82f;
    int hz = (int)horizon;

    // ---- 查找表：手电锥（可分离椭圆）与暗角 ----
    for (int x = 0; x < w; x++) {
        float rx = ((float)x + 0.5f) / (float)w * 2.f - 1.f;
        float k = 1.f - (rx * rx) / (0.92f * 0.92f);
        coneX_[x] = k > 0.f ? k : 0.f;
        vigX_[x] = 1.f - 0.38f * rx * rx;
    }
    for (int y = 0; y < h; y++) {
        float ry = ((float)y + 0.5f - horizon) / (h * 0.5f);
        // 竖直方向软衰减：近处地面 / 头顶天花板仍保留 18% 余光（手持光源洒溢）
        float k = 1.f - ry * ry * 1.1f;
        coneY_[y] = k > 0.18f ? k : 0.18f;
        vigY_[y] = 1.f - 0.38f * ry * ry;
    }

    const float* coneX = coneX_.data();
    const float* coneY = coneY_.data();

    double ambDim = b1 ? 0.05 : 0.42;
    auto ambAt = [&](double d) {
        return ambDim * (1.25 / (1.0 + d * d * 0.055)) * 2.0;
    };
    auto flashAt = [&](double d) {
        return flash ? (3.6 / (1.0 + d * 0.62)) * (double)flicker : 0.0;
    };

    // =====================================================================
    // 1) 地板 / 天花板：逐行透视投射
    // =====================================================================
    {
        const FB& floorTex = b1 ? t.flats[Textures::FL_B1] : t.flats[Textures::FL_HALL];
        const FB& ceilTex  = b1 ? t.flats[Textures::CE_B1] : t.flats[Textures::CE_HALL];
        const uint8_t* fpx = floorTex.px.data();
        const uint8_t* cpx = ceilTex.px.data();
        const int FTW = floorTex.w, FTH = floorTex.h;   // 128
        const int CTW = ceilTex.w, CTH = ceilTex.h;     // 128
        const float posZ = 0.5f * (float)h;             // 相机高度（墙高 1.0）
        // 行方向射线（最左 / 最右）
        const float rdx0 = dirX - planeX, rdy0 = dirY - planeY;
        const float rdx1 = dirX + planeX, rdy1 = dirY + planeY;

        for (int y = 0; y < h; y++) {
            bool isFloor = y > hz;
            float dy = isFloor ? ((float)y + 0.5f - horizon)
                               : (horizon - ((float)y + 0.5f));
            uint8_t* drow = c.row(y);
            if (dy < 0.9f) { // 紧贴地平线：纯黑（雾尽头）
                for (int x = 0; x < w; x++) {
                    uint8_t* d = drow + (size_t)x * 4;
                    d[0] = d[1] = d[2] = 0; d[3] = 255;
                }
                continue;
            }
            double rowDist = (double)posZ / (double)dy;
            if (rowDist > 34.0) { // 超远距离：雾黑
                for (int x = 0; x < w; x++) {
                    uint8_t* d = drow + (size_t)x * 4;
                    d[0] = d[1] = d[2] = 0; d[3] = 255;
                }
                continue;
            }
            double stepX = ((double)rdx1 - (double)rdx0) * rowDist / (double)w;
            double stepY = ((double)rdy1 - (double)rdy0) * rowDist / (double)w;
            double fx = (double)p.x + (double)rdx0 * rowDist;
            double fy = (double)p.y + (double)rdy0 * rowDist;
            double ambRow = ambAt(rowDist);
            double flRow = flashAt(rowDist);
            if (isFloor) flRow *= 1.18; // 地面反射手电更明显
            if (ambRow + flRow < 0.005) {
                for (int x = 0; x < w; x++) {
                    uint8_t* d = drow + (size_t)x * 4;
                    d[0] = d[1] = d[2] = 0; d[3] = 255;
                }
                continue;
            }
            const uint8_t* tex = isFloor ? fpx : cpx;
            const int tw = isFloor ? FTW : CTW;
            const int th = isFloor ? FTH : CTH;
            const float cy = coneY[y];
            const int dIdx = (y & 3) << 2;
            // 64px 对应 1 世界格；负坐标按二补位与运算自然回绕
            const float texPerUnit = 64.f;
            for (int x = 0; x < w; x++, fx += stepX, fy += stepY) {
                int tx = (int)((float)fx * texPerUnit) & (tw - 1);
                int ty = (int)((float)fy * texPerUnit) & (th - 1);
                const uint8_t* s = tex + ((size_t)ty * tw + tx) * 4;
                float coneV = coneX[x] * cy;
                double bmD = ambRow + flRow * (double)coneV * 0.9;
                float bm = bmD > 1.0 ? 1.f : (float)bmD;
                float add = (float)(flRow * (double)coneV); // 手电加性暖光
                if (!isFloor && s[3] != 0) { // 天花板灯盘发光
                    float e = s[3] / 255.f;
                    bm += (1.f - bm) * (0.35f + 0.55f * e);
                }
                bm += BAYER[dIdx | (x & 3)];
                if (bm < 0.f) bm = 0.f;
                if (bm > 1.f) bm = 1.f;
                unsigned m = (unsigned)(bm * 255.f + 0.5f);
                unsigned aR = (unsigned)(add * 44.f + 0.5f);
                if (aR > 120) aR = 120;
                unsigned aG = aR * 242u / 255u;
                unsigned aB = aR * 176u / 255u;
                uint8_t* d = drow + (size_t)x * 4;
                unsigned r0 = (unsigned)s[0] * m / 255 + aR;
                unsigned r1 = (unsigned)s[1] * m / 255 + aG;
                unsigned r2 = (unsigned)s[2] * m / 255 + aB;
                d[0] = r0 > 255 ? 255 : (uint8_t)r0;
                d[1] = r1 > 255 ? 255 : (uint8_t)r1;
                d[2] = r2 > 255 ? 255 : (uint8_t)r2;
                d[3] = 255;
            }
        }
    }

    // =====================================================================
    // 2) 墙面：逐列 DDA（保留薄门滑开半平面判定）
    // =====================================================================
    const auto& gmap = g.lm.grid[p.floor];
    int gmaph = (int)gmap.size();

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
        if (perp > 34.0) { // 超远：雾吞没，地板行已画好远景，跳过墙柱
            zbuf_[x] = (float)perp;
            continue;
        }
        zbuf_[x] = (float)perp;

        int lineH = (int)((double)h / perp);
        int y0 = hz - lineH / 2;
        int cy0 = std::max(0, y0);
        int cy1 = std::min(h, y0 + lineH);
        if (cy1 <= cy0) continue;
        // 水平 + 竖向双线性采样：近距离墙面不再出现竖条纹 / 马赛克
        float texXfl = wallX * (Textures::TS - 1);
        if (texXfl < 0.f) texXfl = 0.f;
        if (texXfl > Textures::TS - 1) texXfl = Textures::TS - 1;
        int tx0 = (int)texXfl;
        int tx1 = tx0 + 1 < Textures::TS ? tx0 + 1 : Textures::TS - 1;
        float tfx = texXfl - tx0;

        double ambCol = ambAt(perp) * (side == 1 ? 0.78 : 1.0);
        double flCol = flashAt(perp);
        const uint8_t* texPx = tex->px.data();
        const int TW = tex->w;
        const float vStep = (float)Textures::TS / (float)lineH; // 竖向纹理步进
        int colSpan = cy1 - cy0;
        const float coneCol = coneX[x];
        for (int yy = 0; yy < colSpan; yy++) {
            int py = cy0 + yy;
            float tyF = (float)(py - y0) * vStep;
            if (tyF > Textures::TS - 1) tyF = Textures::TS - 1;
            int ty0 = (int)tyF;
            int ty1 = ty0 + 1 < Textures::TS ? ty0 + 1 : Textures::TS - 1;
            float tf = tyF - ty0;
            const uint8_t* s00 = texPx + ((size_t)ty0 * TW + tx0) * 4;
            const uint8_t* s01 = texPx + ((size_t)ty0 * TW + tx1) * 4;
            const uint8_t* s10 = texPx + ((size_t)ty1 * TW + tx0) * 4;
            const uint8_t* s11 = texPx + ((size_t)ty1 * TW + tx1) * 4;
            float coneV = coneCol * coneY[py];
            double bmD = ambCol + flCol * (double)coneV * 0.9;
            float bm = bmD > 1.0 ? 1.f : (float)bmD;
            float add = (float)(flCol * (double)coneV); // 手电加性暖光
            bm += BAYER[((py & 3) << 2) | (x & 3)];
            if (bm < 0.f) bm = 0.f;
            if (bm > 1.f) bm = 1.f;
            unsigned m = (unsigned)(bm * 255.f + 0.5f);
            unsigned wv = (unsigned)((1.f - tf) * 256.f);
            unsigned wv1 = 256u - wv;
            unsigned wh = (unsigned)((1.f - tfx) * 256.f);
            unsigned wh1 = 256u - wh;
            unsigned aR = (unsigned)(add * 44.f + 0.5f);
            if (aR > 120) aR = 120;
            unsigned aG = aR * 242u / 255u;
            unsigned aB = aR * 176u / 255u;
            uint8_t* d = c.row(py) + (size_t)x * 4;
            for (int ch = 0; ch < 3; ch++) {
                unsigned colA = (s00[ch] * wv + s10[ch] * wv1) >> 8;
                unsigned colB = (s01[ch] * wv + s11[ch] * wv1) >> 8;
                unsigned texv = (colA * wh + colB * wh1) >> 8;
                unsigned addC = ch == 0 ? aR : (ch == 1 ? aG : aB);
                unsigned rv = texv * m / 255 + addC;
                d[ch] = rv > 255 ? 255 : (uint8_t)rv;
            }
            d[3] = 255;
        }
    }

    // =====================================================================
    // 3) 精灵（远 → 近，zbuffer 裁剪）
    // =====================================================================
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
        // 落点线：地平线 + 半墙高（相机高 0.5）
        float floorLine = horizon + (0.5f * h) / tY;
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
                                                  : (0.30f + 0.70f * b);
        // 字条微光：黑暗中也隐约可见
        if (s.type == Sprite::TYPE_NOTE) vis = std::max(vis, 0.34f);
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

    // =====================================================================
    // 4) 屏幕暗角（电影感 + 聚焦中央）
    // =====================================================================
    applyVignette(c);

    // 5) 手电光柱泛光（尘埃感：中央柔和暖光晕，让光源状态一目了然）
    if (flash) {
        float bx = w * 0.5f;
        float by = horizon;
        c.fillCircle(bx, by, w * 0.30f, Col(255, 238, 190, 10));
        c.fillCircle(bx, by, w * 0.21f, Col(255, 238, 190, 11));
        c.fillCircle(bx, by, w * 0.13f, Col(255, 241, 198, 13));
    }
}
