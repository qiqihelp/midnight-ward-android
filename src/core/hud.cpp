// hud.cpp — HUD / 过场渲染 + 触摸操控（GameView.java 的 C++ 移植）。
#include "hud.h"
#include "textures.h"
#include <algorithm>
#include <cmath>

void HUD::layout(int w, int h, float density) {
    w_ = w; h_ = h; dp_ = density > 0.25f ? density : 1.f;

    btnR_ = 46.f * dp_;
    btnX_ = (float)w - 70.f * dp_;
    btnY_ = (float)h - 120.f * dp_;
    torchR_ = 32.f * dp_;
    torchX_ = (float)w - 70.f * dp_;
    torchY_ = (float)h - 260.f * dp_;
    joyR_ = JOY_MAX * dp_;
    defJoyX_ = 120.f * dp_;
    defJoyY_ = (float)h - 130.f * dp_;

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
// 游戏 HUD
// ---------------------------------------------------------------------------

void HUD::drawPlay(FB& fb, Game& g, float t) {
    const Player& p = g.player;
    float cx = w_ * 0.5f;

    // 目标条
    fb.fillRoundRect(cx - 160.f * dp_, 8.f * dp_, 320.f * dp_, 28.f * dp_,
                     8.f * dp_, Col(0x10, 0x10, 0x10, 136));
    drawTextC(fb, g.questText(), 29.f * dp_, 14.f * dp_, palette::TEXT_MAIN,
              240.f / 255.f, cx);

    // 物品栏（左上，与 Java 版一致：钥匙/房卡/手电/B1钥匙/撬棍）
    float iy = 10.f * dp_;
    float iw = 22.f * dp_;
    if (tex_) {
        if (p.hasKey) { drawIcon(fb, tex_->sprites[Textures::S_KEY], iy, 10.f * dp_, iw); iy += iw + 6.f * dp_; }
        if (p.hasCard) { drawIcon(fb, tex_->sprites[Textures::S_CARD], iy, 10.f * dp_, iw); iy += iw + 6.f * dp_; }
        if (p.hasFlashlight) { drawIcon(fb, tex_->sprites[Textures::S_TORCH], iy, 10.f * dp_, iw); iy += iw + 6.f * dp_; }
        if (p.hasBasementKey) { drawIcon(fb, tex_->sprites[Textures::S_KEY], iy, 10.f * dp_, iw); iy += iw + 6.f * dp_; }
        if (p.hasCrowbar) drawIcon(fb, tex_->sprites[Textures::S_CROWBAR], iy, 10.f * dp_, iw);
    }

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
}

// ---------------------------------------------------------------------------
// 触摸操控（GameView.onTouchEvent 的移植）
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
    // 左侧摇杆 / 右侧转身
    if (x < w_ * 0.45f) {
        joyId_ = id;
        joyBaseX_ = x;
        joyBaseY_ = y;
        joyVecX_ = 0.f;
        joyVecY_ = 0.f;
    } else {
        lookId_ = id;
        lastLookX_ = x;
    }
}

void HUD::touchMove(int id, float x, float y, Game& g) {
    (void)g;
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
        lastLookX_ = x;
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
