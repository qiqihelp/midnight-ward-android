// game.h — Game.java 的 C++ 移植：任务链、门互动、楼层传送、氛围事件。
// 渲染完全解耦（无 Android 依赖），可在桌面无头测试。
#pragma once
#include "levelmap.h"
#include "player.h"
#include "strings.h"
#include <cstdint>
#include <memory>
#include <random>
#include <string>
#include <vector>

// 字条内容（文案常量在 strings.h，保证字体子集覆盖）
namespace notes {
struct NoteDef {
    const char* title;
    const char* lines[6]; // nullptr 结尾
};
extern const NoteDef ALL[]; // 共 Game::NOTE_COUNT 条
} // namespace notes

struct Sprite {
    static constexpr int TYPE_KEY = 0;      // 病房钥匙
    static constexpr int TYPE_CARD = 1;     // 房卡
    static constexpr int TYPE_TORCH = 2;    // 手电筒
    static constexpr int TYPE_BKEY = 3;     // 地下室钥匙
    static constexpr int TYPE_CROWBAR = 4;  // 撬棍
    static constexpr int TYPE_BED = 5;      // 病床（陈设）
    static constexpr int TYPE_GHOST = 6;    // 怨灵（氛围）
    static constexpr int TYPE_NOTE = 7;     // 字条

    float x = 0, y = 0;
    int floor = 0;
    int type = 0;
    int noteId = -1;       // TYPE_NOTE 专用：字条编号
    bool alive = true;
    bool pickable = false;
    float alpha = 1.f;
    float height = 0.3f;   // 世界高度（墙高 = 1.0）
    float aspect = 1.0f;   // 宽 / 高

    Sprite() = default;
    Sprite(int floor_, float x_, float y_, int type_);
};

class Game {
public:
    static constexpr int ST_INTRO = 0;
    static constexpr int ST_PLAY = 1;
    static constexpr int ST_WIN = 2;
    static constexpr int ST_CHOOSE = 3; // 检测到存档：继续 / 重新开始

    static constexpr int NOTE_COUNT = 8; // 字条总数

    struct Msg {
        std::string text;
        float life = 0.f;
        Msg(const std::string& t, float l) : text(t), life(l) {}
    };

    LevelMap lm;
    Player player;

    int state = ST_INTRO;
    int introPage = 0;
    float timePlayed = 0.f;

    // 大门进度：boards 2 → 1 → 0 → 刷卡
    int boards = 2;
    bool gateOpen = false;
    float gateOpenness = 0.f;

    std::vector<Sprite> sprites;
    std::unique_ptr<Sprite> ghost; // 动态怨灵（氛围）
    float ghostLife = 0.f;

    // 字条：已读标记 / 当前展开 / 档案列表（展开时游戏暂停）
    std::vector<bool> noteRead;      // reset() 时置为 NOTE_COUNT 个 false
    int readingNote = -1;   // >= 0：正在阅读某张字条
    bool archiveOpen = false; // 档案列表打开

    std::vector<Msg> messages;

    // 互动探测结果（每帧更新，供按钮显示）
    std::string interactLabel;
    int interactKind = 0;            // 0 无目标 1 门 2 撬 3 刷卡
    int interactTX = 0, interactTY = 0;
    bool hasTarget = false;

    float fadeAlpha = 0.f;  // 黑场
    int fadeDir = 0;        // 0 无 1 渐黑 2 渐显

    int flashFrames = 0;

    // 测试 / 诊断接口
    bool fadingOrPending() const { return fadeDir != 0 || hasPendingTransition; }
    void debugGhostSoon() { ghostTimer = 0.01f; }

    Game() { reset(); }

    void reset();
    void startPlay();
    void addMsg(const std::string& s);
    void update(float dt);
    void interact();
    const char* questText() const;
    // 当前目标在世界中的落点（小地图金色标记用）
    void questTarget(int& floor, float& x, float& y) const;
    int notesReadCount() const;

    // 存档序列化（App 层负责读写文件）
    bool serialize(std::vector<uint8_t>& out) const;
    bool deserialize(const std::vector<uint8_t>& in);

private:
    lm::Transition pendingTransition;
    bool hasPendingTransition = false;
    float ghostTimer = 20.f;
    float lightningTimer = 12.f;
    float beatTimer = 0.f;
    std::mt19937 rng{std::random_device{}()};

    void updateFade(float dt);
    void updateDoors(float dt);
    void checkAutoDoors();
    void checkTransition();
    void doTeleport();
    void checkPickups();
    void checkWin();
    void updateEvents(float dt);
    bool findGhostSpot(float& ox, float& oy) const;
    void scare();
    void updateInteract();
    float rand01() { return (float)(rng() - rng.min()) / (float)(rng.max() - rng.min()); }
};
