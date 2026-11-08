package com.midnight.ward;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.LinearGradient;
import android.graphics.Paint;
import android.graphics.RectF;
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

import java.util.Random;

/**
 * 游戏主视图：帧循环（低分辨率离屏渲染 + 放大）、触摸操控
 * （左半屏虚拟摇杆移动 / 右半屏滑动转身 / 互动与手电筒按钮）、
 * HUD（目标 / 物品 / 消息）与开场、胜利过场。
 */
public class GameView extends View implements Game.Sound {

    private static final long FRAME_MS = 16;
    private static final int RW = 300; // 离屏渲染宽度（像素风）

    private final Game game = new Game();
    private final Textures tex = new Textures();
    private final Raycaster ray = new Raycaster();

    private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint textPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Typeface titleType;
    private final Typeface bodyType;

    private Bitmap frame;
    private Canvas frameCanvas;
    private int rw, rh;
    private final RectF dstRect = new RectF();

    private final Handler handler = new Handler(Looper.getMainLooper());
    private boolean ticking = false;
    private ToneGenerator tone;
    private long lastT = 0L;
    private float flicker = 1f;
    private final Random rng = new Random();

    // 触摸
    private int joyId = -1;
    private int lookId = -1;
    private float joyBaseX, joyBaseY;
    private float joyVecX, joyVecY;   // -1..1
    private float lastLookX;

    // 按钮位置
    private float btnX, btnY, btnR;        // 互动
    private float torchX, torchY, torchR;  // 手电筒
    private float defJoyX, defJoyY, joyR;  // 摇杆

    private int w, h;
    private float density;

    private final Runnable ticker = new Runnable() {
        @Override
        public void run() {
            if (!ticking) {
                return;
            }
            long now = System.currentTimeMillis();
            float dt = lastT == 0L ? 0.016f : (now - lastT) / 1000f;
            lastT = now;
            if (dt <= 0f || dt > 0.05f) {
                dt = 0.016f;
            }
            flicker = 0.9f + 0.1f * rng.nextFloat();
            game.update(dt);
            if (game.state == Game.ST_PLAY) {
                if (joyId != -1 && (joyVecX != 0f || joyVecY != 0f)) {
                    game.player.tryMove(game.lm, game, -joyVecY, joyVecX, dt);
                }
                float bob = (float) Math.sin(game.player.walkPhase) * rh * 0.012f;
                ray.render(frameCanvas, rw, rh, game.lm, game, game.player, tex, bob, flicker);
            }
            invalidate();
            handler.postDelayed(this, FRAME_MS);
        }
    };

    public GameView(Context context) {
        super(context);
        density = getResources().getDisplayMetrics().density;
        titleType = Typeface.create(Typeface.SERIF, Typeface.BOLD);
        bodyType = Typeface.SANS_SERIF;
        tex.init();
        game.sound = this;
        setKeepScreenOn(true);
        try {
            tone = new ToneGenerator(AudioManager.STREAM_MUSIC, 55);
        } catch (RuntimeException e) {
            tone = null;
        }
    }

    // ------------------------------------------------------------------
    // 生命周期
    // ------------------------------------------------------------------

