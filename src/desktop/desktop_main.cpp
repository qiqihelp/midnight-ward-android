// desktop_main.cpp — 桌面模拟器与测试总成（与 Android 共用同一套核心代码）。
// 模式：
//   --logic              无头通关测试：完整任务链（钥匙→房卡→手电→B1钥匙→撬棍→撬门→刷卡→逃脱）
//                        + 全部 8 张字条沿途收集断言
//   --sim DIR            带渲染通关模拟，输出里程碑 BMP 截图
//   --fuzz MIN SEED      随机触摸 + 随机传送 + 字条/档案叠层模糊测试
//   --saveload           存档往返 / 损坏存档 / v2 旧档兼容 / 继续游戏流程测试
// 选项：--assets DIR（默认 assets）、--save PATH（默认 /tmp/mw_test_save.bin）
#include "app.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <functional>
#include <random>
#include <string>
#include <vector>
#include <sys/stat.h>
#include <unistd.h>

static int g_fail = 0;
#define CHECK(cond, msg)                                                     \
    do {                                                                     \
        if (!(cond)) {                                                       \
            std::fprintf(stderr, "FAIL[%s:%d]: %s\n", __FILE__, __LINE__,    \
                         (msg));                                             \
            g_fail++;                                                        \
        }                                                                    \
    } while (0)

static std::string g_assetsDir = "assets";
static std::string g_savePath = "/tmp/mw_test_save.bin";

static std::vector<uint8_t> loadAssetFile(const char* name) {
    std::string p = g_assetsDir + "/" + name;
    FILE* f = std::fopen(p.c_str(), "rb");
    if (!f) return {};
    std::fseek(f, 0, SEEK_END);
    long n = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    std::vector<uint8_t> out;
    if (n > 0 && n < 64 * 1024 * 1024) {
        out.resize((size_t)n);
        if (std::fread(out.data(), 1, (size_t)n, f) != (size_t)n) out.clear();
    }
    std::fclose(f);
    return out;
}

static App::Env makeEnv(int w, int h, float density) {
    App::Env env;
    env.loadAsset = loadAssetFile;
    env.savePath = g_savePath;
    env.w = w;
    env.h = h;
    env.density = density;
    return env;
}

static void writeBMP(const std::string& path, const uint8_t* rgba, int w, int h) {
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return;
    int rowBytes = w * 3;
    int pad = (4 - (rowBytes % 4)) % 4;
    int dataSize = (rowBytes + pad) * h;
    unsigned char hdr[54] = {0};
    hdr[0] = 'B'; hdr[1] = 'M';
    unsigned total = 54 + (unsigned)dataSize;
    hdr[2] = total & 0xFF; hdr[3] = (total >> 8) & 0xFF;
    hdr[4] = (total >> 16) & 0xFF; hdr[5] = (total >> 24) & 0xFF;
    hdr[10] = 54;
    hdr[14] = 40;
    hdr[18] = w & 0xFF; hdr[19] = (w >> 8) & 0xFF;
    hdr[22] = h & 0xFF; hdr[23] = (h >> 8) & 0xFF;
    hdr[26] = 1; hdr[28] = 24;
    hdr[34] = dataSize & 0xFF; hdr[35] = (dataSize >> 8) & 0xFF;
    hdr[36] = (dataSize >> 16) & 0xFF; hdr[37] = (dataSize >> 24) & 0xFF;
    std::fwrite(hdr, 1, 54, f);
    std::vector<unsigned char> row(rowBytes + pad, 0);
    for (int y = h - 1; y >= 0; y--) {
        for (int x = 0; x < w; x++) {
            const uint8_t* p = rgba + ((size_t)y * w + x) * 4;
            row[x * 3] = p[2];
            row[x * 3 + 1] = p[1];
            row[x * 3 + 2] = p[0];
        }
        std::fwrite(row.data(), 1, row.size(), f);
    }
    std::fclose(f);
}

// ---------------------------------------------------------------------------
// 通关脚本（直接驱动 Game，逻辑层回归测试）
// ---------------------------------------------------------------------------

// 关闭字条 / 档案叠层（世界在叠层展开时暂停，测试需要主动收起）
static void dismissOverlays(App& app) {
    Game& g = app.game();
    if (g.readingNote >= 0 || g.archiveOpen) {
        app.touchDown(9, 100.f, 100.f);
        app.touchUp(9, 100.f, 100.f);
    }
}

