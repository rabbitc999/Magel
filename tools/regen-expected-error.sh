#!/bin/sh
# 把「报错样例」的期望首行按当前编译器的实际输出重生成。
#
# check.sh 只比 stderr 的**第一行**，所以这里也只写第一行（后面几行是源码片段 +
# 波浪号，不进期望文件）。用来在报错文案改版后批量刷新期望，跑完必须用
# `git diff example/expected-error` 逐条看过再提交——脚本只是省手，不做判断。
#
# 用法：sh tmp/regen-expected-error.sh [名字...]   # 不点名就刷新全部报错样例
set -e
cd "$(dirname "$0")/.."
export MAGEL_LANG=zh
if [ "$#" -gt 0 ]; then
    names="$*"
else
    names=$(ls example/samples-error/*.mgl | sed -e 's#.*/##' -e 's/\.mgl$//')
fi
for name in $names; do
    src="example/samples-error/$name.mgl"
    [ -f "$src" ] || { echo "跳过 $name（没有 $src）" >&2; continue; }
    set --
    if [ -f "example/expected-error/$name.threads" ]; then set -- --threads; fi
    ./magel.exe run "$src" "$@" >/dev/null 2>"tmp/regen.$name.err" && {
        echo "跳过 $name（本应报错却返回 0）" >&2
        rm -f "tmp/regen.$name.err"
        continue
    }
    # 期望文件在仓库里是 CRLF（与 example/expected-error 下其它文件一致），补回去。
    sed -e 's/\r$//' -e '1!d' -e 's/$/\r/' "tmp/regen.$name.err" >"example/expected-error/$name.txt"
    echo "刷新 $name：$(head -n 1 "example/expected-error/$name.txt")"
    rm -f "tmp/regen.$name.err"
done
