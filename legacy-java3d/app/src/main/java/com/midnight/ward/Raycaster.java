package com.midnight.ward;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Rect;

import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.List;

/**
 * 光线投射（Raycasting）伪 3D 渲染器，Wolfenstein 3D 风格：
 * 逐列 DDA 扫描墙面 + 薄门（滑门）+ billboard 精灵 + zbuffer 裁剪
 * + 手电筒锥形照明 / 距离衰减 / 侧面变暗。
 */
public class Raycaster {

    private float[] zbuf = new float[0];

    private final Paint plain = new Paint();
    private final Paint dark = new Paint();
    private final Rect src = new Rect();
    private final Rect dst = new Rect();

    public Raycaster() {
        plain.setFilterBitmap(false);
        dark.setStyle(Paint.Style.FILL);
        dark.setColor(0xFF000000);
    }

    public void render(Canvas c, int w, int h, LevelMap lm, Game g, Player p,
                       Textures t, float bobY, float flicker) {
        if (zbuf.length < w) {
            zbuf = new float[w];
        }
        float dirX = (float) Math.cos(p.angle);
        float dirY = (float) Math.sin(p.angle);
        float planeLen = 0.72f; // ~72° 视场角
        float planeX = -dirY * planeLen;
        float planeY = dirX * planeLen;

        boolean b1 = p.floor == LevelMap.B1;
        boolean flash = p.flashlightOn && p.hasFlashlight;

        // 天花板 / 地板（B1 更黑）
        dark.setAlpha(255);
        dark.setColor(b1 ? 0xFF030304 : 0xFF0A0D12);
        c.drawRect(0, 0, w, h / 2f, dark);
        dark.setColor(b1 ? 0xFF050404 : 0xFF191510);
        c.drawRect(0, h / 2f, w, h, dark);

        int[][] gmap = lm.grid[p.floor];

        // ---------------- 墙面（逐列 DDA） ----------------
        for (int x = 0; x < w; x++) {
            float cameraX = 2f * x / w - 1f;
            double rayX = dirX + planeX * cameraX;
            double rayY = dirY + planeY * cameraX;

            int mapX = (int) Math.floor(p.x);
            int mapY = (int) Math.floor(p.y);
            double deltaX = Math.abs(rayX) < 1e-9 ? 1e30 : Math.abs(1.0 / rayX);
            double deltaY = Math.abs(rayY) < 1e-9 ? 1e30 : Math.abs(1.0 / rayY);
            int stepX = rayX < 0 ? -1 : 1;
            int stepY = rayY < 0 ? -1 : 1;
            double sdX = rayX < 0 ? (p.x - mapX) * deltaX : (mapX + 1.0 - p.x) * deltaX;
            double sdY = rayY < 0 ? (p.y - mapY) * deltaY : (mapY + 1.0 - p.y) * deltaY;

            double perp = 0.0;
            double wallX = 0.0;
            int side = 0;
            Bitmap tex = null;
            boolean hit = false;

            for (int iter = 0; iter < 96 && !hit; iter++) {
                if (sdX < sdY) {
                    sdX += deltaX;
                    mapX += stepX;
                    side = 0;
                } else {
                    sdY += deltaY;
                    mapY += stepY;
                    side = 1;
                }
                if (mapX < 0 || mapY < 0 || mapY >= gmap.length || mapX >= gmap[mapY].length) {
                    break;
                }
                int cell = gmap[mapY][mapX];
                if (cell == LevelMap.CELL_FLOOR) {
                    continue;
                }
                boolean thin = cell == LevelMap.CELL_DOOR || cell == LevelMap.CELL_METAL
                        || cell == LevelMap.CELL_STAIR || cell == LevelMap.CELL_GATE;
                if (thin) {
                    // 薄门：与格子中平面求交，openness 为滑开量
                    double tt;
                    double coord;
                    if (side == 0) {
                        if (Math.abs(rayX) < 1e-9) {
                            continue;
                        }
                        tt = (mapX + 0.5 - p.x) / rayX;
                        double hy = p.y + tt * rayY;
                        if (tt <= 0 || (int) Math.floor(hy) != mapY) {
                            continue;
                        }
                        coord = hy - mapY;
                    } else {
                        if (Math.abs(rayY) < 1e-9) {
                            continue;
                        }
                        tt = (mapY + 0.5 - p.y) / rayY;
                        double hx = p.x + tt * rayX;
                        if (tt <= 0 || (int) Math.floor(hx) != mapX) {
                            continue;
                        }
                        coord = hx - mapX;
                    }
                    double open = doorOpen(lm, g, p.floor, mapX, mapY, cell);
                    if (coord >= open) {
                        perp = tt;
                        wallX = coord - open; // 纹理随门滑动
                        hit = true;
                        tex = wallTexFor(t, cell, g);
                    }
                    // 否则门已滑开该部分，继续 DDA
                } else {
                    perp = (side == 0) ? sdX - deltaX : sdY - deltaY;
                    double hx = (side == 0) ? p.y + perp * rayY : p.x + perp * rayX;
                    wallX = hx - Math.floor(hx);
                    hit = true;
                    tex = wallTexFor(t, cell, g);
                }
            }

            if (!hit || tex == null) {
                zbuf[x] = 1e9f;
                continue;
            }
            if (perp < 0.02) {
                perp = 0.02;
            }
            zbuf[x] = (float) perp;

            int lineH = (int) (h / perp);
            int midY = h / 2 + (int) bobY;
            int y0 = midY - lineH / 2;
            int cy0 = Math.max(0, y0);
            int cy1 = Math.min(h, y0 + lineH);
            if (cy1 <= cy0) {
                continue;
            }
            int texX = (int) (wallX * (Textures.TS - 1));
            if (texX < 0) {
                texX = 0;
            } else if (texX > Textures.TS - 1) {
                texX = Textures.TS - 1;
            }
            src.set(texX, 0, texX + 1, Textures.TS);
            dst.set(x, cy0, x + 1, cy1);
            c.drawBitmap(tex, src, dst, plain);

            float b = brightness(Math.abs(cameraX), perp, flash, b1, flicker, side == 1);
            if (b < 0.985f) {
                dark.setAlpha((int) ((1f - b) * 255f));
                c.drawRect(dst, dark);
            }
        }
        dark.setAlpha(255);

        // ---------------- 精灵（远 → 近，zbuffer 裁剪） ----------------
        List<Object[]> list = new ArrayList<>(); // {depth², Sprite}
        for (Sprite s : g.sprites) {
            if (s.alive && s.floor == p.floor) {
                float dx = s.x - p.x;
                float dy = s.y - p.y;
                list.add(new Object[]{dx * dx + dy * dy, s});
            }
        }
        if (g.ghost != null && g.ghost.floor == p.floor) {
            float dx = g.ghost.x - p.x;
            float dy = g.ghost.y - p.y;
            list.add(new Object[]{dx * dx + dy * dy, g.ghost});
        }
        Collections.sort(list, new Comparator<Object[]>() {
            @Override
            public int compare(Object[] a, Object[] b) {
                return Double.compare((Double) b[0], (Double) a[0]);
            }
        });

        float invDet = 1f / (planeX * dirY - dirX * planeY);

        for (Object[] o : list) {
            Sprite s = (Sprite) o[1];
            float dx = s.x - p.x;
            float dy = s.y - p.y;
            float tX = invDet * (dirY * dx - dirX * dy);
            float tY = invDet * (-planeY * dx + planeX * dy); // 深度
            if (tY <= 0.12f) {
                continue;
            }
            float sx = (w / 2f) * (1f + tX / tY);
            float scale = h / tY;
            int size = (int) (scale * s.height);
            int width = (int) (size * s.aspect);
            if (size < 2 || width < 2) {
                continue;
            }
            float floorLine = h / 2f + (h / 2f) / tY + bobY;
            float bobItem = s.type == Sprite.TYPE_GHOST
                    ? 0f
                    : (float) Math.sin(g.timePlayed * 5f + s.x * 3f) * scale * 0.02f;
            float drawEnd = floorLine - bobItem;
            float drawStart = drawEnd - size;
            int top = Math.max(0, (int) drawStart);
            int bot = Math.min(h, (int) drawEnd);
            if (bot <= top) {
                continue;
            }
            Bitmap bt = spriteTexFor(t, s.type);
            if (bt == null) {
                continue;
            }
            int bw = bt.getWidth();
            int bh = bt.getHeight();
            int x0 = (int) (sx - width / 2f);
            int x1 = x0 + width;
            float camAbs = Math.abs(tX / tY);
            float b = brightness(camAbs, tY, flash, b1, flicker, false);
            float vis = s.type == Sprite.TYPE_GHOST
                    ? (0.55f + 0.45f * b)
                    : (0.22f + 0.78f * b);
            int alpha = (int) (255f * Math.min(1f, s.alpha) * vis);
            if (alpha <= 6) {
                continue;
            }
            plain.setAlpha(alpha);
            for (int x = Math.max(0, x0); x < Math.min(w, x1); x++) {
                if (tY >= zbuf[x]) {
                    continue;
                }
                int tx = (x - x0) * bw / width;
                if (tx < 0) {
                    tx = 0;
                } else if (tx >= bw) {
                    tx = bw - 1;
                }
                src.set(tx, 0, tx + 1, bh);
                dst.set(x, top, x + 1, bot);
                c.drawBitmap(bt, src, dst, plain);
            }
            plain.setAlpha(255);
        }
    }