static bool waitFade(App& app, const char* tag, float timeout = 10.f) {
    Game& g = app.game();
    float t = 0;
    while (g.fadingOrPending()) {
        dismissOverlays(app);
        g.update(1 / 60.f);
        t += 1 / 60.f;
        if (t > timeout) { std::fprintf(stderr, "TIMEOUT waitFade: %s\n", tag); return false; }
    }
    return true;
}

static bool walkTo(App& app, float tx, float ty, const char* tag,
                   bool allowFloorChange = false) {
    Game& g = app.game();
    Player& p = g.player;
    int startFloor = p.floor;
    float dt = 1 / 60.f;
    float t = 0, lastDist = 1e9, stall = 0;
    while (true) {
        dismissOverlays(app); // 途中拾取字条会暂停世界：先收起再继续
        float dx = tx - p.x, dy = ty - p.y;
        float dist = std::sqrt(dx * dx + dy * dy);
        if (dist < 0.14f) return true;
        if (t > 20.f) { std::fprintf(stderr, "TIMEOUT walkTo: %s\n", tag); return false; }
        if (dist > lastDist - 0.001f) stall += dt; else stall = 0;
        if (stall > 3.f) { std::fprintf(stderr, "STALLED walkTo: %s\n", tag); return false; }
        lastDist = dist;
        p.angle = std::atan2(dy, dx);
        p.tryMove(g.lm, &g, 1.f, 0.f, dt);
        g.update(dt);
        t += dt;
        if (g.fadingOrPending() && !waitFade(app, tag)) return false;
        if (p.floor != startFloor) {
            if (allowFloorChange) return true;
            std::fprintf(stderr, "UNEXPECTED FLOOR CHANGE walkTo: %s\n", tag);
            return false;
        }
    }
}

static bool faceInteract(App& app, float angle, int expectKind, const char* tag) {
    Game& g = app.game();
    dismissOverlays(app);
    Player& p = g.player;
    p.angle = angle;
    g.update(1 / 60.f);
    if (!g.hasTarget || g.interactKind != expectKind) {
        std::fprintf(stderr, "FAIL faceInteract(%s): kind=%d expect=%d label=%s\n",
                     tag, g.interactKind, expectKind, g.interactLabel.c_str());
        return false;
    }
    g.interact();
    return true;
}

static bool waitDoor(App& app, int f, int x, int y, const char* tag) {
    Game& g = app.game();
    auto it = g.lm.doors.find(LevelMap::key(f, x, y));
    if (it == g.lm.doors.end()) { std::fprintf(stderr, "NO DOOR: %s\n", tag); return false; }
    float t = 0;
    while (it->second.openness < 0.75f) {
        dismissOverlays(app);
        g.update(1 / 60.f);
        t += 1 / 60.f;
        if (t > 5.f) { std::fprintf(stderr, "TIMEOUT waitDoor: %s\n", tag); return false; }
    }
    return true;
}

static bool waitGate(App& app, float need, const char* tag) {
    Game& g = app.game();
    float t = 0;
    while (g.gateOpenness < need) {
        dismissOverlays(app);
        g.update(1 / 60.f);
        t += 1 / 60.f;
        if (t > 5.f) { std::fprintf(stderr, "TIMEOUT waitGate: %s\n", tag); return false; }
    }
    return true;
}

