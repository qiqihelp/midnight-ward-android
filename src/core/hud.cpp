// hud.cpp — HUD / 过场渲染 + 触摸操控实现。
#include "hud.h"
#include "textures.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

void HUD::layout(int w, int h, float density) {
    w_ = w; h_ = h; dp_ = density > 0.25f ? density : 1.f;

    btnR_ = 46.f * dp_;
    btnX_ = (float)w - 70.f * dp_;
    btnY_ = (float)h - 120.f * dp_;
    torchR_ = 32.f * dp_;
    torchX_ = (float)w - 70.f * dp_;
    torchY_ = (float)h - 260.f * dp_;
    // 档案按钮（左侧，摇杆上方）
    archR_ = 28.f * dp_;
    archX_ = 120.f * dp_;
    archY_ = (float)h - 250.f * dp_;
    joyR_ = JOY_MAX * dp_;
    defJoyX_ = 120.f * dp_;
    defJoyY_ = (float)h - 130.f * dp_;
    // 小地图（右上）
    mmS_ = 128.f * dp_;
    mmX_ = (float)w - mmS_ - 10.f * dp_;
    mmY_ = 10.f * dp_;

    // 选择页两个按钮
    float bw = 220.f * dp_, bh = 64.f * dp_, gap = 36.f * dp_;
    float totalW = bw * 2 + gap;
    float x0 = ((float)w - totalW) * 0.5f;
    float y0 = h * 0.52f;
    chA_[0] = x0;        chA_[1] = y0; chA_[2] = bw; chA_[3] = bh;
    chB_[0] = x0 + bw + gap; chB_[1] = y0; chB_[2] = bw; chB_[3] = bh;
}

void HUD::drawIcon(FB& fb, const FB& icon, float x, float y, float size) {
    if (icon.w == 0) return;
    FB tmp;
    tmp.resize((int)size, (int)(size * icon.h / icon.w));
    tmp.blitScaled(icon, 0, 0, tmp.w, tmp.h);
    fb.blit(tmp, (int)x, (int)y);
}

// ---------------------------------------------------------------------------
// 过场：简介 / 选择 / 胜利
// ---------------------------------------------------------------------------

void HUD::drawIntro(FB& fb, const Game& g, float t) {
    fb.clear(palette::NIGHT_BG);
    float cx = w_ * 0.5f;
    drawTextC(fb, str::GAME_TITLE, h_ * 0.15f, 42.f * dp_, palette::TEXT_MAIN, 1.f, cx);
    drawTextC(fb, str::GAME_SUBTITLE, h_ * 0.15f + 28.f * dp_, 12.f * dp_,
              palette::TEXT_DIM, 1.f, cx);
    static const char* PAGES[3][3] = {
        {str::INTRO_0_0, str::INTRO_0_1, str::INTRO_0_2},
        {str::INTRO_1_0, str::INTRO_1_1, str::INTRO_1_2},
        {str::INTRO_2_0, str::INTRO_2_1, str::INTRO_2_2},
    };
    int page = std::min(g.introPage, 2);
    for (int i = 0; i < 3; i++)
        drawTextC(fb, PAGES[page][i], h_ * 0.42f + i * 28.f * dp_, 15.f * dp_,
                  palette::TEXT_SOFT, 235.f / 255.f, cx);
    // 底部：呼吸提示 + 版本
    float breath = 120 + 100 * (0.5f + 0.5f * std::sin(t * 4.f));
    drawTextC(fb, str::TOUCH_CONTINUE, h_ * 0.86f, 13.f * dp_, palette::TEXT_MAIN,
              breath / 255.f, cx);
    drawTextC(fb, str::VERSION_TAG, h_ - 12.f * dp_, 10.f * dp_, palette::TEXT_DIM,
              200.f / 255.f, cx);
}

