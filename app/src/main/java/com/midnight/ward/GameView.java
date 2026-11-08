package com.midnight.ward;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.LinearGradient;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.RadialGradient;
import android.graphics.Shader;
import android.graphics.Typeface;
import android.media.AudioManager;
import android.media.ToneGenerator;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.os.VibrationEffect;
import android.os.Vibrator;
import android.os.VibratorManager;
import android.view.MotionEvent;
import android.view.View;

import java.util.ArrayList;
import java.util.List;
import java.util.Random;

/**
 * 《午夜病栋》游戏主视图。
 *
 * 玩法：整栋病栋陷入黑暗，手指按住拖动即手电筒光圈。
 * 用光照到符纸与电池即可拾取；「她」会循着光而来——
 * 被她碰到光心即被抓住。收集齐当夜全部符纸即可撑过一夜，
 * 连撑三夜迎来真结局。电池会持续消耗，电量越低光圈越小、她越快。
 */
public class GameView extends View {

    // ------------------------------------------------------------------
    // 状态机
    // ------------------------------------------------------------------
    private static final int S_TITLE = 0;   // 标题页
    private static final int S_PLAY = 1;    // 游戏中
    private static final int S_SCARE = 2;   // 惊吓瞬间
    private static final int S_OVER = 3;    // 失败
    private static final int S_WIN = 4;     // 真结局
    private static final int S_CLEAR = 5;   // 过夜结算

    private static final int TOTAL_NIGHTS = 3;
    private static final long FRAME_MS = 16;

    // ------------------------------------------------------------------
    // 基础设施
    // ------------------------------------------------------------------
    private final float density;
    private final Random rng = new Random();
    private final Paint paint = new Paint();
    private final Paint textPaint = new Paint();
    private final Path lightPath = new Path();
    private final Typeface titleType;
    private final Typeface bodyType;
    private final Handler handler = new Handler(Looper.getMainLooper());
    private ToneGenerator tone;
    private boolean ticking = false;

    private final Runnable ticker = new Runnable() {
        @Override
        public void run() {
            if (!ticking) {
                return;
            }
            update();
            invalidate();
            handler.postDelayed(this, FRAME_MS);
        }
    };

    // ------------------------------------------------------------------
    // 游戏状态
    // ------------------------------------------------------------------
    private int state = S_TITLE;
    private int night = 1;
    private int nightFrames;
    private int frame;

    private int talismansFound;
    private int talismansTotal;
    private float battery = 100f;

    private float lightX, lightY;         // 光圈当前位置
    private float targetX, targetY;       // 手指位置
    private float lightRadius;            // 当前光圈半径
    private float baseLightRadius;        // 满电光圈半径

    private float ghostX, ghostY, ghostPhase;
    private boolean ghostActive;

    private final List<float[]> talismans = new ArrayList<>(); // {x, y, 角度}
    private final List<float[]> batteries = new ArrayList<>(); // {x, y, 角度}

    private int scareTimer;
    private int clearTimer;
    private int lightningTimer;
    private int lightningFlash;
    private int glimpseTimer;

    private int w, h;
    private Shader darkShader;
    private float darkX, darkY, darkR;

    public GameView(Context context) {
        super(context);
        density = getResources().getDisplayMetrics().density;
        titleType = Typeface.create(Typeface.SERIF, Typeface.BOLD);
        bodyType = Typeface.SANS_SERIF;
        try {
            tone = new ToneGenerator(AudioManager.STREAM_MUSIC, 60);
        } catch (RuntimeException e) {
            tone = null;
        }
        setKeepScreenOn(true);
    }

    // ------------------------------------------------------------------
    // 生命周期
    // ------------------------------------------------------------------

    public void resume() {
        if (!ticking) {
            ticking = true;
            handler.post(ticker);
        }
    }