// 完整任务链（返回是否全部通过；沿途 CHECK）
// 路线原则：轴对齐分段移动，穿门走门格中心（x+0.5），避免擦墙角。
// 8 张字条全部布置在必经之路上（见 game.cpp NOTE_DEFS），随流程自动收集。
static bool runQuestChain(App& app, const std::function<void(const char*)>& snap) {
    Game& g = app.game();
    const float PI = 3.14159265f;

    // 简介三页 → 开始（走 App 输入路径）
    snap("00_intro");
    for (int i = 0; i < 3; i++) {
        app.touchDown(0, app.width() * 0.5f, app.height() * 0.8f);
        app.touchUp(0, app.width() * 0.5f, app.height() * 0.8f);
        app.tick(1 / 30.f);
    }
    CHECK(g.state == Game::ST_PLAY, "简介翻页后应进入游戏");
    snap("01_start_ward");

    // 1) 病房里找钥匙（途中拾取字条 1）
    CHECK(std::strcmp(g.questText(), str::QUEST_0) == 0, "初始目标：找钥匙");
    if (!walkTo(app, 2.5f, 5.5f, "去拿钥匙")) return false;
    CHECK(g.player.hasKey, "拾取病房钥匙");
    CHECK(g.noteRead[0], "字条1（揉皱的便签）已收集");
    CHECK(std::strcmp(g.questText(), str::QUEST_1) == 0, "目标：开病房门");

    // 2) 钥匙开房门 → 走廊（途中拾取字条 2）
    if (!walkTo(app, 3.5f, 6.4f, "走到房门前")) return false;
    if (!faceInteract(app, PI / 2, 1, "开病房门")) return false;
    if (!waitDoor(app, lm::F2, 3, 7, "病房门打开")) return false;
    if (!walkTo(app, 3.5f, 8.5f, "出门进走廊")) return false;
    if (!walkTo(app, 4.5f, 10.4f, "沿走廊去值班室")) return false;
    CHECK(g.noteRead[1], "字条2（值班交接单）已收集");
    snap("02_corridor");

    // 3) 二楼值班室拿房卡（南房间，从走廊朝南开；途中拾取字条 3）
    if (!faceInteract(app, PI / 2, 1, "开值班室门")) return false;
    if (!waitDoor(app, lm::F2, 4, 11, "值班室门打开")) return false;
    if (!walkTo(app, 4.5f, 12.5f, "进值班室")) return false;
    if (!walkTo(app, 5.5f, 14.3f, "拿房卡")) return false;
    CHECK(g.player.hasCard, "拾取房卡");
    CHECK(g.noteRead[2], "字条3（值班室的留言）已收集");
    snap("03_duty_room");
    if (!walkTo(app, 4.5f, 12.5f, "出值班室①")) return false;
    if (!walkTo(app, 4.5f, 10.2f, "出值班室②")) return false;

    // 4) 楼梯下楼到一楼（楼梯门自动开）
    if (!walkTo(app, 20.5f, 10.2f, "沿走廊去楼梯口")) return false;
    if (!waitDoor(app, lm::F2, 20, 11, "楼梯门自动打开")) return false;
    if (!walkTo(app, 20.5f, 11.5f, "进入楼梯格（触发传送）", true)) return false;
    if (!waitFade(app, "F2→F1 传送")) return false;
    CHECK(g.player.floor == lm::F1, "传送后到达一楼");
    snap("04_lobby");

    // 5) 药房拿手电筒（北房间；大厅途中拾取字条 4）
    if (!walkTo(app, 4.5f, 8.4f, "穿大厅去药房门前")) return false;
    CHECK(g.noteRead[3], "字条4（楼层指引）已收集");
    if (!faceInteract(app, -PI / 2, 1, "开药房门")) return false;
    if (!waitDoor(app, lm::F1, 4, 7, "药房门打开")) return false;
    if (!walkTo(app, 4.5f, 6.0f, "进药房①")) return false;
    if (!walkTo(app, 4.5f, 3.5f, "进药房②拿手电筒")) return false;
    CHECK(g.player.hasFlashlight && g.player.flashlightOn, "拾取手电筒（自动点亮）");
    snap("05_pharmacy");
    if (!walkTo(app, 4.5f, 8.8f, "出药房")) return false;

    // 6) 院长办公室拿地下室钥匙（途中拾取字条 5）
    if (!walkTo(app, 14.5f, 8.4f, "去办公室门前")) return false;
    if (!faceInteract(app, -PI / 2, 1, "开办公室门")) return false;
    if (!waitDoor(app, lm::F1, 14, 7, "办公室门打开")) return false;
    if (!walkTo(app, 14.5f, 6.0f, "进办公室①")) return false;
    if (!walkTo(app, 12.5f, 3.5f, "进办公室②拿B1钥匙")) return false;
    CHECK(g.player.hasBasementKey, "拾取地下室钥匙");
    CHECK(g.noteRead[4], "字条5（院长的日记）已收集");
    if (!walkTo(app, 14.5f, 6.0f, "出办公室①")) return false;
    if (!walkTo(app, 14.5f, 8.8f, "出办公室②")) return false;

    // 7) 铁门下地下室（大厅东侧，朝东开）
    if (!walkTo(app, 22.4f, 12.5f, "去铁门前")) return false;
    if (!faceInteract(app, 0.f, 1, "开地下室铁门")) return false;
    if (!waitDoor(app, lm::F1, 23, 12, "铁门打开")) return false;
    if (!walkTo(app, 23.5f, 12.5f, "进入铁门格（触发传送）", true)) return false;
    if (!waitFade(app, "F1→B1 传送")) return false;
    CHECK(g.player.floor == lm::B1, "传送后到达地下室");
    CHECK(std::strcmp(g.questText(), str::QUEST_5) == 0, "目标：取撬棍");
    snap("06_basement_dark");

    // 8) 地下室深处撬棍间（入口路上拾取字条 6，撬棍前拾取字条 7）
    if (!walkTo(app, 9.5f, 7.5f, "绕过货架①")) return false;
    CHECK(g.noteRead[5], "字条6（管线检修记录）已收集");
    if (!walkTo(app, 9.5f, 11.5f, "绕过货架②")) return false;
    if (!walkTo(app, 12.4f, 11.5f, "撬棍间门前")) return false;
    if (!faceInteract(app, 0.f, 1, "开撬棍间门")) return false;
    if (!waitDoor(app, lm::B1, 13, 11, "撬棍间门打开")) return false;
    if (!walkTo(app, 16.5f, 11.5f, "拿撬棍")) return false;
    CHECK(g.player.hasCrowbar, "拾取撬棍");
    CHECK(g.noteRead[6], "字条7（撕碎的病历）已收集");
    CHECK(g.ghost != nullptr, "撬棍惊吓：怨灵在背后显形");
    snap("07_crowbar_scare");

    // 9) 原路返回一楼
    if (!walkTo(app, 12.4f, 11.5f, "回程①")) return false;
    if (!walkTo(app, 9.5f, 11.5f, "回程②")) return false;
    if (!walkTo(app, 9.5f, 7.5f, "回程③")) return false;
    if (!walkTo(app, 1.5f, 7.5f, "回程铁门前")) return false;
    if (!waitDoor(app, lm::B1, 0, 7, "回程铁门自动打开")) return false;
    if (!walkTo(app, 0.5f, 7.5f, "进入回程铁门（触发传送）", true)) return false;
    if (!waitFade(app, "B1→F1 传送")) return false;
    CHECK(g.player.floor == lm::F1, "回到一楼");

    // 10) 大门：穿大厅（拾取字条 8）→ 撬两根木板 + 刷卡
    if (!walkTo(app, 11.5f, 15.5f, "穿大厅去大门")) return false;
    CHECK(g.noteRead[7], "字条8（贴在大门上的封条）已收集");
    CHECK(g.notesReadCount() == Game::NOTE_COUNT, "8 张字条全部收集");
    if (!walkTo(app, 11.5f, 15.8f, "站定大门前")) return false;
    CHECK(std::strcmp(g.questText(), str::QUEST_6) == 0, "目标：撬木板");
    if (!faceInteract(app, PI / 2, 2, "撬木板#1")) return false;
    CHECK(g.boards == 1, "剩一根木板");
    if (!faceInteract(app, PI / 2, 2, "撬木板#2")) return false;
    CHECK(g.boards == 0, "木板清空");
    snap("08_gate_boards_off");
    CHECK(std::strcmp(g.questText(), str::QUEST_7) == 0, "目标：刷卡");
    if (!faceInteract(app, PI / 2, 3, "刷卡")) return false;
    CHECK(g.gateOpen, "大门已开");
    if (!waitGate(app, 0.75f, "大门滑开")) return false;
    CHECK(std::strcmp(g.questText(), str::QUEST_8) == 0, "目标：逃出去");

    // 11) 档案回看流程（全部字条已收集）
    {
        // 档案按钮：布局 archX=120dp archY=h-250dp r=28dp（dp=2, 1280×720）
        float ax = 240.f, ay = 720.f - 500.f;
        app.touchDown(0, ax, ay);
        app.touchUp(0, ax, ay);
        CHECK(g.archiveOpen, "打开档案");
        app.render(); // 填充档案行命中区
        snap("10_archive");
        // 第一行（noteId 0）：面板 940×688 @ (170,16)，行0 列0 (210,200,422,88)
        app.touchDown(1, 421.f, 244.f);
        app.touchUp(1, 421.f, 244.f);
        CHECK(!g.archiveOpen && g.readingNote == 0, "档案行 → 展开字条1");
        app.render();
        snap("11_note_reading");
        app.touchDown(0, 640.f, 300.f);
        app.touchUp(0, 640.f, 300.f);
        CHECK(g.readingNote == -1, "点击任意处收起字条");
    }

    // 12) 冲出大门 → 胜利
    if (!walkTo(app, 11.5f, 17.5f, "冲进大门格")) return false;
    CHECK(g.state == Game::ST_WIN, "触发胜利结局");
    snap("09_win");
    return true;
}