    private double doorOpen(LevelMap lm, Game g, int floor, int x, int y, int cell) {
        if (cell == LevelMap.CELL_GATE) {
            return g.gateOpenness;
        }
        LevelMap.DoorState d = lm.doors.get(LevelMap.key(floor, x, y));
        return d == null ? 0f : d.openness;
    }

    private Bitmap wallTexFor(Textures t, int cell, Game g) {
        switch (cell) {
            case LevelMap.CELL_PLASTER:
                return t.walls[Textures.T_PLASTER];
            case LevelMap.CELL_TILE:
                return t.walls[Textures.T_TILE];
            case LevelMap.CELL_BLOOD:
                return t.walls[Textures.T_BLOOD];
            case LevelMap.CELL_DOOR:
                return t.walls[Textures.T_DOOR];
            case LevelMap.CELL_METAL:
                return t.walls[Textures.T_METAL];
            case LevelMap.CELL_STAIR:
                return t.walls[Textures.T_STAIRS];
            case LevelMap.CELL_GATE:
                if (g.boards >= 2) {
                    return t.walls[Textures.T_GATE_2];
                }
                if (g.boards == 1) {
                    return t.walls[Textures.T_GATE_1];
                }
                return t.walls[Textures.T_GATE_CARD];
            default:
                return t.walls[Textures.T_PLASTER];
        }
    }

