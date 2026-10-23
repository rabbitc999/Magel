#!/bin/sh
# chali/build.sh —— 一键构建与打包。
#
# 用法（在本目录跑）：
#   sh build.sh              生成解析器 → 构建主程序 → 按环境打包插件与书稿
#   sh build.sh --no-gen     跳过文法生成，直接用现有解析器构建
#   sh build.sh --jobs 4     指定并行度（默认取 CPU 核数）
#   sh build.sh --help
#
# 四个阶段：
#   1. gen.sh 生成解析器 / 驱动 / 工程文件。
#   2. 构建主程序，产物落到 dist/。
#   3. 环境检测：npm 决定是否打 VSCode 插件，latexmk / pdflatex 决定是否编书稿；
#      缺哪个就打印提示跳过，**不中断构建**。
#   4. 统一整理到 dist/，命名规范 chali-<组件>-<平台>-<架构>.<扩展名>：
#        dist/chali-<平台>-<架构>[.exe]          CLI 可执行文件
#        dist/chali-vscode-<平台>-<架构>.vsix    VSCode 插件（标准 VSIX）
#        dist/chali-book.pdf                     书稿（需 LaTeX）
#        dist/chali-bundle-<平台>-<架构>.zip     上面已有的组件汇总成包
#
# 环境要求：g++ / make / python 必需；npm、LaTeX 可选。

set -u

root=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd) || exit 2
cd "$root" || exit 2

run_gen=1
jobs=''
while [ "$#" -gt 0 ]; do
    arg=$1
    shift
    case "$arg" in
    --no-gen) run_gen=0 ;;
    --jobs)
        [ "$#" -gt 0 ] || { echo "--jobs 需要正整数" >&2; exit 2; }
        jobs=$1
        shift
        ;;
    --jobs=*) jobs=${arg#--jobs=} ;;
    -h | --help)
        awk 'NR > 1 && /^#/ { sub(/^# ?/, ""); print; next } NR > 1 { exit }' "$0"
        exit 0
        ;;
    *)
        echo "未知参数：$arg（用 --help 看用法）" >&2
        exit 2
        ;;
    esac
done

case "$jobs" in
'' | *[!0-9]*) jobs='' ;;
esac

have() { command -v "$1" >/dev/null 2>&1; }

# 平台与架构：产物名里只出现这一套拼写（win64 / linux / macos + amd64 / arm64）。
case "$(uname -s 2>/dev/null)" in
MINGW* | MSYS* | CYGWIN* | Windows_NT) os=win64 ;;
Darwin) os=macos ;;
Linux) os=linux ;;
*) os=$(uname -s 2>/dev/null | tr 'A-Z' 'a-z') ;;
esac
case "$(uname -m 2>/dev/null)" in
x86_64 | amd64 | AMD64) arch=amd64 ;;
aarch64 | arm64 | ARM64) arch=arm64 ;;
i?86) arch=x86 ;;
*) arch=$(uname -m 2>/dev/null) ;;
esac
[ -n "${os:-}" ] || os=unknown
[ -n "${arch:-}" ] || arch=unknown
exe=''
[ "$os" = win64 ] && exe='.exe'

dist="$root/dist"
mkdir -p "$dist"

# make：优先 GNU make，Windows（MSYS）环境退回 mingw32-make。
if have make; then
    make_cmd=make
elif have mingw32-make; then
    make_cmd=mingw32-make
elif have gmake; then
    make_cmd=gmake
else
    echo "未找到 make：装 GNU make 后重试" >&2
    exit 2
fi
if [ -z "$jobs" ]; then
    jobs=$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo "${NUMBER_OF_PROCESSORS:-4}")
    case "$jobs" in
    '' | *[!0-9]*) jobs=4 ;;
    esac
fi

# python：书稿生成脚本与压缩包都用它。Windows 商店的 python3 可能只是个占位存根，
# 所以除了「在 PATH 上」还要**真的执行一次**才算可用。
py=''
for candidate in python3 python py; do
    if have "$candidate" && "$candidate" -c 'pass' >/dev/null 2>&1; then
        py=$candidate
        break
    fi
done

echo "== 1/4 生成解析器 =="
if [ "$run_gen" -eq 1 ]; then
    sh gen.sh || { echo "文法生成失败，构建中止" >&2; exit 1; }
else
    echo "（--no-gen：跳过 gen.sh，直接用现有解析器）"
fi

echo "== 2/4 构建主程序（-j$jobs）=="
"$make_cmd" -j"$jobs" || { echo "构建失败" >&2; exit 1; }
[ -f "$root/ChaLi.exe" ] || [ -f "$root/ChaLi" ] || { echo "构建结束但没有找到 ChaLi 可执行文件" >&2; exit 1; }

