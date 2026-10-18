#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""生成 Magel 的语言图标（魔术棒 + 「C」+「Value」）。

产物（都重新生成，随时可重跑）：
  * `doc/images/magel-logo.png` —— 512×512，语言自己的标志（README / 文档用）
  * `vscode/icon.png`           —— 256×256，VS Code 插件图标（市场要求 ≥128×128）
  * `doc/images/magel.ico`      —— 16/24/32/48/64/128/256 多尺寸，Windows 资源用

画法：先在 3 倍逻辑画布上画（1536×1536），再用 LANCZOS 降采样，
这样细笔画与小字号在 128 尺寸下也不糊。只用标准库 + Pillow。
"""

from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter, ImageFont

ROOT = Path(__file__).resolve().parents[1]

SIZE = 512          # 逻辑尺寸
SS = 3              # 超采样倍数
W = SIZE * SS

BG_TOP = (27, 43, 75)        # 顶部深靛蓝
BG_BOTTOM = (13, 22, 40)     # 底部近黑蓝
BG_EDGE = (52, 84, 128)
STICK_DARK = (86, 70, 44)
STICK_LIGHT = (247, 243, 232)
GOLD = (255, 209, 102)
WHITE = (255, 255, 255)
TEXT = (244, 247, 251)

# 字体按平台挑一份：Windows 用 Segoe UI，Linux / macOS 退回常见等价物。
FONT_CANDIDATES = [
    r"C:\Windows\Fonts\segoeuib.ttf",
    r"C:\Windows\Fonts\arialbd.ttf",
    "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
    "/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf",
    "/System/Library/Fonts/Supplemental/Arial Bold.ttf",
]


def pick_font():
    """挑一份可用的粗体中文字体（找不到就返回 None，交回 PIL 默认字体）。"""
    for candidate in FONT_CANDIDATES:
        if Path(candidate).is_file():
            return candidate
    return None


FONT_BOLD = pick_font()

# Windows 资源的常用档位（PIL 会从 512 那张逐档降采样）。
ICO_SIZES = [(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)]


def scaled(value):
    """逻辑坐标 → 画布像素。"""
    return int(round(value * SS))


def rounded_background(canvas):
    """圆角方块 + 竖向渐变 + 一圈描边。"""
    gradient = Image.new("RGB", (1, HEIGHT := canvas.height), BG_TOP)
    for y in range(HEIGHT):
        t = y / max(HEIGHT - 1, 1)
        gradient.putpixel((0, y), tuple(
            int(round(BG_TOP[i] + (BG_BOTTOM[i] - BG_TOP[i]) * t)) for i in range(3)))
    gradient = gradient.resize((canvas.width, HEIGHT))

    mask = Image.new("L", (canvas.width, HEIGHT), 0)
    ImageDraw.Draw(mask).rounded_rectangle(
        [0, 0, canvas.width - 1, HEIGHT - 1], radius=scaled(96), fill=255)
    canvas.paste(gradient, (0, 0), mask)
    ImageDraw.Draw(canvas).rounded_rectangle(
        [scaled(3), scaled(3), canvas.width - scaled(4), HEIGHT - scaled(4)],
        radius=scaled(93), outline=BG_EDGE, width=scaled(2))


def sparkle(draw, cx, cy, radius, color, core=WHITE):
    """四角星（魔术棒的火花）。"""
    cx, cy, r = scaled(cx), scaled(cy), scaled(radius)
    thin = max(int(r * 0.26), 1)
    draw.polygon([(cx, cy - r), (cx + thin, cy), (cx, cy + r), (cx - thin, cy)], fill=color)
    draw.polygon([(cx - r, cy), (cx, cy - thin), (cx + r, cy), (cx, cy + thin)], fill=color)
    draw.ellipse([cx - thin, cy - thin, cx + thin, cy + thin], fill=core)


def magic_wand(canvas):
    """斜放的魔术棒：描边 + 亮芯 + 端点火花。"""
    draw = ImageDraw.Draw(canvas)
    start, end = (74, 448), (246, 276)
    for color, width in ((STICK_DARK, 30), (STICK_LIGHT, 21)):
        draw.line([scaled(start[0]), scaled(start[1]), scaled(end[0]), scaled(end[1])],
                  fill=color, width=scaled(width))
        for point in (start, end):
            draw.ellipse([scaled(point[0]) - scaled(width) // 2, scaled(point[1]) - scaled(width) // 2,
                          scaled(point[0]) + scaled(width) // 2, scaled(point[1]) + scaled(width) // 2],
                         fill=color)
    glow = Image.new("RGBA", canvas.size, (0, 0, 0, 0))
    sparkle(ImageDraw.Draw(glow), 268, 250, 50, GOLD + (255,))
    glow = glow.filter(ImageFilter.GaussianBlur(scaled(10)))
    canvas.alpha_composite(glow)
    sparkle(draw, 268, 250, 46, GOLD)
    sparkle(draw, 344, 168, 18, GOLD)
    sparkle(draw, 196, 172, 13, GOLD)
    draw.ellipse([scaled(178) - scaled(5), scaled(112) - scaled(5),
                  scaled(178) + scaled(5), scaled(112) + scaled(5)], fill=GOLD)
    draw.ellipse([scaled(324) - scaled(4), scaled(292) - scaled(4),
                  scaled(324) + scaled(4), scaled(292) + scaled(4)], fill=GOLD)


def wordmark(canvas):
    """右侧的「C」与它下方的「Value」。"""
    draw = ImageDraw.Draw(canvas)
    big = ImageFont.truetype(FONT_BOLD, scaled(204))
    small = ImageFont.truetype(FONT_BOLD, scaled(64))

    box = draw.textbbox((0, 0), "C", font=big)
    draw.text((scaled(402) - (box[2] - box[0]) / 2 - box[0], scaled(242) - (box[3] - box[1]) / 2 - box[1]),
              "C", font=big, fill=TEXT)

    box = draw.textbbox((0, 0), "Value", font=small)
    draw.text((scaled(398) - (box[2] - box[0]) / 2 - box[0], scaled(384) - (box[3] - box[1]) / 2 - box[1]),
              "Value", font=small, fill=GOLD)


def build():
    """画一张逻辑尺寸的大图，返回 512 与 256 两个成品。"""
    canvas = Image.new("RGBA", (W, W), (0, 0, 0, 0))
    rounded_background(canvas)
    magic_wand(canvas)
    wordmark(canvas)
    logo = canvas.resize((SIZE, SIZE), Image.LANCZOS)
    icon = canvas.resize((SIZE // 2, SIZE // 2), Image.LANCZOS)
    return logo, icon


def main():
    logo, icon = build()
    images = ROOT / "doc" / "images"
    images.mkdir(parents=True, exist_ok=True)
    logo_path = images / "magel-logo.png"
    icon_path = ROOT / "vscode" / "icon.png"
    ico_path = images / "magel.ico"
    logo.save(logo_path)
    icon.save(icon_path)
    logo.save(ico_path, sizes=ICO_SIZES)
    print(f"{logo_path.relative_to(ROOT)}  {logo.size[0]}×{logo.size[1]}")
    print(f"{icon_path.relative_to(ROOT)}  {icon.size[0]}×{icon.size[1]}")
    print(f"{ico_path.relative_to(ROOT)}  " + " / ".join(str(size[0]) for size in ICO_SIZES))


if __name__ == "__main__":
    main()

