#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""检查 doc/ 下 Markdown 页面里的相对链接与图片是否存在。

`doc/` 的中文页与其 `doc/en/` 英文版一一对应，跨页引用一律走**相对路径**
（引用仓库根写 `../`、引用 `doc/examples/` 写 `examples/`、引用 `doc/library/`
写 `library/`）。谁挪了目录或改了文件名忘了改链接，页面上就是一个死链——没人点
就发现不了，这里替人挨个点一遍。

判据：每个 Markdown 链接 / 图片的**目标**（相对当前页解析）必须存在。只看相对
路径；`http(s)://`、`mailto:` 之类外部链接与纯锚点（`#小节`）跳过。锚点本身
（`foo.md#小节`）不校验，只认 `#` 前那截路径。

`doc/book/` 不进这个门禁（它是一本独立的入门书，另有自己的校验脚本），见 `SKIP_DIRS`。

用法：`python tools/check_doc_links.py`（在本项目根目录跑）
     不给参数时扫 `doc/` 下全部 `.md`；也可只查指定页，如
     `python tools/check_doc_links.py doc/README.md doc/en/README.md`。

已接进 `ci.sh` 的文档门禁，有缺失就红。
"""

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

# `[文字](目标)` / `![文字](目标)`：目标可以写成 `<带空格 的路径>`，也可以带 `"标题"`。
LINK = re.compile(r'!?\[[^\]]*\]\(\s*(?:<([^>]*)>|([^)\s]+))(?:\s+"[^"]*")?\s*\)')
# 外部协议与空目标不是相对路径，不归这里管。
SKIP_SCHEMES = ("http://", "https://", "mailto:", "tel:", "data:", "ftp:")

# 归档与独立的目录不进这个门禁：`doc/book/` 是一本独立的入门书，另有自己的校验脚本
# （`tmp/book_check.py`），它的章节链接在书内自成一套。
SKIP_DIRS = ("doc/book",)


def pages(argv):
    """要检查的页面 + 跳过的页数：命令行给了就按给定的来，否则扫 doc/ 下所有 `.md`。"""
    if argv:
        return [(ROOT / arg).resolve() for arg in argv], 0
    found = []
    skipped = 0
    for path in sorted((ROOT / "doc").rglob("*.md")):
        rel = path.relative_to(ROOT).as_posix()
        if any(rel.startswith(name + "/") for name in SKIP_DIRS):
            skipped += 1
            continue
        found.append(path)
    return found, skipped


def target_of(match):
    """取链接目标：`<...>` 那组或裸路径那组，谁匹配上用谁。"""
    for group in match.groups():
        if group is not None:
            return group.strip()
    return ""


def check(page):
    """检查单个页面，返回缺失的 (页面, 目标) 列表。"""
    missing = []
    # 宏的 [..Attrs](..Args) 是代码而非 Markdown 链接；围栏与行内代码不参与链接扫描。
    lines = []
    fence = None
    for line in page.read_text(encoding="utf-8").splitlines():
        marker = re.match(r"^ {0,3}(`{3,}|~{3,})(.*)$", line)
        if fence is not None:
            if marker and marker[1][0] == fence[0] and len(marker[1]) >= len(fence) and not marker[2].strip():
                fence = None
            continue
        if marker:
            fence = marker[1]
            continue
        lines.append(re.sub(r"(`+).*?\1", "", line))
    text = "\n".join(lines)
    for match in LINK.finditer(text):
        target = target_of(match).split("#")[0].strip()
        if not target or target.startswith(SKIP_SCHEMES):
            continue
        if not (page.parent / target).exists():
            missing.append((page, target))
    return missing


def show(path):
    """页面路径按仓库根显示成相对形式，方便直接照抄定位。"""
    return path.relative_to(ROOT).as_posix()


def main():
    to_check, skipped = pages(sys.argv[1:])
    if not to_check:
        raise SystemExit("没有找到要检查的页面")
    bad = []
    for page in to_check:
        bad.extend(check(page))
    for page, target in bad:
        print(f"  缺失：{show(page)} -> {target}")
    if skipped:
        print(f"  跳过未纳入门禁的 {skipped} 页（{'、'.join(SKIP_DIRS)}）")
    print(f"检查完成：{len(to_check)} 页，缺失 {len(bad)} 处")
    if bad:
        raise SystemExit("文档相对链接有缺失，按上面的清单修")
    return 0


if __name__ == "__main__":
    sys.exit(main())
