#!/bin/sh
# check-ipage.sh —— magel/example/sql-magel/example/ipage-student 冒烟检查。
#
# 与 check.sh 的区别：这个示例操作的是**外部工程的数据**（表结构与初始数据来自
# ipage-student/student.sql），所以自检**不碰真实库**，而是在独立测试库 `student_check`
# 里用固定夹具 `fixture_ipage.sql` 重建一遍，再用 `--reinit` 强制重导入：
# 这样自增主键每次从 1 开始，回答脚本里写死的 ID 才有确定含义。
#
# 做三件事：
#   1. 重建测试库夹具，跑一遍脚本化输入（登录 + 五个模块的增删改查 + 权限与校验分支）；
#   2. stdout 与 expected_ipage.txt 逐行比对（比对前去掉行尾 \r，兼容 CRLF 检出）；
#   3. stderr 里出现「运行期错误」或「提示：」（泄漏提示）就判失败。
#
# 用法：sh magel/example/sql-magel/tests/check-ipage.sh
#   环境变量 MGCC / MAGEL_COMPILER 可覆盖编译器位置，默认用仓库根的 magel（Windows 为 .exe）。
#   需要本机 MySQL 在跑；缺服务时属于环境未满足，不代表示例失败。

set -u

HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/../../.." && pwd)
MGCC=${MAGEL_COMPILER:-${MGCC:-$ROOT/magel}}
if [ ! -x "$MGCC" ] && [ -x "$MGCC.exe" ]; then MGCC=$MGCC.exe; fi
# 传给编译器 / 程序的路径一律用**相对 ROOT** 的写法：magel 是原生 Windows 程序，
# 不认 MSYS 的 `/d/...` 绝对路径（会报 Cannot open file）。下面先 cd "$ROOT"。
APP=example/sql-magel/example/ipage-student/main.mgl
INPUT=sql-magel/tests/input_ipage.txt
FIXTURE=example/sql-magel/tests/fixture_ipage.sql
TESTDB=student_check

if [ ! -x "$MGCC" ]; then
    echo "找不到编译器 $MGCC（先按 README 构建 Magel，或用 MGCC=... 指定）" >&2
    exit 2
fi

actual=$HERE/.actual_ipage.txt
expected=$HERE/expected_ipage.txt
err=$HERE/.stderr_ipage.txt

( cd "$ROOT" && "$MGCC" "$APP" -- --answers "$INPUT" --db "$TESTDB" --init "$FIXTURE" --reinit ) > "$actual" 2> "$err"
status=$?

fail=0

if [ "$status" -ne 0 ]; then
    echo "FAIL ipage：退出码 $status"
    fail=1
fi
if grep -q -E '运行期错误|提示：' "$err"; then
    echo "FAIL ipage：stderr 有异常输出"
    sed 's/^/    /' "$err"
    fail=1
fi

sed 's/\r$//' "$actual" > "$actual.n"
sed 's/\r$//' "$expected" > "$expected.n"
if diff -u "$expected.n" "$actual.n" > /dev/null 2>&1; then
    echo "ok   ipage"
else
    echo "FAIL ipage：输出与期望不一致"
    diff -u "$expected.n" "$actual.n" | head -40
    fail=1
fi
rm -f "$actual" "$actual.n" "$expected.n" "$err"

if [ "$fail" -eq 0 ]; then
    echo "magel/example/sql-magel ipage-student 检查通过"
else
    echo "magel/example/sql-magel ipage-student 检查失败"
fi
exit $fail
