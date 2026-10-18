#!/usr/bin/env python3
"""Magel 开发工具：编译器驱动的测试与诊断、保守格式化、LSP 和可复现依赖锁定。"""
# 历史 Python 参考实现，仅供文档和回归对照；magel 的开发命令已使用 native_development.cpp。
from pathlib import Path
import argparse
import fnmatch
import hashlib
import shutil
import stat
import tarfile
import zipfile
import json
import os
import re
import subprocess
import sys
import tempfile
import time
import tomllib
from urllib.parse import urlparse, unquote
from urllib.request import urlopen
from magel_lsp import ApiIndex, documentation, inferred_type, matching, expression_name, signature_help, semantic_tokens, TOKEN_TYPES, imports
from magel_api import TOKEN


def _language():
    """采用与本地工具一致的环境优先级及 Windows UI 语言回退。"""
    explicit = os.environ.get("MAGEL_LANG", "")
    def parse(value):
        value = re.split(r"[-_.@]", value.lower())[0]
        return value if value in {"zh", "en"} else "en"
    if explicit and explicit != "auto":
        return parse(explicit)
    for key in ("LC_ALL", "LC_MESSAGES", "LANG"):
        if os.environ.get(key):
            return parse(os.environ[key])
    if os.name == "nt":
        import ctypes
        primary = ctypes.windll.kernel32.GetUserDefaultUILanguage() & 0x3ff
        return {4: "zh"}.get(primary, "en")
    return "en"