void HUD::drawChoose(FB& fb, float t) {
    fb.clear(palette::NIGHT_BG);
    float cx = w_ * 0.5f;
    drawTextC(fb, str::GAME_TITLE, h_ * 0.18f, 34.f * dp_, palette::TEXT_MAIN, 1.f, cx);
    drawTextC(fb, str::CHOOSE_HINT, h_ * 0.40f, 15.f * dp_, palette::TEXT_SOFT,
              235.f / 255.f, cx);

    float breath = 0.75f + 0.25f * std::sin(t * 5.f);
    for (int i = 0; i < 2; i++) {
        const float* r = i == 0 ? chA_ : chB_;
        bool pressed = chooseId_ != -1 && chooseHit_ == i + 1;
        Col fill = pressed ? Col(0x2A, 0x2F, 0x2C, 235) : Col(0x14, 0x18, 0x16, 235);
        fb.fillRoundRect(r[0], r[1], r[2], r[3], 10.f * dp_, fill);
        fb.strokeRoundRect(r[0], r[1], r[2], r[3], 10.f * dp_, 2.5f * dp_,
                           Col(0xE8, 0xE6, 0xD9,
                               (uint8_t)(pressed ? 255 : 150 * breath)));
        drawTextC(fb, i == 0 ? str::CHOOSE_RESUME : str::CHOOSE_RESTART,
                  r[1] + r[3] * 0.5f + 6.f * dp_, 18.f * dp_, palette::TEXT_MAIN,
                  pressed ? 1.f : 0.9f, r[0] + r[2] * 0.5f);
    }
}

void HUD::drawWin(FB& fb, const Game& g, float t) {
    // 黎明渐变（三段）
    fb.fillVGrad(0, 0, w_, h_, Col(0x10, 0x1A, 0x3A), Col(0x4A, 0x3F, 0x6B));
    fb.fillVGrad(0, (int)(h_ * 0.55f), w_, h_ - (int)(h_ * 0.55f),
                 Col(0x4A, 0x3F, 0x6B), Col(0xD9, 0x78, 0x44));
    // 病栋剪影
    fb.fill(0, (int)(h_ * 0.68f), (int)(w_ * 0.22f), h_, Col(0x0B, 0x0B, 0x10));
    fb.fill((int)(w_ * 0.30f), (int)(h_ * 0.62f), (int)(w_ * 0.32f), h_,
            Col(0x0B, 0x0B, 0x10));
    fb.fill((int)(w_ * 0.72f), (int)(h_ * 0.71f), w_ - (int)(w_ * 0.72f), h_,
            Col(0x0B, 0x0B, 0x10));

    float cx = w_ * 0.5f;
    drawTextC(fb, str::WIN_TITLE, h_ * 0.22f, 30.f * dp_, Col(0xF5, 0xEF, 0xE0), 1.f, cx);
    drawTextC(fb, str::WIN_0, h_ * 0.36f, 14.f * dp_, Col(0xEF, 0xE7, 0xD6),
              235.f / 255.f, cx);
    drawTextC(fb, str::WIN_1, h_ * 0.40f, 14.f * dp_, Col(0xEF, 0xE7, 0xD6),
              235.f / 255.f, cx);
    int sec = (int)g.timePlayed;
    char buf[96];
    std::snprintf(buf, sizeof(buf), "%s%d%s%d%s", str::WIN_TIME_PREFIX, sec / 60,
                  str::WIN_TIME_MIN, sec % 60, str::WIN_TIME_SEC);
    drawTextC(fb, buf, h_ * 0.52f, 15.f * dp_, palette::TEXT_WARM, 1.f, cx);
    float breath = 120 + 100 * (0.5f + 0.5f * std::sin(t * 4.f));
    drawTextC(fb, str::WIN_AGAIN, h_ * 0.82f, 15.f * dp_, Col(0xF5, 0xEF, 0xE0),
              breath / 255.f, cx);
}

// ---------------------------------------------------------------------------
// 小地图
// ---------------------------------------------------------------------------

