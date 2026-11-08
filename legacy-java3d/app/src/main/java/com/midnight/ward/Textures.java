package com.midnight.ward;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.RectF;

import java.util.Random;

/**
 * 程序化生成全部贴图：墙面 / 门 / 大门 / 楼梯 / 物品精灵 / 怨灵 / 病床。
 * 无任何素材文件，全部用 Canvas 手绘。
 */
public final class Textures {

    // 墙面纹理索引
    public static final int T_PLASTER = 0;   // 涂料墙
    public static final int T_TILE = 1;      // 瓷砖墙裙
    public static final int T_BLOOD = 2;     // 血迹墙
    public static final int T_DOOR = 3;      // 木门
    public static final int T_METAL = 4;     // 金属门（B1）
    public static final int T_STAIRS = 5;    // 楼梯门
    public static final int T_GATE_2 = 6;    // 大门：两根木板
    public static final int T_GATE_1 = 7;    // 大门：一根木板
    public static final int T_GATE_CARD = 8; // 大门：读卡器

    // 精灵纹理索引
    public static final int S_KEY = 0;
    public static final int S_CARD = 1;
    public static final int S_TORCH = 2;
    public static final int S_CROWBAR = 3;
    public static final int S_GHOST = 4;
    public static final int S_BED = 5;

    public static final int TS = 64; // 墙纹理边长

    public final Bitmap[] walls = new Bitmap[9];
    public final Bitmap[] sprites = new Bitmap[6];

    private final Random rng = new Random(7);
    private final Paint p = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint tp = new Paint(Paint.ANTI_ALIAS_FLAG);

    public void init() {
        walls[T_PLASTER] = make(new Painter() {
            public void draw(Canvas c) {
                plaster(c);
            }
        });
        walls[T_TILE] = make(new Painter() {
            public void draw(Canvas c) {
                tile(c);
            }
        });
        walls[T_BLOOD] = make(new Painter() {
            public void draw(Canvas c) {
                blood(c);
            }
        });
        walls[T_DOOR] = make(new Painter() {
            public void draw(Canvas c) {
                woodDoor(c);
            }
        });
        walls[T_METAL] = make(new Painter() {
            public void draw(Canvas c) {
                metalDoor(c);
            }
        });
        walls[T_STAIRS] = make(new Painter() {
            public void draw(Canvas c) {
                stairs(c);
            }
        });
        walls[T_GATE_2] = make(new Painter() {
            public void draw(Canvas c) {
                gateBase(c);
                plank(c, 16);
                plank(c, 34);
            }
        });
        walls[T_GATE_1] = make(new Painter() {
            public void draw(Canvas c) {
                gateBase(c);
                plank(c, 34);
            }
        });
        walls[T_GATE_CARD] = make(new Painter() {
            public void draw(Canvas c) {
                gateBase(c);
                cardReader(c);
            }
        });

        sprites[S_KEY] = makeSprite(32, 32, new Painter() {
            public void draw(Canvas c) {
                keySprite(c);
            }
        });
        sprites[S_CARD] = makeSprite(32, 32, new Painter() {
            public void draw(Canvas c) {
                cardSprite(c);
            }
        });
        sprites[S_TORCH] = makeSprite(32, 32, new Painter() {
            public void draw(Canvas c) {
                torchSprite(c);
            }
        });
        sprites[S_CROWBAR] = makeSprite(32, 32, new Painter() {
            public void draw(Canvas c) {
                crowbarSprite(c);
            }
        });
        sprites[S_GHOST] = makeSprite(48, 72, new Painter() {
            public void draw(Canvas c) {
                ghostSprite(c);
            }
        });
        sprites[S_BED] = makeSprite(64, 40, new Painter() {
            public void draw(Canvas c) {
                bedSprite(c);
            }
        });
    }

    private interface Painter {
        void draw(Canvas c);
    }

    private Bitmap make(Painter painter) {
        Bitmap b = Bitmap.createBitmap(TS, TS, Bitmap.Config.ARGB_8888);
        painter.draw(new Canvas(b));
        return b;
    }

    private Bitmap makeSprite(int w, int h, Painter painter) {
        Bitmap b = Bitmap.createBitmap(w, h, Bitmap.Config.ARGB_8888);
        painter.draw(new Canvas(b));
        return b;
    }

    // ------------------------------------------------------------------
    // 墙面
    // ------------------------------------------------------------------