# MAGEL_CATALOG_BEGIN
_MESSAGES = {
  "lang 后需要 zh、en 或 auto": [
    "lang requires zh, en or auto"
  ],
  "不支持的显示语言：": [
    "Unsupported display language: "
  ],
  "用法：magel 文件.mgl [程序参数...]\n": [
    "Usage: magel FILE.mgl [program arguments...]\n"
  ],
  "      magel 场景 [文件.mgl] [配置词...] [-- 程序参数...]\n": [
    "       magel COMMAND [FILE.mgl] [settings...] [-- program arguments...]\n"
  ],
  "统一工具链：解释、导出 .svmb、导出 AOT 可执行文件都在这一个程序里。\n": [
    "One toolchain: interpret, export .svmb, or build native AOT executables.\n"
  ],
  "场景指令（运行方式也可用 --mode 指定）：\n": [
    "Commands (execution mode can also be selected with --mode):\n"
  ],
  "  run|interpret  内部解释运行（缺省）；直接传入文件时，其后全为程序参数\n": [
    "  run|interpret  Interpret (default); arguments after a direct file go to the program\n"
  ],
  "  check FILE [json]    仅检查，可输出 JSON 诊断\n": [
    "  check FILE [json]    Check only; optionally emit JSON diagnostics\n"
  ],
  "  help                 显示帮助\n": [
    "  help                 Show help\n"
  ],
  "  emit           只出 .quad / .data / .layout\n": [
    "  emit           Export .quad / .data / .layout only\n"
  ],
  "  svmb           只导出 <前缀>.svmb，不执行\n": [
    "  svmb           Export <prefix>.svmb without executing\n"
  ],
  "  aot FILE [out EXE] [opt LEVEL] [checked|unchecked]  导出 AOT（默认 checked）\n": [
    "  aot FILE [out EXE] [opt LEVEL] [checked|unchecked]  Build AOT (default: checked)\n"
  ],
  "  new DIR [name NAME]  新建 Magel 应用工程（目标目录必须不存在）\n": [
    "  new DIR [name NAME]  Create a Magel project (destination must not exist)\n"
  ],
  "  gen            生成独立编译器工程（转发 magel-generator compiler）\n": [
    "  gen            Generate a standalone compiler project (magel-generator compiler)\n"
  ],
  "  clear out DIR  清理登记过的中间产物（不重新编译）\n": [
    "  clear out DIR  Remove registered intermediate files without recompiling\n"
  ],
  "常用配置词：out 路径、opt 级别、reflect、debug、release；new 使用 name 工程名。\n": [
    "Settings: out PATH, opt LEVEL, reflect, debug, release; new accepts name NAME.\n"
  ],
  "开发场景：test tests backend all、fmt check src、deps lock project . offline。\n": [
    "Development: test tests backend all, fmt check src, deps lock project . offline.\n"
  ],
  "直接文件调用始终运行；文件后的所有内容传给程序。显式场景的高级选项：\n": [
    "A direct file invocation always runs; all following arguments belong to the program. Advanced options:\n"
  ],
  "不给源文件时从标准输入读。选项：\n": [
    "Without a source file, read standard input. Options:\n"
  ],
  "  -h, --help             显示本帮助并退出\n": [
    "  -h, --help             Show this help and exit\n"
  ],
  "  --out 前缀             生成产物的文件名前缀（缺省 build/magel_out）\n": [
    "  --out PREFIX           Output file prefix (default: build/magel_out)\n"
  ],
  "  --separate-modules     AOT 按模块缓存本地目标文件并统一链接\n": [
    "  --separate-modules     Cache AOT object files by module and link together\n"
  ],
  "  --module-cache DIR     指定 AOT 模块目标文件缓存目录\n": [
    "  --module-cache DIR     AOT module object cache directory\n"
  ],
  "  --check                仅检查，不执行、不生成产物\n": [
    "  --check                Check only; do not execute or write artifacts\n"
  ],
  "  --hard-realtime        禁止整个编译单元使用反射\n": [
    "  --hard-realtime        Forbid reflection throughout the compilation unit\n"
  ],
  "  --reflection           启用注解与反射（要求前端和运行时均编入支持）\n": [
    "  --reflection           Enable annotations/reflection (compiler and VM support required)\n"
  ],
  "  --no-reflection        关闭注解与反射（注解与反射缺省已开启）\n": [
    "  --no-reflection        Disable annotations/reflection (enabled by default)\n"
  ],
  "  --diagnostics=json     检查并输出稳定 JSON 诊断\n": [
    "  --diagnostics=json     Check and emit structured JSON diagnostics\n"
  ],
  "  test|fmt|lsp|deps      测试、格式化、语言服务与依赖锁定（原生内建）\n": [
    "  test|fmt|lsp|deps      Tests, formatting, language service, dependency locks (native)\n"
  ],
  "  --emit                 只生成 .quad / .data，不执行\n": [
    "  --emit                 Export .quad / .data without executing\n"
  ],
  "  --svmb                 额外写出可由 stackvm 直接加载的 <前缀>.svmb\n": [
    "  --svmb                 Also write <prefix>.svmb for stackvm\n"
  ],
  "  --debug                写 .svmb 时带上调试节，并在解释执行时打印位置轨迹\n": [
    "  --debug                Include .svmb debug data and interpreter location traces\n"
  ],
  "  --compress-symbols     写无名变长符号表，生成 PC 映射文件\n": [
    "  --compress-symbols     Write compact symbols and a PC mapping file\n"
  ],
  "  --no-compress-symbols  覆盖 magel 配置，关闭符号压缩\n": [
    "  --no-compress-symbols  Override configuration and disable symbol compression\n"
  ],
  "  --symbol-map FILE      映射路径（默认 <前缀>.symbol_map.json）\n": [
    "  --symbol-map FILE      Mapping path (default: <prefix>.symbol_map.json)\n"
  ],
  "  --input \"文本\"        `input(变量);` 的输入项（空白分隔）\n": [
    "  --input \"TEXT\"        Whitespace-separated input for input(variable);\n"
  ],
  "  --raw                  跳过前处理：源文件原样喂给解析器\n": [
    "  --raw                  Skip preprocessing and parse the source directly\n"
  ],
  "  --deps                 只列出 `%include` 拉进来的文件（构建脚本用）\n": [
    "  --deps                 List files loaded by %include (for build scripts)\n"
  ],
  "  --incremental          跨次复用模块预处理与完整编译产物\n": [
    "  --incremental          Reuse module preprocessing and compiled artifacts\n"
  ],
  "  --threads              启用隔离的宿主线程执行\n": [
    "  --threads              Enable isolated host-thread execution\n"
  ],
  "  --no-debug             （兼容别名）显式不带调试信息；现在也是缺省\n": [
    "  --no-debug             Omit debug information (also the default)\n"
  ],
  "  --stats                打印运行时及增量文件校验统计\n": [
    "  --stats                Show runtime and incremental validation statistics\n"
  ],
  "  --memory-size 字节     运行时堆容量（缺省 512MiB，最小 64）\n": [
    "  --memory-size BYTES    Runtime memory capacity (default: 512MiB; minimum: 64)\n"
  ],
  "  --step-limit N         本次执行的指令条数上限（缺省不设上限；0 同缺省）\n": [
    "  --step-limit N         Instruction limit (default: unlimited; 0 means unlimited)\n"
  ],
  "  -I, --include-dir 目录 追加 include 搜索目录（也可写 -I目录）\n": [
    "  -I, --include-dir DIR  Add an include search directory (also -IDIR)\n"
  ],
  "  --include-path 目录    追加 include 搜索目录\n": [
    "  --include-path DIR     Add an include search directory\n"
  ],
  "  --import-dir 目录      追加 import 搜索目录\n": [
    "  --import-dir DIR       Add an import search directory\n"
  ],
  "  -D, --define 符号[=值] 定义预处理符号（也可写 -D符号=值）\n": [
    "  -D, --define NAME[=VALUE] Define a preprocessing symbol (also -DNAME=VALUE)\n"
  ],
  "  --                     后面全部作为程序的命令行参数\n": [
    "  --                     Pass all remaining arguments to the program\n"
  ],
  "  lang zh|en|auto     选择显示语言；默认自动检测系统语言\n": [
    "  lang zh|en|auto     Select display language (default: system detection)\n"
  ],
  "选项 %s 后面要跟一个值\n": [
    "Option %s requires a value\n"
  ],
  "magel：%s 缺少取值\n": [
    "magel: %s requires a value\n"
  ],
  "不认识的选项：%s\n": [
    "Unknown option: %s\n"
  ],
  "不认识的选项：": [
    "Unknown option: "
  ],
  " 缺少取值": [
    " requires a value"
  ],
  "一次只能编译一个源文件\n": [
    "Only one source file can be compiled at a time\n"
  ],
  "一次只能编译一个源文件": [
    "Only one source file can be compiled at a time"
  ],
  "打不开文件：%s\n": [
    "Cannot open file: %s\n"
  ],
  "magel：前处理：%s\n": [
    "magel: preprocessing: %s\n"
  ],
  "magel：泛型：%s\n": [
    "magel: generics: %s\n"
  ],
  "语法错误：%s 第 %zu 行第 %u 列：%s\n": [
    "Syntax error: %s:%zu:%u: %s\n"
  ],
  "语法错误：第 %u 行第 %u 列：%s\n": [
    "Syntax error: line %u, column %u: %s\n"
  ],
  "magel：%s 第 %zu 行第 %zu 列：%s\n": [
    "magel: %s:%zu:%zu: %s\n"
  ],
  "解析失败：%s\n": [
    "Parsing failed: %s\n"
  ],
  "解析失败": [
    "Parsing failed"
  ],
  "硬实时编译禁止启用反射": [
    "Reflection is forbidden in hard real-time compilation"
  ],
  "--reflection 需要 MAGEL_WITH_REFLECTION=1 且运行时 STACKVM_WITH_REFLECTION=1": [
    "--reflection requires MAGEL_WITH_REFLECTION=1 and STACKVM_WITH_REFLECTION=1"
  ],
  "--memory-size 缺少容量\n": [
    "--memory-size requires a capacity\n"
  ],
  "--step-limit 需要非负整数（0 表示不设上限）\n": [
    "--step-limit requires a nonnegative integer (0 means unlimited)\n"
  ],
  "映射路径不能覆盖输入或字节码输出\n": [
    "The mapping path must not overwrite input or bytecode output\n"
  ],
  "--symbol-map 需要启用 --compress-symbols\n": [
    "--symbol-map requires --compress-symbols\n"
  ],
  "magel：--incremental 需要源文件，且不能与 --raw 合用\n": [
    "magel: --incremental requires a source file and cannot be combined with --raw\n"
  ],
  "用法：magel new <目录> [name 工程名]\n": [
    "Usage: magel new <DIR> [name NAME]\n"
  ],
  "创建 magel.toml、src/main.mgl、tests/test_main.mgl、README.md 和 .gitignore。\n": [
    "Create magel.toml, src/main.mgl, tests/test_main.mgl, README.md and .gitignore.\n"
  ],
  "目标目录必须不存在；父目录必须存在。工程名由英文字母开头，后接字母、数字、- 或 _。\n": [
    "The destination must not exist and its parent must exist. Names start with a letter, followed by letters, digits, - or _.\n"
  ],
  "magel：name 必须且只能指定一个工程名\n": [
    "magel: name requires exactly one project name\n"
  ],
  "目标必须是新的工程目录": [
    "The destination must be a new project directory"
  ],
  "magel：无效工程名；请用 name 指定字母开头的字母、数字、- 或 _ 组合\n": [
    "magel: invalid project name; use name with a leading letter and letters, digits, - or _\n"
  ],
  "目标已存在，拒绝覆盖": [
    "The destination already exists; refusing to overwrite"
  ],
  "无法写入工程文件：": [
    "Cannot write project file: "
  ],
  "\n\nMagel 应用工程。以下命令在工程根目录执行：\n\n": [
    "\n\nMagel application. Run these commands from the project root:\n\n"
  ],
  "运行示例输出 `Hello, Magel!`。开发命令原生内建，无需 Python；AOT 需要 C++17 工具链。\n": [
    "The example prints `Hello, Magel!`. Development commands are native and do not require Python; AOT requires a C++17 toolchain.\n"
  ],
  "`magel.toml` 配置项目包；`src/` 放应用源码，`tests/` 放普通 main 测试。\n": [
    "Configure the package in `magel.toml`; `src/` contains source files and `tests/` contains ordinary main tests.\n"
  ],
  "已创建 Magel 工程 %s：%s\n": [
    "Created Magel project %s: %s\n"
  ],
  "进入工程目录后执行：magel run src/main.mgl\n": [
    "From the project directory, run: magel run src/main.mgl\n"
  ],
  "magel：创建工程失败：%s\n": [
    "magel: project creation failed: %s\n"
  ],
  "无法创建开发工具临时目录": [
    "Cannot create the development tool temporary directory"
  ],
  "无法释放内嵌开发工具": [
    "Cannot extract the embedded development tool"
  ],
  "无法启动开发工具": [
    "Cannot start the development tool"
  ],
  "无法等待开发工具": [
    "Cannot wait for the development tool"
  ],
  "无法定位 magel 可执行文件": [
    "Cannot locate the magel executable"
  ],
  "magel：模块编译缓存只支持 aot 模式\n": [
    "magel: module compilation caching requires aot mode\n"
  ],
  "magel：checked / unchecked 只支持 aot 模式\n": [
    "magel: checked / unchecked require aot mode\n"
  ],
  "magel：clear 需要 --out-dir DIR\n": [
    "magel: clear requires --out-dir DIR\n"
  ],
  "清理完成：已删除 %zu 个生成的中间文件\n": [
    "Cleanup complete: removed %zu generated intermediate files\n"
  ],
  "magel：无法创建导出目录：%s\n": [
    "magel: cannot create the export directory: %s\n"
  ],
  "magel：导出失败；中间文件保留在 %s\n": [
    "magel: export failed; intermediate files retained in %s\n"
  ],
  "已生成可执行文件：%s（运行方式：%s）\n": [
    "Executable generated: %s (mode: %s)\n"
  ],
  "中间文件已保留在：%s\n": [
    "Intermediate files retained in: %s\n"
  ],
  "只读引用（`T const&`）只能读，不能写入元素或成员": [
    "A read-only reference (`T const&`) cannot modify elements or members"
  ],
  "只读引用（`T const&`）只能读，不能写字段（要写就用可写引用 `T mut&`）": [
    "A read-only reference (`T const&`) cannot modify fields; use `T mut&`"
  ],
  "只读引用（`T const&`）只能读，不能通过引用写": [
    "A read-only reference (`T const&`) cannot be written through"
  ],
  "只读引用（`T const&`）不能写入元素": [
    "A read-only reference (`T const&`) cannot modify elements"
  ],
  "只读接收者不能调用修改方法：": [
    "A read-only receiver cannot call a mutating method: "
  ],
  "只读常数不能赋值：": [
    "Cannot assign to a read-only constant: "
  ],
  "元组元素是只读的；请构造新元组后给整个变量赋值": [
    "Tuple elements are read-only; assign a new tuple to the whole variable"
  ],
  "引用声明必须初始化": [
    "Reference declarations must be initialized"
  ],
  "引用存储分配失败": [
    "Reference storage allocation failed"
  ],
  "泛型声明名称后需要 <...>": [
    "A generic declaration requires <...> after its name"
  ],
  "泛型参数缺少 >": [
    "Generic parameters require a closing >"
  ],
  "void 函数不能返回值": [
    "A void function cannot return a value"
  ],
  "注解参数类型不匹配：禁止隐式转换": [
    "Annotation argument type mismatch; implicit conversion is forbidden"
  ],
  "注解参数个数过多：": [
    "Too many annotation arguments: "
  ],
  "反射调用 API 与具体结构体或枚举类型不匹配": [
    "Reflection API does not match the concrete struct or enum type"
  ],
  "运行期错误：": [
    "Runtime error: "
  ],
  "\n调用栈（外 → 内）：": [
    "\nCall stack (outer to inner):"
  ],
  "调用栈（外 → 内）：\n": [
    "Call stack (outer to inner):\n"
  ],
  "panic：访问未分配内存（读）：地址 ": [
    "panic: read from unallocated memory at address "
  ],
  "panic：访问未分配内存（写）：地址 ": [
    "panic: write to unallocated memory at address "
  ],
  "字节": [
    " bytes"
  ],
  "未知类型：": [
    "Unknown type: "
  ],
  "类型不存在：": [
    "Type does not exist: "
  ],
  "未声明的变量：": [
    "Undeclared variable: "
  ],
  "找不到变量：": [
    "Variable not found: "
  ],
  "读取失败": [
    "Read failed"
  ],
  "写入失败": [
    "Write failed"
  ],
  "索引越界": [
    "Index out of bounds"
  ],
  "除零": [
    "Division by zero"
  ],
  "栈溢出": [
    "Stack overflow"
  ],
  "内存不足": [
    "Out of memory"
  ],
  "无法创建输出目录：": [
    "Cannot create output directory: "
  ],
  "输出目录不能是符号链接：": [
    "Output directory must not be a symbolic link: "
  ],
  "不安全路径：": [
    "Unsafe path: "
  ],
  "无法删除产物：": [
    "Cannot remove artifact: "
  ],
  "无法检查产物：": [
    "Cannot inspect artifact: "
  ],
  "写不出统一产物清单：": [
    "Cannot write artifact manifest: "
  ],
  "读不出统一产物清单": [
    "Cannot read artifact manifest"
  ],
  "统一产物清单版本无法识别": [
    "Unsupported artifact manifest version"
  ],
  "统一产物清单包含不安全路径：": [
    "Artifact manifest contains an unsafe path: "
  ],
  "magel：不认识的运行方式：%s\n": [
    "magel: unknown execution mode: %s\n"
  ],
  "magel：不认识的运行方式：%s（用 interpret|svmb|aot）\n": [
    "magel: unknown execution mode: %s (use interpret|svmb|aot)\n"
  ],
  "magel：子命令与 --mode=%s 冲突\n": [
    "magel: command conflicts with --mode=%s\n"
  ],
  "magel：找不到仓库树（缺 magel/source/magel-emit/aot.cpp），无法转发 %s\n": [
    "magel: repository tree missing (magel/source/magel-emit/aot.cpp); cannot forward %s\n"
  ],
  "magel：这个构建不含 aot 导出；请用仓库的统一 magel 或 magel-emit\n": [
    "magel: this build lacks AOT export; use the unified magel or magel-emit\n"
  ],
  "magel：这个构建不含 %s 导出；请用仓库的统一 magel 或 magel-emit\n": [
    "magel: this build lacks %s export; use the unified magel or magel-emit\n"
  ],
  "没有声明的类型：": [
    "Undeclared type: "
  ],
  "变量": [
    "variable"
  ],
  "参数": [
    "parameter"
  ],
  "返回类型": [
    "return type"
  ],
  "字段": [
    "field"
  ],
  "静态成员": [
    "static member"
  ],
  "声明": [
    "declaration"
  ],
  "用法：magel new <目录> [name 工程名]\n创建 magel.toml、src/main.mgl、tests/test_main.mgl、README.md 和 .gitignore。\n目标目录必须不存在；父目录必须存在。工程名由英文字母开头，后接字母、数字、- 或 _。\n": [
    "Usage: magel new <DIR> [name NAME]\nCreate magel.toml, src/main.mgl, tests/test_main.mgl, README.md and .gitignore.\nThe destination must not exist and its parent must exist. Names start with a letter, followed by letters, digits, - or _.\n"
  ],
  "\n\nMagel 应用工程。以下命令在工程根目录执行：\n\n```sh\nmagel run src/main.mgl\nmagel check src/main.mgl\nmagel test tests\nmagel aot src/main.mgl out build/main\n```\n\n运行示例输出 `Hello, Magel!`。开发命令原生内建，无需 Python；AOT 需要 C++17 工具链。\n`magel.toml` 配置项目包；`src/` 放应用源码，`tests/` 放普通 main 测试。\n": [
    "\n\nMagel application. Run these commands from the project root:\n\n```sh\nmagel run src/main.mgl\nmagel check src/main.mgl\nmagel test tests\nmagel aot src/main.mgl out build/main\n```\n\nThe example prints `Hello, Magel!`. Development commands are native and do not require Python; AOT requires a C++17 toolchain.\nConfigure the package in `magel.toml`; `src/` contains source files and `tests/` contains ordinary main tests.\n"
  ],
  "用法：": [
    "Usage: "
  ],
  "用法：\n": [
    "Usage:\n"
  ],
  "              [--checked | --unchecked] AOT 默认 checked；unchecked 关闭可选护栏与 panic 跟踪\n": [
    "              [--checked | --unchecked] Default: checked; unchecked disables optional memory guards and panic traces\n"
  ],
  "把四元式和数据区编译为自包含 AOT 可执行文件。默认目标 cpp-aot。\n": [
    "Compile quadruples and data into a standalone AOT executable. Default target: cpp-aot.\n"
  ],
  "  --cpp-output 导出程序时保留生成的 C++ 源码（缺省不写/不留代码）；\n": [
    "  --cpp-output Keep generated C++ source (not retained by default).\n"
  ],
  "  --cpp-output-dir DIR 指定源码写到哪里（隐含 --cpp-output）；\n": [
    "  --cpp-output-dir DIR Source output directory (implies --cpp-output).\n"
  ],
  "  --separate-modules 按 MAGEL 模块编译本地目标文件并缓存；仅 cpp-aot。\n": [
    "  --separate-modules Compile and cache objects by Magel module; cpp-aot only.\n"
  ],
  "  --module-cache DIR 指定模块目标文件缓存目录。\n": [
    "  --module-cache DIR Module object cache directory.\n"
  ],
  "  --execute-output FILE 指定可执行文件 / 二进制的位置（等价扩展 --out）。\n": [
    "  --execute-output FILE Executable/binary output path (an alternative to --out).\n"
  ],
  "  --cxx PATH 编译 / 链接用的 C++ 编译器（缺省环境变量 CXX，再 g++）；\n": [
    "  --cxx PATH C++ compiler/linker (default: CXX environment variable, then g++).\n"
  ],
  "  --g++ PATH / --gxx PATH 是 --cxx 的别名，--gcc PATH 是 --cc 的别名。\n": [
    "  --g++ PATH / --gxx PATH Alias for --cxx; --gcc PATH is an alias for --cc.\n"
  ],
  "  --cc PATH  C 编译器（写进构建脚本的 CC；AOT 产物是 C++，用 --cxx 编）。\n": [
    "  --cc PATH C compiler for the build script; AOT uses the C++ compiler --cxx.\n"
  ],
  "  --make PATH 用指定 make 驱动编译（先生成 <exe>.aot.mk 构建脚本再跑 make；\n": [
    "  --make PATH Build using make and a generated <exe>.aot.mk script;\n"
  ],
  "    缺省不传就直接调编译器）。构建脚本是中间文件，成功后按清单删除。\n": [
    "    by default, invoke the compiler directly. Remove the script after a successful build.\n"
  ],
  "--glue 用于 C++ 导出：传一个原样内嵌的 .cpp（里面定义 bindMagelGlue）。\n": [
    "--glue Embed a .cpp file defining bindMagelGlue for C++ export.\n"
  ],
  "--runtime-arena BYTES 指定运行内存池总容量（默认 512 MiB）。\n": [
    "--runtime-arena BYTES Runtime memory pool capacity (default: 512 MiB).\n"
  ],
  "  --no-panic-details 隐藏 panic 路径及调用记录（上游符号压缩时自动启用）。\n": [
    "  --no-panic-details Hide panic paths and call traces (automatic with compressed symbols).\n"
  ],
  "默认导出程序不带调试信息表（位置没了，程序行为不变）；--debug 才带上。\n": [
    "Debug information is omitted by default; use --debug to include it.\n"
  ],
  "--threads 给 C++ 产物启用隔离的宿主线程与文本消息。\n": [
    "--threads Enable isolated host threads and text messages in C++ artifacts.\n"
  ],
  "默认输出为四元式同名可执行文件；\n": [
    "The default executable uses the quadruple file's name.\n"
  ],
  "已生成 ": [
    "Generated "
  ],
  "（保留中间文件）\n": [
    " (intermediate files retained)\n"
  ],
  "（已清理 ": [
    " (removed "
  ],
  " 个中间文件；用 --keep-intermediate 保留）\n": [
    " intermediate files; use --keep-intermediate to retain)\n"
  ],
  "  生成源码：": [
    "  Generated source: "
  ],
  "  目标文件：": [
    "  Object file: "
  ],
  "  编译日志：": [
    "  Build log: "
  ],
  "  构建脚本：": [
    "  Build script: "
  ],
  "  优化级别：": [
    "  Optimization level: "
  ],
  "  构建脚本已删（--keep-intermediate 可保留）\n": [
    "  Build script removed (use --keep-intermediate to retain)\n"
  ],
  "警告：中间文件清理失败：": [
    "Warning: intermediate file cleanup failed: "
  ],
  "已生成 Magel 编译器工程：": [
    "Generated Magel compiler project: "
  ],
  " 个文件，lang.mgl 来自引用模板）\n": [
    " files; lang.mgl generated from the template)\n"
  ],
  "清理完成，共删除 ": [
    "Cleanup complete; removed "
  ],
  " 个编译器文件\n": [
    " compiler files\n"
  ],
  "编译器工程导出失败：": [
    "Compiler project export failed: "
  ],
  "读取编译器源文件失败\n": [
    "Cannot read compiler source files\n"
  ],
  "编译器模板渲染失败：": [
    "Compiler template rendering failed: "
  ],
  "，字节数 ": [
    ", byte count "
  ],
  "%s：运行失败：%s\n": [
    "%s: execution failed: %s\n"
  ],
  "运行时分配器：内存池容量不足\n": [
    "Runtime allocator: memory pool capacity exceeded\n"
  ],
  "运行失败：": [
    "Execution failed: "
  ],
  "引用声明必须初始化，缺省值请使用 Option<T mut&>": [
    "References must be initialized; use Option<T mut&> for an absent value"
  ],
  "[MGL1204] 保存的引用不能超过来源的生命周期": [
    "[MGL1204] A stored reference cannot outlive its source"
  ],
  "只读引用（`T const&`）只能读，不能通过引用写（要写就用可写引用 `T mut&`）": [
    "A read-only reference (`T const&`) cannot be written through; use `T mut&`"
  ],
  "Magel 开发工具：编译器驱动的测试与诊断、保守格式化、LSP 和可复现依赖锁定。": [
    "Magel development tools: compiler-driven tests, formatting, LSP and reproducible dependency locks."
  ],
  "发现并运行 test_*.mgl / *_test.mgl": [
    "Discover and run test_*.mgl / *_test.mgl"
  ],
  "规范缩进；保持字面量和宏不变": [
    "Normalize indentation; preserve literals and macros"
  ],
  "启动 stdio 语言服务器": [
    "Start the stdio language server"
  ],
  "锁定或离线校验依赖": [
    "Lock dependencies or validate them offline"
  ],
  "package.compiler 必须为 >=X.Y.Z": [
    "package.compiler must use >=X.Y.Z"
  ],
  "Git 依赖必须使用 git+URL#完整40位提交号": [
    "Git dependencies require git+URL# followed by a full 40-digit commit hash"
  ],
  "LSP 消息过大": [
    "LSP message exceeds the size limit"
  ],
  "非法依赖名称：": [
    "Invalid dependency name: "
  ],
  "离线缓存缺失：": [
    "Offline cache missing: "
  ],
  "；请先运行 magel deps lock": [
    "; run magel deps lock first"
  ],
  "依赖目录不支持符号链接：": [
    "Symbolic links are forbidden in dependency directories: "
  ],
  "依赖文件不支持符号链接：": [
    "Symbolic links are forbidden in dependency files: "
  ],
  "编译器版本不满足 ": [
    "Compiler version does not satisfy "
  ],
  " 需要非空来源字符串": [
    " requires a nonempty source string"
  ],
  "依赖 ": [
    "Dependency "
  ],
  "usage: ": [
    "Usage: "
  ],
  "positional arguments": [
    "Positional arguments"
  ],
  "options": [
    "Options"
  ],
  "show this help message and exit": [
    "Show this help message and exit"
  ],
  "lambda 不能按值捕获引用或不可安全复制的资源：": [
    "lambda cannot capture references or resources without safe value copying: "
  ],
  "lambda 不能隐式捕获 self 或接收者字段：": [
    "lambda cannot implicitly capture self or receiver fields: "
  ],
  "定时器异步入口不支持带捕获的 lambda": [
    "Timer callbacks do not support capturing lambdas"
  ],
  "线程异步入口不支持带捕获的 lambda": [
    "Thread entry points do not support capturing lambdas"
  ],
  "协程异步入口不支持带捕获的 lambda": [
    "Coroutine entry points do not support capturing lambdas"
  ],
  "用法：magel test [路径...] [--filter 文本] [--backend interpret|aot|all] [--timeout 秒] [--json|--list]\n": [
    "Usage: magel test [PATH...] [--filter TEXT] [--backend interpret|aot|all] [--timeout SECONDS] [--json|--list]\n"
  ],
  "用法：magel fmt 路径... [--check|--write]\n": [
    "Usage: magel fmt PATH... [--check|--write]\n"
  ],
  "用法：magel deps lock|check [--project 目录] [--offline]\n": [
    "Usage: magel deps lock|check [--project DIR] [--offline]\n"
  ],
  "用法：magel lsp [--macros]\n": [
    "Usage: magel lsp [--macros]\n"
  ],
  "核心与类型": [
    "Core and types"
  ],
  "容器与迭代": [
    "Collections and iteration"
  ],
  "文本与数据": [
    "Text and data"
  ],
  "数学与数值": [
    "Mathematics and numerics"
  ],
  "输入输出与集成": [
    "I/O and integration"
  ],
  "并发与事件": [
    "Concurrency and events"
  ],
  "内存与系统": [
    "Memory and system"
  ],
  "Magel 标准库手册": [
    "Magel standard library manual"
  ],
  "返回值": [
    "Return value"
  ],
  "默认值": [
    "Default value"
  ],
  "说明": [
    "Description"
  ],
  "说明原文语言：zh": [
    "Documentation source language: zh"
  ],
  "错误与边界": [
    "Errors and boundaries"
  ],
  "调用形式": [
    "Usage"
  ],
  "完整示例": [
    "Complete example"
  ],
  "示例输出": [
    "Example output"
  ],
  "候选目标": [
    "Suggested targets"
  ],
  "未找到手册目标：": [
    "Manual target not found: "
  ],
  "该目标没有可运行示例": [
    "No runnable example for this target"
  ],
  "手册示例执行失败": [
    "Manual example failed"
  ],
  "--search 后需要关键词": [
    "--search requires a keyword"
  ],
  "手册参数无效：": [
    "Invalid manual argument: "
  ],
  "手册参数不能组合使用": [
    "Incompatible manual arguments"
  ],
  "--update 需要 Magel 源码仓库和 Python 构建环境": [
    "--update requires the Magel source repository and Python build environment"
  ],
  "手册数据已生成；重新构建后进入可执行文件。\n": [
    "Manual data generated; rebuild to embed it in the executable.\n"
  ],
  "用法：magel man [目标] [--search 关键词] [--exact] [--json] [--run] [--lang zh|en] [--update]\n": [
    "Usage: magel man [TARGET] [--search KEYWORD] [--exact] [--json] [--run] [--lang zh|en] [--update]\n"
  ],
  "  man [目标]            查询内嵌标准库手册、参数、边界与示例\n": [
    "  man [TARGET]          Query embedded library docs, parameters, boundaries and examples\n"
  ],
  "按值捕获并转移外部变量所有权；创建闭包后，被移出的源变量不能继续读取。": [
    "Capture external values by moving ownership; moved sources cannot be read after closure creation."
  ],
  "按值捕获外部变量的副本；原变量仍可继续使用。": [
    "Capture copies of external values; the originals remain usable."
  ],
  "按只读借用捕获外部变量；借用来源必须在闭包使用期间保持有效。": [
    "Capture external values by read-only borrow; sources must remain valid while the closure is used."
  ],
  "按可写借用捕获外部变量；闭包读写当前值，借用来源必须保持有效。": [
    "Capture external values by mutable borrow; the closure accesses current values and sources must remain valid."
  ],
  "用法：magel 文件.mgl [程序参数...]\n      magel 场景 [文件.mgl] [配置词...] [-- 程序参数...]\n\n统一工具链：解释、导出 .svmb、导出 AOT 可执行文件都在这一个程序里。\n场景指令（运行方式也可用 --mode 指定）：\n  run|interpret  内部解释运行（缺省）；直接传入文件时，其后全为程序参数\n  check FILE [json]    仅检查，可输出 JSON 诊断\n  help                 显示帮助\n  emit           只出 .quad / .data.bin / .layout\n  svmb           只导出 <前缀>.svmb，不执行\n  aot FILE [out EXE] [opt LEVEL] [checked|unchecked]  导出 AOT（默认 checked）\n  new DIR [name NAME]  新建 Magel 应用工程（目标目录必须不存在）\n  gen            生成独立编译器工程（转发 magel-generator compiler）\n  clear out DIR  清理登记过的中间产物（不重新编译）\n  lang zh|en|auto     选择显示语言；默认自动检测系统语言\n常用配置词：out 路径、opt 级别、reflect、debug、release；new 使用 name 工程名。\n开发场景：test tests backend all、fmt check src、deps lock project . offline。\n直接文件调用始终运行；文件后的所有内容传给程序。显式场景的高级选项：\n不给源文件时从标准输入读。选项：\n  -h, --help             显示本帮助并退出\n  --out 前缀             生成产物的文件名前缀（缺省 build/magel_out）\n  --separate-modules     AOT 按模块缓存本地目标文件并统一链接\n  --shared / --object    AOT 库输出，可省略 main；.dll/.so/.o 自动选择类型\n  --module-cache DIR     指定 AOT 模块目标文件缓存目录\n  --check                仅检查，不执行、不生成产物\n  --hard-realtime        禁止整个编译单元使用反射\n  --reflection           启用注解与反射（要求前端和运行时均编入支持）\n  --no-reflection        关闭注解与反射（注解与反射缺省已开启）\n  --diagnostics=json     检查并输出稳定 JSON 诊断\n  test|fmt|lsp|deps      测试、格式化、语言服务与依赖锁定（原生内建）\n  --emit                 只生成 .quad / .data.bin / .layout，不执行\n  --svmb                 额外写出可由 stackvm 直接加载的 <前缀>.svmb\n  --debug                写 .svmb 时带上调试节，并在解释执行时打印位置轨迹\n  --dap                  作为调试适配器运行（VS Code 断点调试用，stdio 收发 DAP）\n  --compress-symbols     写无名变长符号表，生成 PC 映射文件\n  --no-compress-symbols  覆盖 magel 配置，关闭符号压缩\n  --symbol-map FILE      映射路径（默认 <前缀>.symbol_map.json）\n  --input \"文本\"        `input(变量);` 的输入项（空白分隔）；不给就从标准读，调试时可直接敲\n  --raw                  跳过前处理：源文件原样喂给解析器\n  --deps                 只列出 `%include` 拉进来的文件（构建脚本用）\n  --incremental          跨次复用模块预处理与完整编译产物\n  --threads              启用隔离的宿主线程执行\n  --no-debug             （兼容别名）显式不带调试信息；现在也是缺省\n  --stats                打印运行时及增量文件校验统计\n  --memory-size 字节     运行时堆容量（缺省 512MiB，最小 64）\n  --watchdog             打开可选软重启看门狗（分配失败 / 显式请求 / 超水位重跑）\n  --no-watchdog          关闭软重启看门狗（缺省）\n  --max-restarts N       最多软重启几次（缺省 3；隐含 --watchdog）\n  --watchdog-watermark N 内存水位（字节；隐含 --watchdog）\n  --step-limit N         本次执行的指令条数上限（缺省不设上限；0 同缺省）\n  -I, --include-dir 目录 追加 include 搜索目录（也可写 -I目录）\n  --include-path 目录    追加 include 搜索目录\n  --import-dir 目录      追加 import 搜索目录\n  -D, --define 符号[=值] 定义预处理符号（也可写 -D符号=值）\n  --                     后面全部作为程序的命令行参数\n": [
    "Usage: magel FILE.mgl [program arguments...]\n       magel COMMAND [FILE.mgl] [settings...] [-- program arguments...]\n\nOne toolchain: interpret, export .svmb, or build native AOT executables.\nCommands (execution mode can also be selected with --mode):\n  run|interpret  Interpret (default); arguments after a direct file go to the program\n  check FILE [json]    Check only; optionally emit JSON diagnostics\n  help                 Show help\n  emit           Export .quad / .data.bin / .layout only\n  svmb           Export <prefix>.svmb without executing\n  aot FILE [out EXE] [opt LEVEL] [checked|unchecked]  Build AOT (default: checked)\n  new DIR [name NAME]  Create a Magel project (destination must not exist)\n  gen            Generate a standalone compiler project (magel-generator compiler)\n  clear out DIR  Remove registered intermediate files without recompiling\n  lang zh|en|auto     Select display language (default: system detection)\nSettings: out PATH, opt LEVEL, reflect, debug, release; new accepts name NAME.\nDevelopment: test tests backend all, fmt check src, deps lock project . offline.\nA direct file invocation always runs; all following arguments belong to the program. Advanced options:\nWithout a source file, read standard input. Options:\n  -h, --help             Show this help and exit\n  --out PREFIX           Output file prefix (default: build/magel_out)\n  --separate-modules     Cache AOT object files by module and link together\n  --shared / --object    AOT library output; main optional; .dll/.so/.o select the output kind\n  --module-cache DIR     AOT module object cache directory\n  --check                Check only; do not execute or write artifacts\n  --hard-realtime        Forbid reflection throughout the compilation unit\n  --reflection           Enable annotations/reflection (compiler and VM support required)\n  --no-reflection        Disable annotations/reflection (enabled by default)\n  --diagnostics=json     Check and emit structured JSON diagnostics\n  test|fmt|lsp|deps      Tests, formatting, language service, dependency locks (native)\n  --emit                 Export .quad / .data.bin without executing\n  --svmb                 Also write <prefix>.svmb for stackvm\n  --debug                Include .svmb debug data and interpreter location traces\n  --compress-symbols     Write compact symbols and a PC mapping file\n  --no-compress-symbols  Override configuration and disable symbol compression\n  --symbol-map FILE      Mapping path (default: <prefix>.symbol_map.json)\n  --input \"TEXT\"        Whitespace-separated input for input(variable);\n  --raw                  Skip preprocessing and parse the source directly\n  --deps                 List files loaded by %include (for build scripts)\n  --incremental          Reuse module preprocessing and compiled artifacts\n  --threads              Enable isolated host-thread execution\n  --no-debug             Omit debug information (also the default)\n  --stats                Show runtime and incremental validation statistics\n  --memory-size BYTES    Runtime memory capacity (default: 512MiB; minimum: 64)\n  --step-limit N         Instruction limit (default: unlimited; 0 means unlimited)\n  -I, --include-dir DIR  Add an include search directory (also -IDIR)\n  --include-path DIR     Add an include search directory\n  --import-dir DIR       Add an import search directory\n  -D, --define NAME[=VALUE] Define a preprocessing symbol (also -DNAME=VALUE)\n  --                     Pass all remaining arguments to the program\n  --dap                  Run the stdio debug adapter for VSCode.\n  --watchdog             Enable failure recovery and memory monitoring.\n  --no-watchdog          Disable failure recovery (default).\n  --max-restarts N       Maximum restart count (default: 3).\n  --watchdog-watermark N Memory watermark in bytes.\n"
  ]
}
# MAGEL_CATALOG_END


