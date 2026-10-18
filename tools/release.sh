#!/bin/sh
# 统一发布入口；具体参数使用 --help 查看。
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$root"
if command -v python3 >/dev/null 2>&1 && python3 -c 'import sys; sys.exit(sys.version_info < (3, 11))' 2>/dev/null; then
    exec python3 tools/release.py "$@"
fi
exec python tools/release.py "$@"
