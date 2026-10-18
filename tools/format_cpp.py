#!/usr/bin/env python3
"""按根目录 .clang-format 整理受版本控制的 C/C++ 源码，支持只读检查。"""

import argparse
import os
from pathlib import Path
import re
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
EXTENSIONS = {".c", ".cc", ".cpp", ".cxx", ".h", ".hpp", ".hxx", ".inc"}


def source_files():
    """列出格式化目标；具体排除范围由 .clang-format-ignore 统一维护。"""
    names = subprocess.check_output(
        ["git", "ls-files", "-z"], cwd=ROOT
    ).decode("utf-8").split("\0")
    return [name for name in names if Path(name).suffix in EXTENSIONS]


def main():
    """执行格式化或检查，透传 clang-format 的失败状态。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="只检查，不改写文件")
    args = parser.parse_args()
    executable = shutil.which(os.environ.get("CLANG_FORMAT", "clang-format"))
    if not executable:
        parser.error("找不到 clang-format；请安装 LLVM 或通过 CLANG_FORMAT 指定路径")
    version = subprocess.check_output([executable, "--version"], text=True)
    major = re.search(r"\bversion\s+(\d+)", version)
    if major is None or int(major.group(1)) < 19:
        parser.error("需要 clang-format 19 或更新版本，以正确应用 .clang-format-ignore")
    files = source_files()
    flags = ["--style=file", "--fallback-style=none"]
    flags += ["--dry-run", "--Werror"] if args.check else ["-i"]
    # 分批传参，兼容 Windows 的命令行长度限制。
    for start in range(0, len(files), 30):
        result = subprocess.run([executable, *flags, *files[start:start + 30]], cwd=ROOT)
        if result.returncode:
            raise SystemExit(result.returncode)
    print(f"C/C++ 格式{'检查' if args.check else '整理'}通过（排除范围见 .clang-format-ignore）")


if __name__ == "__main__":
    main()
