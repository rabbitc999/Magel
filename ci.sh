#!/bin/sh
# magel/ci.sh —— Magel 的门禁：分两档。
#
# 用法（在本目录跑）：
#   sh ci.sh                 快速门禁（默认）：构建 + 样例回归 + 核心测试 + 文档门禁
#   sh ci.sh --full          发布门禁：在快速门禁之外，再跑全部 stackvm / Python 回归
#   sh ci.sh --no-python     （只对 --full 有意义）只编 / 跑工具链，不跑 tests/*.py
#   sh ci.sh --jobs 4       并行独立回归；--jobs 1 串行（也可设 MAGEL_GATE_JOBS）
#   sh ci.sh --help
#
# 环境要求：g++ / gcc / make / python3.11+ 必需；cmake 仅发布门禁需要。
# 还要一份 **ParseEngine 文法生成器**：默认找 `${ENGINE:-../build/bin/ParseEngine}`，
# 它由 ParseEngine 仓库（不含在本项目里）产出；不在就报错退出 2。
#
# 退出码：0 全绿；1 有检查失败；2 环境不满足。

set -u
# 诊断与样例输出都是中文，显示语言固定，免得随执行主机变化。
export MAGEL_LANG=zh
export PARSEENGINE_LANG=zh
export PYTHONUTF8=1
export PYTHONIOENCODING=utf-8

root=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd) || exit 2
cd "$root" || exit 2

run_python=1
full=0
jobs=${MAGEL_GATE_JOBS:-4}
while [ "$#" -gt 0 ]; do
    arg=$1
    shift
    case "$arg" in
    --jobs)
        [ "$#" -gt 0 ] || { echo "--jobs 需要正整数" >&2; exit 2; }
        jobs=$1
        shift
        ;;
    --jobs=*) jobs=${arg#--jobs=} ;;
    --no-python) run_python=0 ;;
    --full) full=1 ;;
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

case "$jobs" in ''|*[!0-9]*|0) echo "--jobs 需要正整数" >&2; exit 2 ;; esac
[ "$jobs" -gt 0 ] 2>/dev/null || { echo "--jobs 需要正整数" >&2; exit 2; }

have() { command -v "$1" >/dev/null 2>&1; }

echo "== 环境检测 =="
missing=''
# make：优先 GNU make，Windows（MSYS）环境退回 mingw32-make。
MAKE=${MAKE:-}
if [ -z "$MAKE" ]; then
    if have make; then
        MAKE=make
    elif have mingw32-make; then
        MAKE=mingw32-make
    fi
fi
if [ -n "$MAKE" ] && have "$MAKE"; then
    printf '  %-8s %s\n' make "$(command -v "$MAKE")"
else
    printf '  %-8s 缺失（必需）\n' make
    missing="$missing make"
fi
for tool in g++ gcc; do
    if have "$tool"; then
        printf '  %-8s %s\n' "$tool" "$(command -v "$tool")"
    else
        printf '  %-8s 缺失（必需）\n' "$tool"
        missing="$missing $tool"
    fi
done
# cmake 只有发布门禁（--full）的 stackvm 单测需要。
if [ "$full" = 1 ]; then
    if have cmake; then
        printf '  %-8s %s\n' cmake "$(command -v cmake)"
    else
        printf '  %-8s 缺失（必需）\n' cmake
        missing="$missing cmake"
    fi
fi
PYTHON=''
for candidate in python3 python; do
    if have "$candidate" && [ "$("$candidate" -c 'import sys; print("MAGEL-PYTHON-OK" if sys.version_info >= (3, 11) else "too-old")' 2>/dev/null)" = "MAGEL-PYTHON-OK" ]; then PYTHON="$candidate"; break; fi
done
if [ -n "$PYTHON" ]; then
    printf '  %-8s %s\n' python "$(command -v "$PYTHON")"
else
    printf '  %-8s 缺失（必需）\n' python
    missing="$missing python"
