package com.midnight.ward;

import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.Random;

/**
 * 游戏逻辑状态机：任务链、门与大门互动、楼层传送、氛围事件。
 * 与渲染完全解耦（无 Android 依赖），可本地无头测试。
 *
 * 任务链（用户剧本）：
 *   病床上醒来 → 找钥匙开房门 → 二楼拿房卡 → 下楼 →
 *   一楼拿手电筒+地下室钥匙 → 开铁门下地下室（开手电筒）→ 取撬棍 →
 *   回大门：撬下两根木板 → 刷房卡 → 逃出。
 */
public class Game {

    public static final int ST_INTRO = 0;
    public static final int ST_PLAY = 1;
    public static final int ST_WIN = 2;

    /** 声音/震动回调（由视图层实现）。 */
    public interface Sound {
        void beep();     // 拾取
        void ack();      // 开锁/成功
        void nack();     // 上锁/失败
        void creak();    // 撬木板
        void error();    // 惊吓
        void whisper();  // 低语
        void thunder();  // 雷声
        void beat();     // 心跳
        void vibrate(long ms);
    }

    public static final class Msg {
        public final String text;
        public float life;

        public Msg(String text, float life) {
            this.text = text;
            this.life = life;
        }
    }

    public final LevelMap lm = new LevelMap();
    public final Player player = new Player();
    public Sound sound;

    public int state = ST_INTRO;
    public int introPage = 0;
    public float timePlayed;

    // 大门进度：boards 2 → 1 → 0 → 刷卡
    public int boards = 2;
    public boolean gateOpen = false;
    public float gateOpenness = 0f;

    public final List<Sprite> sprites = new ArrayList<>();
    public Sprite ghost = null;      // 动态怨灵（氛围）
    public float ghostLife;

    public final List<Msg> messages = new ArrayList<>();

    // 互动探测结果（每帧更新，供按钮显示）
    public String interactLabel = "";
    public int interactKind = 0;     // 0 无目标 1 门 2 撬 3 刷卡
    public int[] interactTarget = null;

    public float fadeAlpha = 0f;     // 黑场
    private int fadeDir = 0;         // 0 无 1 渐黑 2 渐显
    private LevelMap.Transition pendingTransition = null;

    private float ghostTimer = 20f;
    private float lightningTimer = 12f;
    private float beatTimer = 0f;
    public int flashFrames = 0;
    private final Random rng = new Random();

    public Game() {
        reset();
    }

    public void reset() {
        player.reset(lm);
        for (LevelMap.DoorState d : lm.doors.values()) {
            d.openness = 0f;
            d.opening = false;
        }
        boards = 2;
        gateOpen = false;
        gateOpenness = 0f;
        sprites.clear();
        sprites.addAll(lm.spawnItems());
        ghost = null;
        ghostLife = 0f;
        messages.clear();
        timePlayed = 0f;
        state = ST_INTRO;
        introPage = 0;
        fadeAlpha = 0f;
        fadeDir = 0;
        pendingTransition = null;
        ghostTimer = 18f + rng.nextFloat() * 15f;
        lightningTimer = 10f + rng.nextFloat() * 14f;
        beatTimer = 0f;
        flashFrames = 0;
        interactLabel = "";
        interactKind = 0;
        interactTarget = null;
    }

    public void startPlay() {
        state = ST_PLAY;
    }

    public void addMsg(String s) {
        messages.add(new Msg(s, 3.8f));
        if (messages.size() > 4) {
            messages.remove(0);
        }
    }

    // ------------------------------------------------------------------
    // 主更新
    // ------------------------------------------------------------------

    public void update(float dt) {
        if (flashFrames > 0) {
            flashFrames--;
        }
        if (state != ST_PLAY) {
            return;
        }
        timePlayed += dt;

        updateFade(dt);
        updateDoors(dt);
        checkAutoDoors();
        checkTransition();
        checkPickups();
        checkWin();
        updateEvents(dt);
        updateInteract();

        Iterator<Msg> it = messages.iterator();
        while (it.hasNext()) {
            Msg m = it.next();
            m.life -= dt;
            if (m.life <= 0f) {
                it.remove();
            }
        }
    }

