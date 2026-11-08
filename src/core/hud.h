// hud.h — HUD / 过场渲染 + 触摸操控（GameView.java 的 C++ 演进版）。
// v2.1 新增：小地图 + 目标标记、档案回看、字条阅读页、楼层名、俯仰视角。
#pragma once
#include "fb.h"
#include "font.h"
#include "game.h"
#include <functional>
#include <vector>

class Textures;

class HUD {
public:
    // App 注入的语义回调
    std::function<void()> onIntroFinished;  // 简介翻完（App 决定进游戏还是选择页）
    std::function<void()> onWinTapped;      // 胜利页点击（App 清存档 + reset）
    std::function<void(bool resume)> onChoose; // 选择页：继续 / 重新开始

    HUD(Font& font) : font_(font) {}
    void setTextures(Textures* t) { tex_ = t; }

    void layout(int w, int h, float density);

    // 渲染（全部画到全分辨率 fb）
    void drawIntro(FB& fb, const Game& g, float t);
    void drawChoose(FB& fb, float t);
    void drawWin(FB& fb, const Game& g, float t);
    void drawPlay(FB& fb, Game& g, float t); // 3D 之上的 HUD + 叠层

    // 触摸（多指），返回是否被 HUD 消费
    void touchDown(int id, float x, float y, Game& g);
    void touchMove(int id, float x, float y, Game& g);
    void touchUp(int id, float x, float y, Game& g);
    void touchCancel();

    // 摇杆输入（App::tick 读取用于移动）
    bool joyActive() const { return joyId_ != -1; }
    float joyVecX() const { return joyVecX_; }
    float joyVecY() const { return joyVecY_; }

    static constexpr float JOY_MAX = 62.f;     // dp
    static constexpr float TURN_PER_PX = 0.0042f;
    static constexpr float PITCH_PER_PX = 0.0016f; // 屏高比例 / px

private:
    Font& font_;
    Textures* tex_ = nullptr;
    int w_ = 1280, h_ = 720;
    float dp_ = 2.f; // density

    // 按钮（px）
    float btnX_ = 0, btnY_ = 0, btnR_ = 0;         // 互动
    float torchX_ = 0, torchY_ = 0, torchR_ = 0;   // 手电筒
    float archX_ = 0, archY_ = 0, archR_ = 0;      // 档案
    float defJoyX_ = 0, defJoyY_ = 0, joyR_ = 0;   // 摇杆
    // 小地图
    float mmX_ = 0, mmY_ = 0, mmS_ = 0;
    // 选择页按钮
    float chA_[4] = {0, 0, 0, 0};  // 继续游戏 x,y,w,h
    float chB_[4] = {0, 0, 0, 0};  // 重新开始

    // 档案列表行命中区（绘制时填充，触摸时消费）
    std::vector<float> archRects_;   // x,y,w,h × n
    std::vector<int> archIds_;

    // 触摸状态
    int joyId_ = -1, lookId_ = -1, chooseId_ = -1;
    int chooseHit_ = 0; // 0 无 1 左（继续） 2 右（重新开始）
    float joyBaseX_ = 0, joyBaseY_ = 0;
    float joyVecX_ = 0, joyVecY_ = 0;
    float lastLookX_ = 0, lastLookY_ = 0;

    void drawTextC(FB& fb, const char* s, float y, float sizePx, Col c, float alpha,
                   float cx) {
        font_.drawCentered(fb, s, cx, y, sizePx, c, alpha);
    }
    void drawIcon(FB& fb, const FB& icon, float x, float y, float size);
    void drawMinimap(FB& fb, Game& g, float t);
    void drawNoteOverlay(FB& fb, Game& g, float t);
    void drawArchiveOverlay(FB& fb, Game& g, float t);
    bool inRectf(float x, float y, const float* r) const {
        return x >= r[0] && x <= r[0] + r[2] && y >= r[1] && y <= r[1] + r[3];
    }
};