static int modeLogic() {
    unlink(g_savePath.c_str());
    App app(makeEnv(1280, 720, 2.f));
    CHECK(app.mapValid(), "地图自检");
    CHECK(app.fontOk(), "字体加载");
    auto nop = [](const char*) {};
    bool ok = runQuestChain(app, nop);
    CHECK(ok, "完整任务链通关");
    if (g_fail == 0) std::printf("[logic] PASS 完整任务链通关（含 8 张字条）✓\n");
    return g_fail == 0 ? 0 : 1;
}

static int modeSim(const std::string& outDir) {
    unlink(g_savePath.c_str());
    mkdir(outDir.c_str(), 0755);
    App app(makeEnv(1280, 720, 2.f));
    std::string dir = outDir;
    auto snap = [&](const char* name) {
        app.render();
        std::string p = dir + "/" + name + ".bmp";
        writeBMP(p, app.pixels(), app.width(), app.height());
        std::printf("snap: %s\n", p.c_str());
    };
    bool ok = runQuestChain(app, snap);
    CHECK(ok, "带渲染通关");
    if (g_fail == 0) std::printf("[sim] PASS 渲染通关完成，截图已输出\n");
    return g_fail == 0 ? 0 : 1;
}

// ---------------------------------------------------------------------------
// 模糊测试
// ---------------------------------------------------------------------------

