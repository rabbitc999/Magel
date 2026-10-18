#!/usr/bin/env python3
"""Qt GUI 后端的无头端到端检查（Linux，QT_QPA_PLATFORM=offscreen）。

覆盖：
  * `libmcguiqt.so` 构建；
  * `counter.mgl` 的 `close` 分支（控件创建 + UTF-8 文本往返 + 关窗）；
  * `qt_interrupt.mgl` 的事件回调链：Qt 周期事件 / 注入事件 → 胶水桥 →
    `Machine::raiseInterrupt` → 指令边界交付 MAGEL 中断处理函数；
  * `gui::pumpEvery(n)`：长循环里每 N 条指令泵一次 Qt。

用法：
    python magel/example/gui/test_qt.py            # 用默认临时目录 build/magel-gui-qt
    python magel/example/gui/test_qt.py <目录>

需要 Qt5 Widgets、g++、已构建的 `magel/magel`。
`MAGEL_DEMO` 环境变量可覆盖编译器路径。
"""

from __future__ import annotations

import os
from pathlib import Path
import shutil
import subprocess
import sys


def run(command: list[str], cwd: Path, env: dict[str, str] | None = None) -> None:
    result = subprocess.run(command, cwd=cwd, env=env, capture_output=True, text=True)
    if result.returncode != 0:
        raise SystemExit(
            f"命令失败（{result.returncode}）：{' '.join(command)}\n"
            f"stdout:\n{result.stdout}\nstderr:\n{result.stderr}"
        )


def main() -> None:
    if os.name == "nt":
        raise SystemExit("这个无头测试面向 Linux/macOS；Windows 用 tests/magel_desktop.py")

    gui_dir = Path(__file__).resolve().parent
    repo = gui_dir.parents[2]
    out = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else repo / "build" / "magel-gui-qt"
    out.mkdir(parents=True, exist_ok=True)

    demo = os.environ.get("MAGEL_DEMO", str(repo / "magel" / "magel"))
    if not Path(demo).is_file():
        raise SystemExit(f"找不到 {demo}；先构建 Magel 或设置 MAGEL_DEMO")

    # 1) 构建 Qt 共享库。
    run(["sh", str(gui_dir / "qt" / "build.sh"), str(out)], cwd=repo)

    env = dict(os.environ)
    env["QT_QPA_PLATFORM"] = "offscreen"

    def build_example(source: Path, name: str) -> Path:
        binary = out / name
        run([demo, "aot", str(source), "-I", str(gui_dir),
             "--glue", str(gui_dir / "glue.cpp"), "--out", str(binary),
             "--opt-level=0"], cwd=repo)
        return binary

    # 2) counter.mgl 的 close 分支：UTF-8 文本往返。
    counter = build_example(gui_dir / "examples" / "counter.mgl", "counter")
    result = subprocess.run([str(counter), "close"], cwd=out, env=env,
                            capture_output=True, text=True)
    assert result.returncode == 0, (result.stdout, result.stderr)
    first = result.stdout.splitlines()[0] if result.stdout.splitlines() else ""
    assert first == "你好，Magel", f"counter close 输出不对：{result.stdout!r}"

    # 3) qt_interrupt.mgl：事件回调 + 每 N 条指令泵一次。
    interrupt = build_example(gui_dir / "examples" / "qt_interrupt.mgl", "qt_interrupt")
    result = subprocess.run([str(interrupt)], cwd=out, env=env,
                            capture_output=True, text=True)
    assert result.returncode == 0, (result.stdout, result.stderr)
    line = result.stdout.strip().splitlines()[-1] if result.stdout.strip() else ""
    assert line == "clicks=1 ticks=1 closed=1", f"qt_interrupt 输出不对：{result.stdout!r}"

    print("Qt GUI 事件回调检查通过")


if __name__ == "__main__":
    main()
