// player.h — Player.java 的 C++ 移植。
#pragma once
#include "levelmap.h"

class Game;

class Player {
public:
    static constexpr float RADIUS = 0.18f;
    static constexpr float SPEED = 2.6f; // 格 / 秒

    float x = 0, y = 0, angle = 0;
    int floor = 0;
    bool hasKey = false, hasCard = false, hasFlashlight = false;
    bool flashlightOn = false, hasBasementKey = false, hasCrowbar = false;
    float walkPhase = 0.f;
    float pitch = 0.f; // 俯仰视角（-0.30..0.30，屏高比例；不存档）

    void reset(const LevelMap& lm);
    // forward 前+ / 后-，strafe 右+ / 左-；分轴碰撞，避免卡角
    void tryMove(const LevelMap& lm, Game* g, float forward, float strafe, float dt);

private:
    bool blockedAt(const LevelMap& lm, Game* g, float fx, float fy) const;
};