static int modeFuzz(float minutes, unsigned seed) {
    unlink(g_savePath.c_str());
    std::mt19937 rng(seed);
    auto rnd = [&](int lo, int hi) { return lo + (int)(rng() % (unsigned)(hi - lo + 1)); };
    auto rndf = [&]() { return (float)((rng() % 10000) / 10000.0); };

    {
        App app(makeEnv(1280, 720, 2.f));
        CHECK(app.mapValid(), "地图自检");
        // 过简介
        for (int i = 0; i < 3; i++) {
            app.touchDown(0, 640, 500);
            app.touchUp(0, 640, 500);
            app.tick(1 / 30.f);
        }
        long frames = (long)(minutes * 60.f * 30.f);
        int pointerId = -1;
        float px = 0, py = 0;
        long nextTeleport = 1800;
        long nextPause = 3600;
        long nextOverlay = 900;
        for (long f = 0; f < frames; f++) {
            // 合成触摸
            if (pointerId == -1) {
                if (rnd(0, 99) < 4) {
                    pointerId = rnd(0, 9);
                    px = (float)rnd(0, 1279);
                    py = (float)rnd(0, 719);
                    app.touchDown(pointerId, px, py);
                }
            } else {
                px += (float)(rnd(-30, 30));
                py += (float)(rnd(-30, 30));
                px = std::max(0.f, std::min(1279.f, px));
                py = std::max(0.f, std::min(719.f, py));
                app.touchMove(pointerId, px, py);
                if (rnd(0, 99) < 5) {
                    app.touchUp(pointerId, px, py);
                    pointerId = -1;
                }
            }
            app.tick(1 / 30.f);
            if (f % 7 == 0) app.render();
            // 不变量
            Game& gg = app.game();
            Player& p = gg.player;
            CHECK(p.x == p.x && p.y == p.y, "位置非 NaN");
            CHECK(p.floor >= 0 && p.floor < lm::FLOOR_COUNT, "楼层合法");
            CHECK(p.x > -5.f && p.x < 40.f && p.y > -5.f && p.y < 40.f, "位置在界内");
            CHECK(p.pitch >= -0.31f && p.pitch <= 0.31f, "俯仰在界内");
            CHECK(gg.readingNote >= -1 && gg.readingNote < Game::NOTE_COUNT,
                  "字条编号合法");
            CHECK(gg.notesReadCount() >= 0 && gg.notesReadCount() <= Game::NOTE_COUNT,
                  "字条计数合法");
            if (g_fail) break;
            // 周期事件
            if (f >= nextTeleport) {
                nextTeleport = f + 1800;
                int fl = rnd(0, 2);
                int tries = 0;
                while (tries++ < 200) {
                    int cx = rnd(1, 22), cy = rnd(1, 16);
                    if (app.game().lm.cellAt(fl, cx, cy) == lm::CELL_FLOOR) {
                        p.floor = fl; p.x = cx + 0.5f; p.y = cy + 0.5f;
                        p.angle = rndf() * 6.283f;
                        break;
                    }
                }
                if (rnd(0, 1)) p.flashlightOn = !p.flashlightOn;
                if (rnd(0, 1)) app.game().debugGhostSoon(); // 逼怨灵显形
            }
            if (f >= nextOverlay) {
                // 强制叠层状态（字条阅读 / 档案），随机触摸随后将其关闭
                nextOverlay = f + 900;
                if (rnd(0, 2) == 0) gg.readingNote = rnd(0, Game::NOTE_COUNT - 1);
                if (rnd(0, 2) == 0) gg.archiveOpen = true;
            }
            if (f >= nextPause) {
                nextPause = f + 3600;
                app.onPause();
                app.onResume();
            }
            // 重新开始 / 胜利流程也过一遍
            if (f == frames / 2) {
                app.game().state = Game::ST_WIN;
                app.render();
                app.touchDown(0, 640, 400); // 胜利页点击 → reset
                app.touchUp(0, 640, 400);
                for (int i = 0; i < 3; i++) { // 重过简介
                    app.touchDown(0, 640, 500);
                    app.touchUp(0, 640, 500);
                    app.tick(1 / 30.f);
                }
            }
        }
        app.onPause(); // 收尾存档
        if (g_fail == 0)
            std::printf("[fuzz] PASS %.1f 分钟随机输入+叠层无崩溃（seed=%u）\n",
                        minutes, seed);
    }
    // 存档继续 + 损坏存档
    {
        App app(makeEnv(1280, 720, 2.f));
        CHECK(app.hasSaveFile(), "模糊测试产生存档");
        std::vector<uint8_t> sv;
        CHECK(app.loadSaveFile(sv) && app.game().deserialize(sv), "存档可加载");
        app.resumeFromSave();
        for (int i = 0; i < 300; i++) { app.tick(1 / 30.f); app.render(); }
        // 损坏存档
        std::vector<uint8_t> bad = sv;
        for (size_t i = 4; i < bad.size(); i += 3) bad[i] ^= 0xFF;
        CHECK(!app.game().deserialize(bad), "损坏存档被拒绝");
        std::vector<uint8_t> trunc(sv.begin(), sv.begin() + sv.size() / 2);
        CHECK(!app.game().deserialize(trunc), "截断存档被拒绝");
        if (g_fail == 0) std::printf("[fuzz] PASS 存档续玩 / 损坏存档防护\n");
    }
    return g_fail == 0 ? 0 : 1;
}

