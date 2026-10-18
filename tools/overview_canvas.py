#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""总览图的画布与主题：Magel 工程自带一份，绘制不依赖外部仓库。

只用标准库 + Pillow；画的是确定性的示意图，随时可以重新生成：

    python tools/make_overview.py

这份文件是 ParseEngine 仓库 `tools/make_overview_image.py` 里"画布 + 字体 + 配色"那段的
副本（跟 `library/json/`、`library/toml/` 一样：Magel 独立成项目后，引用到的东西各自拷一份）。外面那份
继续服务别的总览图，改主题时两边各自改。

导出：`Canvas`（圆角块 / 文本 + SVG / PNG 落盘）、`font()` 与整套配色常量。
"""


from __future__ import annotations

import os
import sys

from PIL import Image, ImageDraw, ImageFont

# 逻辑画布（所有坐标都按这个尺寸写），实际按 SCALE 倍画、最后缩到 OUT_SCALE 倍导出。
# 默认 4 倍画 + 3 倍导出：PNG 是 4680x2700，字形按 4 倍渲染再降采样，
# 放大看也清楚；想要更大就把 SE_IMAGE_SCALE / SE_IMAGE_OUT_SCALE 调高。
WIDTH, HEIGHT = 1560, 900
SCALE = int(os.environ.get("SE_IMAGE_SCALE", "4"))
OUT_SCALE = int(os.environ.get("SE_IMAGE_OUT_SCALE", "3"))
OUT_WIDTH, OUT_HEIGHT = WIDTH * OUT_SCALE, HEIGHT * OUT_SCALE

FONT_DIR = os.path.join(os.environ.get("WINDIR", r"C:\Windows"), "Fonts")


def _pick_font(candidates):
    """按顺序取第一个存在的字体，返回 (路径, 字体集合内的序号)。

    Windows 优先用微软雅黑（原图口径）；Linux / macOS 回退到 Noto Sans CJK SC /
    苹方，换平台也能重新生成同一张图（字形略有出入属正常）。
    """
    for path, index in candidates:
        if os.path.exists(path):
            return path, index
    return candidates[0]


# 正文无衬线字体：微软雅黑 → Noto Sans CJK SC（.ttc 里 SC 是第 2 个）→ 苹方。
FONT_SANS, FONT_SANS_INDEX = _pick_font((
    (os.path.join(FONT_DIR, "msyh.ttc"), 0),
    ("/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc", 2),
    ("/usr/share/fonts/SourceHanSansSC-Regular-2.otf", 0),
    ("/System/Library/Fonts/PingFang.ttc", 0),
))
FONT_SANS_BOLD, FONT_SANS_BOLD_INDEX = _pick_font((
    (os.path.join(FONT_DIR, "msyhbd.ttc"), 0),
    ("/usr/share/fonts/opentype/noto/NotoSansCJK-Bold.ttc", 2),
    ("/System/Library/Fonts/PingFang.ttc", 0),
))

# 字体路径 → .ttc 集合序号，供 font() 拼装时查。
_FONT_INDEX = {FONT_SANS: FONT_SANS_INDEX, FONT_SANS_BOLD: FONT_SANS_BOLD_INDEX}

# 颜色（与 README 的浅色主题一致）
BG = "#f4f7fb"
PANEL = "#ffffff"
PANEL_EDGE = "#d8e1ee"
CARD = "#f7f9fc"
CARD_EDGE = "#e2e9f3"
TITLE = "#10233f"
SUBTITLE = "#4a5b73"
PTITLE = "#14507e"
CTITLE = "#33475f"
CODE = "#22405f"
NOTE = "#55677f"
STEP_FILL = "#f4f8fd"
STEP_EDGE = "#cfe0f3"
STEP_TEXT = "#16405f"
CHIP_FILL = "#eef4fb"
CHIP_EDGE = "#c9dcf0"
CHIP_TEXT = "#1d4f7a"
LANG_FILL = "#eaf6ef"
LANG_EDGE = "#bfe0cc"
LANG_TEXT = "#1e6b46"
BADGE = "#14507e"
ARROW = "#8ea2bb"
FOOT = "#64748b"

# 四栏统一版式
COL_X = [40, 410, 780, 1150]   # 每栏左边界
COL_W = 360                    # 每栏宽度
COL_TOP = 118                  # 每栏顶部
COL_H = 700                    # 每栏高度


def font(path: str, size: int) -> ImageFont.FreeTypeFont:
    return ImageFont.truetype(path, int(size * SCALE), index=_FONT_INDEX.get(path, 0))


class Canvas:
    """按 1 倍坐标写、实际按 SCALE 倍画；同时记录 SVG 元素。"""

    def __init__(self) -> None:
        self.image = Image.new("RGB", (WIDTH * SCALE, HEIGHT * SCALE), BG)
        self.draw = ImageDraw.Draw(self.image)
        self.svg = []
        self.texts = []          # (内容, 字体) —— 生成后拿来自查字形

    def rect(self, x, y, w, h, fill, edge=None, radius=0, width=1):
        box = [x * SCALE, y * SCALE, (x + w) * SCALE, (y + h) * SCALE]
        if radius:
            self.draw.rounded_rectangle(box, radius=radius * SCALE, fill=fill,
                                        outline=edge, width=int(width * SCALE))
        else:
            self.draw.rectangle(box, fill=fill, outline=edge,
                                width=int(width * SCALE))
        self.svg.append(
            '<rect x="%g" y="%g" width="%g" height="%g"%s fill="%s"%s/>' % (
                x, y, w, h,
                ' rx="%g"' % radius if radius else "",
                fill,
                ' stroke="%s" stroke-width="%g"' % (edge, width) if edge else ""))

    def text(self, x, baseline, content, font_, fill):
        """按「基线」定位画文字。"""
        ascent, _descent = font_.getmetrics()
        self.draw.text((x * SCALE, baseline * SCALE - ascent), content,
                       font=font_, fill=fill)
        self.texts.append((content, font_))
        self.svg.append(self._svg_text(x, baseline, content, font_, fill))

    def text_center(self, cx, cy, content, font_, fill):
        self.draw.text((cx * SCALE, cy * SCALE), content, font=font_, fill=fill,
                       anchor="mm")
        self.texts.append((content, font_))
        self.svg.append(self._svg_text(cx, cy, content, font_, fill,
                                       anchor="middle", central=True))

    def arrow(self, x1, y1, x2, y2, color=ARROW, width=2, head=7):
        self.draw.line([x1 * SCALE, y1 * SCALE, x2 * SCALE, y2 * SCALE],
                       fill=color, width=int(width * SCALE))
        self.draw.polygon([(x2 * SCALE, y2 * SCALE),
                           ((x2 - head) * SCALE, (y2 - head * 0.5) * SCALE),
                           ((x2 - head) * SCALE, (y2 + head * 0.5) * SCALE)],
                          fill=color)
        self.svg.append('<line x1="%g" y1="%g" x2="%g" y2="%g" stroke="%s" '
                        'stroke-width="%g"/>' % (x1, y1, x2, y2, color, width))
        self.svg.append('<polygon points="%g,%g %g,%g %g,%g" fill="%s"/>' % (
            x2, y2, x2 - head, y2 - head * 0.5, x2 - head, y2 + head * 0.5, color))

    def _svg_text(self, x, y, content, font_, fill, anchor=None, central=False):
        family = ("Consolas, monospace"
                  if font_.path.endswith(("consola.ttf", "consolab.ttf"))
                  else "Microsoft YaHei, Segoe UI, Noto Sans SC, sans-serif")
        weight = (' font-weight="700"'
                  if "bd" in font_.path or "Bold" in font_.path else "")
        extra = ""
        if anchor:
            extra += ' text-anchor="%s"' % anchor
        if central:
            extra += ' dominant-baseline="central"'
        return ('<text x="%g" y="%g" font-family="%s" font-size="%g"%s fill="%s"%s>'
                '%s</text>' % (x, y, family, font_.size / SCALE, weight, fill, extra,
                               _escape(content)))

    # ---------- 字形自查：缺字 / 全是豆腐块时直接报错 ----------

    def check_glyphs(self) -> None:
        seen = {}
        for content, font_ in self.texts:
            for ch in content:
                if ch.isspace():
                    continue
                mask = font_.getmask(ch)
                if mask.getbbox() is None:
                    sys.exit("字形缺失：字体 %s 画不出 %r" % (font_.path, ch))
                key = (font_.path, ch)
                if key in seen:
                    continue
                seen[key] = (hash(mask.tobytes()) if hasattr(mask, "tobytes")
                             else hash(bytes(mask)))
        # 豆腐块的特征：不同汉字渲染出完全相同的位图
        by_font = {}
        for (path, ch), digest in seen.items():
            by_font.setdefault(path, {}).setdefault(digest, []).append(ch)
        for path, groups in by_font.items():
            for chars in groups.values():
                if len(chars) > 1:
                    sys.exit("字形可疑（像是豆腐块）：字体 %s 下 %s 渲染结果相同"
                             % (path, "".join(sorted(chars))))

    def save(self, png_path: str, svg_path: str) -> None:
        self.check_glyphs()
        final = self.image.resize((OUT_WIDTH, OUT_HEIGHT), Image.LANCZOS)
        final.save(png_path)
        svg = ('<svg xmlns="http://www.w3.org/2000/svg" width="%d" height="%d" '
               'viewBox="0 0 %d %d">\n  <rect width="%d" height="%d" fill="%s"/>\n'
               '  %s\n</svg>\n' % (WIDTH, HEIGHT, WIDTH, HEIGHT, WIDTH, HEIGHT, BG,
                                   "\n  ".join(self.svg)))
        with open(svg_path, "w", encoding="utf-8", newline="\n") as handle:
            handle.write(svg)


def _escape(text: str) -> str:
    return (text.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;"))