def _text(original):
    """只翻译工具定义的消息，缺失译文时保留完整原文。"""
    choice = _language()
    if choice == "zh":
        return {"usage: ": "用法：", "positional arguments": "位置参数", "options": "选项", "show this help message and exit": "显示帮助并退出"}.get(original, original)
    return _MESSAGES.get(original, (original, original))[0]


VERSION = "1.0.0"
IGNORED = {".git", ".magel", "build", "tmp", "node_modules", "__pycache__"}
EXTENSIONS = {".mgl", ".inc", ".h", ".hpp", ".c", ".cpp", ".toml", ".json"}
KEYWORDS = "_ abi auto bool break cast char const continue copy defer delete do drop elif else enum export false float32 float64 fn for heap if impl import in inline int16 int32 int64 int8 interface loop match module move mut new return scope static struct true type uint16 uint32 uint64 uint8 void while yield".split()


def invoke(args, **kwargs):
    """按 argv 执行子进程，Windows 不显示额外控制台。"""
    if os.name == "nt":
        kwargs["creationflags"] = subprocess.CREATE_NO_WINDOW
    return subprocess.run([str(x) for x in args], **kwargs)


def fingerprint(root):
    """源码/配置的 FNV-1a 指纹；与编译器一致地规范 CRLF 和相对文件名。"""
    names = []
    for directory, dirs, files in os.walk(root):
        for name in list(dirs):
            path = Path(directory) / name
            if path.is_symlink():
                raise ValueError(f"依赖目录不支持符号链接：{path}")
            if name in IGNORED:
                dirs.remove(name)
        for name in files:
            path = Path(directory) / name
            if path.is_symlink():
                raise ValueError(f"依赖文件不支持符号链接：{path}")
            if path.suffix in EXTENSIONS or name == "magel.lock":
                names.append(path.relative_to(root).as_posix())
    value = 14695981039346656037
    for name in sorted(names, key=lambda n: n.encode("utf-8")):
        data = name.encode("utf-8") + b"\0" + (root / name).read_bytes().replace(b"\r\n", b"\n") + b"\0"
        for byte in data:
            value = ((value ^ byte) * 1099511628211) & ((1 << 64) - 1)
    return f"{value:016x}"