// ---------------------------------------------------------------------------
// 存档专项
// ---------------------------------------------------------------------------

static int modeSaveload() {
    unlink(g_savePath.c_str());
    float px0, py0;
    int floor0; (void)0;
    {
        App app(makeEnv(1280, 720, 2.f));
        for (int i = 0; i < 3; i++) {
            app.touchDown(0, 640, 500);
            app.touchUp(0, 640, 500);
            app.tick(1 / 30.f);
        }
        Game& g = app.game();
        walkTo(app, 2.5f, 5.5f, "拿钥匙（顺路拾取字条1）");
        CHECK(g.player.hasKey, "拿到钥匙");
        CHECK(g.noteRead[0], "字条1已读");
        CHECK(app.saveToFile(), "存档写入");
        px0 = g.player.x; py0 = g.player.y; floor0 = g.player.floor;

        // v3 存档结构检查 + 构造 v2 旧档（无字条精灵）验证兼容读取
        std::vector<uint8_t> v3;
        CHECK(g.serialize(v3), "v3 序列化");
        Game legacy;
        // v2 布局 = v3 去掉后 8 对精灵（14→6 对，每对 8 字节），版本号改 2
        // 偏移：ver@4，nSpr@44，pairs@48（14 对 112B），doors 数@160
        std::vector<uint8_t> v2(v3.begin(), v3.begin() + 44);
        v2[4] = 2; v2[5] = 0; v2[6] = 0; v2[7] = 0;                          // ver=2
        v2.push_back(6); v2.push_back(0); v2.push_back(0); v2.push_back(0);  // nSpr=6
        v2.insert(v2.end(), v3.begin() + 48, v3.begin() + 48 + 6 * 8);      // 前 6 对
        v2.insert(v2.end(), v3.begin() + 160, v3.end());                    // 门 + 尾部
        CHECK(legacy.deserialize(v2), "v2 旧档可读取");
        CHECK(legacy.player.hasKey, "v2 旧档：钥匙状态恢复");
        CHECK(!legacy.noteRead[0], "v2 旧档：字条视为未读");
        CHECK(legacy.notesReadCount() == 0, "v2 旧档：字条计数为 0");
    }
    {
        App app(makeEnv(1280, 720, 2.f));
        CHECK(app.hasSaveFile(), "存档存在");
        std::vector<uint8_t> sv;
        CHECK(app.loadSaveFile(sv), "读档");
        CHECK(app.game().deserialize(sv), "反序列化");
        Game& g = app.game();
        CHECK(g.player.hasKey && g.player.x == px0 && g.player.y == py0
              && g.player.floor == floor0, "状态一致");
        CHECK(g.noteRead[0], "字条已读状态跨存档保留");
    }
    {
        // 选择页流程：翻完简介 → ST_CHOOSE → 继续游戏
        App app(makeEnv(1280, 720, 2.f));
        for (int i = 0; i < 3; i++) {
            app.touchDown(0, 640, 500);
            app.touchUp(0, 640, 500);
            app.tick(1 / 30.f);
        }
        CHECK(app.game().state == Game::ST_CHOOSE, "有存档 → 选择页");
        app.render();
        writeBMP("/tmp/mw_choose.bmp", app.pixels(), app.width(), app.height());
        // 点「继续游戏」（左按钮）与「重新开始」（右按钮）中心：
        // 布局 bw=220dp gap=36dp d=2 → 总宽 952，x0=164，A 中心 (384,438)，B 中心 (896,438)
        float ax = 384.f, bx = 896.f, by = 720 * 0.52f + 64.f;
        (void)ax;
        app.touchDown(0, 384.f, by);
        app.touchUp(0, 384.f, by);
        app.tick(1 / 30.f);
        CHECK(app.game().state == Game::ST_PLAY, "继续游戏进入 PLAY");
        CHECK(app.game().player.hasKey, "恢复进度（钥匙在手）");
        CHECK(app.game().noteRead[0], "恢复进度（字条1已读）");
        bool resumed = false;
        for (const auto& m : app.game().messages)
            if (m.text == str::MSG_RESUMED) resumed = true;
        CHECK(resumed, "恢复提示消息");
        // 重新开始：清档
        app.game().state = Game::ST_CHOOSE;
        app.touchDown(1, bx, by);
        app.touchUp(1, bx, by);
        CHECK(app.game().state == Game::ST_PLAY, "重新开始进入 PLAY");
        CHECK(!app.game().player.hasKey, "重新开始为全新进度");
        CHECK(app.game().notesReadCount() == 0, "重新开始字条全未读");
        CHECK(!app.hasSaveFile(), "重新开始已清档");
    }
    if (g_fail == 0) std::printf("[saveload] PASS 存档往返 / v2 旧档兼容 / 选择页 / 清档\n");
    return g_fail == 0 ? 0 : 1;
}

int main(int argc, char** argv) {
    std::string mode = "--logic";
    std::string simDir = "/tmp/mw_sim";
    float fuzzMin = 2.f;
    unsigned seed = 42;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--logic" || a == "--sim" || a == "--fuzz" || a == "--saveload")
            mode = a;
        else if (a == "--assets" && i + 1 < argc) g_assetsDir = argv[++i];
        else if (a == "--save" && i + 1 < argc) g_savePath = argv[++i];
        else if (a == "--dir" && i + 1 < argc) simDir = argv[++i];
        else if (a == "--min" && i + 1 < argc) fuzzMin = (float)atof(argv[++i]);
        else if (a == "--seed" && i + 1 < argc) seed = (unsigned)atoi(argv[++i]);
    }
    std::printf("午夜病栋 C++ 桌面测试 | mode=%s assets=%s\n", mode.c_str(),
                g_assetsDir.c_str());
    if (mode == "--logic") return modeLogic();
    if (mode == "--sim") return modeSim(simDir);
    if (mode == "--fuzz") return modeFuzz(fuzzMin, seed);
    if (mode == "--saveload") return modeSaveload();
    return 2;
}
