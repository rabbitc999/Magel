#!/bin/sh
# magel/check.sh —— 跑全部样例：
#   1) example/samples/*.mgl        必须成功退出，且 stdout 与 example/expected/*.txt 逐行一致；
#   2) example/samples-error/*.mgl  必须报错退出（非 0），首行与 example/expected-error/*.txt 一致。
#
# 正常样例的 stderr 还会单独挡一道：出现"提示：程序结束…"这种泄漏文案就判失败
# （否则对象 / 堆没释放只写在 stderr，stdout 比对根本看不见）。确实要留提示的样例，
# 把整段 stderr 写进 example/expected/<name>.stderr，脚本就会按期望比对。
#
# 用法：cd magel && sh check.sh
#   （路径与 magel_out.* 都相对当前目录，必须在 magel/ 里跑；发布门禁
#    `ci/run.sh` 就是这么调的。Makefile 里没有 check 目标。）
#
# 每个样例使用临时目录中的独立产物前缀，避免与其他模块门禁互相覆盖。
# 想保留并查看某个样例的四元式，可以单独执行：
#
#   ./magel emit example/samples/array2d.mgl out /tmp/array2d
#
# 期望文件是"已验证正确"的输出固化下来的：一旦降级 / 四元式 / 栈机改动，
# 这里会立刻报出是哪个样例、哪一行不同。
#
# 期望文件在 Windows 上检出时可能是 CRLF（仓库 core.autocrlf），比对前两边都去掉
# 行尾的 \r，这样同一份期望文件在 Windows / Linux 都可用。

# 期望错误文件为中文，显示语言不随执行主机变化。
export MAGEL_LANG=zh
# 门禁要确定性：样例缺输入时照旧报错，别去等标准输入（调试时才用那份兜底）。
export MAGEL_NO_STDIN_INPUT=1

set -u

# 临时文件：`mktemp` 默认往 /tmp 写，而有些环境（Windows 的 Git Bash）里 /tmp 不可写。
# 所以显式给模板：默认用仓库里的 tmp/（已 gitignore），要换地方就设 MAGEL_TMP。
CHECK_TMP=${MAGEL_TMP:-tmp}
mkdir -p "$CHECK_TMP" 2>/dev/null || true
if [ ! -w "$CHECK_TMP" ]; then
    echo "临时目录不可写：$CHECK_TMP（用 MAGEL_TMP=… 换一个）" >&2
    exit 2
fi

# 注意 `./`：Git Bash / MSYS 下 `./magel` 能自动找到 `magel.exe`，
# 光写 `magel` 会被当成 PATH 查找（找不到）。
DEMO=${DEMO:-./magel}
SAMPLES=${SAMPLES:-example/samples}
EXPECTED=${EXPECTED:-example/expected}
ERROR_SAMPLES=${ERROR_SAMPLES:-example/samples-error}
EXPECTED_ERROR=${EXPECTED_ERROR:-example/expected-error}
# 可指定空格分隔的样例名，供模块门禁只复测受影响的样例。
CHECK_NAMES=${MAGEL_SAMPLE_NAMES:-}

if [ ! -x "$DEMO" ]; then
    echo "找不到 $DEMO；先跑 make（生成 + 编译）" >&2
    exit 2
fi

fail=0
count=0

normalize() {
    sed 's/\r$//' "$1" > "$2"
}