    private void updateFade(float dt) {
        if (fadeDir == 1) {
            fadeAlpha += dt * 2.8f;
            if (fadeAlpha >= 1f) {
                fadeAlpha = 1f;
                doTeleport();
                fadeDir = 2;
            }
        } else if (fadeDir == 2) {
            fadeAlpha -= dt * 2.4f;
            if (fadeAlpha <= 0f) {
                fadeAlpha = 0f;
                fadeDir = 0;
            }
        }
    }

    private void updateDoors(float dt) {
        for (LevelMap.DoorState d : lm.doors.values()) {
            if (d.opening && d.openness < 1f) {
                d.openness = Math.min(1f, d.openness + dt * 1.7f);
            }
        }
        if (gateOpen && gateOpenness < 1f) {
            gateOpenness = Math.min(1f, gateOpenness + dt * 1.2f);
        }
    }

    /** 楼梯门 / B1 回程铁门：靠近自动打开。 */
    private void checkAutoDoors() {
        for (LevelMap.DoorState d : lm.doors.values()) {
            if (!d.isAutoOpen() || d.floor != player.floor || d.openness >= 1f) {
                continue;
            }
            float dx = d.x + 0.5f - player.x;
            float dy = d.y + 0.5f - player.y;
            if (dx * dx + dy * dy < 1.5f * 1.5f) {
                d.opening = true;
            }
        }
    }

    /** 走进门格 → 触发楼层传送。 */
    private void checkTransition() {
        if (pendingTransition != null || fadeDir != 0) {
            return;
        }
        int cx = (int) Math.floor(player.x);
        int cy = (int) Math.floor(player.y);
        String k = LevelMap.key(player.floor, cx, cy);
        LevelMap.Transition tr = lm.transitions.get(k);
        if (tr != null) {
            LevelMap.DoorState d = lm.doors.get(k);
            if (d != null && d.openness > 0.75f) {
                pendingTransition = tr;
                fadeDir = 1; // 渐黑后传送
            }
        }
    }

    private void doTeleport() {
        if (pendingTransition == null) {
            return;
        }
        player.floor = pendingTransition.floor;
        player.x = pendingTransition.x;
        player.y = pendingTransition.y;
        player.angle = pendingTransition.angle;
        pendingTransition = null;
    }

    private void checkPickups() {
        for (Sprite s : sprites) {
            if (!s.alive || !s.pickable || s.floor != player.floor) {
                continue;
            }
            float dx = s.x - player.x;
            float dy = s.y - player.y;
            if (dx * dx + dy * dy > 0.62f * 0.62f) {
                continue;
            }
            s.alive = false;
            switch (s.type) {
                case Sprite.TYPE_KEY:
                    player.hasKey = true;
                    addMsg("拿到了一把锈迹斑斑的钥匙。");
                    sBeep();
                    break;
                case Sprite.TYPE_CARD:
                    player.hasCard = true;
                    addMsg("一张房卡，卡面印着『青山病院 · 001』。");
                    sBeep();
                    break;
                case Sprite.TYPE_TORCH:
                    player.hasFlashlight = true;
                    player.flashlightOn = true;
                    addMsg("一支军用手电筒——「咔哒」，光柱刺破黑暗。");
                    sBeep();
                    break;
                case Sprite.TYPE_BKEY:
                    player.hasBasementKey = true;
                    addMsg("一把沉重的黄铜钥匙，标签上刻着：B1。");
                    sBeep();
                    break;
                case Sprite.TYPE_CROWBAR:
                    player.hasCrowbar = true;
                    addMsg("撬棍到手——身后忽然传来指甲刮擦玻璃的声音……快走。");
                    scare();
                    break;
                default:
                    break;
            }
        }
    }

    private void checkWin() {
        if (player.floor == LevelMap.F1
                && (int) Math.floor(player.x) == 11
                && (int) Math.floor(player.y) == 17) {
            state = ST_WIN;
            sAck();
        }
    }

    // ------------------------------------------------------------------
    // 氛围事件：闪电 / 心跳 / 怨灵显形
    // ------------------------------------------------------------------