void HUD::drawMinimap(FB& fb, Game& g, float t) {
    const Player& p = g.player;
    const float cell = mmS_ / 13.f; // 可视范围 13×13 格
    const float cx = mmX_ + mmS_ * 0.5f;
    const float cy = mmY_ + mmS_ * 0.5f;

    fb.fillRoundRect(mmX_, mmY_, mmS_, mmS_, 8.f * dp_, Col(6, 8, 8, 148));
    fb.strokeRoundRect(mmX_, mmY_, mmS_, mmS_, 8.f * dp_, 2.f * dp_,
                       Col(232, 230, 217, 66));

    int pcx = (int)std::floor(p.x), pcy = (int)std::floor(p.y);
    for (int dy = -6; dy <= 6; dy++) {
        for (int dx = -6; dx <= 6; dx++) {
            int mcx = pcx + dx, mcy = pcy + dy;
            int cellType = g.lm.cellAt(p.floor, mcx, mcy);
            float sx = cx + ((float)mcx + 0.5f - p.x) * cell;
            float sy = cy + ((float)mcy + 0.5f - p.y) * cell;
            if (sx < mmX_ + 2 || sx > mmX_ + mmS_ - 2 || sy < mmY_ + 2
                || sy > mmY_ + mmS_ - 2)
                continue;
            float px = sx - cell * 0.5f + 0.75f, py = sy - cell * 0.5f + 0.75f;
            float ps = cell - 1.5f;
            if (ps < 1.f) ps = 1.f;
            switch (cellType) {
                case lm::CELL_FLOOR:
                    fb.fill((int)px, (int)py, (int)ps, (int)ps, Col(56, 66, 60, 44));
                    break;
                case lm::CELL_DOOR: {
                    auto it = g.lm.doors.find(LevelMap::key(p.floor, mcx, mcy));
                    float open = it == g.lm.doors.end() ? 0.f : it->second.openness;
                    fb.fill((int)px, (int)py, (int)ps, (int)ps,
                            open > 0.75f ? Col(201, 162, 87, 80)
                                         : Col(201, 162, 87, 225));
                    break;
                }
                case lm::CELL_METAL:
                    fb.fill((int)px, (int)py, (int)ps, (int)ps, Col(142, 154, 166, 210));
                    break;
                case lm::CELL_STAIR:
                    fb.fill((int)px, (int)py, (int)ps, (int)ps, Col(122, 197, 197, 225));
                    break;
                case lm::CELL_GATE:
                    fb.fill((int)px, (int)py, (int)ps, (int)ps, Col(232, 177, 92, 235));
                    break;
                default: // 墙
                    fb.fill((int)px, (int)py, (int)ps, (int)ps, Col(154, 166, 156, 185));
                    break;
            }
        }
    }

    // 目标标记（同层才显示；闪烁金点）
    int tf;
    float tx, ty;
    g.questTarget(tf, tx, ty);
    if (tf == p.floor) {
        float sx = cx + (tx - p.x) * cell;
        float sy = cy + (ty - p.y) * cell;
        sx = std::max(mmX_ + 7.f * dp_, std::min(mmX_ + mmS_ - 7.f * dp_, sx));
        sy = std::max(mmY_ + 7.f * dp_, std::min(mmY_ + mmS_ - 7.f * dp_, sy));
        float pulse = 0.6f + 0.4f * std::sin(t * 6.f);
        fb.fillCircle(sx, sy, 3.4f * dp_, Col(0xE8, 0xB1, 0x5C, (uint8_t)(235 * pulse)));
        fb.strokeCircle(sx, sy, 5.2f * dp_, 1.5f * dp_,
                        Col(0xE8, 0xB1, 0x5C, (uint8_t)(150 * pulse)));
    }

    // 玩家箭头（朝向随视角）
    float a = p.angle;
    float fx = std::cos(a), fy = std::sin(a);
    float lx = -fy, ly = fx;
    float tip[2] = {cx + fx * 7.f * dp_, cy + fy * 7.f * dp_};
    float b1[2] = {cx - fx * 4.5f * dp_ + lx * 4.2f * dp_,
                   cy - fy * 4.5f * dp_ + ly * 4.2f * dp_};
    float b2[2] = {cx - fx * 4.5f * dp_ - lx * 4.2f * dp_,
                   cy - fy * 4.5f * dp_ - ly * 4.2f * dp_};
    float tri[6] = {tip[0], tip[1], b1[0], b1[1], b2[0], b2[1]};
    fb.fillPoly(tri, 3, Col(0xF2, 0xF0, 0xE4, 245));
}