    private void plaster(Canvas c) {
        p.setStyle(Paint.Style.FILL);
        p.setColor(0xFF39413C);
        c.drawRect(0, 0, 64, 64, p);
        // 噪点
        for (int i = 0; i < 90; i++) {
            p.setColor(rng.nextBoolean() ? 0x11363028 : 0x11495044);
            float x = rng.nextFloat() * 64f;
            float y = rng.nextFloat() * 64f;
            c.drawRect(x, y, x + 2, y + 2, p);
        }
        // 竖向污渍
        for (int i = 0; i < 7; i++) {
            p.setColor(0x14201812);
            float x = rng.nextFloat() * 64f;
            c.drawRect(x, rng.nextFloat() * 10f, x + 3f + rng.nextFloat() * 4f, 64f, p);
        }
        // 裂缝
        p.setStyle(Paint.Style.STROKE);
        p.setStrokeWidth(1f);
        p.setColor(0xFF232B27);
        for (int i = 0; i < 2; i++) {
            Path cr = new Path();
            cr.moveTo(rng.nextFloat() * 64f, 0f);
            cr.rLineTo(rng.nextFloat() * 8f - 4f, 14f);
            cr.rLineTo(rng.nextFloat() * 10f - 5f, 16f);
            cr.rLineTo(rng.nextFloat() * 8f - 4f, 18f);
            c.drawPath(cr, p);
        }
        // 踢脚线
        p.setStyle(Paint.Style.FILL);
        p.setColor(0xFF2C332F);
        c.drawRect(0, 57, 64, 64, p);
        p.setColor(0xFF4A544E);
        c.drawRect(0, 57, 64, 58, p);
    }

    private void tile(Canvas c) {
        plaster(c);
        p.setStyle(Paint.Style.FILL);
        p.setColor(0xFF40544A);
        c.drawRect(0, 26, 64, 64, p);
        p.setStyle(Paint.Style.STROKE);
        p.setStrokeWidth(2f);
        p.setColor(0xFF2A3A33);
        for (int y = 26; y <= 64; y += 12) {
            c.drawLine(0, y, 64, y, p);
        }
        for (int x = 0; x <= 64; x += 16) {
            c.drawLine(x, 26, x, 64, p);
        }
        p.setStyle(Paint.Style.FILL);
        p.setColor(0xFF54685C);
        c.drawRect(0, 26, 64, 28, p);
    }

    private void blood(Canvas c) {
        plaster(c);
        p.setStyle(Paint.Style.FILL);
        p.setColor(0xFF4C120F);
        c.drawCircle(22, 18, 7, p);
        c.drawCircle(31, 24, 4, p);
        c.drawCircle(14, 30, 5, p);
        c.drawCircle(24, 52, 6, p);
        p.setColor(0xFF3D0E0B);
        for (int i = 0; i < 5; i++) {
            float x = 12f + rng.nextFloat() * 22f;
            c.drawRect(x, 24f + rng.nextFloat() * 8f, x + 2f, 44f + rng.nextFloat() * 18f, p);
        }
    }

    private void woodDoor(Canvas c) {
        p.setStyle(Paint.Style.FILL);
        p.setColor(0xFF4B3826);
        c.drawRect(0, 0, 64, 64, p);
        // 木纹
        for (int i = 0; i < 26; i++) {
            p.setColor(0x33241A10);
            float x = rng.nextFloat() * 64f;
            c.drawRect(x, 0, x + 1, 64, p);
        }
        // 边框
        p.setStyle(Paint.Style.STROKE);
        p.setStrokeWidth(3f);
        p.setColor(0xFF2E2117);
        c.drawRect(2, 2, 62, 62, p);
        // 门板嵌线
        p.setStrokeWidth(2f);
        c.drawRect(10, 10, 54, 28, p);
        c.drawRect(10, 34, 54, 56, p);
        // 门牌
        p.setStyle(Paint.Style.FILL);
        p.setColor(0xFFD8D4C4);
        c.drawRect(24, 4, 40, 10, p);
        tp.setColor(0xFF222222);
        tp.setTextSize(7f);
        tp.setTextAlign(Paint.Align.CENTER);
        c.drawText("302", 32, 9.5f, tp);
        // 黄铜把手
        p.setColor(0xFFC9A227);
        c.drawCircle(56, 34, 3, p);
    }

