"""编译并执行卫生宏基础模块及权威生成解析器的真实 C++ 回归。"""
import os
from pathlib import Path
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]


def execute(command):
    """运行独立构建或测试，失败时保留完整编译诊断。"""
    result = subprocess.run(list(map(str, command)), cwd=ROOT, capture_output=True,
                            text=True, encoding="utf-8", errors="replace", timeout=300)
    if result.returncode:
        raise AssertionError(f"{command}\n{result.stdout}\n{result.stderr}")
    if result.stdout:
        print(result.stdout, end="", flush=True)


def main():
    """在独立目录编译模块回归，并使用现有解析器库验证真实宏语法节点。"""
    base = ROOT / "build/macro-hygiene"
    base.mkdir(parents=True, exist_ok=True)
    includes = ["source", "library", "source/preprocess", "source/simple_ast", "source/stackvm",
                "source/console", "source/lztext", "source/bignum", "source/json"]
    flags = ["g++", "-std=c++17", "-O3", "-Wall", "-Wextra", "-pthread",
             "-ffunction-sections", "-fdata-sections", "-DCHALI_CUSTOM_ALLOCATOR=1",
             "-DCHALI_WITH_REFLECTION=1", "-DSTACKVM_WITH_REFLECTION=1",
             *["-I" + value for value in includes]]
    libraries = ["build/chali-frontend/libparser.a", "build/chali-runtime/libsimple_ast.a",
                 "build/chali-stackvm-core/libstackvm.a", "build/chali-host-records/librecords.a",
                 "build/chali-runtime/libpreprocess.a"]
    with tempfile.TemporaryDirectory(prefix="foundations-", dir=base) as directory:
        for name, module in (("macro_hygiene", "hygiene"), ("macro_regex", "regex_matcher"),
                             ("macro_source", "source"), ("macro_fragments", "fragments"), ("macro_ast_substitution", "ast_substitution"),
                             ("macro_template_binding", ["template_binding", "ast_substitution", "hygiene", "registry", "regex_matcher", "source/literals/string_escape.cpp"]),
                             ("syntax_constants", ["source/syntax/source_text.cpp", "source/syntax/constant_expression.cpp"]),
                             ("macro_parser", None)):
            binary = Path(directory) / (name + (".exe" if os.name == "nt" else ""))
            sources = ["tests/" + name + ".cpp"]
            sources += [item if item.startswith("source/") else "source/macro/" + item + ".cpp" for item in (module if isinstance(module, list) else [module])] if module else libraries
            system = ["-lwinhttp"] if os.name == "nt" and module is None else []
            execute([*flags, *sources, "-Wl,--gc-sections", *system, "-o", binary])
            execute([binary])
    print("卫生宏基础模块及生成解析器回归通过", flush=True)


if __name__ == "__main__":
    main()