def manifest(path):
    """读取 BOM/UTF-8 项目清单。"""
    return tomllib.loads(path.read_text(encoding="utf-8-sig"))


def check_version(data):
    """支持明确的最低三段版本约束。"""
    requirement = data.get("package", {}).get("compiler", "")
    if requirement:
        if not re.fullmatch(r">=\d+\.\d+\.\d+", requirement):
            raise ValueError(_text("package.compiler 必须为 >=X.Y.Z"))
        if tuple(map(int, VERSION.split('.'))) < tuple(map(int, requirement[2:].split('.'))):
            raise ValueError(f"编译器版本不满足 {requirement}")


def extract_dependency(archive, destination):
    """展开 ZIP/TAR 普通文件，拒绝越界路径、链接及过量解压，不执行包内脚本。"""
    count=total=0
    def target(name,size):
        nonlocal count,total
        normalized=name.replace('\\','/')
        parts=Path(normalized).parts
        if not normalized or normalized.startswith('/') or ':' in normalized or '..' in parts:
            raise ValueError(f"在线依赖包含不安全路径：{name}")
        path=destination/normalized
        count+=1;total+=size
        if count>10000 or total>256*1024*1024:
            raise ValueError("在线依赖解压大小或文件数量超出限制")
        path.parent.mkdir(parents=True,exist_ok=True)
        return path
    if zipfile.is_zipfile(archive):
        with zipfile.ZipFile(archive) as package:
            for member in package.infolist():
                mode=member.external_attr>>16
                if stat.S_ISLNK(mode) or stat.S_IFMT(mode) not in (0,stat.S_IFREG,stat.S_IFDIR):
                    raise ValueError("在线依赖不支持链接或特殊文件")
                path=target(member.filename,member.file_size)
                if member.is_dir():path.mkdir(exist_ok=True)
                else:
                    with package.open(member) as source,path.open('wb') as output:shutil.copyfileobj(source,output)
    else:
        with tarfile.open(archive,'r:*') as package:
            for member in package:
                if not member.isfile() and not member.isdir():
                    raise ValueError("在线依赖不支持链接或特殊文件")
                path=target(member.name,member.size)
                if member.isdir():path.mkdir(exist_ok=True)
                else:
                    with package.extractfile(member) as source,path.open('wb') as output:shutil.copyfileobj(source,output)
    if (destination/'magel.toml').is_file():return destination
    children=list(destination.iterdir())
    if len(children)==1 and children[0].is_dir() and (children[0]/'magel.toml').is_file():return children[0]
    raise ValueError("在线依赖压缩包根目录或唯一顶层目录需要 magel.toml")


