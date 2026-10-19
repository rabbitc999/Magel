#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""CHANGELOG 的生成与门禁。

`CHANGELOG.md` 由 [`tools/changelog_data.json`](tools/changelog_data.json) 生成：
JSON 里每个版本是一个对象——

```json
{
  "version": "1.0.0",
  "title": "一句话标题",
  "dates": [
    { "date": "2026-10-18",
      "entries": [
        "一条不带子项的变更",
        { "text": "带子项的变更", "items": ["子项一", "子项二"] }
      ] }
  ]
}
```

这个脚本把两件事做成命令：

* `--build` —— 从 JSON 重新生成 `CHANGELOG.md`（改完数据跑这个）。
* `--check` —— 门禁。校验：`CHANGELOG.md` 与 JSON 渲染结果逐字节一致；`VERSION` ↔ 顶级
                节的前三段版本号；版本号唯一且自新到旧递减；每个 `vX.Y.Z` tag ↔ 有对应节。
                任何一条不满足就非零退出。

用法（在项目根目录跑）：

    python tools/changelog.py --build
    python tools/changelog.py --check
"""

import argparse
import json
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DATA = ROOT / "tools" / "changelog_data.json"
CHANGELOG = ROOT / "CHANGELOG.md"
VERSION_FILE = ROOT / "VERSION"

INTRO = (
    f"版本号写在 [`VERSION`](VERSION) 里，格式 `MAJOR.MINOR.PATCH`。当前语言版本为 **{VERSION_FILE.read_text(encoding='utf-8').strip()}**："
    "v1.0.0 是项目冻结并重新初始化 git 后的稳定基线。不兼容变更升主版本，兼容功能新增升次版本，"
    "修复升补丁版本；所有变更均须记入本文件（改 [`tools/changelog_data.json`](tools/changelog_data.json) "
    "后跑 `--build`），并同步更新 [`doc/21-semantics-frozen.md`](doc/21-semantics-frozen.md) 与 "
    "`AGENTS.md`。1.0.0 之前的历史版本只保留一行摘要。")

TAG = re.compile(r"^v(\d+(?:\.\d+)*)$")
VERSION_TEXT = re.compile(r"^\d+(?:\.\d+)*$")


def version_key(text):
    """`0.12.2` / `0.3.2.3` → 整数元组，可直接比大小。"""
    return tuple(int(part) for part in text.split("."))


def read_version():
    text = VERSION_FILE.read_text(encoding="utf-8").strip()
    if not VERSION_TEXT.match(text):
        raise SystemExit(f"读不懂 VERSION 的内容：{text!r}")
    return text


def load_data():
    return json.loads(DATA.read_text(encoding="utf-8"))


def render(data):
    """把 JSON 数据渲染成 CHANGELOG.md 的完整文本。"""
    lines = ["# Magel 变更记录", "", INTRO, ""]
    for ver in data["versions"]:
        lines += ["", f"## {ver['version']} — {ver['title']}", ""]
        for group in ver["dates"]:
            lines += [f"### {group['date']}", ""]
            for entry in group["entries"]:
                if isinstance(entry, str):
                    lines.append(f"- {entry}")
                    continue
                lines.append(f"- {entry['text']}")
                lines += [f"  - {item}" for item in entry.get("items", [])]
            lines.append("")
    return "\n".join(lines).rstrip("\n") + "\n"


def tags():
    """仓库里所有 `vX.Y.Z` 形式的 tag（版本号字符串，不带 v）。"""
    done = subprocess.run(["git", "tag", "--list"], cwd=ROOT, text=True, capture_output=True)
    if done.returncode != 0:
        raise SystemExit(f"git tag 失败：{done.stderr.strip()}")
    found = [TAG.match(name).group(1) for name in done.stdout.splitlines() if TAG.match(name)]
    return sorted(found, key=version_key)


def check():
    problems = []
    data = load_data()
    expected = render(data)

    actual = CHANGELOG.read_text(encoding="utf-8")
    if actual != expected:
        problems.append("CHANGELOG.md 与 tools/changelog_data.json 的渲染结果不一致："
                        "改完 JSON 后跑 `python tools/changelog.py --build`")

    versions = [ver["version"] for ver in data["versions"]]
    if not versions:
        problems.append("JSON 里没有任何版本")

    version = read_version()
    if versions and version_key(versions[0])[:3] != version_key(version)[:3]:
        problems.append(f"顶级节是 {versions[0]}，VERSION 是 {version}：改了 VERSION 就要在 JSON 最上面补一节")

    for older, newer in zip(versions[1:], versions[:-1]):
        if version_key(older) >= version_key(newer):
            problems.append(f"版本顺序不对：{newer} 之后应是更旧的版本，却拍到 {older}")
    seen = set()
    for ver in versions:
        if ver in seen:
            problems.append(f"版本 {ver} 出现了多次，版本号必须唯一")
        seen.add(ver)

    # 每个版本至少一个日期组，每组至少一条条目。
    for ver in data["versions"]:
        if not ver["dates"]:
            problems.append(f"{ver['version']} 节没有任何日期组")
        for group in ver["dates"]:
            if not group["entries"]:
                problems.append(f"{ver['version']} 的 {group['date']} 没有任何条目")

    known = seen
    for tag_version in tags():
        if tag_version not in known:
            problems.append(f"tag v{tag_version} 没有对应的 CHANGELOG 版本")

    if problems:
        print("CHANGELOG 门禁未通过：", file=sys.stderr)
        for problem in problems:
            print(f"  - {problem}", file=sys.stderr)
        return 1
    print(f"CHANGELOG：{len(versions)} 个版本，与 JSON 一致，顶级 {versions[0]} 与 "
          f"VERSION({version}) 一致，{len(tags())} 个 tag 均有记录")
    return 0


def build():
    text = render(load_data())
    CHANGELOG.write_text(text, encoding="utf-8")
    count = text.count("\n## ")
    print(f"已从 tools/changelog_data.json 重新生成 CHANGELOG.md（{count} 个版本）")
    return 0


def main():
    parser = argparse.ArgumentParser(
        prog="tools/changelog.py",
        description="CHANGELOG 的生成与门禁（数据源：tools/changelog_data.json）",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__)
    parser.add_argument("--build", action="store_true",
                        help="从 JSON 重新生成 CHANGELOG.md")
    parser.add_argument("--check", action="store_true",
                        help="门禁：md 与 JSON 一致、VERSION / tag / 版本序对齐")
    args = parser.parse_args()

    if args.check == args.build:
        parser.print_help()
        return 2
    return check() if args.check else build()


if __name__ == "__main__":
    sys.exit(main())
