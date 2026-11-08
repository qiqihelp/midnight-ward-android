// app.cpp — 应用控制器实现。
#include "app.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <random>
#include <sys/stat.h>

static bool fileExists(const std::string& p) {
    struct stat st;
    return ::stat(p.c_str(), &st) == 0 && st.st_size > 0;
}
static bool writeFileAtomic(const std::string& path, const std::vector<uint8_t>& data) {
    if (path.empty()) return false;
    std::string tmp = path + ".tmp";
    FILE* f = std::fopen(tmp.c_str(), "wb");
    if (!f) return false;
    size_t n = std::fwrite(data.data(), 1, data.size(), f);
    std::fclose(f);
    if (n != data.size()) { std::remove(tmp.c_str()); return false; }
    if (std::rename(tmp.c_str(), path.c_str()) != 0) { std::remove(tmp.c_str()); return false; }
    return true;
}

App::App(const Env& env) : game_(), hud_(font_) {
    env_ = env;
    mapValid_ = game_.lm.checkValid(); // 启动自检：三层地图 / 门 / 传送合法性
    // 字体
    if (env_.loadAsset) {
        auto data = env_.loadAsset("font_sdf.bin");
        if (!data.empty()) font_.load(data.data(), data.size());
    }
    tex_.init();
    hud_.setTextures(&tex_);
    // 语义回调
    hud_.onIntroFinished = [this]() {
        std::vector<uint8_t> sv;
        if (!env_.savePath.empty() && loadSaveFile(sv) && game_.deserialize(sv)) {
            game_.state = Game::ST_CHOOSE; // 有进度 → 选择页
        } else {
            game_.startPlay();
        }
    };
    hud_.onWinTapped = [this]() {
        clearSaveFile(); // 逃脱成功，清档
        game_.reset();
    };
    hud_.onChoose = [this](bool resume) {
        if (resume) resumeFromSave();
        else restartNewGame();
    };
    resize(env_.w, env_.h, env_.density);
}

App::~App() = default;

void App::resize(int w, int h, float density) {
    if (w <= 0 || h <= 0) return;
    env_.w = w; env_.h = h; env_.density = density;
    winFb_.resize(w, h);
    int rh = (int)((float)LO_RES_W * (float)h / (float)w);
    rh = std::max(160, std::min(720, rh));
    loFb_.resize(LO_RES_W, rh);
    hud_.layout(w, h, density);
}

void App::tick(float dt) {
    if (dt <= 0.f || dt > 0.05f) dt = 0.016f; // 与 Java 版一致
    timeSecs_ += dt;
    flicker_ = 0.9f + 0.1f * ((float)(rng_() & 0xFFFF) / 65535.f);
    game_.update(dt);
    if (game_.state == Game::ST_PLAY && hud_.joyActive()) {
        game_.player.tryMove(game_.lm, &game_, -hud_.joyVecY(), hud_.joyVecX(), dt);
    }
    // 周期性自动存档（游戏进行中）
    if (game_.state == Game::ST_PLAY) {
        autosaveTimer_ += dt;
        if (autosaveTimer_ >= 5.f) {
            autosaveTimer_ = 0.f;
            saveNow_();
        }
    }
}

void App::render() {
    if (!winFb_.valid()) return;
    switch (game_.state) {
        case Game::ST_INTRO:
            hud_.drawIntro(winFb_, game_, timeSecs_);
            break;
        case Game::ST_CHOOSE:
            hud_.drawChoose(winFb_, timeSecs_);
            break;
        case Game::ST_WIN:
            hud_.drawWin(winFb_, game_, timeSecs_);
            break;
        default: {
            float bob = std::sin(game_.player.walkPhase) * loFb_.h * 0.012f;
            ray_.render(loFb_, game_, tex_, bob, flicker_);
            winFb_.blitScaled(loFb_, 0, 0, winFb_.w, winFb_.h);
            hud_.drawPlay(winFb_, game_, timeSecs_);
            // 闪电
            if (game_.flashFrames > 0) {
                uint8_t a = game_.flashFrames > 3 ? 0x55 : 0x22;
                winFb_.fill(0, 0, winFb_.w, winFb_.h, Col(255, 255, 255, a));
            }
            // 楼层传送黑场
            if (game_.fadeAlpha > 0.f) {
                int a = (int)(std::min(1.f, game_.fadeAlpha) * 255.f);
                winFb_.fill(0, 0, winFb_.w, winFb_.h, Col(0, 0, 0, (uint8_t)a));
            }
            break;
        }
    }
}

void App::touchDown(int id, float x, float y) { hud_.touchDown(id, x, y, game_); }
void App::touchMove(int id, float x, float y) { hud_.touchMove(id, x, y, game_); }
void App::touchUp(int id, float x, float y) { hud_.touchUp(id, x, y, game_); }
void App::touchCancel() { hud_.touchCancel(); }

bool App::back() {
    if (game_.state == Game::ST_PLAY) return true; // 游戏中吞掉返回键，防误退
    return false; // 其余界面允许系统退出
}

void App::onPause() { saveNow_(); }
void App::onResume() {}

bool App::hasSaveFile() const { return fileExists(env_.savePath); }

bool App::saveToFile() {
    std::vector<uint8_t> out;
    if (!game_.serialize(out)) return false;
    return writeFileAtomic(env_.savePath, out);
}

bool App::loadSaveFile(std::vector<uint8_t>& out) const {
    out.clear();
    if (env_.savePath.empty()) return false;
    FILE* f = std::fopen(env_.savePath.c_str(), "rb");
    if (!f) return false;
    std::fseek(f, 0, SEEK_END);
    long n = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (n <= 0 || n > 4096) { std::fclose(f); return false; }
    out.resize((size_t)n);
    size_t rd = std::fread(out.data(), 1, (size_t)n, f);
    std::fclose(f);
    return rd == (size_t)n;
}

void App::resumeFromSave() {
    std::vector<uint8_t> sv;
    if (loadSaveFile(sv) && game_.deserialize(sv)) {
        game_.startPlay();
        game_.addMsg(str::MSG_RESUMED);
    } else {
        game_.reset();
        game_.startPlay();
    }
}

void App::clearSaveFile() {
    if (!env_.savePath.empty()) std::remove(env_.savePath.c_str());
    autosaveTimer_ = 0.f;
}

void App::restartNewGame() {
    clearSaveFile();
    game_.reset();
    game_.startPlay();
    autosaveTimer_ = 0.f;
}

void App::saveNow_() {
    if (env_.savePath.empty()) return;
    if (game_.state != Game::ST_PLAY) return; // 仅保存进行中的进度
    std::vector<uint8_t> out;
    if (game_.serialize(out)) writeFileAtomic(env_.savePath, out);
}