def dependency_root(base, name, source, offline, refresh, libraries=None):
    """路径依赖直接定位；固定 Git 提交与带 SHA-256 的 HTTP 包存入工具链 libraries。"""
    if not re.fullmatch(r"[A-Za-z0-9_-]+", name):
        raise ValueError(f"非法依赖名称：{name}")
    if not isinstance(source, str) or not source:
        raise ValueError(f"依赖 {name} 需要非空来源字符串")
    libraries=Path(libraries or Path(__file__).resolve().parents[1]/'libraries').resolve()
    if source.startswith(('http://','https://')):
        match=re.fullmatch(r'(https?://[^#]+)#sha256=([0-9a-fA-F]{64})',source)
        if not match:raise ValueError("HTTP 依赖必须使用 URL#sha256=完整64位摘要")
        url,digest=match.groups();digest=digest.lower()
        root=libraries/(name+'-'+digest)
        if root.exists():return root.resolve()
        if offline or not refresh:raise ValueError(f"离线缓存缺失：{name}；请先运行 magel deps lock")
        libraries.mkdir(parents=True,exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='.download-',dir=libraries) as folder:
            temporary=Path(folder);archive=temporary/'package';checksum=hashlib.sha256();size=0
            with urlopen(url,timeout=30) as response,archive.open('wb') as output:
                if urlparse(response.geturl()).scheme not in ('http','https'):
                    raise ValueError("在线依赖重定向到不支持的协议")
                while chunk:=response.read(64*1024):
                    size+=len(chunk)
                    if size>64*1024*1024:raise ValueError("在线依赖下载超过 64 MiB")
                    checksum.update(chunk);output.write(chunk)
            if checksum.hexdigest()!=digest:raise ValueError(f"在线依赖 SHA-256 不匹配：{name}")
            extracted=temporary/'unpacked';extracted.mkdir()
            package=extract_dependency(archive,extracted)
            # 完整展开并验证清单后再发布缓存，失败不留下半成品。
            manifest(package/'magel.toml')
            try:package.rename(root)
            except OSError:
                if not root.is_dir():raise
        return root.resolve()
    if not source.startswith("git+"):
        return (base / source.removeprefix("path:")).resolve()
    match = re.fullmatch(r"git\+(.+)#([0-9a-fA-F]{40})", source)
    if not match or match[1].startswith('-'):
        raise ValueError(_text("Git 依赖必须使用 git+URL#完整40位提交号"))
    url, commit = match.groups()
    root = libraries / f"{name}-{commit}"
    if not root.exists():
        # 已有锁文件仍可使用旧版项目缓存；新下载统一进入 libraries。
        legacy=base/'.magel/deps'/f"{name}-{commit}"
        if legacy.exists():root=legacy
        elif offline or not refresh:
            raise ValueError(f"离线缓存缺失：{name}；请先运行 magel deps lock")
        else:
            libraries.mkdir(parents=True,exist_ok=True)
            with tempfile.TemporaryDirectory(prefix='.git-download-',dir=libraries) as folder:
                checkout=Path(folder)/'package'
                invoke(["git", "clone", "--no-checkout", "--", url, checkout], check=True, stdout=sys.stderr)
                invoke(["git", "-C", checkout, "checkout", "--detach", commit], check=True, stdout=sys.stderr)
                checkout.rename(root)
    result = invoke(["git", "-C", root, "rev-parse", "HEAD"], check=True, capture_output=True, text=True)
    if result.stdout.strip().lower() != commit.lower():
        raise ValueError(f"依赖 {name} 缓存提交号不符")
    origin=invoke(['git','-C',root,'remote','get-url','origin'],check=True,capture_output=True,text=True)
    if origin.stdout.strip()!=url:raise ValueError(f"依赖 {name} 缓存来源不符")
    return root.resolve()