// ---------------------------------------------------------------------------
// 字条阅读 / 档案叠层
// ---------------------------------------------------------------------------

void HUD::drawNoteOverlay(FB& fb, Game& g, float t) {
    int id = g.readingNote;
    if (id < 0 || id >= Game::NOTE_COUNT) return;
    const notes::NoteDef& n = notes::ALL[id];

    fb.fill(0, 0, w_, h_, Col(0, 0, 0, 152));

    int lineCount = 0;
    while (lineCount < 6 && n.lines[lineCount]) lineCount++;
    float lh = 27.f * dp_;
    float pw = std::min(w_ - 56.f * dp_, 500.f * dp_);
    float ph = 58.f * dp_ + lineCount * lh + 52.f * dp_;
    float px0 = (w_ - pw) * 0.5f;
    float py0 = (h_ - ph) * 0.5f;

    // 纸张
    fb.fillRoundRect(px0, py0, pw, ph, 6.f * dp_, Col(0xE9, 0xE2, 0xC9, 246));
    fb.strokeRoundRect(px0, py0, pw, ph, 6.f * dp_, 2.f * dp_, Col(0x8A, 0x7A, 0x55, 200));
    // 标题 + 分隔线
    font_.drawCentered(fb, n.title, px0 + pw * 0.5f, py0 + 38.f * dp_,
                       19.f * dp_, Col(0x3A, 0x30, 0x22, 255));
    fb.fill((int)(px0 + 30.f * dp_), (int)(py0 + 50.f * dp_),
            (int)(pw - 60.f * dp_), (int)(1.5f * dp_), Col(0x9A, 0x8A, 0x66, 190));
    // 正文（左对齐）
    float ly = py0 + 78.f * dp_;
    for (int i = 0; i < lineCount; i++) {
        font_.draw(fb, n.lines[i], px0 + 36.f * dp_, ly, 14.5f * dp_,
                   Col(0x4A, 0x3E, 0x2C, 255));
        ly += lh;
    }
    // 收起提示
    float pulse = 0.55f + 0.45f * std::sin(t * 5.f);
    drawTextC(fb, str::NOTE_CLOSE_HINT, py0 + ph - 24.f * dp_, 12.f * dp_,
              Col(0x6A, 0x5A, 0x40, 255), pulse, w_ * 0.5f);
}

void HUD::drawArchiveOverlay(FB& fb, Game& g, float t) {
    fb.fill(0, 0, w_, h_, Col(0, 0, 0, 158));

    float pw = std::min(w_ - 56.f * dp_, 470.f * dp_);
    float rowH = 44.f * dp_, gap = 8.f * dp_;
    float ph = 96.f * dp_ + 4 * (rowH + gap) + 40.f * dp_;
    float px0 = (w_ - pw) * 0.5f;
    float py0 = (h_ - ph) * 0.5f;

    fb.fillRoundRect(px0, py0, pw, ph, 8.f * dp_, Col(0x14, 0x18, 0x16, 242));
    fb.strokeRoundRect(px0, py0, pw, ph, 8.f * dp_, 2.f * dp_,
                       Col(0xE8, 0xE6, 0xD9, 120));

    char buf[96];
    std::snprintf(buf, sizeof(buf), str::ARCHIVE_COUNT, g.notesReadCount());
    font_.drawCentered(fb, str::ARCHIVE_TITLE, px0 + pw * 0.5f, py0 + 40.f * dp_,
                       19.f * dp_, palette::TEXT_MAIN, 1.f);
    font_.drawCentered(fb, buf, px0 + pw * 0.5f, py0 + 66.f * dp_,
                       12.f * dp_, palette::TEXT_WARM, 0.9f);

    // 双列网格：只列已收集的字条
    archRects_.clear();
    archIds_.clear();
    float colW = (pw - 40.f * dp_ - gap) * 0.5f;
    int placed = 0;
    for (int i = 0; i < Game::NOTE_COUNT; i++) {
        if (i >= (int)g.noteRead.size() || !g.noteRead[i]) continue;
        int col = placed & 1, row = placed >> 1;
        float rx = px0 + 20.f * dp_ + col * (colW + gap);
        float ry = py0 + 92.f * dp_ + row * (rowH + gap);
        fb.fillRoundRect(rx, ry, colW, rowH, 5.f * dp_, Col(0x24, 0x2A, 0x26, 235));
        fb.strokeRoundRect(rx, ry, colW, rowH, 5.f * dp_, 1.5f * dp_,
                           Col(0xE8, 0xE6, 0xD9, 90));
        font_.draw(fb, notes::ALL[i].title, rx + 14.f * dp_,
                   ry + rowH * 0.5f + 5.f * dp_, 13.f * dp_,
                   Col(0xEF, 0xE7, 0xD6, 245));
        archRects_.insert(archRects_.end(), {rx, ry, colW, rowH});
        archIds_.push_back(i);
        placed++;
    }

    float pulse = 0.55f + 0.45f * std::sin(t * 5.f);
    drawTextC(fb, str::NOTE_CLOSE_HINT, py0 + ph - 20.f * dp_, 11.5f * dp_,
              palette::TEXT_DIM, pulse, w_ * 0.5f);
}