    private void metalDoor(Canvas c) {
        p.setStyle(Paint.Style.FILL);
        p.setColor(0xFF3E444A);
        c.drawRect(0, 0, 64, 64, p);
        // 拉丝
        for (int y = 0; y < 64; y += 3) {
            p.setColor(rng.nextBoolean() ? 0x11333B41 : 0x11505860);
            c.drawRect(0, y, 64, y + 1, p);
        }
        // 铆钉
        p.setColor(0xFF5A646C);
        for (int x = 6; x < 64; x += 13) {
            c.drawCircle(x, 5, 1.5f, p);
            c.drawCircle(x, 59, 1.5f, p);
        }
        // 锁箱
        p.setColor(0xFF262B30);
        c.drawRect(22, 24, 42, 44, p);
        p.setStyle(Paint.Style.STROKE);
        p.setStrokeWidth(2f);
        p.setColor(0xFF12161A);
        c.drawRect(22, 24, 42, 44, p);
        p.setStyle(Paint.Style.FILL);
        p.setColor(0xFFC9A227);
        c.drawCircle(32, 32, 2.5f, p);
        c.drawRect(31, 32, 33, 38, p);
        // B1 字样
        tp.setColor(0xFFB8BFC6);
        tp.setTextSize(10f);
        tp.setTextAlign(Paint.Align.CENTER);
        c.drawText("B1", 32, 15, tp);
    }

    private void stairs(Canvas c) {
        p.setStyle(Paint.Style.FILL);
        p.setColor(0xFF101318);
        c.drawRect(0, 0, 64, 64, p);
        // 侧壁
        p.setColor(0xFF1A1F26);
        Path s1 = new Path();
        s1.moveTo(0, 0);
        s1.lineTo(8, 10);
        s1.lineTo(8, 62);
        s1.lineTo(0, 64);
        s1.close();
        c.drawPath(s1, p);
        Path s2 = new Path();
        s2.moveTo(64, 0);
        s2.lineTo(56, 10);
        s2.lineTo(56, 62);
        s2.lineTo(64, 64);
        s2.close();
        c.drawPath(s2, p);
        // 逐级变暗的台阶
        for (int i = 0; i < 6; i++) {
            int v = 34 + i * 5;
            p.setColor(Color.rgb(v, v + 2, v + 5));
            c.drawRect(8 + i * 3, 12 + i * 8, 56 - i * 3, 20 + i * 8, p);
        }
        // 向下箭头
        p.setColor(0xFFC9C6B8);
        Path a = new Path();
        a.moveTo(32, 2);
        a.lineTo(27, 8);
        a.lineTo(37, 8);
        a.close();
        c.drawPath(a, p);
        c.drawRect(31, 2, 33, 6, p);
    }

    private void gateBase(Canvas c) {
        p.setStyle(Paint.Style.FILL);
        p.setColor(0xFF2F3B33);
        c.drawRect(0, 0, 64, 64, p);
        // 门纹
        for (int i = 0; i < 20; i++) {
            p.setColor(0x22000000);
            float x = rng.nextFloat() * 64f;
            c.drawRect(x, 0, x + 1, 64, p);
        }
        // 横向加固条
        p.setColor(0xFF26302A);
        c.drawRect(0, 6, 64, 11, p);
        c.drawRect(0, 53, 64, 58, p);
        // 中缝
        p.setColor(0xFF101613);
        c.drawRect(30, 0, 34, 64, p);
    }

    private void plank(Canvas c, int y) {
        p.setStyle(Paint.Style.FILL);
        p.setColor(0xFF7A5C39);
        c.drawRect(1, y, 63, y + 13, p);
        for (int i = 0; i < 8; i++) {
            p.setColor(0x305A4026);
            float x = rng.nextFloat() * 60f + 2;
            c.drawRect(x, y + 2, x + 1, y + 11, p);
        }
        p.setStyle(Paint.Style.STROKE);
        p.setStrokeWidth(2f);
        p.setColor(0xFF4A3620);
        c.drawRect(1, y, 63, y + 13, p);
        p.setStyle(Paint.Style.FILL);
        p.setColor(0xFF3A3A3A);
        c.drawCircle(7, y + 6, 2, p);
        c.drawCircle(57, y + 6, 2, p);
    }

    private void cardReader(Canvas c) {
        p.setStyle(Paint.Style.FILL);
        p.setColor(0xFF1E2126);
        c.drawRect(42, 22, 61, 42, p);
        p.setStyle(Paint.Style.STROKE);
        p.setStrokeWidth(2f);
        p.setColor(0xFF0E1114);
        c.drawRect(42, 22, 61, 42, p);
        p.setStyle(Paint.Style.FILL);
        p.setColor(0xFFFF4444);   // 红灯
        c.drawCircle(51, 28, 2.5f, p);
        p.setColor(0xFF0A0C0E);   // 卡槽
        c.drawRect(46, 34, 57, 37, p);
    }

    // ------------------------------------------------------------------
    // 精灵
    // ------------------------------------------------------------------