    public void resume() {
        if (!ticking) {
            ticking = true;
            lastT = 0L;
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
        density = getResources().getDisplayMetrics().density;
        rw = RW;
        rh = Math.max(160, Math.min(720, (int) (RW * (float) newH / newW)));
        frame = Bitmap.createBitmap(rw, rh, Bitmap.Config.ARGB_8888);
        frameCanvas = new Canvas(frame);
        dstRect.set(0, 0, w, h);

        btnR = 46f * density;
        btnX = w - 70f * density;
        btnY = h - 120f * density;
        torchR = 32f * density;
        torchX = w - 70f * density;
        torchY = h - 260f * density;
        joyR = 62f * density;
        defJoyX = 120f * density;
        defJoyY = h - 130f * density;
    }

    // ------------------------------------------------------------------
    // 绘制
    // ------------------------------------------------------------------

    @Override
    protected void onDraw(Canvas c) {
        c.drawColor(Color.BLACK);
        if (game.state == Game.ST_PLAY) {
            if (frame != null) {
                paint.reset();
                paint.setAntiAlias(true);
                paint.setFilterBitmap(true);
                c.drawBitmap(frame, null, dstRect, paint);
            }
            drawHud(c);
            // 闪电
            if (game.flashFrames > 0) {
                paint.reset();
                paint.setAntiAlias(true);
                paint.setColor(game.flashFrames > 3 ? 0x55FFFFFF : 0x22FFFFFF);
                c.drawRect(0, 0, w, h, paint);
            }
            // 楼层传送黑场
            if (game.fadeAlpha > 0f) {
                paint.reset();
                paint.setAntiAlias(true);
                int a = (int) (Math.min(1f, game.fadeAlpha) * 255f);
                paint.setColor(Color.argb(a, 0, 0, 0));
                c.drawRect(0, 0, w, h, paint);
            }
        } else if (game.state == Game.ST_INTRO) {
            drawIntro(c);
        } else if (game.state == Game.ST_WIN) {
            drawWin(c);
        }
    }

    private void drawHud(Canvas c) {
        Player p = game.player;

        // 目标条
        String quest = game.questText();
        paint.reset();
        paint.setAntiAlias(true);
        paint.setColor(0x88101010);
        float qw = 320f * density;
        c.drawRoundRect(new RectF(w / 2f - qw / 2f, 8f * density,
                w / 2f + qw / 2f, 36f * density), 8f * density, 8f * density, paint);
        drawTextCenter(c, quest, 29f * density, 14f * density, 0xFFE8E6D9, 240);

        // 物品栏（左上）
        float iy = 10f * density;
        float iw = 22f * density;
        if (p.hasKey) {
            drawIcon(c, tex.sprites[Textures.S_KEY], iy, iy, iw);
            iy += iw + 6f * density;
        }
        if (p.hasCard) {
            drawIcon(c, tex.sprites[Textures.S_CARD], iy, 10f * density, iw);
            iy += iw + 6f * density;
        }
        if (p.hasFlashlight) {
            drawIcon(c, tex.sprites[Textures.S_TORCH], iy, 10f * density, iw);
            iy += iw + 6f * density;
        }
        if (p.hasBasementKey) {
            drawIcon(c, tex.sprites[Textures.S_KEY], iy, 10f * density, iw);
            iy += iw + 6f * density;
        }
        if (p.hasCrowbar) {
            drawIcon(c, tex.sprites[Textures.S_CROWBAR], iy, 10f * density, iw);
        }

        // 准星
        paint.reset();
        paint.setAntiAlias(true);
        paint.setColor(0x99FFFFFF);
        c.drawCircle(w / 2f, h / 2f, 2.5f * density, paint);

        // 消息（底部中间）
        float msgY = h - 300f * density;
        int shown = 0;
        for (int i = game.messages.size() - 1; i >= 0 && shown < 2; i--) {
            Game.Msg m = game.messages.get(i);
            float alpha = Math.min(1f, m.life / 0.6f);
            drawTextCenter(c, m.text, msgY, 14f * density, 0xFFEFE7D6,
                    (int) (235 * alpha));
            msgY -= 24f * density;
            shown++;
        }

        // 互动按钮
        boolean active = game.interactLabel.length() > 0;
        paint.reset();
        paint.setAntiAlias(true);
        paint.setColor(active ? 0x66101010 : 0x33000000);
        c.drawCircle(btnX, btnY, btnR, paint);
        paint.setStyle(Paint.Style.STROKE);
        paint.setStrokeWidth(2.5f * density);
        paint.setColor(active ? 0xFFE8E6D9 : 0x55FFFFFF);
        c.drawCircle(btnX, btnY, btnR, paint);
        drawTextAt(c, active ? game.interactLabel : "·", btnX, btnY + 5f * density,
                16f * density, active ? 0xFFE8E6D9 : 0x66FFFFFF, Paint.Align.CENTER);

        // 手电筒按钮
        if (p.hasFlashlight) {
            paint.reset();
            paint.setAntiAlias(true);
            paint.setColor(0x33000000);
            c.drawCircle(torchX, torchY, torchR, paint);
            paint.setStyle(Paint.Style.STROKE);
            paint.setStrokeWidth(2.5f * density);
            paint.setColor(p.flashlightOn ? 0xFFF5E9A8 : 0x88FFFFFF);
            c.drawCircle(torchX, torchY, torchR, paint);
            // 手电筒小图标
            paint.setStyle(Paint.Style.FILL);
            paint.setColor(0xFF9AA1A6);
            c.drawRect(torchX - 8f * density, torchY - 4f * density,
                    torchX + 10f * density, torchY + 4f * density, paint);
            paint.setColor(0xFF3B4247);
            c.drawRect(torchX - 14f * density, torchY - 6f * density,
                    torchX - 8f * density, torchY + 6f * density, paint);
            paint.setColor(p.flashlightOn ? 0xFFF5E9A8 : 0xFF666055);
            c.drawCircle(torchX - 11f * density, torchY, 3f * density, paint);
            drawTextAt(c, "手电", torchX, torchY + 20f * density,
                    11f * density, p.flashlightOn ? 0xFFF5E9A8 : 0x88FFFFFF, Paint.Align.CENTER);
        }

        // 虚拟摇杆
        float jx = joyId == -1 ? defJoyX : joyBaseX;
        float jy = joyId == -1 ? defJoyY : joyBaseY;
        paint.reset();
        paint.setAntiAlias(true);
        paint.setStyle(Paint.Style.STROKE);
        paint.setStrokeWidth(2f * density);
        paint.setColor(0x55FFFFFF);
        c.drawCircle(jx, jy, joyR, paint);
        paint.setStyle(Paint.Style.FILL);
        paint.setColor(0x883A3F44);
        c.drawCircle(jx + joyVecX * joyR * 0.55f, jy + joyVecY * joyR * 0.55f,
                24f * density, paint);

        // 地下室黑暗提示
        if (p.floor == LevelMap.B1 && !p.flashlightOn) {
            int pulse = 120 + (int) (110 * (0.5 + 0.5 * Math.sin(game.timePlayed * 6f)));
            drawTextCenter(c, "太黑了——打开手电筒！", h * 0.30f,
                    15f * density, 0xFFCC4444, pulse);
        }
    }

    private void drawIcon(Canvas c, Bitmap bmp, float x, float y, float size) {
        paint.reset();
        paint.setAntiAlias(true);
        paint.setFilterBitmap(true);
        c.drawBitmap(bmp, null, new RectF(x, y, x + size, y + size), paint);
    }

    // ------------------------------------------------------------------
    // 开场 / 胜利
    // ------------------------------------------------------------------

    private static final String[][] INTRO_PAGES = {
            {"……消毒水的气味。", "你在青山病院的病床上醒来。", "头很沉——你想不起自己为什么在这里。"},
            {"房门锁着。", "走廊尽头，有什么东西", "在轻轻地、轻轻地拖行。"},
            {"逃出去。", "（左手拖动 移动 · 右手滑动 转身）", "（右下按钮：开门 / 撬板 / 刷卡）"},
    };

    private void drawIntro(Canvas c) {
        c.drawColor(0xFF040404);
        drawTextCenter(c, "午 夜 病 栋", h * 0.15f, 42f * density, 0xFFE8E6D9, 255);
        drawTextCenter(c, "MIDNIGHT WARD · 3D", h * 0.15f + 28f * density,
                12f * density, 0xFF8A8F8C, 255);
        String[] page = INTRO_PAGES[Math.min(game.introPage, INTRO_PAGES.length - 1)];
        for (int i = 0; i < page.length; i++) {
            drawTextCenter(c, page[i], h * 0.42f + i * 28f * density,
                    15f * density, 0xFFCFCFC6, 235);
        }
        int breath = 120 + (int) (100 * (0.5 + 0.5 * Math.sin(game.timePlayed * 0.001f
                + System.currentTimeMillis() * 0.004f)));
        drawTextCenter(c, "—— 触摸继续 ——", h * 0.86f, 13f * density, 0xFFE8E6D9, breath);
    }

    private void drawWin(Canvas c) {
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

        drawTextCenter(c, "你 推 开 了 大 门", h * 0.22f, 30f * density, 0xFFF5EFE0, 255);
        drawTextCenter(c, "冷风灌进领口。", h * 0.36f, 14f * density, 0xFFEFE7D6, 235);
        drawTextCenter(c, "身后的病栋在晨雾里沉默着——你逃出来了。", h * 0.40f, 14f * density, 0xFFEFE7D6, 235);
        int sec = (int) game.timePlayed;
        drawTextCenter(c, "逃脱用时  " + sec / 60 + " 分 " + (sec % 60) + " 秒",
                h * 0.52f, 15f * density, 0xFFE8B15C, 255);
        int breath = 120 + (int) (100 * (0.5 + 0.5 * Math.sin(System.currentTimeMillis() * 0.004f)));
        drawTextCenter(c, "—— 触摸屏幕 · 再来一次 ——", h * 0.82f, 15f * density, 0xFFF5EFE0, breath);
    }

    // ------------------------------------------------------------------
    // 文字工具
    // ------------------------------------------------------------------

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

    private void drawTextAt(Canvas c, String s, float x, float y, float sizePx,
                            int color, Paint.Align align) {
        textPaint.reset();
        textPaint.setAntiAlias(true);
        textPaint.setTypeface(bodyType);
        textPaint.setTextAlign(align);
        textPaint.setTextSize(sizePx);
        textPaint.setColor(color);
        c.drawText(s, x, y, textPaint);
    }

    // ------------------------------------------------------------------
    // 触摸操控
    // ------------------------------------------------------------------

    @Override
    public boolean onTouchEvent(MotionEvent e) {
        switch (e.getActionMasked()) {
            case MotionEvent.ACTION_DOWN:
            case MotionEvent.ACTION_POINTER_DOWN: {
                int idx = e.getActionIndex();
                float x = e.getX(idx);
                float y = e.getY(idx);
                int id = e.getPointerId(idx);
                if (game.state == Game.ST_INTRO) {
                    game.introPage++;
                    if (game.introPage >= INTRO_PAGES.length) {
                        game.startPlay();
                    }
                    return true;
                }
                if (game.state == Game.ST_WIN) {
                    game.reset();
                    return true;
                }
                // 按钮
                if (dist2(x, y, btnX, btnY) < (btnR * 1.4f) * (btnR * 1.4f)) {
                    game.interact();
                    return true;
                }
                if (game.player.hasFlashlight
                        && dist2(x, y, torchX, torchY) < (torchR * 1.6f) * (torchR * 1.6f)) {
                    game.player.flashlightOn = !game.player.flashlightOn;
                    playTone(ToneGenerator.TONE_PROP_BEEP, 60);
                    return true;
                }
                // 左侧摇杆 / 右侧转身
                if (x < w * 0.45f) {
                    joyId = id;
                    joyBaseX = x;
                    joyBaseY = y;
                    joyVecX = 0f;
                    joyVecY = 0f;
                } else {
                    lookId = id;
                    lastLookX = x;
                }
                return true;
            }
            case MotionEvent.ACTION_MOVE: {
                for (int i = 0; i < e.getPointerCount(); i++) {
                    int id = e.getPointerId(i);
                    float x = e.getX(i);
                    float y = e.getY(i);
                    if (id == joyId) {
                        float dx = x - joyBaseX;
                        float dy = y - joyBaseY;
                        float m = (float) Math.sqrt(dx * dx + dy * dy);
                        if (m > joyR) {
                            dx = dx / m * joyR;
                            dy = dy / m * joyR;
                        }
                        joyVecX = dx / joyR;
                        joyVecY = dy / joyR;
                    } else if (id == lookId) {
                        game.player.angle += (x - lastLookX) * 0.0042f;
                        lastLookX = x;
                    }
                }
                return true;
            }
            case MotionEvent.ACTION_POINTER_UP:
            case MotionEvent.ACTION_UP:
            case MotionEvent.ACTION_CANCEL: {
                int idx = e.getActionIndex();
                int id = e.getPointerId(idx);
                if (id == joyId) {
                    joyId = -1;
                    joyVecX = 0f;
                    joyVecY = 0f;
                }
                if (id == lookId) {
                    lookId = -1;
                }
                return true;
            }
            default:
                return true;
        }
    }

    private static float dist2(float x1, float y1, float x2, float y2) {
        float dx = x1 - x2;
        float dy = y1 - y2;
        return dx * dx + dy * dy;
    }

    // ------------------------------------------------------------------
    // Game.Sound 实现
    // ------------------------------------------------------------------

    private void playTone(int type, int ms) {
        if (tone != null) {
            try {
                tone.startTone(type, ms);
            } catch (Exception ignored) {
            }
        }
    }

    @Override
    public void beep() {
        playTone(ToneGenerator.TONE_PROP_BEEP, 90);
    }

    @Override
    public void ack() {
        playTone(ToneGenerator.TONE_PROP_ACK, 130);
    }

    @Override
    public void nack() {
        playTone(ToneGenerator.TONE_PROP_NACK, 180);
    }

    @Override
    public void creak() {
        playTone(ToneGenerator.TONE_PROP_BEEP2, 220);
        vibrate(90);
    }

    @Override
    public void error() {
        playTone(ToneGenerator.TONE_SUP_ERROR, 700);
        vibrate(500);
    }

    @Override
    public void whisper() {
        playTone(ToneGenerator.TONE_PROP_BEEP2, 320);
    }

    @Override
    public void thunder() {
        playTone(ToneGenerator.TONE_PROP_NACK, 480);
    }

    @Override
    public void beat() {
        playTone(ToneGenerator.TONE_PROP_BEEP, 60);
    }

    @Override
    public void vibrate(long ms) {
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