// ---------------------------------------------------------------------------
// 游戏 HUD
// ---------------------------------------------------------------------------

void HUD::drawPlay(FB& fb, Game& g, float t) {
    const Player& p = g.player;
    float cx = w_ * 0.5f;

    // 目标条
    fb.fillRoundRect(cx - 180.f * dp_, 8.f * dp_, 360.f * dp_, 28.f * dp_,
                     8.f * dp_, Col(0x10, 0x10, 0x10, 136));
    drawTextC(fb, g.questText(), 29.f * dp_, 14.f * dp_, palette::TEXT_MAIN,
              240.f / 255.f, cx);

    // 物品栏（左上）
    float iy = 10.f * dp_;
    float iw = 22.f * dp_;
    if (tex_) {
        if (p.hasKey) { drawIcon(fb, tex_->sprites[Textures::S_KEY], iy, 10.f * dp_, iw); iy += iw + 6.f * dp_; }
        if (p.hasCard) { drawIcon(fb, tex_->sprites[Textures::S_CARD], iy, 10.f * dp_, iw); iy += iw + 6.f * dp_; }
        if (p.hasFlashlight) { drawIcon(fb, tex_->sprites[Textures::S_TORCH], iy, 10.f * dp_, iw); iy += iw + 6.f * dp_; }
        if (p.hasBasementKey) { drawIcon(fb, tex_->sprites[Textures::S_KEY], iy, 10.f * dp_, iw); iy += iw + 6.f * dp_; }
        if (p.hasCrowbar) drawIcon(fb, tex_->sprites[Textures::S_CROWBAR], iy, 10.f * dp_, iw);
    }

    // 小地图 + 楼层名（右上）
    drawMinimap(fb, g, t);
    const char* floorName = p.floor == lm::F2 ? str::FLOOR_NAME_F2
                          : p.floor == lm::F1 ? str::FLOOR_NAME_F1
                                              : str::FLOOR_NAME_B1;
    font_.drawCentered(fb, floorName, mmX_ + mmS_ * 0.5f, mmY_ + mmS_ + 18.f * dp_,
                       11.5f * dp_, Col(0xCF, 0xCF, 0xC6, 210));

    // 消息（底部中间）
    float msgY = (float)h_ - 300.f * dp_;
    int shown = 0;
    for (int i = (int)g.messages.size() - 1; i >= 0 && shown < 2; i--, shown++) {
        const Game::Msg& m = g.messages[i];
        float alpha = std::min(1.f, m.life / 0.6f);
        drawTextC(fb, m.text.c_str(), msgY, 14.f * dp_, Col(0xEF, 0xE7, 0xD6),
                  235.f / 255.f * alpha, cx);
        msgY -= 24.f * dp_;
    }

    // 准星
    fb.fillCircle(cx, h_ * 0.5f, 2.5f * dp_, Col(255, 255, 255, 153));

    // 互动按钮
    bool active = g.hasTarget;
    fb.fillCircle(btnX_, btnY_, btnR_,
                  active ? Col(0x10, 0x10, 0x10, 102) : Col(0, 0, 0, 51));
    fb.strokeCircle(btnX_, btnY_, btnR_, 2.5f * dp_,
                    active ? palette::TEXT_MAIN : Col(255, 255, 255, 85));
    drawTextC(fb, active ? g.interactLabel.c_str() : str::LABEL_DOT,
              btnY_ + 5.f * dp_, 16.f * dp_,
              active ? palette::TEXT_MAIN : Col(255, 255, 255, 102), 1.f, btnX_);

    // 手电筒按钮
    if (p.hasFlashlight) {
        fb.fillCircle(torchX_, torchY_, torchR_, Col(0, 0, 0, 51));
        fb.strokeCircle(torchX_, torchY_, torchR_, 2.5f * dp_,
                        p.flashlightOn ? Col(0xF5, 0xE9, 0xA8) : Col(255, 255, 255, 136));
        fb.fill((int)(torchX_ - 8.f * dp_), (int)(torchY_ - 4.f * dp_),
                (int)(18.f * dp_), (int)(8.f * dp_), Col(0x9A, 0xA1, 0xA6));
        fb.fill((int)(torchX_ - 14.f * dp_), (int)(torchY_ - 6.f * dp_),
                (int)(6.f * dp_), (int)(12.f * dp_), Col(0x3B, 0x42, 0x47));
        fb.fillCircle(torchX_ - 11.f * dp_, torchY_, 3.f * dp_,
                      p.flashlightOn ? Col(0xF5, 0xE9, 0xA8) : Col(0x66, 0x60, 0x55));
        drawTextC(fb, str::LABEL_TORCH, torchY_ + 20.f * dp_, 11.f * dp_,
                  p.flashlightOn ? Col(0xF5, 0xE9, 0xA8) : Col(255, 255, 255, 136),
                  1.f, torchX_);
    }

    // 档案按钮（已收集任何字条才显示）
    if (g.notesReadCount() > 0) {
        fb.fillCircle(archX_, archY_, archR_, Col(0, 0, 0, 51));
        fb.strokeCircle(archX_, archY_, archR_, 2.f * dp_, Col(232, 230, 217, 130));
        drawTextC(fb, str::ARCHIVE_BTN, archY_ + 5.f * dp_, 13.f * dp_,
                  palette::TEXT_MAIN, 0.95f, archX_);
    }

    // 虚拟摇杆
    float jx = joyId_ == -1 ? defJoyX_ : joyBaseX_;
    float jy = joyId_ == -1 ? defJoyY_ : joyBaseY_;
    fb.strokeCircle(jx, jy, joyR_, 2.f * dp_, Col(255, 255, 255, 85));
    fb.fillCircle(jx + joyVecX_ * joyR_ * 0.55f, jy + joyVecY_ * joyR_ * 0.55f,
                  24.f * dp_, Col(0x3A, 0x3F, 0x44, 136));

    // 地下室黑暗提示
    if (p.floor == lm::B1 && !p.flashlightOn) {
        int pulse = (int)(120 + 110 * (0.5f + 0.5f * std::sin(t * 6.f)));
        drawTextC(fb, str::WARN_DARK, h_ * 0.30f, 15.f * dp_, palette::TEXT_RED,
                  pulse / 255.f, cx);
    }

    // 叠层最后绘制
    if (g.archiveOpen) drawArchiveOverlay(fb, g, t);
    if (g.readingNote >= 0) drawNoteOverlay(fb, g, t);
}