fi
# 文法生成器是外部依赖：本项目只有 .gram，生成器由 ParseEngine 仓库产出。
# 查找顺序与 gen.sh 保持一致——先看本项目根目录，再退回 ParseEngine 仓库那两处
# （本仓是独立工程，生成器就放在根目录；只认 ../build/bin/ 会漏掉它）。
# Windows 上 `.exe` 排在前面：MSYS 的 `test -x ParseEngine` 会因 .exe 后缀假命中。
if [ -n "${ENGINE:-}" ]; then
    engine_found="$ENGINE"
else
    engine_found=''
    for candidate in "./ParseEngine.exe" "./ParseEngine" \
                     "../ParseEngine/build/bin/ParseEngine.exe" "../ParseEngine/build/bin/ParseEngine" \
                     "../build/bin/ParseEngine.exe" "../build/bin/ParseEngine"; do
        if [ -x "$candidate" ]; then engine_found="$candidate"; break; fi
    done
fi
if [ -n "$engine_found" ] && [ -x "$engine_found" ]; then
    printf '  %-8s %s\n' engine "$engine_found"
    ENGINE="$engine_found"
else
    printf '  %-8s 缺失（必需）：用 ENGINE=<路径> 指一份 ParseEngine 生成器\n' engine
    missing="$missing ParseEngine"
fi
if [ -n "$missing" ]; then
    echo
    echo "缺少必需工具：$missing —— 装好再跑。" >&2
    exit 2
fi

# 先登记完整命令；Python 调度器仅并行明确审查过的独立 Python 与 VM 回归。
# 生成、构建和未知任务形成屏障，确保不会边运行编译器边覆盖可执行文件。
export ENGINE
mkdir -p tmp/ci/logs
manifest=$(mktemp tmp/ci/gates.XXXXXX) || exit 2
trap 'rm -f "$manifest"' EXIT
trap 'exit 129' HUP
trap 'exit 130' INT
trap 'exit 143' TERM
run_step() {
    "$PYTHON" tools/parallel_gates.py append "$manifest" "$@" || exit 2
}
run_required_step() {
    "$PYTHON" tools/parallel_gates.py append --required "$manifest" "$@" || exit 2
}

if [ "$full" = 1 ]; then
    run_required_step "配置 stackvm 单测（cmake）" \
        cmake -S library/stackvm -B tmp/ci/stackvm -DCMAKE_BUILD_TYPE=Release \
              -DSTACKVM_WITH_REFLECTION=ON \
              -DCMAKE_RUNTIME_OUTPUT_DIRECTORY="$root/tmp/ci/stackvm" \
              -DCMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE="$root/tmp/ci/stackvm"
    run_required_step "构建 stackvm（CLI + 单测）" \
        cmake --build tmp/ci/stackvm --config Release --target magel-stackvm magel-stackvm-tests magel-artifact-tests magel-symbol-tests magel-panic-tests svmb_symtool -j4
    run_step "运行二进制中间产物等价性与损坏输入回归" tmp/ci/stackvm/magel-artifact-tests
    run_step "运行 stackvm 单测" \
        tmp/ci/stackvm/magel-stackvm-tests library/stackvm/examples
    run_step "运行变长符号与十万符号回归" \
        tmp/ci/stackvm/magel-symbol-tests tmp/ci/stackvm/symbol-fixtures
    run_step "运行 panic 调用栈与压缩隐私回归" \
        tmp/ci/stackvm/magel-panic-tests tmp/ci/stackvm/panic-compact.svmb
    run_required_step "配置无 panic 栈构建" \
        cmake -S library/stackvm -B tmp/ci/panic-off -DCMAKE_BUILD_TYPE=Release \
              -DSTACKVM_WITH_REFLECTION=ON \
              -DCMAKE_RUNTIME_OUTPUT_DIRECTORY="$root/tmp/ci/panic-off" \
              -DCMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE="$root/tmp/ci/panic-off" \
              -DSTACKVM_WITH_PANIC_TRACE=OFF -DSTACKVM_WITH_BIGINT=OFF -DSTACKVM_WITH_BIGFLOAT=OFF
    run_required_step "构建无 panic 栈测试" cmake --build tmp/ci/panic-off --config Release --target magel-panic-tests -j4
    run_step "运行无 panic 栈测试" tmp/ci/panic-off/magel-panic-tests tmp/ci/panic-off/panic-compact.svmb
