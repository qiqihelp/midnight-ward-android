#!/usr/bin/env python3
# gen_font_sdf.py — 生成 SDF 字体图集 assets/font_sdf.bin
# 扫描 src/core/strings.h 的全部字符串字面量 + ASCII 补集 → NotoSansSC 渲染
# → 4x 尺寸距离变换 → 下采样 → 图集二进制（与 src/core/font.cpp 的布局对应）。
import re
import struct
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont
from scipy.ndimage import distance_transform_edt

ROOT = Path(__file__).resolve().parent.parent
FONT_PATH = "/usr/share/fonts/truetype/chinese/NotoSansSC-Regular.ttf"
NOMINAL = 64          # 名义字号（图集基准）
SS = 4                # 超采样倍数
SIZE = NOMINAL * SS   # 256px 渲染
SPREAD = 12           # 名义尺度距离场半径（px）
ATLAS_W = 2048
OUT = ROOT / "assets" / "font_sdf.bin"
PREVIEW = ROOT / "scripts" / "preview" / "font_sdf_preview.png"


def collect_codepoints():
    src = (ROOT / "src" / "core" / "strings.h").read_text(encoding="utf-8")
    literals = re.findall(r'"((?:[^"\\]|\\.)*)"', src)
    text = "".join(literals)
    cps = set()
    for ch in text:
        cps.add(ord(ch))
    # 转义序列展开（\n 等暂未使用，保守处理）
    cps.update(range(0x20, 0x7F))   # 全部可打印 ASCII
    cps.update(range(0x30, 0x3A))   # 数字（双保险）
    cps.add(0xFFFD)
    # 常用全角标点 / 中文常用符号（双保险）
    for ch in "：·「」『』（）！？。，、—…；：《》""''【】":
        cps.add(ord(ch))
    return sorted(c for c in cps if c != 0 and not (0xD800 <= c <= 0xDFFF))


def main():
    cps = collect_codepoints()
    print(f"unique codepoints: {len(cps)}")

    font = ImageFont.truetype(FONT_PATH, SIZE)
    ascent, descent = font.getmetrics()
    ascent_nom = round(ascent / SS)
    descent_nom = round(descent / SS)

    pad = 96  # 画布安全边距（256px 字形 + 距离场外扩）
    canvas_w = SIZE + pad * 2
    canvas_h = ascent + descent + pad * 2
    ascentQ = round(ascent / SS)  # 名义尺度顶线高度（= 头部 ascent 字段）

    glyphs = []  # (cp, left, top, adv_q4, w, h, cell uint8 array)
    for cp in cps:
        ch = chr(cp)
        adv = font.getlength(ch)
        bbox = font.getbbox(ch)  # 相对原点 (0,0)，默认锚 la（y0 距顶线）
        x0, y0, x1, y1 = bbox
        if x1 <= x0 or y1 <= y0:
            # 空字形（空格等）：只存步进
            glyphs.append((cp, 0, 0, int(round(adv / SS * 16)), 0, 0, None))
            continue
        # 名义尺度整数化（4x 画布对齐裁切）
        L = x0 // SS
        x1c = -((-x1) // SS)  # ceil
        y0f = y0 // SS
        y1c = -((-y1) // SS)  # ceil
        w = x1c - L
        h = y1c - y0f
        # 关键：top = 基线轴承（顶线高度 - 字形顶行），渲染端 dyTop = baseline - top*scale
        top = ascentQ - y0f
        if w <= 0 or h <= 0 or w > 120 or h > 120:
            glyphs.append((cp, 0, 0, int(round(adv / SS * 16)), 0, 0, None))
            continue

        img = Image.new("L", (canvas_w, canvas_h), 0)
        draw = ImageDraw.Draw(img)
        draw.text((pad, pad), ch, font=font, fill=255)
        mask = np.asarray(img, dtype=np.uint8) >= 128
        # 有向距离（画布 4x 像素单位）：内部为正
        d_in = distance_transform_edt(mask)
        d_out = distance_transform_edt(~mask)
        signed = np.where(mask, d_in, -d_out).astype(np.float32)

        cx0 = pad + L * SS
        cx1 = pad + x1c * SS
        cy0 = pad + y0f * SS
        cy1 = pad + y1c * SS
        crop = signed[cy0:cy1, cx0:cx1]
        if crop.shape != (h * SS, w * SS):
            # 防御：跳过异常字形
            glyphs.append((cp, 0, 0, int(round(adv / SS * 16)), 0, 0, None))
            continue
        # 块均值下采样 → 名义尺度距离
        cell = crop.reshape(h, SS, w, SS).mean(axis=(1, 3)) / SS
        val = np.clip(128.0 + cell / SPREAD * 127.0, 0, 255).astype(np.uint8)

        glyphs.append((cp, L, top, int(round(adv / SS * 16)), w, h, val))

    # ---- shelf 打包 ----
    solids = [g for g in glyphs if g[6] is not None]
    empties = [g for g in glyphs if g[6] is None]
    solids.sort(key=lambda g: (-g[5], -g[4], g[0]))
    placed = []
    x = y = row_h = 0
    for cp, L, T, adv, w, h, val in solids:
        if x + w > ATLAS_W:
            x = 0
            y += row_h
            row_h = 0
        placed.append((cp, L, T, adv, w, h, val, x, y))
        x += w
        row_h = max(row_h, h)
    atlas_h = y + row_h
    print(f"atlas: {ATLAS_W}x{atlas_h}")

    atlas = np.zeros((atlas_h, ATLAS_W), dtype=np.uint8)
    records = []
    for cp, L, T, adv, w, h, val, ax, ay in placed:
        atlas[ay:ay + h, ax:ax + w] = val
        records.append((cp, ax, ay, w, h, L, T, adv))
    for cp, L, T, adv, w, h, val in empties:
        records.append((cp, 0, 0, 0, 0, 0, 0, adv))
    records.sort(key=lambda r: r[0])
    for i in range(1, len(records)):
        assert records[i][0] > records[i - 1][0], "cp 必须严格递增"

    OUT.parent.mkdir(parents=True, exist_ok=True)
    with open(OUT, "wb") as f:
        f.write(b"MWF1")
        f.write(struct.pack("<I", len(records)))
        f.write(struct.pack("<HHhh", ATLAS_W, atlas_h, ascent_nom, descent_nom))
        f.write(struct.pack("<BHB", SPREAD, NOMINAL, 0))
        for cp, ax, ay, w, h, L, T, adv in records:
            f.write(struct.pack("<IHHHHhhHH", cp, ax, ay, w, h, L, T, adv, 0))
        f.write(atlas.tobytes())
    size = OUT.stat().st_size
    print(f"wrote {OUT} ({size} bytes, {len(records)} glyphs)")

    Image.fromarray(atlas).save(PREVIEW)
    print(f"preview: {PREVIEW}")


if __name__ == "__main__":
    main()