    private void updateEvents(float dt) {
        if (player.floor != LevelMap.B1) {
            lightningTimer -= dt;
            if (lightningTimer <= 0f) {
                flashFrames = 6;
                sThunder();
                lightningTimer = 14f + rng.nextFloat() * 16f;
            }
        } else {
            beatTimer -= dt;
            if (beatTimer <= 0f) {
                sBeat();
                beatTimer = 1.2f;
            }
        }

        if (ghost == null) {
            ghostTimer -= dt;
            if (ghostTimer <= 0f) {
                ghostTimer = 22f + rng.nextFloat() * 22f;
                float[] spot = findGhostSpot();
                if (spot != null) {
                    ghost = new Sprite(player.floor, spot[0], spot[1], Sprite.TYPE_GHOST);
                    ghostLife = 2.4f;
                    sWhisper();
                }
            }
        } else {
            ghostLife -= dt;
            float t = 1f - Math.max(0f, ghostLife) / 2.4f;
            float a = (float) Math.sin(Math.PI * Math.min(1f, Math.max(0f, t)));
            ghost.alpha = a * 0.8f;
            if (ghostLife <= 0f) {
                ghost = null;
            }
        }
    }

    /** 在玩家前方 3~8 格找一个地板格作为怨灵显形点。 */
    private float[] findGhostSpot() {
        float dx = (float) Math.cos(player.angle);
        float dy = (float) Math.sin(player.angle);
        for (int d = 3; d <= 8; d++) {
            float fx = player.x + dx * d;
            float fy = player.y + dy * d;
            int cx = (int) Math.floor(fx);
            int cy = (int) Math.floor(fy);
            if (lm.cellAt(player.floor, cx, cy) == LevelMap.CELL_FLOOR) {
                return new float[]{fx, fy};
            }
        }
        return null;
    }

    /** 撬棍惊吓：背后显形。 */
    private void scare() {
        sError();
        sVibrate(500);
        float bx = player.x - (float) Math.cos(player.angle) * 1.4f;
        float by = player.y - (float) Math.sin(player.angle) * 1.4f;
        if (!lm.blocked(this, player.floor, bx, by)) {
            ghost = new Sprite(player.floor, bx, by, Sprite.TYPE_GHOST);
            ghostLife = 2.6f;
        }
    }

    // ------------------------------------------------------------------
    // 互动（右下按钮）：开门 / 撬木板 / 刷卡
    // ------------------------------------------------------------------

    /** 每帧探测面前的可互动目标。 */
    private void updateInteract() {
        interactLabel = "";
        interactKind = 0;
        interactTarget = null;

        float dx = (float) Math.cos(player.angle);
        float dy = (float) Math.sin(player.angle);
        for (float t = 0.2f; t <= 1.6f; t += 0.12f) {
            int cx = (int) Math.floor(player.x + dx * t);
            int cy = (int) Math.floor(player.y + dy * t);
            int cell = lm.cellAt(player.floor, cx, cy);
            if (cell == LevelMap.CELL_FLOOR) {
                continue;
            }
            if (cell == LevelMap.CELL_GATE) {
                if (!gateOpen) {
                    if (boards > 0) {
                        interactLabel = player.hasCrowbar ? "撬开" : "木板";
                        interactKind = 2;
                    } else {
                        interactLabel = player.hasCard ? "刷卡" : "读卡器";
                        interactKind = 3;
                    }
                    interactTarget = new int[]{cx, cy};
                }
                return;
            }
            if (cell == LevelMap.CELL_DOOR || cell == LevelMap.CELL_METAL
                    || cell == LevelMap.CELL_STAIR) {
                LevelMap.DoorState d = lm.doors.get(LevelMap.key(player.floor, cx, cy));
                if (d != null && d.openness < 1f) {
                    if (d.lock == LevelMap.LOCK_ROOM && !player.hasKey) {
                        interactLabel = "上锁";
                    } else if (d.lock == LevelMap.LOCK_BASEMENT && !player.hasBasementKey) {
                        interactLabel = "上锁";
                    } else {
                        interactLabel = "开门";
                    }
                    interactKind = 1;
                    interactTarget = new int[]{cx, cy};
                }
                return;
            }
            return; // 实心墙：探测终止
        }
    }