fi

# ---- 3. Magel：由文法生成解析器，再编驱动 ----
run_required_step "生成 MAGEL 解析器（gen.sh）" sh gen.sh
run_required_step "构建 MAGEL 驱动（magel）" "$MAKE" -j4

run_step "验证并行门禁调度与失败汇总" "$PYTHON" tests/test_parallel_gates.py
run_step "验证卫生宏绑定、正则、AST 替换及生成解析器" "$PYTHON" tests/macro_foundations.py

# ---- 快速门禁核心三件套：样例回归 + 本次新增能力测试 + 文档同步 ----
run_step "MAGEL 样例回归（check.sh）" \
    sh -c 'cd "$1" && MAGEL_TMP="$1/tmp/ci/check" sh check.sh' sh "$root"
run_step "运行 std::image 与 StringBuilder 回归" "$PYTHON" tests/magel_image.py --magel magel
run_step "运行标准错误流与文件打印回归" "$PYTHON" tests/magel_print_streams.py --magel magel
run_step "运行临时值与资源释放合并回归" "$PYTHON" tests/magel_resource_lifetime.py --magel magel
run_step "运行闭包自动类型与嵌套返回语言服务回归" "$PYTHON" tests/magel_lsp_closure_types.py --magel magel
run_required_step "编译无墓碑分配身份回归" g++ -std=c++17 -O2 -DMAGEL_CUSTOM_ALLOCATOR=1 -Isource -Ilibrary -Isource/allocator tests/allocation_identity.cpp -o tmp/ci/allocation-identity
run_step "运行百万次分配身份回归" tmp/ci/allocation-identity
run_step "运行数组按值传参与引用回归" "$PYTHON" tests/magel_array_parameters.py --magel magel
run_required_step "编译字面量数据合并单元测试" g++ -std=c++17 -O2 -DMAGEL_CUSTOM_ALLOCATOR=0 -DMAGEL_WITH_REFLECTION=1 -DSTACKVM_WITH_REFLECTION=1 library/simple_ast/test_simple_ast.cpp library/simple_ast/simple_ast.cpp -o tmp/ci/simple-ast-literals
run_step "运行字面量数据合并单元测试" tmp/ci/simple-ast-literals
run_step "运行字面量合并与字符串副本回归" "$PYTHON" tests/magel_string_literals.py --magel magel
run_step "检查 opcode 声明与不可复制限制" "$PYTHON" tests/magel_opcode_declarations.py --magel magel
run_step "检查区间与循环值表达式" "$PYTHON" tests/magel_range_loop_values.py --magel magel
run_step "检查 opcode 资源所有权与借用" "$PYTHON" tests/magel_opcode_resources.py --magel magel
run_step "检查 opcode 反射与语言服务隐藏" "$PYTHON" tests/magel_opcode_visibility.py --magel magel
run_step "检查标准对象与 VM 核心隔离" "$PYTHON" tests/opcode_core_isolation.py
run_step "检查 VM 集合宿主胶水与同步回调" "$PYTHON" tests/magel_collection_glue.py --magel magel
run_step "检查 Map Pair 迭代元素类型" "$PYTHON" tests/magel_map_pair_iterator.py --magel magel
run_step "检查泛型命名构造入口推导" "$PYTHON" tests/magel_generic_constructors.py --magel magel
run_step "检查标准库 opcode 存储与资源边界" "$PYTHON" tests/magel_opcode_std_storage.py --magel magel
run_step "运行闭包来源与借用控制流回归" "$PYTHON" tests/magel_borrow_flow.py --magel magel
run_step "运行泛型闭包与特化回归" "$PYTHON" tests/magel_generic_closures.py --magel magel
run_step "检查模块路径与文本包含指令删除" "$PYTHON" tests/magel_package_paths.py
run_step "检查 VM 命令与未知文件" "$PYTHON" tests/magel_tool.py
run_step "检查手册与标准库共享索引同步" "$PYTHON" tools/library_reference.py --check