def lock_project(base, offline, refresh, active=None, libraries=None):
    """递归生成或校验锁文件，拒绝循环依赖；编译前不隐式更新锁文件。"""
    active = set() if active is None else active
    base = base.resolve()
    if base in active or len(active) >= 128:
        raise ValueError(f"循环依赖或嵌套过深：{base}")
    active.add(base)
    try:
        data = manifest(base / "magel.toml")
        check_version(data)
        dependencies = data.get("dependencies", {})
        records = {}
        for name, source in sorted(dependencies.items()):
            root = dependency_root(base, name, source, offline, refresh, libraries)
            child = manifest(root / "magel.toml")
            if child.get("dependencies") or (root / "magel.lock").exists():
                lock_project(root, offline, refresh, active, libraries)
            check_version(child)
            try:relative = Path(os.path.relpath(root, base)).as_posix()
            except ValueError:relative = root.as_posix()  # Windows 跨盘目录使用绝对路径。
            records[name] = [source, relative, child.get("package", {}).get("version", ""), fingerprint(root)]
        path = base / "magel.lock"
        if refresh:
            lines = ['[lock]', 'version = "1"', f'compiler = "{VERSION}"', '', '[dependencies]']
            for name, record in records.items():
                lines.append(name + " = " + json.dumps(record, ensure_ascii=False))
            text = '\n'.join(lines) + '\n'
            with tempfile.NamedTemporaryFile(mode="w", encoding="utf-8", newline="\n", dir=base, prefix=".magel-lock-", delete=False) as output:
                output.write(text)
                temporary = output.name
            os.replace(temporary, path)
        else:
            locked = manifest(path)
            if locked.get("lock", {}).get("version") != "1" or locked.get("dependencies", {}) != records:
                raise ValueError(f"锁文件与来源或内容不符：{path}；运行 magel deps lock")
        return path
    finally:
        active.remove(base)


def deps_command(args):
    """管理最近项目或显式项目的依赖锁定。"""
    base = args.project.resolve()
    if base.is_file():
        base = base.parent
    while not (base / "magel.toml").exists():
        if base.parent == base:
            raise ValueError("找不到 magel.toml")
        base = base.parent
    result = lock_project(base, args.offline, args.action == "lock", libraries=args.magel.parent/"libraries")
    print(f"依赖{ '已锁定' if args.action == 'lock' else '校验通过' }：{result}")
    return 0


def tokens(text):
    """生成排除字符串和注释的词法片段，原始偏移保持不变。"""
    return [m for m in TOKEN.finditer(text) if not m[0].startswith(('//', '/*', '"', "'"))]


def formatted(text):
    """仅规范缩进和代码行尾空白；多行注释、字符串及宏指令逐字保留。"""
    mask = list(text)
    protected = set()
    for match in TOKEN.finditer(text):
        if match[0].startswith(('//', '/*', '"', "'")):
            first = text.count('\n', 0, match.start())
            if '\n' in match[0]:
                protected.update(range(first, first + match[0].count('\n') + 1))
            for i in range(match.start(), match.end()):
                if mask[i] != '\n':
                    mask[i] = ' '
    depth = 0
    result = []
    for number, (line, code) in enumerate(zip(text.splitlines(), ''.join(mask).splitlines())):
        stripped = code.lstrip()
        closing = len(stripped) - len(stripped.lstrip('}'))
        indent = max(0, depth - closing)
        if number in protected or line.lstrip().startswith(('$', '%')):
            result.append(line)
        else:
            result.append('    ' * indent + line.strip() if line.strip() else '')
        depth = max(0, depth + code.count('{') - code.count('}'))
    return '\n'.join(result) + ('\n' if text else '')


def fmt_command(args):
    """检查或写回保守格式；目录递归选择 .mgl 文件。"""
    files = set()
    for entry in args.paths:
        files.update(entry.rglob('*.mgl') if entry.is_dir() else [entry])
    changed = []
    for path in sorted(files):
        original = path.read_bytes().decode('utf-8-sig')
        output = formatted(original)
        if output != original.replace('\r\n', '\n'):
            changed.append(str(path))
            if args.write:
                path.write_bytes(output.encode('utf-8'))
        if not args.check and not args.write:
            if len(files) > 1:
                raise ValueError("多个文件请使用 --write 或 --check")
            sys.stdout.write(output)
    if args.check:
        for name in changed:
            print(f"需要格式化：{name}", file=sys.stderr)
        return int(bool(changed))
    return 0


def test_command(args):
    """逐文件隔离执行测试，支持过滤、跳过、超时与三后端输出比较。"""
    files = set()
    for path in args.paths:
        if path.is_dir():
            files.update(p for p in path.rglob('*.mgl') if p.name.startswith('test_') or p.name.endswith('_test.mgl'))
        elif path.suffix == '.mgl':
            files.add(path)
    files = [p.resolve() for p in sorted(files) if not args.filter or args.filter in str(p)]
    if args.list:
        for p in files:
            print(p)
        return 0
    if not files:
        raise ValueError("没有找到测试文件（test_*.mgl 或 *_test.mgl）")
    results = []
    for source in files:
        started = time.monotonic()
        result = {"file": str(source), "status": "passed", "message": ""}
        sidecar = source.with_suffix('.test.toml')
        config = manifest(sidecar) if sidecar.exists() else {}
        if config.get('skip'):
            result.update(status="skipped", message=str(config['skip']))
        else:
            try:
                outputs = []
                with tempfile.TemporaryDirectory(prefix='magel-test-') as directory:
                    directory = Path(directory)
                    for backend in (('interpret', 'aot') if args.backend == 'all' else (args.backend,)):
                        prefix = directory / ('test-' + backend + ('.exe' if os.name == 'nt' else ''))
                        timeout = config.get('timeout', args.timeout)
                        if backend == 'interpret':
                            command = [args.magel, 'interpret', source, '--out', prefix, '--', *config.get('args', [])]
                        else:
                            compilation = invoke([args.magel, backend, source, '--out', prefix, '--opt-level=0'], capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=timeout)
                            if compilation.returncode:
                                raise ValueError(compilation.stderr or compilation.stdout)
                            command = [prefix, *config.get('args', [])]
                        execution = invoke(command, capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=timeout)
                        if execution.returncode != config.get('expected_exit', 0):
                            raise ValueError(f"{backend} 退出码 {execution.returncode}：{execution.stderr}")
                        expected = source.with_suffix('.expected')
                        if expected.exists() and execution.stdout != expected.read_text(encoding='utf-8-sig'):
                            raise ValueError(f"{backend} 输出不符合 {expected}：{execution.stdout!r}")
                        if execution.stderr:
                            raise ValueError(f"{backend} 标准错误非空：{execution.stderr}")
                        outputs.append(execution.stdout)
                    if len(set(outputs)) > 1:
                        raise ValueError("各执行方式输出不同")
            except (ValueError, OSError, subprocess.TimeoutExpired) as error:
                result.update(status="failed", message=str(error))
        result['durationMs'] = round((time.monotonic() - started) * 1000)
        results.append(result)
        if not args.json:
            print(f"[{result['status']}] {source}" + (f"：{result['message']}" if result['message'] else ''), flush=True)
    summary = {status: sum(r['status'] == status for r in results) for status in ('passed', 'failed', 'skipped')}
    if args.json:
        print(json.dumps({"schemaVersion": 1, "tests": results, "summary": summary}, ensure_ascii=False))
    else:
        print(f"通过 {summary['passed']}，失败 {summary['failed']}，跳过 {summary['skipped']}")
    return int(bool(summary['failed']))


def uri_path(uri):
    """解析 file URI，兼容 Windows 盘符和网络路径。"""
    parsed = urlparse(uri)
    if parsed.scheme != 'file':
        raise ValueError("仅支持 file URI")
    value = unquote(parsed.path)
    if parsed.netloc:
        value = '//' + parsed.netloc + value
    elif os.name == 'nt' and re.match(r'^/[A-Za-z]:', value):
        value = value[1:]
    return Path(value)


