#!/usr/bin/env python3
"""Qt GUI 后端的无头端到端检查（Linux，QT_QPA_PLATFORM=offscreen）。

覆盖：
  * `libmcguiqt.so` 构建；
  * `counter.mgl` 的 `close` 分支（控件创建 + UTF-8 文本往返 + 关窗）；
  * `qt_events.mgl` 的事件回调链：Qt 周期事件 / 注入事件 → 胶水桥 →
    `EventPoster` → MAGEL 显式接收事件；
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
    repo = gui_dir.parents[1]
    out = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else repo / "build" / "magel-gui-qt"
    out.mkdir(parents=True, exist_ok=True)

    demo = os.environ.get("MAGEL_DEMO", str(repo / "magel"))
    if not Path(demo).is_file():
        raise SystemExit(f"找不到 {demo}；先构建 Magel 或设置 MAGEL_DEMO")

    # 1) 构建 Qt 共享库。
    run(["sh", str(gui_dir / "qt" / "build.sh"), str(out)], cwd=repo)

    env = dict(os.environ)
    env["QT_QPA_PLATFORM"] = "offscreen"

    host_source = out / "vm_host.cpp"
    host_source.write_text((repo / "example/native-callbacks/vm_host.cpp").read_text(encoding="utf-8").replace('#include "glue.cpp"', '#include "' + (gui_dir / "glue.cpp").as_posix() + '"'), encoding="utf-8")
    host = out / "vm-host"
    run([os.environ.get("CXX", "g++"), "-std=c++17", "-O2", "-pthread",
         "-DMAGEL_WITH_REFLECTION=1", "-DSTACKVM_WITH_REFLECTION=1",
         "-DSTACKVM_WITH_BIGINT=0", "-DSTACKVM_WITH_BIGFLOAT=0",
         "-Isource", "-Ilibrary", "-Ilibrary/stackvm", str(host_source),
         *[str(repo / "library/stackvm" / name) for name in (repo / "library/stackvm/core_sources.txt").read_text().splitlines()],
         "source/glue/std_collection.cpp", "source/glue/std_collection_storage.cpp",
         "source/glue/std_native_record.cpp", "-ldl", "-o", str(host)], cwd=repo)

    def build_example(source: Path, name: str) -> list[str]:
        prefix = out / name
        run([demo, "emit", str(source), "-I", str(gui_dir),
             "--svmb", "--out", str(prefix)], cwd=repo)
        return [str(host), str(prefix.with_suffix(".svmb")),
                str(prefix.with_suffix(".data.bin")), str(prefix.with_suffix(".layout"))]

    # 2) counter.mgl 的 close 分支：UTF-8 文本往返。
    counter = build_example(gui_dir / "examples" / "counter.mgl", "counter")
    result = subprocess.run([*counter, "close"], cwd=out, env=env,
                            capture_output=True, text=True)
    assert result.returncode == 0, (result.stdout, result.stderr)
    first = result.stdout.splitlines()[0] if result.stdout.splitlines() else ""
    assert first == "你好，Magel", f"counter close 输出不对：{result.stdout!r}"

    # 3) qt_events.mgl：事件投递和显式接收。
    events = build_example(gui_dir / "examples" / "qt_events.mgl", "qt_events")
    result = subprocess.run(events, cwd=out, env=env,
                            capture_output=True, text=True)
    assert result.returncode == 0, (result.stdout, result.stderr)
    line = result.stdout.strip().splitlines()[-1] if result.stdout.strip() else ""
    assert line == "clicks=1 ticks=1 closed=1", f"qt_events 输出不对：{result.stdout!r}"

    print("Qt GUI 显式事件检查通过")


if __name__ == "__main__":
    main()