# ---- 以下均为发布门禁（--full）扩展步骤 ----
if [ "$full" = 1 ]; then
run_step "运行整数区间端点与溢出回归" "$PYTHON" tests/magel_foreach_intervals.py --magel magel
run_required_step "编译场景命令单元测试" g++ -std=c++17 -O1 tests/magel_cli_test.cpp -o tmp/ci/magel-cli
run_step "运行场景命令单元测试" tmp/ci/magel-cli
run_required_step "编译宏临时编号单元测试" g++ -std=c++17 -O1 -DMAGEL_CUSTOM_ALLOCATOR=0 -Ilibrary -Ilibrary/preprocess tests/magel_macro_slots_test.cpp source/magel_macro.cpp source/magel_macro_controls.cpp -o tmp/ci/magel-macro-slots
run_step "运行宏临时编号单元测试" tmp/ci/magel-macro-slots
run_required_step "编译宏结构化拼接与字符串化单元测试" g++ -std=c++17 -O1 -DMAGEL_CUSTOM_ALLOCATOR=0 -Ilibrary -Ilibrary/preprocess tests/magel_macro_structured_test.cpp source/magel_macro.cpp source/magel_macro_controls.cpp -o tmp/ci/magel-macro-structured
run_step "运行宏结构化拼接与字符串化单元测试" tmp/ci/magel-macro-structured
run_required_step "编译宏多括号文本分组回归" g++ -std=c++17 -O2 -DMAGEL_CUSTOM_ALLOCATOR=0 -DMAGEL_WITH_REFLECTION=1 -DSTACKVM_WITH_REFLECTION=1 -Ilibrary -Ilibrary/preprocess tests/magel_macro_groups_test.cpp source/magel_macro.cpp source/magel_macro_controls.cpp -o tmp/ci/magel-macro-groups
run_step "运行宏多括号文本分组回归" tmp/ci/magel-macro-groups
run_step "验证宏多括号完整前端行列映射" "$PYTHON" tests/magel_macro_groups.py
run_step "验证返回表达式、具名闭包与字段派生" "$PYTHON" tests/magel_declaration_sugars.py

run_required_step "编译宏参数括号规则回归" g++ -std=c++17 -O1 -DMAGEL_CUSTOM_ALLOCATOR=0 -Ilibrary -Ilibrary/preprocess tests/magel_macro_parentheses_test.cpp source/magel_macro.cpp source/magel_macro_controls.cpp -o tmp/ci/magel-macro-parentheses
run_step "运行宏参数括号规则回归" tmp/ci/magel-macro-parentheses

# ---- lztext：内嵌标准库/运行时资产的压缩存储与运行时解压 ----
# magel/library/lztext 与 ParseEngine 的 library/lztext 是同一份实现，改一个要同步另一个。
# 这里既构建构建期要用的 lztext_pack（tools/embed_stdlib.py 通过它压缩），
# 也跑一遍库自检（往返一致性 + 边界用例 + 随机语料模糊测试）。
run_required_step "构建 lztext 打包工具" "$MAKE" -C library/lztext pack
run_required_step "编译 lztext 单元测试" g++ -std=c++17 -O1 -Ilibrary/lztext library/lztext/test_lztext.cpp library/lztext/lztext.cpp -o tmp/ci/lztext
run_step "运行 lztext 单元测试" tmp/ci/lztext
fi