// ---------------------------------------------------------------------------
// 触摸操控
// ---------------------------------------------------------------------------

static bool inCircle(float x, float y, float cx, float cy, float r) {
    float dx = x - cx, dy = y - cy;
    return dx * dx + dy * dy <= r * r;
}

void HUD::touchDown(int id, float x, float y, Game& g) {
    if (g.state == Game::ST_INTRO) {
        g.introPage++;
        if (g.introPage >= 3) {
            if (onIntroFinished) onIntroFinished();
            else g.startPlay();
        }
        return;
    }
    if (g.state == Game::ST_WIN) {
        if (onWinTapped) onWinTapped();
        else g.reset();
        return;
    }
    if (g.state == Game::ST_CHOOSE) {
        chooseId_ = id;
        chooseHit_ = inRectf(x, y, chA_) ? 1 : (inRectf(x, y, chB_) ? 2 : 0);
        return;
    }
    if (g.state != Game::ST_PLAY) return;

    // 字条阅读中：任意点击收起
    if (g.readingNote >= 0) {
        g.readingNote = -1;
        return;
    }
    // 档案中：点行回看字条，点空白关闭
    if (g.archiveOpen) {
        for (size_t i = 0; i < archIds_.size() && i < archRects_.size() / 4; i++) {
            if (x >= archRects_[i * 4] && x <= archRects_[i * 4] + archRects_[i * 4 + 2]
                && y >= archRects_[i * 4 + 1]
                && y <= archRects_[i * 4 + 1] + archRects_[i * 4 + 3]) {
                g.archiveOpen = false;
                g.readingNote = archIds_[i];
                return;
            }
        }
        g.archiveOpen = false;
        return;
    }
    // 档案按钮
    if (g.notesReadCount() > 0 && inCircle(x, y, archX_, archY_, archR_ * 1.7f)) {
        g.archiveOpen = true;
        return;
    }
    // 互动按钮
    if (inCircle(x, y, btnX_, btnY_, btnR_ * 1.4f)) {
        g.interact();
        return;
    }
    // 手电筒按钮
    if (g.player.hasFlashlight && inCircle(x, y, torchX_, torchY_, torchR_ * 1.6f)) {
        g.player.flashlightOn = !g.player.flashlightOn;
        return;
    }
    // 左侧摇杆 / 右侧环顾（水平转身 + 俯仰）
    if (x < w_ * 0.45f) {
        joyId_ = id;
        joyBaseX_ = x;
        joyBaseY_ = y;
        joyVecX_ = 0.f;
        joyVecY_ = 0.f;
    } else {
        lookId_ = id;
        lastLookX_ = x;
        lastLookY_ = y;
    }
}