def position(text, offset):
    """源码偏移转 LSP UTF-16 坐标。"""
    line = text.count('\n', 0, offset)
    start = text.rfind('\n', 0, offset) + 1
    return {'line': line, 'character': len(text[start:offset].encode('utf-16-le')) // 2}


def offset_at(text, pos):
    """LSP UTF-16 坐标转源码偏移。"""
    lines = text.splitlines(keepends=True)
    line = min(pos['line'], len(lines))
    prefix = sum(len(s) for s in lines[:line])
    if line == len(lines):
        return len(text)
    used = 0
    for index, char in enumerate(lines[line]):
        width = len(char.encode('utf-16-le')) // 2
        if used + width > pos['character']:
            return prefix + index
        used += width
        if used == pos['character']:
            return prefix + index + 1
    return prefix + len(lines[line])


def declarations(uri, text):
    """保守索引当前文档的显式声明；语义诊断始终由编译器提供。"""
    stream = tokens(text)
    found = []
    depth = 0
    scopes = [0]
    next_scope = 0
    for index, token in enumerate(stream):
        word = token[0]
        if word == '{':
            next_scope += 1
            scopes.append(next_scope)
            depth += 1
        elif word == '}':
            if len(scopes) > 1:
                scopes.pop()
            depth = max(0, depth - 1)
        elif re.fullmatch(r'[A-Za-z_]\w*', word) and word not in KEYWORDS and index:
            previous = stream[index - 1][0]
            following = stream[index + 1][0] if index + 1 < len(stream) else ''
            type_decl = previous in ('struct', 'enum', 'interface', 'module')
            typed = previous in ('auto','bool','char','float32','float64','int8','int16','int32','int64','uint8','uint16','uint32','uint64','void') or (re.fullmatch(r'[A-Za-z_]\w*', previous) and previous not in ('return', 'import', 'throw', 'new'))
            if previous=='>':
                angle=1;at=index-2
                while at>=0 and angle:
                    if stream[at][0]=='>':angle+=1
                    elif stream[at][0]=='<':angle-=1
                    at-=1
                typed=not angle and at>=0 and bool(re.fullmatch(r'[A-Za-z_]\w*',stream[at][0]))
            elif previous=='&':
                typed=index>=2 and bool(re.fullmatch(r'[A-Za-z_]\w*|>',stream[index-2][0]))
            if type_decl or (typed and following in ('(', '=', ';', ',', '[', ')')):
                end = text.find('\n', token.start())
                signature = text[text.rfind('\n', 0, token.start()) + 1:end if end >= 0 else len(text)].strip()
                found.append({'name': word, 'uri': uri, 'range': {'start': position(text, token.start()), 'end': position(text, token.end())}, 'offset': token.start(), 'scope': tuple(scopes), 'signature': signature, 'kind': 7 if type_decl else 3 if following == '(' else 6})
    return found


class LanguageServer:
    """同步 stdio LSP：编译器诊断、显式声明索引、补全、悬停与格式化。"""
    def __init__(self, compiler):
        self.compiler = compiler
        self.documents = {}
        self.api_index = ApiIndex()
        self.workspace_roots = []
        self.shutdown = False
        self.initialized = False

    def send(self, payload):
        raw = json.dumps(payload, ensure_ascii=False, separators=(',', ':')).encode('utf-8')
        sys.stdout.buffer.write(f"Content-Length: {len(raw)}\r\n\r\n".encode('ascii') + raw)
        sys.stdout.buffer.flush()

    def notify(self, method, params):
        self.send({'jsonrpc': '2.0', 'method': method, 'params': params})

    def diagnose(self, uri):
        document = self.documents[uri]
        path = uri_path(uri)
        try:
            result = invoke([self.compiler, '--check', '--diagnostics=json'], input=document['text'], cwd=path.parent, capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=30)
            report = json.loads(result.stdout)
            output = []
            for entry in report['diagnostics']:
                span = entry['span']
                # 根文档使用标准输入；导入文件的错误保留真实位置，作为相关信息。
                line = max(0, span['line'] - 1)
                source_lines = document['text'].splitlines()
                start = max(0, span['column'] - 1)
                end = max(start + 1, span['endColumn'] - 1)
                if line < len(source_lines):
                    encoded = source_lines[line].encode('utf-8')
                    start = len(encoded[:start].decode('utf-8', errors='ignore').encode('utf-16-le')) // 2
                    end = len(encoded[:end].decode('utf-8', errors='ignore').encode('utf-16-le')) // 2
                diagnostic = {'range': {'start': {'line': line, 'character': start}, 'end': {'line': line, 'character': end}}, 'severity': 1, 'code': entry['code'], 'source': 'magel', 'message': entry['message']}
                if span['file'] not in ('<标准输入>', '<stdin>', '') and Path(span['file']).resolve() != path.resolve():
                    diagnostic['range'] = {'start': {'line': 0, 'character': 0}, 'end': {'line': 0, 'character': 1}}
                    diagnostic['message'] += f"\n{span['file']}:{span['line']}"
                output.append(diagnostic)
        except (ValueError, KeyError, OSError, subprocess.TimeoutExpired) as error:
            output = [{'range': {'start': {'line': 0, 'character': 0}, 'end': {'line': 0, 'character': 1}}, 'severity': 1, 'source': 'magel', 'message': f'编译器检查失败：{error}'}]
        from magel_lsp import naming_warnings
        output.extend(naming_warnings(document['text']))
        self.notify('textDocument/publishDiagnostics', {'uri': uri, 'version': document['version'], 'diagnostics': output})

    def workspace_symbols(self, query):
        """搜索工作区源码与未保存文档，跳过构建产物和版本控制目录。"""
        sources = dict(self.documents)
        for root in self.workspace_roots:
            candidates=[]
            for directory, children, files in os.walk(root):
                children[:]=[name for name in children if name not in {'.git','build','node_modules','.magel','tmp','libraries','bin'}]
                candidates.extend(Path(directory)/name for name in files if name.endswith('.mgl'))
            for path in candidates:
                uri = path.resolve().as_uri()
                if uri not in sources:
                    try:
                        sources[uri] = {'text': path.read_text(encoding='utf-8-sig')}
                    except (OSError, UnicodeError):
                        continue
        result = []
        for uri, document in sources.items():
            functions = self.api_index.scan(uri, document['text'])
            for record in [*functions, *self.api_index.symbols.get(uri, [])]:
                if query.lower() not in record['qualified'].lower():
                    continue
                start = position(document['text'], record['offset'])
                end = position(document['text'], record['offset'] + len(record['name']))
                result.append({'name': record['qualified'], 'kind': {'struct':23,'enum':10,'interface':11,'member':8}.get(record['kind'], 6 if record['owner'] else 12), 'location': {'uri': uri, 'range': {'start': start, 'end': end}}, 'containerName': record['module']})
        return result

    def query(self, method, params):
        if method == 'initialize':
            self.initialized = True
            self.workspace_roots = [uri_path(folder['uri']) for folder in params.get('workspaceFolders', []) or []]
            if not self.workspace_roots and params.get('rootUri'):
                self.workspace_roots = [uri_path(params['rootUri'])]
            return {'capabilities': {'positionEncoding': 'utf-16', 'textDocumentSync': 1, 'completionProvider': {'triggerCharacters': [':', '.']}, 'hoverProvider': True, 'semanticTokensProvider': {'legend': {'tokenTypes': TOKEN_TYPES, 'tokenModifiers': ['declaration','readonly']}, 'full': True}, 'signatureHelpProvider': {'triggerCharacters': ['(', ','], 'retriggerCharacters': [',']}, 'workspaceSymbolProvider': True, 'definitionProvider': True, 'documentSymbolProvider': True, 'documentFormattingProvider': True}, 'serverInfo': {'name': 'Magel', 'version': VERSION}}
        if method == 'shutdown':
            self.shutdown = True
            return None
        if method == 'workspace/symbol':
            return self.workspace_symbols(params.get('query', ''))
        if method not in ('textDocument/semanticTokens/full', 'textDocument/signatureHelp', 'textDocument/completion', 'textDocument/definition', 'textDocument/hover', 'textDocument/documentSymbol', 'textDocument/formatting'):
            raise LookupError(method)
        uri = params.get('textDocument', {}).get('uri')
        document = self.documents.get(uri)
        if not document:
            return [] if method in ('textDocument/completion', 'textDocument/definition', 'textDocument/documentSymbol', 'textDocument/formatting') else None
        text = document['text']
        local = declarations(uri, text)
        if method == 'textDocument/documentSymbol':
            return [{'name': s['name'], 'kind': {3:12,6:13,7:23,9:2}.get(s['kind'],13), 'range': s['range'], 'selectionRange': s['range']} for s in local]
        if method == 'textDocument/formatting':
            output = formatted(text)
            return [] if output == text else [{'range': {'start': {'line': 0, 'character': 0}, 'end': position(text, len(text))}, 'newText': output}]
        cursor = offset_at(text, params.get('position', {'line': 0, 'character': 0}))
        records = self.api_index.collect(uri, text, self.documents, uri_path)
        records = [*records, *(r for target in self.api_index.visible_uris for r in self.api_index.symbols.get(target, []))]
        if method == 'textDocument/semanticTokens/full':
            return semantic_tokens(text, records, KEYWORDS, position, self.api_index.type_names)
        if method == 'textDocument/signatureHelp':
            return signature_help([r for r in records if r.get('kind', 'function') == 'function'], text, cursor)
        scope = [0]
        serial = 0
        for token in tokens(text):
            if token.start() > cursor:
                break
            if token[0] == '{':
                serial += 1
                scope.append(serial)
            elif token[0] == '}' and len(scope) > 1:
                scope.pop()
        visible = [s for s in local if tuple(scope[:len(s['scope'])]) == s['scope']]
        if method == 'textDocument/completion':
            for token in TOKEN.finditer(text):
                value=token.group()
                if token.start()<cursor<=token.end() and value.startswith(('/*','//','"',"'")):
                    if cursor<token.end() or value.startswith('//') or value.startswith('/*') and not value.endswith('*/'):
                        return []
            names = {s['name']: s['kind'] for s in visible if s['offset'] <= cursor or len(s['scope']) == 1}
            names.update({word: 14 for word in KEYWORDS})
            rich={r['name'] for r in records if r.get('uri')==uri}
            result = [{'label': name, 'kind': kind} for name, kind in sorted(names.items()) if kind!=3 or name not in rich]
            modules=set(self.api_index.module_imports)
            modules.update(item['containerName'] for item in self.workspace_symbols('') if item.get('containerName'))
            import_context=re.search(r'\bimport\s+([\w:]*)$',text[:cursor])
            module_prefix=import_context.group(1) if import_context else ''
            qualifier = re.search(r'(.+)(::|\.)[A-Za-z_0-9]*$', expression_name(text,cursor))
            def with_import(item,module):
                """补全未导入模块的声明时追加独立 import 编辑，保留 module 的首行位置。"""
                if module not in ('std::lang','std',*imports(text)):
                    head=tokens(text);insertion=0
                    if head and head[0][0]=='module':
                        insertion=next((t.end() for t in head if t[0]==';'),0)
                    item['additionalTextEdits']=[{'range':{'start':position(text,insertion),'end':position(text,insertion)},'newText':('\n' if insertion else '')+'import '+module+';\n'}]
                return item
            api = records
            if qualifier and qualifier.group(2)=='::':
                receiver=qualifier.group(1)
                module=next((m for m in sorted(modules,key=len,reverse=True) if receiver==m or receiver.startswith(m+'::')),None)
                if module and module not in ('std::lang','std'):
                    api=self.api_index.collect(uri,'import '+module+';\n'+text,self.documents,uri_path)
                    api=[*api, *(r for target in self.api_index.visible_uris for r in self.api_index.symbols.get(target, []))]
            if qualifier:
                receiver, separator = qualifier.groups()
                if separator == '.':
                    api = matching(api, receiver + '.', text[:cursor])
                else:
                    api = [r for r in api if r['qualified'].rsplit('::',1)[0] == receiver or
                           ('::' not in receiver and r['qualified'].rsplit('::',1)[0].endswith('::' + receiver))]
                result = []
                if separator=='::':
                    owners={r['owner'].split('::')[0] for r in self.api_index.standard if r['module']==receiver and r['owner']}
                    owners.update(r['owner'].split('::')[0] for r in records if r['module']==receiver and r['owner'])
                    owners.difference_update(r['name'] for r in api if r.get('kind') in ('struct','enum','interface') and not r['owner'])
                    result.extend(with_import({'label':name,'kind':7,'detail':'类型 '+receiver+'::'+name,'insertText':name},receiver) for name in sorted(owners))
            else:
                api = [r for r in records if not r['owner']]
            # 同模块重载不算冲突；跨模块同名及当前作用域遮蔽使用限定名。
            completion_names = {}
            for record in records:
                if not record['owner']:
                    completion_names.setdefault(record['name'], set()).add(record['qualified'])
            for record in api:
                label = record['name'] if qualifier or record.get('uri') == uri else record['qualified']
                shadowed = any(binding['name'] == record['name'] and
                               (binding['offset'] <= cursor or len(binding['scope']) == 1) and
                               not (record.get('uri') == uri and (binding['offset'] == record.get('offset') or
                                    record.get('kind','function') == 'function' and binding['kind'] == 3 and len(binding['scope']) == 1))
                               for binding in visible)
                owners = set(completion_names.get(record['name'], ()))
                if record['module'] != 'std::lang': owners.discard('std::lang::'+record['name'])
                insertion = record['name'] if qualifier or (len(owners) == 1 and not shadowed) else record['qualified']
                item={'label': label, 'kind': {'struct':22,'enum':13,'interface':8,'member':5}.get(record.get('kind'),3), 'detail': record['signature'], 'insertText': insertion, 'documentation': {'kind': 'markdown', 'value': record['documentation']}}
                if qualifier and qualifier.group(2)=='::':with_import(item,record['module'])
                result.append(item)
            if qualifier and qualifier.group(2)=='::':
                module_prefix=qualifier.group(1)+'::'
            if import_context or qualifier and qualifier.group(2)=='::':
                prefix=module_prefix.rsplit('::',1)[0]+'::' if '::' in module_prefix else ''
                choices={m[len(prefix):].split('::')[0] for m in modules if m.startswith(module_prefix) and len(m)>len(prefix)}
                if import_context and prefix=='std::': choices.discard('lang')
                if import_context:result=[]
                result.extend({'label':name,'kind':9,'detail':'模块 '+prefix+name,'insertText':name} for name in sorted(choices))
            elif not qualifier:
                result.extend({'label':name,'kind':9,'detail':'模块','insertText':name} for name in sorted({m.split('::')[0] for m in modules}))
            return result
        token = next((m for m in tokens(text) if m.start() <= cursor <= m.end() and re.fullmatch(r'[A-Za-z_]\w*', m[0])), None)
        if not token:
            return None
        name = expression_name(text, token.end())
        api = matching(records, name, text[:cursor])
        if '::' not in name and '.' not in name and any(s['name']==token[0] and s['kind']!=3 and s['offset']<=cursor for s in visible):
            api=[]
        if api and method == 'textDocument/hover':
            return {'contents': {'kind': 'markdown', 'value': '\n\n---\n\n'.join('```magel\n' + r['signature'] + '\n```\n\n' + r['documentation'] for r in api)}, 'range': {'start': position(text, token.start()), 'end': position(text, token.end())}}
        if api and method == 'textDocument/definition':
            locations = []
            for r in api:
                target = r.get('uri')
                if not target:
                    continue
                opened = self.documents.get(target)
                try:
                    source = opened['text'] if opened else uri_path(target).read_text(encoding='utf-8-sig')
                except OSError:
                    continue
                locations.append({'uri': target, 'range': {'start': position(source, r['offset']), 'end': position(source, r['offset']+len(r['name']))}})
            if locations:
                return locations
        if method == 'textDocument/hover' and token[0]=='auto':
            following=re.match(r'\s*(?:(?:const|mut)\s*)?&?\s*(\w+)',text[token.end():])
            if following:
                name=following.group(1)
                concrete=inferred_type(records,name,text,token.end()+following.end())
                if concrete:
                    return {'contents': {'kind':'markdown','value':'```magel\n'+concrete+' '+name+'\n```'},
                            'range':{'start':position(text,token.start()),'end':position(text,token.end())}}
        candidates = [s for s in visible if s['name'] == token[0] and (s['offset'] <= cursor or len(s['scope']) == 1)]
        if candidates:
            depth = max(len(s['scope']) for s in candidates)
            candidates = [s for s in candidates if len(s['scope']) == depth]
        # 只返回确定的单一声明；重载或跨块同名时避免错误跳转。
        if len(candidates) != 1:
            return None
        symbol = candidates[0]
        if method == 'textDocument/definition':
            return {'uri': uri, 'range': symbol['range']}
        if method == 'textDocument/hover':
            inferred = inferred_type(records, token[0], text, cursor) if re.search(r'\bauto\b',symbol['signature']) else ''
            signature = inferred+' '+token[0] if inferred else symbol['signature']
            return {'contents': {'kind': 'markdown', 'value': '```magel\n' + signature + '\n```'}, 'range': {'start': position(text, token.start()), 'end': position(text, token.end())}}
        raise LookupError(method)

    def run(self):
        while True:
            header = {}
            while True:
                line = sys.stdin.buffer.readline()
                if not line:
                    return 0 if self.shutdown else 1
                if line in (b'\r\n', b'\n'):
                    break
                name, value = line.decode('ascii').split(':', 1)
                header[name.lower()] = value.strip()
            size = int(header['content-length'])
            if size < 0 or size > 16 * 1024 * 1024:
                raise ValueError(_text('LSP 消息过大'))
            message = json.loads(sys.stdin.buffer.read(size))
            method, params = message.get('method'), message.get('params', {})
            if method == 'exit':
                return 0 if self.shutdown else 1
            if 'id' in message:
                try:
                    result = self.query(method, params)
                    self.send({'jsonrpc': '2.0', 'id': message['id'], 'result': result})
                except LookupError:
                    self.send({'jsonrpc': '2.0', 'id': message['id'], 'error': {'code': -32601, 'message': 'Method not found'}})
                continue
            if method == 'textDocument/didClose':
                uri = params['textDocument']['uri']
                self.documents.pop(uri, None)
                self.notify('textDocument/publishDiagnostics', {'uri': uri, 'diagnostics': []})
            elif method in ('textDocument/didOpen', 'textDocument/didChange', 'textDocument/didSave'):
                item = params['textDocument']
                uri = item['uri']
                if method == 'textDocument/didOpen':
                    self.documents[uri] = {'text': item['text'], 'version': item.get('version')}
                elif method == 'textDocument/didChange':
                    if uri not in self.documents:
                        continue
                    self.documents[uri] = {'text': params['contentChanges'][-1]['text'], 'version': item.get('version')}
                if uri in self.documents:
                    self.diagnose(uri)



def main():
    """统一工具参数入口；错误不会污染 LSP 标准输出。"""
    argparse._ = _text
    parser = argparse.ArgumentParser(prog="magel", description=_text(__doc__))
    parser.add_argument('--magel', type=Path, required=True)
    commands = parser.add_subparsers(dest='command', required=True)
    test = commands.add_parser('test', help=_text('发现并运行 test_*.mgl / *_test.mgl'))
    test.add_argument('paths', type=Path, nargs='*', default=[Path('tests')])
    test.add_argument('--filter', default='')
    test.add_argument('--backend', choices=['interpret', 'aot', 'all'], default='interpret')
    test.add_argument('--timeout', type=float, default=120)
    test.add_argument('--json', action='store_true')
    test.add_argument('--list', action='store_true')
    fmt = commands.add_parser('fmt', help=_text('规范缩进；保持字面量和宏不变'))
    fmt.add_argument('paths', type=Path, nargs='+')
    group = fmt.add_mutually_exclusive_group()
    group.add_argument('--check', action='store_true')
    group.add_argument('--write', action='store_true')
    commands.add_parser('lsp', help=_text('启动 stdio 语言服务器'))
    deps = commands.add_parser('deps', help=_text('锁定或离线校验依赖'))
    deps.add_argument('action', choices=['lock', 'check'])
    deps.add_argument('--project', type=Path, default=Path.cwd())
    deps.add_argument('--offline', action='store_true')
    args = parser.parse_args()
    args.magel = args.magel.resolve()
    try:
        if args.command == 'test':
            return test_command(args)
        if args.command == 'fmt':
            return fmt_command(args)
        if args.command == 'deps':
            return deps_command(args)
        return LanguageServer(args.magel).run()
    except (ValueError, OSError, tarfile.TarError, zipfile.BadZipFile, subprocess.CalledProcessError, tomllib.TOMLDecodeError) as error:
        print(f'magel：{error}', file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.stdout.reconfigure(encoding='utf-8')
    sys.stderr.reconfigure(encoding='utf-8')
    raise SystemExit(main())
