#!/usr/bin/env python3
"""展开权威文法的 %include，兼容尚未内置该指令的外置 ParseEngine。"""
from pathlib import Path
import re
import sys


def expand(path, parents=()):
    """递归展开词法文法；检测循环，保留其它生成器指令。"""
    path = Path(path).resolve()
    if path in parents:
        raise ValueError(f"循环文法包含：{path}")
    text = path.read_text(encoding="utf-8")
    return re.sub(r'^%include\s+"([^"]+)"\s*$',
                  lambda match: expand(path.parent / match[1], parents + (path,)),
                  text, flags=re.MULTILINE)


if __name__ == "__main__":
    Path(sys.argv[2]).write_text(expand(sys.argv[1]), encoding="utf-8")