cli="chali-$os-$arch$exe"
if [ -f "$root/ChaLi.exe" ]; then
    cp "$root/ChaLi.exe" "$dist/$cli"
else
    cp "$root/ChaLi" "$dist/$cli"
fi
chmod +x "$dist/$cli" 2>/dev/null
echo "已产出：dist/$cli"

echo "== 3/4 环境检测 =="
have npm && echo "npm：有（打包 VSCode 插件）" || echo "npm：无（跳过 VSCode 插件）"
if have latexmk || have pdflatex; then
    echo "LaTeX：有（编译书稿）"
else
    echo "LaTeX：无（跳过书稿）"
fi

echo "== 4/4 打包 =="
# VSCode 插件：内嵌编译器 → 清单自检 → vsce package（测试在 ci.sh 里跑，这里跳过）。
if have npm; then
    if (cd "$root/vscode" && npm run package -- --skip-tests --out "$dist"); then
        for vsix in "$dist"/chali-editor-*.vsix; do
            [ -f "$vsix" ] || continue
            mv "$vsix" "$dist/chali-vscode-$os-$arch.vsix"
        done
        if [ -f "$dist/chali-vscode-$os-$arch.vsix" ]; then
            echo "已产出：dist/chali-vscode-$os-$arch.vsix"
        else
            echo "插件打包没有产出 .vsix，已跳过" >&2
        fi
    else
        echo "插件打包失败（环境或网络问题），已跳过，不影响主程序产物" >&2
    fi
else
    echo "跳过 VSCode 插件：未找到 npm"
fi

# 书稿：先由 Markdown 生成章节 .tex，再排版；封面单独跑一次，正文两遍使目录页码稳定。
if have latexmk || have pdflatex; then
    book_ok=0
    if [ -n "$py" ] && [ -f "$root/doc/book/latex/tools/md2tex.py" ]; then
        "$py" "$root/doc/book/latex/tools/md2tex.py" || echo "章节生成失败，继续用现有 .tex 排版" >&2
    fi
    if have latexmk && have xelatex; then
        (cd "$root/doc/book/latex" &&
            xelatex -interaction=nonstopmode -halt-on-error -output-directory=figure figure/cover.tex >/dev/null 2>&1 &&
            latexmk -xelatex -interaction=nonstopmode -halt-on-error book.tex >book.build.log 2>&1) && book_ok=1
        [ "$book_ok" -eq 1 ] || tail -20 "$root/doc/book/latex/book.build.log" 2>/dev/null
    elif have xelatex; then
        (cd "$root/doc/book/latex" &&
            xelatex -interaction=nonstopmode -halt-on-error -output-directory=figure figure/cover.tex >/dev/null 2>&1 &&
            xelatex -interaction=nonstopmode -halt-on-error book.tex >/dev/null 2>&1 &&
            xelatex -interaction=nonstopmode -halt-on-error book.tex >/dev/null 2>&1) && book_ok=1
    else
        echo "跳过书稿：书稿用 xelatex 排版（中文字体），当前只有 pdflatex" >&2
    fi
    if [ "$book_ok" -eq 1 ] && [ -f "$root/doc/book/latex/book.pdf" ]; then
        cp "$root/doc/book/latex/book.pdf" "$dist/chali-book.pdf"
        echo "已产出：dist/chali-book.pdf"
    elif [ "$book_ok" -eq 1 ]; then
        echo "书稿排版结束但没有 book.pdf" >&2
    fi
else
    echo "跳过书稿：未找到 latexmk / pdflatex"
fi

# 汇总压缩包：把上面产出的组件打成一个 zip（自身不进包）。
zip_name="chali-bundle-$os-$arch.zip"
rm -f "$dist/$zip_name"
members=''
for f in "$dist/chali-$os-$arch$exe" "$dist/chali-vscode-$os-$arch.vsix" "$dist/chali-book.pdf"; do
    [ -f "$f" ] && members="$members $(basename "$f")"
done
if [ -n "$members" ]; then
    if [ -n "$py" ]; then
        # shellcheck disable=SC2086
        (cd "$dist" && "$py" -c 'import sys, zipfile
name = sys.argv[1]
with zipfile.ZipFile(name, "w", zipfile.ZIP_DEFLATED) as archive:
    for member in sys.argv[2:]:
        archive.write(member, member)' "$zip_name" $members) && echo "已产出：dist/$zip_name" || echo "压缩包生成失败，已跳过" >&2
    elif have zip; then
        # shellcheck disable=SC2086
        (cd "$dist" && zip -q "$zip_name" $members) && echo "已产出：dist/$zip_name" || echo "压缩包生成失败，已跳过" >&2
    else
        echo "跳过压缩包：未找到 python / zip" >&2
    fi
fi

echo
echo "== 产物清单 =="
ls -lh "$dist"