    private void keySprite(Canvas c) {
        p.setStyle(Paint.Style.FILL);
        p.setColor(0x44E8C860);
        c.drawCircle(16, 16, 13, p);
        p.setStyle(Paint.Style.STROKE);
        p.setStrokeWidth(3f);
        p.setColor(0xFFC9A227);
        c.drawCircle(10, 16, 5, p);
        p.setStyle(Paint.Style.FILL);
        c.drawRect(14, 14, 27, 18, p);
        c.drawRect(22, 18, 25, 23, p);
        c.drawRect(26, 18, 28, 24, p);
        p.setColor(0xFFE8D48A);
        c.drawRect(14, 14, 27, 15, p);
    }

    private void cardSprite(Canvas c) {
        p.setStyle(Paint.Style.FILL);
        p.setColor(0x33FFFFFF);
        c.drawRoundRect(new RectF(2, 6, 30, 26), 3, 3, p);
        p.setColor(0xFFE8E8E0);
        c.drawRoundRect(new RectF(4, 8, 28, 24), 3, 3, p);
        p.setColor(0xFFB03A3A);
        c.drawRect(4, 8, 28, 13, p);
        p.setColor(0xFFC9A227);
        c.drawRect(7, 16, 13, 20, p);
        p.setColor(0xFF888888);
        c.drawRect(15, 16, 25, 17, p);
        c.drawRect(15, 19, 25, 20, p);
    }

    private void torchSprite(Canvas c) {
        p.setStyle(Paint.Style.FILL);
        p.setColor(0x55F5E9A8);
        c.drawCircle(8, 16, 9, p);
        p.setColor(0xFF6E777D);
        c.drawRect(10, 12, 30, 20, p);
        p.setColor(0xFF4A5257);
        c.drawRect(14, 12, 16, 20, p);
        c.drawRect(20, 12, 22, 20, p);
        p.setColor(0xFF3B4247);
        c.drawRect(4, 10, 10, 22, p);
        p.setColor(0xFFF5E9A8);
        c.drawCircle(7, 16, 3.5f, p);
    }

    private void crowbarSprite(Canvas c) {
        p.setStyle(Paint.Style.STROKE);
        p.setStrokeWidth(5f);
        p.setColor(0xFF8A2F27);
        Path bar = new Path();
        bar.moveTo(5, 28);
        bar.quadTo(18, 24, 25, 10);
        bar.quadTo(29, 4, 27, 12);
        c.drawPath(bar, p);
        p.setStrokeWidth(2f);
        p.setColor(0xFFC05248);
        Path hl = new Path();
        hl.moveTo(7, 26);
        hl.quadTo(18, 22, 23, 10);
        c.drawPath(hl, p);
    }

    private void ghostSprite(Canvas c) {
        p.setStyle(Paint.Style.FILL);
        // 白裙
        p.setColor(0xFFE9E9E2);
        Path dress = new Path();
        dress.moveTo(24, 6);
        dress.lineTo(6, 62);
        dress.lineTo(42, 62);
        dress.close();
        c.drawPath(dress, p);
        c.drawCircle(24, 12, 9, p);
        // 长发
        p.setColor(0xFF0C0C10);
        Path hair = new Path();
        hair.moveTo(14, 12);
        hair.quadTo(24, -2, 34, 12);
        hair.lineTo(37, 26);
        hair.lineTo(31, 14);
        hair.lineTo(29, 40);
        hair.lineTo(23, 16);
        hair.lineTo(18, 36);
        hair.lineTo(14, 14);
        hair.close();
        c.drawPath(hair, p);
        // 双眼
        p.setColor(0xFF000000);
        c.drawCircle(20, 12, 2, p);
        c.drawCircle(28, 12, 2, p);
    }

    private void bedSprite(Canvas c) {
        p.setStyle(Paint.Style.FILL);
        // 影子
        p.setColor(0x22000000);
        c.drawOval(new RectF(6, 32, 58, 38), p);
        // 床头板
        p.setColor(0xFF2E3335);
        c.drawRect(6, 6, 11, 28, p);
        // 床架
        p.setColor(0xFF3A3F3D);
        c.drawRect(8, 18, 60, 26, p);
        // 床垫
        p.setColor(0xFFC9C6B8);
        c.drawRect(10, 12, 58, 20, p);
        // 枕头
        p.setColor(0xFFE4E0D2);
        c.drawRoundRect(new RectF(12, 8, 26, 15), 3, 3, p);
        // 脚轮
        p.setColor(0xFF191D20);
        c.drawCircle(16, 30, 3, p);
        c.drawCircle(52, 30, 3, p);
    }
}
