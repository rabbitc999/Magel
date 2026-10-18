#!/bin/sh
# 构建 Qt GUI 后端的共享库 libmcguiqt.so。
#
#   sh magel/example/gui/qt/build.sh [输出目录]
#
# 默认输出到 <仓库>/build/magel-gui-qt。Qt5 Widgets 用 pkg-config 找；没装 Qt 时给出提示。
set -eu

here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../../.." && pwd)
out=${1:-"$root/build/magel-gui-qt"}
mkdir -p "$out"

if ! pkg-config --exists Qt5Widgets 2>/dev/null; then
    echo "找不到 Qt5Widgets；请先安装 Qt5 开发包（Debian/Ubuntu：qtbase5-dev）" >&2
    exit 1
fi

# shellcheck disable=SC2046
g++ -std=c++17 -O2 -Wall -Wextra -fPIC -shared \
    $(pkg-config --cflags Qt5Widgets) \
    "$here/gui_qt.cpp" -o "$out/libmcguiqt.so" \
    $(pkg-config --libs Qt5Widgets)

echo "已生成 $out/libmcguiqt.so"
