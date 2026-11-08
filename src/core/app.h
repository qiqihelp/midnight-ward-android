// app.h — 应用控制器：编排 Game / HUD / Raycaster / 字体 / 帧缓冲 / 存档。
// 平台无关：Android 层与桌面模拟器共用同一套代码。
#pragma once
#include "font.h"
#include "game.h"
#include "hud.h"
#include "raycaster.h"
#include "textures.h"
#include <cstdint>
#include <functional>
#include <random>
#include <string>
#include <vector>

class App {
public:
    struct Env {
        // 读资源（Android: AAsset；桌面: 文件）
        std::function<std::vector<uint8_t>(const char* name)> loadAsset;
        std::string savePath; // 可为空（禁用存档）
        int w = 1280, h = 720;
        float density = 2.f;
    };

    explicit App(const Env& env);
    ~App();

    void resize(int w, int h, float density);
    void tick(float dt);   // 逻辑推进（dt 秒，内部钳制）
    void render();         // 渲染到内部帧缓冲
    const uint8_t* pixels() const { return winFb_.px.data(); }
    int width() const { return winFb_.w; }
    int height() const { return winFb_.h; }

    // 输入
    void touchDown(int id, float x, float y);
    void touchMove(int id, float x, float y);
    void touchUp(int id, float x, float y);
    void touchCancel();
    bool back(); // 返回键：true=已消费（游戏中防误退），false=允许退出

    // 生命周期
    void onPause();  // 自动存档
    void onResume();

    // 存档
    bool hasSaveFile() const;
    bool saveToFile();
    bool loadSaveFile(std::vector<uint8_t>& out) const;
    void resumeFromSave();
    void clearSaveFile();
    void restartNewGame(); // 清档 + 重置 + 直接开始

    // 测试 / 诊断
    Game& game() { return game_; }
    HUD& hud() { return hud_; }
    Font& font() { return font_; }
    bool mapValid() const { return mapValid_; }
    bool fontOk() const { return font_.ok(); }
    float timeNow() const { return timeSecs_; }

private:
    Game game_;
    Textures tex_;
    Raycaster ray_;
    Font font_;
    HUD hud_;
    FB winFb_;
    FB loFb_; // 低分辨率 3D 缓冲
    Env env_;

    float timeSecs_ = 0.f;
    float flicker_ = 1.f;
    float autosaveTimer_ = 0.f;
    bool mapValid_ = true;
    std::mt19937 rng_{12345u};

    static constexpr int LO_RES_W = 720; // 3D 渲染宽度（透视贴图 + 双线性上屏）

    void saveNow_();
};