void HUD::touchMove(int id, float x, float y, Game& g) {
    if (id == joyId_) {
        float dx = x - joyBaseX_;
        float dy = y - joyBaseY_;
        float m = std::sqrt(dx * dx + dy * dy);
        if (m > joyR_) {
            dx = dx / m * joyR_;
            dy = dy / m * joyR_;
        }
        joyVecX_ = dx / joyR_;
        joyVecY_ = dy / joyR_;
    } else if (id == lookId_) {
        g.player.angle += (x - lastLookX_) * TURN_PER_PX;
        // 俯仰：上滑抬头、下滑低头（钳制 ±0.30 屏高）
        g.player.pitch += -(y - lastLookY_) * PITCH_PER_PX;
        if (g.player.pitch > 0.30f) g.player.pitch = 0.30f;
        if (g.player.pitch < -0.30f) g.player.pitch = -0.30f;
        lastLookX_ = x;
        lastLookY_ = y;
    }
}

void HUD::touchUp(int id, float x, float y, Game& g) {
    if (id == joyId_) {
        joyId_ = -1;
        joyVecX_ = 0.f;
        joyVecY_ = 0.f;
    }
    if (id == lookId_) lookId_ = -1;
    if (id == chooseId_) {
        int hit = inRectf(x, y, chA_) ? 1 : (inRectf(x, y, chB_) ? 2 : 0);
        if (hit != 0 && hit == chooseHit_) {
            if (onChoose) onChoose(hit == 1);
        }
        chooseId_ = -1;
        chooseHit_ = 0;
    }
}

void HUD::touchCancel() {
    joyId_ = -1;
    lookId_ = -1;
    chooseId_ = -1;
    joyVecX_ = 0.f;
    joyVecY_ = 0.f;
}