if [ "$full" = 1 ] && [ "$run_python" = 1 ]; then
    run_step "运行符号转换与 CLI 集成回归" \
        "$PYTHON" tests/test_svmb_symbols.py --build tmp/ci/stackvm --magel magel
    run_step "运行 MAGEL 与 VM panic 诊断回归" \
        "$PYTHON" tests/test_magel_panic.py --magel magel
    run_step "运行源码行号诊断回归" \
        "$PYTHON" tests/test_magel_source_lines.py --magel magel
    run_step "运行字符串清理与循环重入VM 多入口回归" \
        "$PYTHON" tests/magel_string_reentry.py --magel magel
    run_step "运行语言功能、值类型与引用VM 多入口回归" \
        "$PYTHON" tests/magel_reference_values.py --magel magel
fi

if [ "$full" = 1 ] && [ "$run_python" = 1 ]; then
    run_step "验证最新生成器下载与失败清理" "$PYTHON" tests/gen_download.py
    run_step "运行 match 两种区间与或模式回归" "$PYTHON" tests/magel_match_patterns.py --magel magel
    run_step "运行 lambda 捕获与回调回归" "$PYTHON" tests/magel_lambdas.py --magel magel --vm tmp/ci/stackvm/magel-stackvm
    run_step "运行泛型单态化与嵌套组合回归" "$PYTHON" tests/magel_generic_monomorphization.py --magel magel
    run_step "运行函数默认参数回归" "$PYTHON" tests/magel_default_arguments.py --magel magel
    run_step "验证旧库删除与标准模块迁移" "$PYTHON" tests/magel_removed_libraries.py --magel magel
    run_step "运行语言移动与只读析构限制回归" "$PYTHON" tests/magel_language_move.py --magel magel
    run_step "运行默认参数歧义诊断回归" "$PYTHON" tests/magel_default_ambiguity.py --magel magel
    run_step "运行构造函数与资源清理回归" "$PYTHON" tests/magel_constructors.py --magel magel
    run_step "运行转换接口与资源清理回归" "$PYTHON" tests/magel_conversions.py --magel magel
    run_step "运行数值、数组安全与 float32 后缀回归" "$PYTHON" tests/magel_numeric_v101.py --magel magel
    run_step "运行 match 整数模式重叠警告回归" "$PYTHON" tests/magel_match_overlap.py --magel magel
    run_step "运行 fn 匿名函数与 lambda 兼容回归" "$PYTHON" tests/magel_fn.py --magel magel
    # 软重启看门狗（PC 宿主）：--watchdog 打开前后的行为、输出回滚与重启上限。
    run_step "运行内存访问护栏回归" "$PYTHON" tests/magel_memory_guard.py --magel magel
    run_step "运行软重启看门狗回归" "$PYTHON" tests/magel_watchdog.py --magel magel
    # 总览图（各项目的 make_*_overview.py → 各自 overview 目录；Magel 那张由 magel 自己
    # 绘制，落在 doc/overview/）：能重跑、成对产出、重复生成一致。
    run_step "运行简短场景指令与拖入文件回归" "$PYTHON" tests/magel_scenes.py --magel magel
    run_step "运行新建工程回归" "$PYTHON" tests/magel_project.py --magel magel
    run_step "运行表达式对象冲突及目标求值回归" \
        "$PYTHON" tests/magel_update_restrictions.py --magel magel
    run_step "运行多诊断与独立借用检查回归" \
        "$PYTHON" tests/magel_analysis_diagnostics.py --magel magel
    run_step "运行数据结构及 NOIP 算法鲁棒性回归" \
        "$PYTHON" tests/magel_algorithm_robustness.py --magel magel
    run_step "运行多值返回与泛型元组回归" \
        "$PYTHON" tests/magel_tuple_returns.py --magel magel
    # 裁剪构建：单文件导出在 NO_MAGEL_* 定义下仍能跑，且大数符号真的被裁掉。
    run_step "运行裁剪构建回归" "$PYTHON" tests/trim_builds.py --magel magel
    # 关键字三处对齐（补全表 / 文法登记 / 语法高亮），谁加了关键字忘了同步就红。
    run_step "运行编码、CSV、UTC 日期的 Python 交叉验证" "$PYTHON" tests/magel_std_data.py --magel magel
    run_step "检查补全关键字与高亮关键字一致" "$PYTHON" tools/check_keywords.py