# ---- 正常样例：stdout 与期望逐行一致 ----
for src in "$SAMPLES"/*.mgl; do
    name=${src##*/}
    name=${name%.mgl}
    if [ -n "$CHECK_NAMES" ]; then
        case " $CHECK_NAMES " in *" $name "*) ;; *) continue ;; esac
    fi
    expected="$EXPECTED/$name.txt"
    actual=$(mktemp "$CHECK_TMP/magel_check.XXXXXX") || exit 2
    count=$((count + 1))
    if [ ! -f "$expected" ]; then
        echo "[FAIL] $name：缺少期望文件 $expected"
        fail=1
        rm -f "$actual"
        continue
    fi
    # 有 example/expected/<name>.in 就把它当程序输入传给 --input（`input(变量);` 用）
    set -- 
    if [ -f "$EXPECTED/$name.threads" ]; then
        set -- --threads
    fi
    if [ -f "$EXPECTED/$name.in" ]; then
        set -- --input "$(cat "$EXPECTED/$name.in")"
    fi
    # 有 example/expected/<name>.args 时，每行作为一个独立的 main 参数；空行也是合法参数。
    if [ -f "$EXPECTED/$name.args" ]; then
        set -- "$@" --
        while IFS= read -r argument || [ -n "$argument" ]; do
            argument=$(printf '%s' "$argument" | sed 's/\r$//')
            set -- "$@" "$argument"
        done < "$EXPECTED/$name.args"
    fi
    raw="$actual.raw"
    "$DEMO" run "$src" --out "$actual.compile" "$@" >"$raw" 2>"$actual.err"
    actual_status=$?
    sed 's/\r$//' "$raw" >"$actual"
    expected_status=0
    if [ -f "$EXPECTED/$name.status" ]; then
        expected_status=$(sed -e 's/\r$//' -e '1!d' "$EXPECTED/$name.status")
    fi
    if [ "$actual_status" -ne "$expected_status" ]; then
        echo "[FAIL] $name：退出码应为 $expected_status，实际为 $actual_status"
        sed 's/^/       /' "$actual.err"
        fail=1
        rm -f "$actual" "$actual.err" "$raw"
        continue
    fi
    normalize "$expected" "$actual.exp"
    # 泄漏提示走 stderr —— 单独挡一道，防止所有权回归（`example/expected/<name>.stderr` 可放行）。
    leakBad=0
    if [ -f "$EXPECTED/$name.stderr" ]; then
        normalize "$EXPECTED/$name.stderr" "$actual.err.exp"
        sed 's/\r$//' "$actual.err" > "$actual.err.norm"
        if diff -u "$actual.err.exp" "$actual.err.norm" >"$actual.err.diff" 2>&1; then
            :
        else
            echo "[FAIL] $name：stderr 与期望不一致"
            sed 's/^/       /' "$actual.err.diff"
            fail=1
        fi
        rm -f "$actual.err.exp" "$actual.err.norm" "$actual.err.diff"
    elif grep -q "提示：程序结束" "$actual.err"; then
        echo "[FAIL] $name：stderr 出现泄漏提示（对象 / 堆没释放）"
        sed 's/^/       /' "$actual.err"
        fail=1
        leakBad=1
    fi
    if diff -u "$actual.exp" "$actual" >"$actual.diff" 2>&1; then
        [ "$leakBad" -eq 0 ] && echo "[ok]   $name"
    else
        echo "[FAIL] $name：输出与期望不一致"
        sed 's/^/       /' "$actual.diff"
        fail=1
    fi
    rm -f "$actual" "$actual.exp" "$actual.err" "$actual.diff" "$raw" "$actual.compile".*
done

if [ "$fail" -eq 0 ]; then
    echo "[ok]   $count 个正常样例"
fi

# ---- 错误样例：必须报错退出，且首行文案与期望一致 ----
errorCount=0
for src in "$ERROR_SAMPLES"/*.mgl; do
    if [ ! -f "$src" ]; then
        break
    fi
    name=${src##*/}
    name=${name%.mgl}
    expected="$EXPECTED_ERROR/$name.txt"
    if [ -n "$CHECK_NAMES" ]; then
        case " $CHECK_NAMES " in *" $name "*) ;; *) continue ;; esac
    fi
    actual=$(mktemp "$CHECK_TMP/magel_check.XXXXXX") || exit 2
    errorCount=$((errorCount + 1))
    if [ ! -f "$expected" ]; then
        echo "[FAIL] $name：缺少期望文件 $expected"
        fail=1
        rm -f "$actual"
        continue
    fi
    if [ -f "$EXPECTED_ERROR/$name.threads" ]; then
        set -- --threads
    else
        set --
    fi
    if "$DEMO" run "$src" --out "$actual.compile" "$@" >/dev/null 2>"$actual"; then
        echo "[FAIL] $name：本应报错退出，却成功返回"
        fail=1
        rm -f "$actual"
        continue
    fi
    sed -e 's/\r$//' -e '1!d' "$actual" >"$actual.first"
    normalize "$expected" "$actual.exp"
    # 地址依赖数据区/堆布局；用占位符只断言错误种类，不把偶然地址固化进测试。
    if grep -q '<ADDRESS>' "$actual.exp"; then
        sed -E 's/地址 [0-9]+/地址 <ADDRESS>/g' "$actual.first" >"$actual.addr"
        mv "$actual.addr" "$actual.first"
    fi
    if diff -u "$actual.exp" "$actual.first" >"$actual.diff" 2>&1; then
        echo "[ok]   $name（报错：$(head -n 1 "$actual.exp")）"
    else
        echo "[FAIL] $name：报错文案与期望不一致"
        sed 's/^/       /' "$actual.diff"
        fail=1
    fi
    rm -f "$actual" "$actual.exp" "$actual.first" "$actual.diff" "$actual.compile".*
done

if [ "$fail" -eq 0 ]; then
    echo "全部通过（$count 个正常样例 + $errorCount 个错误样例）"
fi
exit "$fail"
