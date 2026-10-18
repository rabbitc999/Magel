#!/bin/sh
# check.sh —— magel/example/sql-magel 示例工程（学生信息管理系统）冒烟检查。
#
# 做三件事：
#   1. 用 MAGEL 解释器跑一遍脚本化输入（浏览 + 增删改查 + 统计）；
#   2. stdout 与 expected_<名字>.txt 逐行比对（比对前去掉行尾 \r，兼容 CRLF 检出）；
#   3. stderr 里出现「运行期错误」或「提示：」（泄漏提示）就判失败。
#
# 用法：sh magel/example/sql-magel/tests/check.sh   （在仓库根目录或任意位置都行）
#   环境变量 MGCC 可覆盖编译器位置，默认用仓库根的 magel（Windows 为 .exe），也可用 MAGEL_COMPILER。
#   需要本机 MySQL 在跑；缺服务时属于环境未满足，不代表示例失败。

set -u

HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/../../.." && pwd)
MGCC=${MAGEL_COMPILER:-${MGCC:-$ROOT/magel}}
if [ ! -x "$MGCC" ] && [ -x "$MGCC.exe" ]; then MGCC=$MGCC.exe; fi
# 传给编译器 / 程序的路径一律用**相对 ROOT** 的写法：magel 是原生 Windows 程序，
# 不认 MSYS 的 `/d/...` 绝对路径（会报 Cannot open file）。下面的 run() 会先 cd "$ROOT"。
APP=example/sql-magel/example/student/main.mgl

if [ ! -x "$MGCC" ]; then
    echo "找不到编译器 $MGCC（先按 README 构建 Magel，或用 MGCC=... 指定）" >&2
    exit 2
fi

fail=0

run() {
    name=$1
    input=example/sql-magel/tests/input_$name.txt
    expected=$HERE/expected_$name.txt
    actual=$HERE/.actual_$name.txt
    err=$HERE/.stderr_$name.txt

    # `--answers` 从 UTF-8 文件读取脚本化回答，避免回归依赖交互输入。
    ( cd "$ROOT" && "$MGCC" "$APP" -- --answers "$input" ) > "$actual" 2> "$err"
    status=$?

    if [ "$status" -ne 0 ]; then
        echo "FAIL $name：退出码 $status"
        fail=1
    fi
    if grep -q -E '运行期错误|提示：' "$err"; then
        echo "FAIL $name：stderr 有异常输出"
        sed 's/^/    /' "$err"
        fail=1
    fi

    sed 's/\r$//' "$actual" > "$actual.n"
    sed 's/\r$//' "$expected" > "$expected.n"
    if diff -u "$expected.n" "$actual.n" > /dev/null 2>&1; then
        echo "ok   $name"
    else
        echo "FAIL $name：输出与期望不一致"
        diff -u "$expected.n" "$actual.n" | head -40
        fail=1
    fi
    rm -f "$actual" "$actual.n" "$expected.n" "$err"
}

run crud

if [ "$fail" -eq 0 ]; then
    echo "magel/example/sql-magel 检查通过"
else
    echo "magel/example/sql-magel 检查失败"
fi
exit $fail
