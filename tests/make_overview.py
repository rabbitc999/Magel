"""Magel 总览图回归：`tools/make_overview.py` 能重跑，且两次生成逐字节一致。

用法：`python tests/make_overview.py`（在本项目根目录跑）。
"""

from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def main():
    """跑两遍生成器，比对 SVG 字节与 PNG 签名。"""
    script = ROOT / "tools" / "make_overview.py"
    svg = ROOT / "doc" / "overview" / "Magel-overview.svg"
    png = ROOT / "doc" / "overview" / "Magel-overview.png"
    first = None
    for round_index in (0, 1):
        subprocess.run([sys.executable, str(script)], cwd=ROOT, check=True)
        if not svg.is_file() or not png.is_file():
            raise SystemExit(f"生成器没有产出 {svg.name} / {png.name}")
        if svg.stat().st_size == 0 or "<svg" not in svg.read_text(encoding="utf-8")[:2000]:
            raise SystemExit(f"{svg.name} 不像 SVG")
        if png.read_bytes()[:8] != b"\x89PNG\r\n\x1a\n":
            raise SystemExit(f"{png.name} 不是 PNG")
        if round_index == 0:
            first = svg.read_bytes()
        elif svg.read_bytes() != first:
            raise SystemExit("总览图不确定：两次生成的 SVG 不一致")
    print("Magel 总览图回归通过（重复生成逐字节一致）")
    return 0


if __name__ == "__main__":
    sys.exit(main())