    private Bitmap spriteTexFor(Textures t, int type) {
        switch (type) {
            case Sprite.TYPE_KEY:
                return t.sprites[Textures.S_KEY];
            case Sprite.TYPE_CARD:
                return t.sprites[Textures.S_CARD];
            case Sprite.TYPE_TORCH:
                return t.sprites[Textures.S_TORCH];
            case Sprite.TYPE_BKEY:
                return t.sprites[Textures.S_KEY];
            case Sprite.TYPE_CROWBAR:
                return t.sprites[Textures.S_CROWBAR];
            case Sprite.TYPE_BED:
                return t.sprites[Textures.S_BED];
            case Sprite.TYPE_GHOST:
                return t.sprites[Textures.S_GHOST];
            default:
                return null;
        }
    }

    /**
     * 光照：环境光随距离衰减 + 手电筒锥形光束（带火苗抖动）+ 侧面变暗。
     */
    private float brightness(float camAbs, double dist, boolean flash, boolean b1,
                             float flicker, boolean side) {
        double amb = b1 ? 0.05 : 0.42;
        double fade = 1.25 / (1.0 + dist * dist * 0.055);
        double b = amb * fade * 2.0;
        if (flash) {
            double cone = Math.max(0.0, 1.0 - camAbs * 1.85);
            cone *= cone;
            b += cone * (1.55 / (1.0 + dist * 0.62)) * flicker;
        }
        if (side) {
            b *= 0.78;
        }
        return (float) Math.max(0.0, Math.min(1.0, b));
    }
}