fi

# ---- 样例回归已在快速门禁跑过；--full 的扩展测试继续 ----


# ---- 5. 分配器单元测试（C 与 C++ 两份，之前没人自动跑）----



# ---- 6. Python 门禁 ----
if [ "$full" = 1 ] && [ "$run_python" = 1 ]; then
    for t in magel_tool magel_generator magel_incremental magel_package magel_zip magel_i18n magel_development magel_package_paths magel_platform_abi_paths magel_abi_values; do
        if [ "$t" = magel_zip ] || [ "$t" = magel_i18n ] || [ "$t" = magel_development ]; then
            run_step "python tests/$t.py" "$PYTHON" "tests/$t.py" --magel magel
        else
            run_step "python tests/$t.py" "$PYTHON" "tests/$t.py"
        fi
    done
fi

# ---- 7. 文档门禁 ----
if [ "$full" = 1 ] && [ "$run_python" = 1 ]; then
    # doc/ 各页面的相对链接与图片；`doc/book/` 是独立入门书、不在门禁内，脚本会打印跳过页数。
    run_step "检查文档相对链接" "$PYTHON" tools/check_doc_links.py
    run_step "检查标准库公开符号命名" "$PYTHON" tools/check_std_naming.py
    run_step "检查手册与标准库共享索引同步" "$PYTHON" tools/library_reference.py --check
    run_step "运行原生命令行手册回归" "$PYTHON" tests/magel_man.py
    run_step "运行行输入与文件文本读取回归" "$PYTHON" tests/magel_text_readers.py
    run_step "检查内嵌书稿关键字与 SKILL 同步" "$PYTHON" tools/embed_manual.py --check
    run_step "运行格式化多行字符串保真回归" "$PYTHON" tests/magel_formatter_literals.py
    run_step "运行 void main 的 VM 回归" "$PYTHON" tests/magel_void_main.py
    run_step "运行类型声明可选尾分号回归" "$PYTHON" tests/magel_type_decl_terminators.py
    run_step "运行发布脚本与归档一致性回归" "$PYTHON" tests/magel_release.py
    run_step "运行新增书本示例与 LaTeX 同步回归" "$PYTHON" tests/magel_book_updates.py
    run_step "运行嵌套函数值的内联 fn 实参回归" "$PYTHON" tests/magel_inline_fn_arguments.py
    run_step "运行胶水调用 Magel 函数的 VM回归" "$PYTHON" tests/magel_native_callbacks.py
    run_step "运行共享语义、宏分组提示与安全重构回归" "$PYTHON" tests/magel_lsp_semantic_features.py
    run_step "运行 pause 终端输出顺序回归" "$PYTHON" tests/magel_pause_output.py
    # EBNF 附录由 magel.gram 生成：改了文法忘了重生成附录，这里就红（--check 不改文件）。
    run_step "检查 EBNF 附录与文法同步" "$PYTHON" tools/export_magel_ebnf.py --check
    # CHANGELOG 与 VERSION / 发布 tag 对齐：改了版本忘写一节、打了 tag 没有记录，这里就红。
    run_step "检查 CHANGELOG 与版本一致" "$PYTHON" tools/changelog.py --check
fi

# ---- 执行与汇总 ----
# 调度前固定版本与命令清单，执行期间修改本脚本不会改变已登记步骤。
echo "版本：$(cat VERSION)"
exec "$PYTHON" tools/parallel_gates.py run "$manifest" --output tmp/ci/logs --jobs "$jobs" --cleanup-manifest
