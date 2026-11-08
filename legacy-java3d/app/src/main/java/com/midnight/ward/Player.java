package com.midnight.ward;

/**
 * 玩家：位置 / 朝向 / 楼层 / 背包。
 */
public class Player {

    public static final float RADIUS = 0.18f;
    private static final float SPEED = 2.6f; // 格 / 秒

    public float x, y, angle;
    public int floor;
    public boolean hasKey, hasCard, hasFlashlight, flashlightOn, hasBasementKey, hasCrowbar;
    public float walkPhase;

    public void reset(LevelMap lm) {
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
        walkPhase = 0f;
    }

    /**
     * 按摇杆输入移动（forward 前+ / 后-，strafe 右+ / 左-）。
     * 分轴碰撞检测，避免卡角。
     */
    public void tryMove(LevelMap lm, Game g, float forward, float strafe, float dt) {
        float sp = SPEED * dt;
        float dx = (float) Math.cos(angle);
        float dy = (float) Math.sin(angle);
        // 前向 dir + 右向 (-dirY, dirX)
        float vx = (dx * forward - dy * strafe) * sp;
        float vy = (dy * forward + dx * strafe) * sp;
        if (vx != 0f && !blockedAt(lm, g, x + vx, y)) {
            x += vx;
        }
        if (vy != 0f && !blockedAt(lm, g, x, y + vy)) {
            y += vy;
        }
        float mag = Math.abs(forward) + Math.abs(strafe);
        if (mag > 1f) {
            mag = 1f;
        }
        walkPhase += mag * dt * 7.5f;
    }

    private boolean blockedAt(LevelMap lm, Game g, float fx, float fy) {
        return lm.blocked(g, floor, fx - RADIUS, fy - RADIUS)
                || lm.blocked(g, floor, fx + RADIUS, fy - RADIUS)
                || lm.blocked(g, floor, fx - RADIUS, fy + RADIUS)
                || lm.blocked(g, floor, fx + RADIUS, fy + RADIUS);
    }
}
