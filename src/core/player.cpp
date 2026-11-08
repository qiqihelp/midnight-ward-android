// player.cpp — Player.java 的 C++ 移植。
#include "player.h"
#include "game.h"
#include <cmath>

void Player::reset(const LevelMap& lm) {
    x = lm.spawn.x;
    y = lm.spawn.y;
    angle = lm.spawn.angle;
    floor = lm.spawn.floor;
    hasKey = false;
    hasCard = false;
    hasFlashlight = false;
    flashlightOn = false;
    hasBasementKey = false;
    hasCrowbar = false;
    walkPhase = 0.f;
}

void Player::tryMove(const LevelMap& lm, Game* g, float forward, float strafe, float dt) {
    float sp = SPEED * dt;
    float dx = std::cos(angle);
    float dy = std::sin(angle);
    float vx = (dx * forward - dy * strafe) * sp;
    float vy = (dy * forward + dx * strafe) * sp;
    if (vx != 0.f && !blockedAt(lm, g, x + vx, y)) x += vx;
    if (vy != 0.f && !blockedAt(lm, g, x, y + vy)) y += vy;
    float mag = std::abs(forward) + std::abs(strafe);
    if (mag > 1.f) mag = 1.f;
    walkPhase += mag * dt * 7.5f;
}

bool Player::blockedAt(const LevelMap& lm, Game* g, float fx, float fy) const {
    return lm.blocked(g, floor, fx - RADIUS, fy - RADIUS)
        || lm.blocked(g, floor, fx + RADIUS, fy - RADIUS)
        || lm.blocked(g, floor, fx - RADIUS, fy + RADIUS)
        || lm.blocked(g, floor, fx + RADIUS, fy + RADIUS);
}