    /** 玩家按下互动按钮。 */
    public void interact() {
        if (state != ST_PLAY || interactTarget == null) {
            return;
        }
        int cx = interactTarget[0];
        int cy = interactTarget[1];
        int cell = lm.cellAt(player.floor, cx, cy);

        if (cell == LevelMap.CELL_GATE) {
            if (boards > 0) {
                if (!player.hasCrowbar) {
                    addMsg("大门被两根木板钉死了——需要一根撬棍。");
                    sNack();
                    return;
                }
                boards--;
                sCreak();
                sVibrate(120);
                if (boards == 1) {
                    addMsg("「嘎吱——」撬下一根木板，还剩一根。");
                } else {
                    addMsg("两根木板都撬掉了。门禁读卡器露了出来，红灯闪烁。");
                }
                return;
            }
            if (!gateOpen) {
                if (!player.hasCard) {
                    addMsg("读卡器红灯一闪——你需要一张房卡。");
                    sNack();
                    return;
                }
                gateOpen = true;
                sAck();
                addMsg("「嘀——」绿灯亮起，门锁弹开了！快走！");
            }
            return;
        }

        // 门
        LevelMap.DoorState d = lm.doors.get(LevelMap.key(player.floor, cx, cy));
        if (d == null || d.openness >= 1f) {
            return;
        }
        if (d.lock == LevelMap.LOCK_ROOM && !player.hasKey) {
            addMsg("门锁着。钥匙应该就在这间病房里。");
            sNack();
            return;
        }
        if (d.lock == LevelMap.LOCK_BASEMENT && !player.hasBasementKey) {
            addMsg("沉重的铁门纹丝不动——锁孔是黄铜的，需要对应的钥匙。");
            sNack();
            return;
        }
        d.opening = true;
        sAck();
        if (d.lock == LevelMap.LOCK_ROOM) {
            addMsg("钥匙转动，「咔哒」——病房门开了。");
        } else if (d.lock == LevelMap.LOCK_BASEMENT) {
            addMsg("黄铜钥匙转动，铁门吱呀开启。下面一片漆黑。");
        }
    }

    // ------------------------------------------------------------------
    // 动态目标提示
    // ------------------------------------------------------------------

    public String questText() {
        if (state == ST_WIN) {
            return "你逃出来了。";
        }
        if (!player.hasKey) {
            return "目标：在病房里找到钥匙";
        }
        LevelMap.DoorState roomDoor = lm.doors.get(LevelMap.key(LevelMap.F2, 3, 7));
        if (roomDoor == null || roomDoor.openness < 1f) {
            return "目标：用钥匙打开病房门";
        }
        if (!player.hasCard) {
            return "目标：去二楼值班室找房卡";
        }
        if (!player.hasFlashlight) {
            return "目标：一楼药房找手电筒";
        }
        if (!player.hasBasementKey) {
            return "目标：一楼院长办公室找地下室钥匙";
        }
        if (!player.hasCrowbar) {
            return "目标：下地下室取撬棍（记得开手电筒）";
        }
        if (boards > 0) {
            return "目标：回一楼大厅，撬开大门的木板";
        }
        if (!gateOpen) {
            return "目标：用房卡在读卡器上刷卡";
        }
        return "目标：逃出去！！";
    }

    // 声音回调的小包装（允许 sound 为 null，便于无头测试）
    private void sBeep() {
        if (sound != null) sound.beep();
    }

    private void sAck() {
        if (sound != null) sound.ack();
    }

    private void sNack() {
        if (sound != null) sound.nack();
    }

    private void sCreak() {
        if (sound != null) sound.creak();
    }

    private void sError() {
        if (sound != null) sound.error();
    }

    private void sWhisper() {
        if (sound != null) sound.whisper();
    }

    private void sThunder() {
        if (sound != null) sound.thunder();
    }

    private void sBeat() {
        if (sound != null) sound.beat();
    }

    private void sVibrate(long ms) {
        if (sound != null) sound.vibrate(ms);
    }
}
