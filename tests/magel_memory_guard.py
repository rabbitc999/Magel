"""验证自定义分配器访问护栏，以及真实 MAGEL 非法访问的 panic、源码行与调用栈。"""
from pathlib import Path
import argparse
import os
import subprocess

ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / "build/magel-memory-guard"


def execute(*args):
    """执行工具，保留诊断，并设置超时以发现 panic 未终止程序的问题。"""
    return subprocess.run([str(arg) for arg in args], cwd=ROOT, capture_output=True,
                          encoding="utf-8", errors="replace", timeout=240,
                          env=dict(os.environ, MAGEL_LANG="zh"))


def main():
    """先检查自定义堆契约，再检查解释与 AOT 的实际错误位置。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--magel", type=Path, default=ROOT / "magel.exe")
    parser.add_argument("--cxx", default="g++")
    args = parser.parse_args()
    compiler = args.magel.resolve()
    WORK.mkdir(parents=True, exist_ok=True)
    suffix = ".exe" if os.name == "nt" else ""
    unit = WORK / ("unit" + suffix)
    result = execute(args.cxx, "-std=c++17", "-pthread", "-DMAGEL_CUSTOM_ALLOCATOR=1",
                     "-DSTACKVM_WITH_REFLECTION=1", "-Ilibrary/stackvm", "tests/magel_memory_guard_test.cpp",
                     "build/magel-stackvm/libstackvm.a", "-lm", "-o", unit)
    assert result.returncode == 0, result
    result = execute(unit)
    assert result.returncode == 0 and result.stdout == "custom heap memory guard OK\n", result
    unchecked_unit = WORK / ("unit-unchecked" + suffix)
    result = execute(args.cxx, "-std=c++17", "-pthread", "-DMAGEL_CUSTOM_ALLOCATOR=1",
                     "-DSTACKVM_WITH_BIGINT=0", "-DSTACKVM_WITH_BIGFLOAT=0", "-DSTACKVM_WITH_ALLOCATION_GUARDS=0",
                     "-Ilibrary/stackvm", "tests/magel_memory_guard_test.cpp", "library/stackvm/stackvm.cpp",
                     "library/stackvm/stackvm_glue.cpp", "-lm", "-o", unchecked_unit)
    assert result.returncode == 0, result
    result = execute(unchecked_unit)
    assert result.returncode == 0 and result.stdout == "custom heap unchecked OK\n", result
    cases = {
        "read": ("int64", "int64 leaf(int64 & p) { return p; }", "return outer(stale);"),
        "write": ("int64", "int64 leaf(int64 mut& p) { p = 9; return 0; }", "return outer(stale);"),
        "float": ("float64", "int64 leaf(float64 & p) { float64 value = p; return 0; }", "return outer(stale);"),
        "char": ("char", "int64 leaf(char & p) { char value = p; return 0; }", "return outer(stale);"),
        "char_write": ("char", "int64 leaf(char mut& p) { p = 'A'; return 0; }", "return outer(stale);"),
    }
    for name, (type_name, leaf, body) in cases.items():
        permission = "mut&" if name in ("write", "char_write") else "&"
        source = WORK / (name + ".mgl")
        source.write_text(leaf + "\nint64 outer(" + type_name + " " + permission + " p) { return leaf(p); }\n"
                          "void dispose(" + type_name + " mut& p) { delete p; }\n"
                          "int64 main() {\n" + type_name + " mut& owner = new;\n"
                          + type_name + " " + permission + " stale = owner;\n"
                          "dispose(owner);\n" + body + "\n}\n", encoding="utf-8")
        result = execute(compiler, "--lang", "zh", "run", source, "--out", WORK / (name + "-interpret"))
        results = [("解释执行", result)]
        output = WORK / (name + "-aot" + suffix)
        built = execute(compiler, "--lang", "zh", "aot", source, "checked", "--out", output, "--opt-level=0", "--cpp-output")
        assert built.returncode == 0, built
        exported = Path(str(output) + ".aot.cpp").read_text(encoding="utf-8")
        assert "#define STACKVM_WITH_ALLOCATION_GUARDS 0" not in exported, name
        assert "#define NO_MAGEL_PANIC_TRACE 1" not in exported, name
        results.append(("AOT", execute(output)))
        for backend, result in results:
            assert result.returncode != 0 and "panic：" in result.stderr, (name, backend, result)
            assert "调用栈" in result.stderr and "leaf" in result.stderr and "outer" in result.stderr, (name, backend, result)
            assert source.name + ":1" in result.stderr, (name, backend, result)
            assert "Segmentation" not in result.stderr and "terminate called" not in result.stderr, result
        print(f"[通过] {name}：解释执行 / AOT，panic、原始行号和调用栈", flush=True)
    # 明确启用隐私裁剪后，panic 仍终止程序，但不泄露原始路径、函数和行号。
    result = execute(compiler, "--lang", "zh", "run", source, "--compress-symbols", "--out", WORK / "hidden-interpret")
    assert result.returncode != 0 and "panic：" in result.stderr and "调用栈" not in result.stderr, result
    output = WORK / ("hidden-aot" + suffix)
    built = execute(compiler, "--lang", "zh", "aot", source, "checked", "--compress-symbols", "--out", output, "--opt-level=0")
    assert built.returncode == 0, built
    result = execute(output)
    assert result.returncode != 0 and "panic：" in result.stderr, result
    assert "调用栈" not in result.stderr and source.name not in result.stderr and "leaf" not in result.stderr, result
    # unchecked 只改变 AOT 部署配置；用有效程序验收，避免对非法访问施加确定行为假设。
    safe = ROOT / "example/samples/reference_sugar.mgl"
    expected = "reference sugar OK\n"
    for name, selection, extra in (("default", [], []), ("direct", ["unchecked"], []),
                                   ("make", [], ["--make", "make"]),
                                   ("modules", [], ["modules", "cache", WORK / "unchecked-cache"])):
        directory = WORK / ("unchecked-" + name)
        output = directory / ("deploy" + suffix)
        built = execute(compiler, "--lang", "zh", "aot", safe, *selection, "out", output, "opt", "0",
                        "--cpp-output-dir", directory, *extra)
        assert built.returncode == 0, built
        result = execute(output)
        assert result.returncode == 0 and result.stdout == expected and not result.stderr, result
        exported = "\n".join(path.read_text(encoding="utf-8") for path in directory.glob("*.cpp"))
        exported += "\n".join(path.read_text(encoding="utf-8") for path in directory.glob("*.hpp"))
        assert "#define STACKVM_WITH_ALLOCATION_GUARDS 0" in exported, name
        assert "#define NO_MAGEL_PANIC_TRACE 1" in exported, name
    result = execute(compiler, "--lang", "zh", "run", safe, "unchecked", "--out", WORK / "rejected")
    assert result.returncode != 0 and "checked / unchecked 只支持 aot" in result.stderr, result
    # 显式语言 panic 仍会终止 unchecked 程序，调用栈及源码定位被裁剪。
    source = WORK / "deployment-panic.mgl"
    source.write_text('int64 leaf() { panic("deploy stop"); return 0; }\n'
                      'int64 main() { return leaf(); }\n', encoding="utf-8")
    output = WORK / ("deployment-panic" + suffix)
    built = execute(compiler, "--lang", "zh", "aot", source, "out", output, "opt", "0")
    assert built.returncode == 0, built
    result = execute(output)
    assert result.returncode != 0 and "deploy stop" in result.stderr, result
    assert "调用栈" not in result.stderr and source.name not in result.stderr and "leaf" not in result.stderr, result
    print("[通过] AOT unchecked：直接 / make / 模块导出，显式 panic，解释模式拒绝", flush=True)
    print(f"内存访问护栏通过：自定义堆单元测试，{len(cases)} 个真实 MAGEL 场景解释 / AOT，隐私裁剪")


if __name__ == "__main__":
    main()
