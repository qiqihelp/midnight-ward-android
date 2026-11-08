#!/usr/bin/env python3
# gen_icons.py — 生成启动器图标（PNG，全密度；替代旧版 vector XML，兼容所有机型）。
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "res"
SIZES = {  # density: 边长px
    "mdpi": 48,
    "hdpi": 72,
    "xhdpi": 96,
    "xxhdpi": 144,
    "xxxhdpi": 192,
}


def master(size=512):
    img = Image.new("RGB", (size, size), (13, 16, 20))
    d = ImageDraw.Draw(img, "RGBA")

    # 夜空渐变
    for y in range(size):
        t = y / size
        c = (int(10 + 8 * t), int(13 + 10 * t), int(18 + 16 * t))
        d.line([(0, y), (size, y)], fill=c)

    # 稀疏星点
    import random
    rng = random.Random(7)
    for _ in range(40):
        x = rng.randint(0, size - 1)
        y = rng.randint(0, size // 2)
        v = rng.randint(90, 160)
        d.point((x, y), fill=(v, v, v + 20))

    # 血月（残月：大圆减偏移圆）
    moon_r = size * 0.30
    mc = (size * 0.60, size * 0.36)
    d.ellipse([mc[0] - moon_r, mc[1] - moon_r, mc[0] + moon_r, mc[1] + moon_r],
              fill=(214, 74, 61))
    cut = (size * 0.48, size * 0.30)
    d.ellipse([cut[0] - moon_r * 0.95, cut[1] - moon_r * 0.95,
               cut[0] + moon_r * 0.95, cut[1] + moon_r * 0.95],
              fill=(10, 13, 18))

    # 病栋剪影
    g = size * 0.62
    d.rectangle([size * 0.06, g, size * 0.42, size], fill=(5, 6, 8))
    d.rectangle([size * 0.46, g - size * 0.06, size * 0.94, size], fill=(5, 6, 8))
    # 窗光
    for wx in (0.50, 0.60, 0.70, 0.80):
        d.rectangle([size * wx, g + size * 0.02, size * wx + size * 0.045,
                     g + size * 0.10], fill=(226, 178, 92))
    d.rectangle([size * 0.14, g + size * 0.04, size * 0.185, g + size * 0.12],
                fill=(150, 44, 38))

    # 红十字（病栋标志，置于月下）
    cx, cy = size * 0.30, size * 0.245
    arm = size * 0.045
    d.rectangle([cx - arm, cy - arm * 3.4, cx + arm, cy + arm * 3.4],
                fill=(190, 38, 32))
    d.rectangle([cx - arm * 3.4, cy - arm, cx + arm * 3.4, cy + arm],
                fill=(190, 38, 32))

    return img.filter(ImageFilter.GaussianBlur(0.4))


def main():
    img = master()
    for dens, px in SIZES.items():
        d = OUT / f"mipmap-{dens}"
        d.mkdir(parents=True, exist_ok=True)
        img.resize((px, px), Image.LANCZOS).save(d / "ic_launcher.png")
        print(f"res/mipmap-{dens}/ic_launcher.png ({px}px)")
    (OUT / "mipmap-xxxhdpi" / "ic_launcher.png").replace(
        OUT / "mipmap-xxxhdpi" / "ic_launcher.png")
    # 附带一张大图便于预览
    import pathlib
    prev = ROOT / "scripts" / "preview"
    prev.mkdir(parents=True, exist_ok=True)
    img.save(prev / "icon_preview.png")
    print("done")


if __name__ == "__main__":
    main()
