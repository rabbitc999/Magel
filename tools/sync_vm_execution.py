"""检查 VM 已统一到 stackvm；兼容原有 --check 门禁入口，不再同步副本。"""
from pathlib import Path
import argparse

ROOT = Path(__file__).resolve().parents[1]


def main():
    """禁止恢复板端核心副本，检查两种构建都引用唯一源码。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true")
    parser.parse_args()
    duplicate = ROOT / "source/stackvm_mcu/vm"
    if duplicate.exists():
        raise SystemExit("禁止维护第二份 VM：source/stackvm_mcu/vm 应已移除")
    for file in ("source/stackvm_mcu/Makefile", "source/stackvm_mcu/CMakeLists.txt"):
        if "library/stackvm" not in (ROOT / file).read_text(encoding="utf-8"):
            raise SystemExit("板端构建没有引用唯一 VM 源码：" + file)
    print("VM 核心唯一，桌面及板端均引用 library/stackvm")


if __name__ == "__main__":
    main()