    public void pause() {
        ticking = false;
        handler.removeCallbacks(ticker);
    }

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();
        resume();
    }

    @Override
    protected void onDetachedFromWindow() {
        pause();
        if (tone != null) {
            tone.release();
            tone = null;
        }
        super.onDetachedFromWindow();
    }

    @Override
    protected void onSizeChanged(int newW, int newH, int oldW, int oldH) {
        super.onSizeChanged(newW, newH, oldW, oldH);
        w = newW;
        h = newH;
        targetX = w * 0.5f;
        targetY = h * 0.55f;
        if (lightRadius <= 0f) {
            lightX = targetX;
            lightY = targetY;
            lightRadius = 150f * density;
        }
    }

    // ------------------------------------------------------------------
    // 主循环
    // ------------------------------------------------------------------

    private void update() {
        frame++;
        switch (state) {
            case S_PLAY:
                updatePlay();
                break;
            case S_SCARE:
                if (scareTimer > 0) {
                    scareTimer--;
                    if (scareTimer == 0) {
                        state = S_OVER;
                    }
                }
                break;
            case S_CLEAR:
                if (clearTimer > 0) {
                    clearTimer--;
                    if (clearTimer == 0) {
                        if (night >= TOTAL_NIGHTS) {
                            state = S_WIN;
                        } else {
                            startNight(night + 1);
                        }
                    }
                }
                break;
            default:
                break;
        }
    }

    private void updatePlay() {
        nightFrames++;
        // 手电筒光圈缓动跟随手指
        lightX += (targetX - lightX) * 0.22f;
        lightY += (targetY - lightY) * 0.22f;

        // 电量消耗：约 80 秒耗尽
        battery = Math.max(0f, battery - 0.021f);

        // 光圈半径：随电量缩小 + 火苗抖动
        float targetR = baseLightRadius * (0.45f + 0.55f * (battery / 100f));
        lightRadius += (targetR - lightRadius) * 0.1f;
        lightRadius *= (0.97f + 0.05f * rng.nextFloat());

        // 雷电
        if (lightningTimer > 0) {
            lightningTimer--;
        }
        if (lightningTimer <= 0) {
            lightningFlash = 8;
            lightningTimer = (8 + rng.nextInt(10)) * 60;
            playTone(ToneGenerator.TONE_PROP_NACK, 350);
        }
        if (lightningFlash > 0) {
            lightningFlash--;
        }

        // 余光瞥见（她偶尔在光圈外显形一瞬）
        if (glimpseTimer > 0) {
            glimpseTimer--;
        }
        if (glimpseTimer <= 0) {
            glimpseTimer = (4 + rng.nextInt(7)) * 60;
        }

        // 拾取：符纸
        for (int i = talismans.size() - 1; i >= 0; i--) {
            float[] it = talismans.get(i);
            if (dist(it[0], it[1], lightX, lightY) < lightRadius * 0.9f) {
                talismans.remove(i);
                talismansFound++;
                playTone(ToneGenerator.TONE_PROP_BEEP, 120);
                vibrate(20);
            }
        }
        // 拾取：电池
        for (int i = batteries.size() - 1; i >= 0; i--) {
            float[] bt = batteries.get(i);
            if (dist(bt[0], bt[1], lightX, lightY) < lightRadius * 0.9f) {
                batteries.remove(i);
                battery = Math.min(100f, battery + 38f);
                playTone(ToneGenerator.TONE_PROP_ACK, 120);
                vibrate(20);
            }
        }

        // 过夜判定
        if (talismansFound >= talismansTotal) {
            state = S_CLEAR;
            clearTimer = 150;
            playTone(ToneGenerator.TONE_PROP_BEEP2, 300);
            return;
        }

        // 怨灵：循光而来
        if (ghostActive) {
            ghostPhase += 0.05f;
            float speed = (0.55f + 0.4f * (night - 1)) * density;
            if (battery < 25f) {
                speed *= 1.6f; // 黑暗里她更快
            }
            float dx = lightX - ghostX;
            float dy = lightY - ghostY;
            float d = (float) Math.sqrt(dx * dx + dy * dy);
            if (d > 1f) {
                ghostX += dx / d * speed;
                ghostY += dy / d * speed + ((float) Math.sin(ghostPhase)) * 0.5f;
            }
            if (d < 52f * density) {
                // 被抓住
                state = S_SCARE;
                scareTimer = 80;
                playTone(ToneGenerator.TONE_SUP_ERROR, 900);
                vibrate(600);
            }
        }
    }

    private void startNight(int n) {
        night = n;
        nightFrames = 0;
        talismansTotal = 2 + n;             // 3 / 4 / 5 张符纸
        talismansFound = 0;
        battery = 100f;
        baseLightRadius = (150f - (n - 1) * 16f) * density;
        talismans.clear();
        batteries.clear();
        for (int i = 0; i < talismansTotal; i++) {
            talismans.add(randomSpot());
        }
        for (int i = 0; i < n + 1; i++) {
            batteries.add(randomSpot());
        }
        ghostActive = true;
        ghostX = (rng.nextBoolean() ? 0.1f : 0.9f) * w;
        ghostY = (rng.nextBoolean() ? 0.15f : 0.85f) * h;
        ghostPhase = 0f;
        lightningTimer = (6 + rng.nextInt(8)) * 60;
        lightningFlash = 0;
        glimpseTimer = (4 + rng.nextInt(6)) * 60;
        targetX = w * 0.5f;
        targetY = h * 0.55f;
        lightX = targetX;
        lightY = targetY;
        lightRadius = baseLightRadius;
        state = S_PLAY;
    }

    private float[] randomSpot() {
        for (int attempt = 0; attempt < 60; attempt++) {
            float x = (0.08f + 0.84f * rng.nextFloat()) * w;
            float y = (0.12f + 0.78f * rng.nextFloat()) * h;
            boolean ok = true;
            for (float[] t : talismans) {
                if (dist(t[0], t[1], x, y) < 0.14f * w) {
                    ok = false;
                }
            }
            for (float[] t : batteries) {
                if (dist(t[0], t[1], x, y) < 0.14f * w) {
                    ok = false;
                }
            }
            if (ok) {
                return new float[]{x, y, rng.nextFloat() * 40f - 20f};
            }
        }
        return new float[]{0.5f * w, 0.5f * h, 0f};
    }

    // ------------------------------------------------------------------
    // 触摸
    // ------------------------------------------------------------------

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        switch (event.getActionMasked()) {
            case MotionEvent.ACTION_DOWN:
                if (state == S_TITLE) {
                    startNight(1);
                } else if (state == S_OVER) {
                    startNight(night); // 重试当前夜
                } else if (state == S_WIN) {
                    state = S_TITLE;
                } else if (state == S_PLAY) {
                    targetX = event.getX();
                    targetY = event.getY();
                }
                return true;
            case MotionEvent.ACTION_MOVE:
            case MotionEvent.ACTION_UP:
                if (state == S_PLAY) {
                    targetX = event.getX();
                    targetY = event.getY();
                }
                return true;
            default:
                return true;
        }
    }

    // ------------------------------------------------------------------
    // 绘制
    // ------------------------------------------------------------------

    @Override
    protected void onDraw(Canvas canvas) {
        if (w == 0 || h == 0) {
            canvas.drawColor(Color.BLACK);
            return;
        }
        switch (state) {
            case S_TITLE:
                renderTitle(canvas);
                break;
            case S_PLAY:
                renderPlay(canvas);
                break;
            case S_SCARE:
                renderScare(canvas);
                break;
            case S_OVER:
                renderOver(canvas);
                break;
            case S_CLEAR:
                renderClear(canvas);
                break;
            case S_WIN:
                renderWin(canvas);
                break;
            default:
                canvas.drawColor(Color.BLACK);
                break;
        }
    }

    private void renderTitle(Canvas c) {
        c.drawColor(0xFF050505);
        // 背后极暗的病房剪影
        c.save();
        c.translate(0f, ((float) Math.sin(frame * 0.008f)) * 6f * density);
        drawScene(c);
        c.restore();
        paint.reset();
        paint.setAntiAlias(true);
        paint.setColor(0xC4000000);
        c.drawRect(0, 0, w, h, paint);

        drawTextCenter(c, "午 夜 病 栋", h * 0.28f, 46f * density, 0xFFE8E6D9, 255);
        drawTextCenter(c, "MIDNIGHT WARD", h * 0.28f + 30f * density, 13f * density, 0xFF8A8F8C, 255);
        drawTextCenter(c, "—— 手电筒之夜 ——", h * 0.365f, 16f * density, 0xFFB03A3A, 255);

        float lineY = h * 0.50f;
        drawTextCenter(c, "▸ 按住并拖动屏幕，移动手电筒", lineY, 14f * density, 0xFFCFCFC6, 220);
        drawTextCenter(c, "▸ 用光照到符纸与电池即可拾取", lineY + 24f * density, 14f * density, 0xFFCFCFC6, 220);
        drawTextCenter(c, "▸ 「她」会循着光而来，别被碰到", lineY + 48f * density, 14f * density, 0xFFCFCFC6, 220);
        drawTextCenter(c, "▸ 电量越低光圈越小，撑过三夜等到黎明", lineY + 72f * density, 14f * density, 0xFFCFCFC6, 220);

        int breath = 120 + (int) (100 * (0.5 + 0.5 * Math.sin(frame * 0.05)));
        drawTextCenter(c, "—— 触摸屏幕 开始第一夜 ——", h * 0.80f, 15f * density, 0xFFE8E6D9, breath);
    }

    private void renderPlay(Canvas c) {
        c.drawColor(0xFF020202);

        // 1) 只有光圈内能看到场景
        lightPath.reset();
        lightPath.addCircle(lightX, lightY, Math.max(4f, lightRadius), Path.Direction.CW);
        int sc = c.save();
        c.clipPath(lightPath);
        drawScene(c);
        c.restoreToCount(sc);

        // 2) 光圈外的黑暗（径向渐变压暗全屏）
        ensureDarkShader();
        paint.reset();
        paint.setAntiAlias(true);
        paint.setShader(darkShader);
        c.drawRect(0, 0, w, h, paint);
        paint.setShader(null);

        // 3) 闪电：整屋亮一瞬（她也会现形）
        if (lightningFlash > 0) {
            drawScene(c);
            paint.reset();
            paint.setAntiAlias(true);
            paint.setColor(lightningFlash > 4 ? 0x66FFFFFF : 0x26FFFFFF);
            c.drawRect(0, 0, w, h, paint);
        }

        // 4) 余光瞥见：光圈外的她偶尔一闪
        boolean ghostInLight = ghostActive
                && dist(ghostX, ghostY, lightX, lightY) < lightRadius * 1.05f;
        if (ghostActive && !ghostInLight && glimpseTimer < 14) {
            drawGhost(c, ghostX, ghostY, 46);
        }

        // 5) HUD
        drawHud(c);

        // 6) 教学与低电提示
        if (night == 1 && nightFrames < 360) {
            int a = nightFrames < 300 ? 220 : (360 - nightFrames) * 4;
            drawTextCenter(c, "在黑暗中拖动，寻找符纸", h * 0.70f, 15f * density, 0xFFD9D7C7,
                    Math.max(0, Math.min(255, a)));
        }
        if (battery < 20f && (frame / 16) % 2 == 0) {
            drawTextCenter(c, "手电筒在变暗……", h * 0.115f, 13f * density, 0xFFCC4444, 230);
        }
    }

    private void renderScare(Canvas c) {
        c.drawColor(0xFF040404);
        int sc = c.save();
        // 画面抖动
        c.translate((rng.nextFloat() - 0.5f) * 14f * density,
                (rng.nextFloat() - 0.5f) * 14f * density);
        float cx = w / 2f;
        float cy = h * 0.40f;

        paint.reset();
        paint.setAntiAlias(true);
        paint.setColor(0xFFEDEDE6);                       // 脸
        c.drawCircle(cx, cy, 118f * density, paint);
        paint.setColor(0xFF0A0A0C);                       // 发际（盖住上半张脸）
        c.drawCircle(cx, cy - 36f * density, 120f * density, paint);
        paint.setColor(0xFF000000);                       // 双眼
        c.drawCircle(cx - 44f * density, cy + 6f * density, 17f * density, paint);
        c.drawCircle(cx + 44f * density, cy + 10f * density, 21f * density, paint);
        c.drawOval(cx - 13f * density, cy + 48f * density,  // 拉长的嘴
                cx + 13f * density, cy + 92f * density, paint);
        // 雪花噪点
        for (int i = 0; i < 70; i++) {
            paint.setColor(0xFFFFFFFF);
            paint.setAlpha(20 + rng.nextInt(110));
            float y = rng.nextFloat() * h;
            c.drawLine(0, y, w, y + (rng.nextFloat() * 8f - 4f) * density, paint);
        }
        paint.setAlpha(255);
        c.restoreToCount(sc);

        // 红色暗角
        paint.reset();
        paint.setAntiAlias(true);
        paint.setShader(new RadialGradient(w / 2f, h / 2f, Math.max(w, h) * 0.7f,
                new int[]{0x00000000, 0x66330000, 0xB0000000},
                new float[]{0.35f, 0.75f, 1f}, Shader.TileMode.CLAMP));
        c.drawRect(0, 0, w, h, paint);
        paint.setShader(null);

        drawTextCenter(c, "她 找 到 了 你", h * 0.78f, 30f * density, 0xFFFF5555, 255);
    }

    private void renderOver(Canvas c) {
        c.drawColor(0xFF070707);
        drawTextCenter(c, "你没能等到黎明", h * 0.34f, 30f * density, 0xFFCC4444, 255);
        drawTextCenter(c, "倒在了 第 " + night + " 夜 · 符纸 " + talismansFound + " / " + talismansTotal,
                h * 0.44f, 14f * density, 0xFFB9B7AC, 220);
        int breath = 120 + (int) (100 * (0.5 + 0.5 * Math.sin(frame * 0.05)));
        drawTextCenter(c, "—— 触摸屏幕 重试这一夜 ——", h * 0.62f, 15f * density, 0xFFE8E6D9, breath);
    }

    private void renderClear(Canvas c) {
        c.drawColor(0xFF050807);
        int breath = 170 + (int) (70 * (0.5 + 0.5 * Math.sin(frame * 0.06)));
        drawTextCenter(c, "第 " + night + " 夜 · 你活了下来", h * 0.40f, 26f * density, 0xFFE8E6D9, breath);
        if (night < TOTAL_NIGHTS) {
            drawTextCenter(c, "天快亮了……但她还在附近的阴影里",
                    h * 0.50f, 14f * density, 0xFF9AA09B, 220);
        }
    }

    private void renderWin(Canvas c) {
        // 黎明渐变天空
        paint.reset();
        paint.setAntiAlias(true);
        paint.setShader(new LinearGradient(0, 0, 0, h,
                new int[]{0xFF101A3A, 0xFF4A3F6B, 0xFFD97844},
                new float[]{0f, 0.55f, 1f}, Shader.TileMode.CLAMP));
        c.drawRect(0, 0, w, h, paint);
        paint.setShader(null);

        // 病栋剪影
        paint.setColor(0xFF0B0B10);
        c.drawRect(0, h * 0.68f, w * 0.22f, h, paint);
        c.drawRect(w * 0.30f, h * 0.62f, w * 0.62f, h, paint);
        c.drawRect(w * 0.72f, h * 0.71f, w, h, paint);
        paint.setColor(0x33E8C170);
        c.drawRect(w * 0.40f, h * 0.70f, w * 0.45f, h * 0.74f, paint);
        c.drawRect(w * 0.55f, h * 0.70f, w * 0.60f, h * 0.74f, paint);

        drawTextCenter(c, "黎 明 来 临", h * 0.28f, 36f * density, 0xFFF5EFE0, 255);
        drawTextCenter(c, "三夜已过。晨光爬上病栋的窗，", h * 0.40f, 14f * density, 0xFFEFE7D6, 230);
        drawTextCenter(c, "她退回阴影深处——这一次，没有人被留下。",
                h * 0.445f, 14f * density, 0xFFEFE7D6, 230);
        drawTextCenter(c, "★ 真结局 · 超度 ★", h * 0.54f, 16f * density, 0xFFE8B15C, 255);
        int breath = 120 + (int) (100 * (0.5 + 0.5 * Math.sin(frame * 0.05)));
        drawTextCenter(c, "—— 触摸屏幕 再来一夜 ——", h * 0.82f, 15f * density, 0xFFF5EFE0, breath);
    }

    // ------------------------------------------------------------------
    // 场景与元素
    // ------------------------------------------------------------------

    private void drawScene(Canvas c) {
        paint.reset();
        paint.setAntiAlias(true);
        // 墙 / 地板
        paint.setColor(0xFF262B29);
        c.drawRect(0, 0, w, h * 0.78f, paint);
        paint.setColor(0xFF171412);
        c.drawRect(0, h * 0.78f, w, h, paint);
        // 地板缝
        paint.setColor(0xFF241F1B);
        paint.setStrokeWidth(1.5f * density);
        for (int i = 1; i < 7; i++) {
            float y = h * 0.78f + i * (h * 0.22f / 7f);
            c.drawLine(0, y, w, y, paint);
        }
        // 墙裙
        paint.setColor(0xFF1C2220);
        c.drawRect(0, h * 0.70f, w, h * 0.78f, paint);

        drawDoor(c, w * 0.050f, h * 0.30f, w * 0.260f, h * 0.92f, "301");
        drawDoor(c, w * 0.395f, h * 0.32f, w * 0.605f, h * 0.92f, "302");
        drawDoor(c, w * 0.740f, h * 0.28f, w * 0.950f, h * 0.92f, "?");
        drawWindow(c, w * 0.290f, h * 0.10f, w * 0.440f, h * 0.27f);
        drawWindow(c, w * 0.605f, h * 0.095f, w * 0.755f, h * 0.265f);
        drawBed(c, w * 0.06f, h * 0.86f);
        drawBed(c, w * 0.55f, h * 0.88f);
        drawIV(c, w * 0.915f, h * 0.30f);

        // 血迹
        paint.reset();
        paint.setAntiAlias(true);
        paint.setColor(0xFF43100E);
        c.drawCircle(w * 0.345f, h * 0.665f, 15f * density, paint);
        c.drawCircle(w * 0.375f, h * 0.685f, 9f * density, paint);
        c.drawCircle(w * 0.330f, h * 0.680f, 6f * density, paint);

        // 物品
        for (float[] t : talismans) {
            drawTalisman(c, t[0], t[1], t[2]);
        }
        for (float[] b : batteries) {
            drawBattery(c, b[0], b[1]);
        }

        // 她（画在场景层里，只在光圈内可见）
        if (ghostActive) {
            drawGhost(c, ghostX, ghostY, 235);
        }
    }

    private void drawDoor(Canvas c, float l, float t, float r, float b, String no) {
        paint.reset();
        paint.setAntiAlias(true);
        paint.setColor(0xFF2E241C);
        c.drawRect(l, t, r, b, paint);
        paint.setColor(0xFF3A2D22);
        c.drawRect(l + 3f * density, t + 3f * density, r - 3f * density, b - 3f * density, paint);
        // 门牌
        paint.setColor(0xFFC9C6B8);
        c.drawRect(l + (r - l) * 0.30f, t - 12f * density, l + (r - l) * 0.70f, t - 2f * density, paint);
        textPaint.reset();
        textPaint.setAntiAlias(true);
        textPaint.setColor(0xFF111111);
        textPaint.setTextAlign(Paint.Align.CENTER);
        textPaint.setTextSize(8f * density);
        c.drawText(no, (l + r) / 2f, t - 4.5f * density, textPaint);
        // 把手
        paint.setColor(0xFFB8A968);
        c.drawCircle(r - 8f * density, (t + b) / 2f, 3f * density, paint);
    }

    private void drawWindow(Canvas c, float l, float t, float r, float b) {
        paint.reset();
        paint.setAntiAlias(true);
        paint.setColor(0xFF11161C);
        c.drawRect(l, t, r, b, paint);
        paint.setColor(0x264C5E75); // 夜色微光
        c.drawRect(l, t, r, b, paint);
        paint.setColor(0xFF39424A);
        paint.setStrokeWidth(2f * density);
        float cx = (l + r) / 2f;
        float cy = (t + b) / 2f;
        c.drawLine(cx, t, cx, b, paint);
        c.drawLine(l, cy, r, cy, paint);
        paint.setStrokeWidth(3f * density);
        c.drawRect(l, t, r, b, paint);
        // 月光斜射到地面的光带
        paint.reset();
        paint.setAntiAlias(true);
        paint.setColor(0x143E5C78);
        c.drawRect(l - 10f * density, b, r + 26f * density, h * 0.78f, paint);
    }

    private void drawBed(Canvas c, float l, float t) {
        float bw = w * 0.30f;
        float bh = 14f * density;
        paint.reset();
        paint.setAntiAlias(true);
        paint.setColor(0xFF3A3F3D);
        c.drawRect(l, t, l + bw, t + bh, paint);
        paint.setColor(0xFFD8D4C4); // 枕头
        c.drawRect(l + 3f * density, t - 5f * density, l + bw * 0.22f, t + bh - 2f * density, paint);
        paint.setColor(0xFF23272B);
        c.drawRect(l, t + bh, l + bw, t + bh + 3f * density, paint);
        paint.setStrokeWidth(3f * density);
        c.drawLine(l + 4f * density, t + bh + 3f * density, l + 4f * density, t + bh + 12f * density, paint);
        c.drawLine(l + bw - 4f * density, t + bh + 3f * density, l + bw - 4f * density, t + bh + 12f * density, paint);
    }

    private void drawIV(Canvas c, float x, float top) {
        paint.reset();
        paint.setAntiAlias(true);
        paint.setColor(0xFF8E9296);
        paint.setStrokeWidth(3f * density);
        c.drawLine(x, top, x, top + 60f * density, paint);
        paint.setColor(0xFF20262B);
        c.drawRect(x - 6f * density, top + 6f * density, x + 6f * density, top + 26f * density, paint);
    }

    private void drawTalisman(Canvas c, float x, float y, float angle) {
        int sc = c.save();
        c.translate(x, y);
        c.rotate(angle);
        paint.reset();
        paint.setAntiAlias(true);
        int glow = 40 + (int) (26 * (0.5 + 0.5 * Math.sin(frame * 0.09f)));
        paint.setColor(Color.argb(glow, 230, 200, 90));
        c.drawCircle(0, 0, 26f * density, paint);
        paint.setColor(0xFFE4D48A); // 纸
        c.drawRect(-9f * density, -13f * density, 9f * density, 13f * density, paint);
        paint.setColor(0xFF8A2B22); // 符纹
        paint.setStrokeWidth(1.6f * density);
        c.drawLine(-4f * density, -8f * density, 4f * density, 8f * density, paint);
        c.drawLine(4f * density, -8f * density, -4f * density, 8f * density, paint);
        c.drawLine(0, -10f * density, 0, 10f * density, paint);
        c.restoreToCount(sc);
    }

    private void drawBattery(Canvas c, float x, float y) {
        paint.reset();
        paint.setAntiAlias(true);
        int glow = 36 + (int) (22 * (0.5 + 0.5 * Math.sin(frame * 0.08f + 2f)));
        paint.setColor(Color.argb(glow, 120, 220, 120));
        c.drawCircle(x, y, 22f * density, paint);
        paint.setColor(0xFF2E7D4F);
        c.drawRect(x - 5f * density, y - 10f * density, x + 5f * density, y + 8f * density, paint);
        paint.setColor(0xFFC9D3C6);
        c.drawRect(x - 2.5f * density, y - 13f * density, x + 2.5f * density, y - 10f * density, paint);
    }

    private void drawGhost(Canvas c, float x, float y, int alpha) {
        int sc = c.save();
        float sway = ((float) Math.sin(ghostPhase)) * 6f * density;
        c.translate(sway, 0f);
        paint.reset();
        paint.setAntiAlias(true);
        // 白裙
        paint.setColor(Color.argb(alpha, 233, 233, 226));
        Path dress = new Path();
        dress.moveTo(x, y - 44f * density);
        dress.lineTo(x - 26f * density, y + 46f * density);
        dress.lineTo(x + 26f * density, y + 46f * density);
        dress.close();
        c.drawPath(dress, paint);
        // 头
        c.drawCircle(x, y - 42f * density, 15f * density, paint);
        // 长发
        paint.setColor(Color.argb((int) (alpha * 0.92f), 12, 12, 16));
        Path hair = new Path();
        hair.moveTo(x - 17f * density, y - 40f * density);
        hair.quadTo(x, y - 66f * density, x + 17f * density, y - 40f * density);
        hair.lineTo(x + 23f * density, y + 6f * density);
        hair.lineTo(x + 12f * density, y - 12f * density);
        hair.lineTo(x + 6f * density, y + 26f * density);
        hair.lineTo(x - 4f * density, y - 8f * density);
        hair.lineTo(x - 12f * density, y + 22f * density);
        hair.lineTo(x - 20f * density, y - 14f * density);
        hair.close();
        c.drawPath(hair, paint);
        // 双眼
        paint.setColor(Color.argb(alpha, 0, 0, 0));
        c.drawCircle(x - 6f * density, y - 42f * density, 3.4f * density, paint);
        c.drawCircle(x + 6f * density, y - 42f * density, 3.4f * density, paint);
        c.restoreToCount(sc);
    }

    private void drawHud(Canvas c) {
        textPaint.reset();
        textPaint.setAntiAlias(true);
        textPaint.setTypeface(bodyType);
        textPaint.setColor(0xFFE8E6D9);
        textPaint.setTextAlign(Paint.Align.LEFT);
        textPaint.setTextSize(17f * density);
        c.drawText("第 " + night + " 夜", 14f * density, 30f * density, textPaint);
        c.drawText("符纸 " + talismansFound + " / " + talismansTotal, 14f * density, 52f * density, textPaint);

        // 电量条
        float bw = 104f * density;
        float bh = 10f * density;
        float bx = w - bw - 14f * density;
        float by = 22f * density;
        paint.reset();
        paint.setAntiAlias(true);
        paint.setStyle(Paint.Style.STROKE);
        paint.setStrokeWidth(2f * density);
        paint.setColor(0x80FFFFFF);
        c.drawRect(bx, by, bx + bw, by + bh, paint);
        paint.setStyle(Paint.Style.FILL);
        paint.setColor(battery < 25f
                ? (((frame / 12) % 2 == 0) ? 0xFFCC3333 : 0x55CC3333)
                : 0xFFCDBB4C);
        float fill = (bw - 4f * density) * (battery / 100f);
        if (fill > 0f) {
            c.drawRect(bx + 2f * density, by + 2f * density,
                    bx + 2f * density + fill, by + bh - 2f * density, paint);
        }
    }

    private void drawTextCenter(Canvas c, String s, float y, float sizePx, int color, int alpha) {
        textPaint.reset();
        textPaint.setAntiAlias(true);
        textPaint.setTypeface(titleType);
        textPaint.setTextAlign(Paint.Align.CENTER);
        textPaint.setTextSize(sizePx);
        textPaint.setColor(color);
        textPaint.setAlpha(Math.max(0, Math.min(255, alpha)));
        c.drawText(s, w / 2f, y, textPaint);
    }

    // ------------------------------------------------------------------
    // 杂项
    // ------------------------------------------------------------------

    private void ensureDarkShader() {
        if (darkShader == null || Math.abs(lightX - darkX) > 2f
                || Math.abs(lightY - darkY) > 2f || Math.abs(lightRadius - darkR) > 4f) {
            float r = Math.max(10f, lightRadius * 1.28f);
            darkShader = new RadialGradient(lightX, lightY, r,
                    new int[]{0x00000000, 0x00000000, 0xE6000000, 0xFF000000},
                    new float[]{0f, 0.72f, 0.92f, 1f}, Shader.TileMode.CLAMP);
            darkX = lightX;
            darkY = lightY;
            darkR = lightRadius;
        }
    }

    private static float dist(float x1, float y1, float x2, float y2) {
        float dx = x1 - x2;
        float dy = y1 - y2;
        return (float) Math.sqrt(dx * dx + dy * dy);
    }

    private void playTone(int type, int ms) {
        if (tone != null) {
            try {
                tone.startTone(type, ms);
            } catch (Exception ignored) {
            }
        }
    }

    private void vibrate(long ms) {
        try {
            if (Build.VERSION.SDK_INT >= 31) {
                Object vm = getContext().getSystemService(Context.VIBRATOR_MANAGER_SERVICE);
                if (vm instanceof VibratorManager) {
                    ((VibratorManager) vm).getDefaultVibrator()
                            .vibrate(VibrationEffect.createOneShot(ms, VibrationEffect.DEFAULT_AMPLITUDE));
                    return;
                }
            }
            Vibrator v = (Vibrator) getContext().getSystemService(Context.VIBRATOR_SERVICE);
            if (v != null) {
                if (Build.VERSION.SDK_INT >= 26) {
                    v.vibrate(VibrationEffect.createOneShot(ms, VibrationEffect.DEFAULT_AMPLITUDE));
                } else {
                    v.vibrate(ms);
                }
            }
        } catch (Exception ignored) {
        }
    }
}
