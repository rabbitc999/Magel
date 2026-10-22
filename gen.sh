#!/bin/sh
# 重新生成解析器 / 驱动 / 工程文件（产物统一落在 source/，构建文件仍由仓库维护）。
#
#   sh gen.sh
#
# 文法生成器（ParseEngine）是外部依赖，按下面顺序找（也可用 ENGINE=<路径> 指定）：
#   ./ParseEngine.exe、./ParseEngine             —— 放到本项目根目录（最省事）
#   ../ParseEngine/build/bin/ParseEngine[.exe]   —— 与 ParseEngine 仓库并排时
#   ../build/bin/ParseEngine[.exe]               —— 还住在 ParseEngine 仓库里时
# Windows 未找到时自动下载最新发布版 ParseEngine.exe 到本项目根目录。
set -e
cd "$(dirname "$0")"

# 文法生成器（ParseEngine）在哪：ENGINE=<路径> 显式指定；否则先看**本项目根目录**（默认就
# 找它），再退回同级的 ParseEngine 仓库 build/bin/。生成器是外部依赖：本项目只有 .gram。
if [ -z "${ENGINE:-}" ]; then
    # 两条讲究：① 候选要写成带 `./` 的路径 —— 裸名会被 bash 当成 PATH 查找，
    # 当前目录不在 PATH 里，于是报 `ParseEngine.exe: command not found`；
    # ② Windows 上 `.exe` 排在前面 —— MSYS 的 `test -x ParseEngine` 会因 .exe 后缀假命中。
    for candidate in "./ParseEngine.exe" "./ParseEngine" \
                     "../ParseEngine/build/bin/ParseEngine.exe" "../ParseEngine/build/bin/ParseEngine" \
                     "../build/bin/ParseEngine.exe" "../build/bin/ParseEngine"; do
        if [ -x "$candidate" ]; then
            ENGINE="$candidate"
            break
        fi
    done
fi
if [ -z "${ENGINE:-}" ]; then
    case "${OS:-}:$(uname -s)" in
    Windows_NT:*|*:MINGW*|*:MSYS*|*:CYGWIN*)
        echo "未找到 ParseEngine，正在下载最新发布版……"
        python - <<'PY'
import os
from pathlib import Path
import shutil
import tempfile
import urllib.request

# 发行版下载地址：Gitee 与 GitHub 各一份，按顺序尝试，先成功的那个用。
# 两个远端都发同样的 `latest/ParseEngine.exe`；哪边还没发版就自动落到下一个。解析器生成器只能使用v1.1.0版本的
SOURCES = ["https://gitee.com/rabbitc999/ParseEngine/releases/download/v1.1.0/ParseEngine.exe", "https://github.com/rabbitc999/ParseEngine/releases/download/v1.1.0/ParseEngine.exe"]
temporary = None
try:
    failures = []
    for url in SOURCES:
        try:
            with tempfile.NamedTemporaryFile(prefix=".ParseEngine-", suffix=".download", dir=".", delete=False) as output:
                temporary = Path(output.name)
                with urllib.request.urlopen(url, timeout=60) as response:
                    shutil.copyfileobj(response, output)
            with temporary.open("rb") as downloaded:
                if downloaded.read(2) != b"MZ":
                    raise ValueError("下载内容不是 Windows 可执行文件")
            temporary.chmod(0o755)
            os.replace(temporary, "ParseEngine.exe")
            break
        except Exception as error:
            failures.append(url + "（" + str(error) + "）")
            if temporary is not None:
                temporary.unlink(missing_ok=True)
                temporary = None
    else:
        raise ValueError("；".join(failures))
except Exception as error:
    raise SystemExit("下载 ParseEngine 失败：" + str(error))
finally:
    if temporary is not None:
        temporary.unlink(missing_ok=True)
PY
        ENGINE=./ParseEngine.exe
        ;;
    esac
fi
if [ -z "${ENGINE:-}" ] || [ ! -x "$ENGINE" ]; then
    echo "找不到 ParseEngine 文法生成器（本项目只有 .gram，生成器是外部依赖）。任选一种：" >&2
    echo "  1) 从旁边的 ParseEngine 仓库构建：make -C ../ParseEngine/source -j4" >&2
    echo "     产出 ../ParseEngine/build/bin/ParseEngine[.exe]，之后本脚本会自动找到它" >&2
    echo "  2) 复制到本项目根目录：cp ../ParseEngine/build/bin/ParseEngine.exe ." >&2
    echo "     （PowerShell：Copy-Item ..\ParseEngine\build\bin\ParseEngine.exe .）" >&2
    echo "  3) 显式指路径：ENGINE=/path/to/ParseEngine.exe sh gen.sh" >&2
    exit 2
fi

echo "文法生成器：$ENGINE"

# 先把标准库 .mgl 源码内嵌进编译器（magel_stdlib_embedded.cpp），
# 这样 exe 自包含，import std::xxx 不依赖磁盘上的 .mgl 文件。
python tools/embed_i18n.py
python tools/stackvm_sources.py --build-assets
python tools/cas_sources.py --build-assets
# 手册数据先于内嵌：embed_stdlib.py 会把 magel_manual.json 一起压进内嵌资产表。
python tools/embed_manual.py
python tools/embed_stdlib.py

# --no-build-files：本目录的 Makefile 是手写、纳入版本管理的「生成代码 → 构建」
# 工程文件（见 Makefile / local.mk），不能让生成器覆盖它，也别删它。
# 解析器、驱动、报告与生成清单统一写入 source/，不散落在仓库根目录。
# 词法包含由本项目展开，兼容旧版外置生成器；权威定义仍为 source/magel.gram。
expanded=source/.magel-build.gram
trap 'rm -f "$expanded"' EXIT HUP INT TERM
python tools/expand_grammar.py source/magel.gram "$expanded"

# 增量生成：默认复用持久化的文法分析缓存（--cache-dir 同时启用增量生成），
# 文法没变时省掉 LR/IELR 重算。需要强制全量重算时用 `REBUILD=1 sh gen.sh`。
CACHE_DIR=build/parse-engine-cache
if [ -n "${REBUILD:-}" ]; then
    "$ENGINE" "$expanded" --mode ielr --cpp-lexer std --no-build-files --out-dir source --rebuild
else
    mkdir -p "$CACHE_DIR"
    "$ENGINE" "$expanded" --mode ielr --cpp-lexer std --no-build-files --out-dir source --incremental --cache-dir "$CACHE_DIR"
fi
