"""同步 Magel 工程内的共用 UTF-8 控制台头。

master 是本项目自己的 [`library/console/utf8.hpp`](../library/console/utf8.hpp)，逐字节复制到：

  * `source/stackvm_mcu/console/utf8.hpp` —— 板端 MCU 工程（自包含，跟着 magel 一起分发）

这份头最早来自 ParseEngine 的 `library/console/utf8.hpp`；两个项目剥离之后各自独立，
改这边不会自动同步过去（那边有它自己的一份与 `source/include/ParseEngine/tool_console.hpp`）。
改动流程：改 `library/console/utf8.hpp`，再跑本脚本（`gen.sh` 会在构建时自动跑）。
"""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

SOURCE = "library/console/utf8.hpp"

TARGETS = ("source/stackvm_mcu/console/utf8.hpp",)


def main():
    """把源文件逐字节复制到各副本；仅在内容变化时写入，避免无意义的全部重编译。"""
    source = (ROOT / SOURCE).read_bytes()
    for name in TARGETS:
        target = ROOT / name
        if not target.exists() or target.read_bytes() != source:
            target.write_bytes(source)


if __name__ == "__main__":
    main()
