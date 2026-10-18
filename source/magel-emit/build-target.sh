#!/bin/sh
# magel/source/magel-emit/build-target.sh —— 一条龙：MAGEL 源码 → AOT 独立可执行文件。
#
# 用法（在仓库根目录）：
#
#   sh magel/source/magel-emit/build-target.sh magel/example/samples/containers.mgl
#   sh magel/source/magel-emit/build-target.sh magel/example/samples/bigint.mgl "123456789012345678901234567890"
#   sh magel/source/magel-emit/build-target.sh <前缀>.quad          # 已经有 .quad 的情况
#
# 做了什么：
#   1. `.mgl` 先过 `magel --emit` 出 `<前缀>.quad` / `.data.bin` / `.layout`；
#   2. `magel-emit` 将 MAGEL 函数翻译为 C++ 基本块，调用宿主编译器生成 AOT 程序。
#
# 产物落在 `tmp/emit/`（仓库里已 gitignore 的临时目录；用 OUT_DIR=… 改）。

set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
cd "$ROOT_DIR"

if [ $# -lt 1 ]; then
    echo "用法：sh magel/source/magel-emit/build-target.sh <源.mgl 或 前缀.quad> [程序输入…]" >&2
    exit 2
fi

SOURCE=$1
shift

OUT_DIR=${OUT_DIR:-tmp/emit}
mkdir -p "$OUT_DIR"

DEMO=${DEMO:-magel/magel}
GENERATOR=${GENERATOR:-build/magel-emit/magel-emit}
CXX=${CXX:-g++}
THREAD_FLAGS=""
[ "${THREADS:-0}" = 1 ] && THREAD_FLAGS="--threads"

case "$SOURCE" in
    *.mgl)
        NAME=$(basename "$SOURCE" .mgl)
        PREFIX="$OUT_DIR/$NAME"
        echo "[1/2] 编译源码：$SOURCE → $PREFIX.quad"
        "$DEMO" run "$SOURCE" --emit --out "$PREFIX" $THREAD_FLAGS >/dev/null
        ;;
    *.quad)
        PREFIX=${SOURCE%.quad}
        NAME=$(basename "$PREFIX")
        ;;
    *)
        PREFIX=$SOURCE
        NAME=$(basename "$PREFIX")
        ;;
esac

QUAD="$PREFIX.quad"
DATA="$PREFIX.data.bin"
LAYOUT="$PREFIX.layout"
[ -f "$QUAD" ] || { echo "找不到 $QUAD" >&2; exit 2; }

EXTRA=""
[ -f "$DATA" ] && EXTRA="$EXTRA --data $DATA"
[ -f "$LAYOUT" ] && EXTRA="$EXTRA --layout $LAYOUT"

TARGET_EXE="$OUT_DIR/$NAME"
echo "[2/2] AOT 编译：$QUAD → $TARGET_EXE"
# shellcheck disable=SC2086
"$GENERATOR" --target cpp-aot --quad "$QUAD" $EXTRA $THREAD_FLAGS --cxx "$CXX" --out "$TARGET_EXE"
echo "产物：$TARGET_EXE（自包含 AOT 可执行文件）"
