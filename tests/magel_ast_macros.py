"""通过真实编译器和 VM 验证 AST 表达式宏的阶段性语义接入。"""
import os
import argparse
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    """验证结构替换、重复求值、嵌套展开、签名错误及递归预算。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--magel", type=Path, default=ROOT / ("magel.exe" if os.name == "nt" else "magel"))
    parser.add_argument("--default", action="store_true", help="使用含标准库的默认编译路径")
    options = parser.parse_args()
    compiler = options.magel.resolve()
    work = ROOT / "build/macro-hygiene"
    work.mkdir(parents=True, exist_ok=True)
    cases = [
        ("precedence", "$macro SQUARE(x)\nx * x\n$end_macro\nint64 main() { int32 a = 2; int32 b = 3; int32 result = $SQUARE(a + b); if (result == 25) { return 0; } return 1 / (result - result); }", None),
        ("repeat", "$macro TWICE(x)\nx + x\n$end_macro\nint32 counter = 0; int32 next() { counter += 1; return counter; } int64 main() { int32 b = $TWICE(next()); if (counter == 2 && b == 3) { return 0; } return 1 / (counter - counter); }", None),
        ("nested", "$macro DOUBLE(x)\nx + x\n$end_macro\n$macro FOUR(x)\n$DOUBLE($DOUBLE(x))\n$end_macro\nint64 main() { int32 result = $FOUR(3); if (result == 12) { return 0; } return 1 / (result - result); }", None),
        ("bare", "$macro VALUE\n42\n$end_macro\nint64 main() { int32 result = $VALUE; if (result == 42) { return 0; } return 1 / (result - result); }", None),
        ("empty", "$macro VALUE()\n42\n$end_macro\nint64 main() { int32 result = $VALUE(); if (result == 42) { return 0; } return 1 / (result - result); }", None),
        ("missing_group", "$macro VALUE()\n42\n$end_macro\nint64 main() { return $VALUE; }", "参数组"),
        ("extra_group", "$macro VALUE\n42\n$end_macro\nint64 main() { return $VALUE(); }", "参数组"),
        ("arity", "$macro VALUE(x)\nx\n$end_macro\nint64 main() { return $VALUE(); }", "实参数量"),
        ("duplicate", "$macro VALUE\n1\n$end_macro\n$macro VALUE\n2\n$end_macro\nint64 main() { return 0; }", "重复定义"),
        ("parameters", "$macro VALUE(x, x)\nx\n$end_macro\nint64 main() { return 0; }", "形参重复"),
        ("unknown", "int64 main() { return $MISSING; }", "未定义宏"),
        ("recursive", "$macro RECURSE(x)\n$RECURSE(x)\n$end_macro\nint64 main() { return $RECURSE(0); }", "预算"),
        ("capture", "$macro VALUE\nhelper\n$end_macro\nint64 main() { int32 helper = 1; return $VALUE; }", "没声明的变量"),
        ("update_conflict", "$macro TWICE(x)\nx + x\n$end_macro\nint64 main() { int32 a = 0; return $TWICE(++a); }", "MGL1301"),
        ("helper_global", "$macro VALUE\nhelper\n$end_macro\nint64 helper = 7; int64 main() { int64 helper = 100; int64 result = $VALUE; if (result == 7) { return 0; } return 1 / (result - result); }", None),
        ("statement_hygiene", "$macro BUMP(x)\nint64 temp = x; x = temp + 1;\n$end_macro\nint64 main() { int64 temp = 100; int64 value = 1; $BUMP(value); $BUMP(value); if (temp == 100 && value == 3) { return 0; } return 1 / (value - value); }", None),
        ("export_identifier", "$macro DECLARE(name, value)\nint64 name = value;\n$end_macro\nint64 main() { $DECLARE(created, 42); if (created == 42) { return 0; } return 1 / (created - created); }", None),
        ("export_preserves_caller", "$macro SHADOW(name, value, out)\nint64 name = 10; out = value;\n$end_macro\nint64 main() { int64 value = 42; int64 out = 0; { $SHADOW(value, value, out); if (value != 10 || out != 42) { return 1 / (out - out); } } if (value == 42) { return 0; } return 1 / (value - value); }", None),
        ("closure_actual", "$macro ID(x)\nx\n$end_macro\nint64 main() { int64 bias = 40; auto f = $ID(fn(int64 x) { x + bias }); int64 value = f(2); if (value == 42) { return 0; } return 1 / (value - value); }", None),
        ("closure_template", "$macro MAKE(x)\nfn(int64 value) { value + x }\n$end_macro\nint64 main() { int64 bias = 40; auto f = $MAKE(bias); int64 value = f(2); if (value == 42) { return 0; } return 1 / (value - value); }", None),
        ("closure_shadow", "$macro MAKE(x)\nfn(int64 value) { value + x }\n$end_macro\nint64 main() { int64 value = 40; auto f = $MAKE(value); int64 answer = f(2); if (answer == 42) { return 0; } return 1 / (answer - answer); }", None),
        ("listed_capture_parameter", "$macro MAKE(x)\nfn copy[x](int64 value) { value + x }\n$end_macro\nint64 main() { int64 value = 40; auto f = $MAKE(value); value = 100; int64 answer = f(2); if (answer == 42) { return 0; } return 1 / (answer - answer); }", None),
        ("nested_returning_closure", "$macro MAKE(x)\nfn copy[x]() { fn copy[x](int64 value) { value + x } }\n$end_macro\nint64 main() { int64 bias = 40; auto f = $MAKE(bias); auto g = f(); int64 answer = g(2); if (answer == 42) { return 0; } return 1 / (answer - answer); }", None),
        ("member_identifier", "$macro GET(obj, field)\nobj.field\n$end_macro\nstruct Box { int64 value; } int64 main() { Box box; box.value = 42; int64 answer = $GET(box, value); if (answer == 42) { return 0; } return 1 / (answer - answer); }", None),
        ("member_literal", "$macro GET(obj)\nobj.value\n$end_macro\nstruct Box { int64 value; } int64 main() { Box box; box.value = 42; int64 value = 100; int64 answer = $GET(box); if (answer == 42 && value == 100) { return 0; } return 1 / (answer - answer); }", None),
        ("enum_pattern", "$macro TAKE(item)\nmatch(item) { Choice::Value(value) => { value } Choice::Empty => { 0 } }\n$end_macro\nenum Choice { Value(int64 value), Empty } int64 main() { int64 value = 100; int64 answer = $TAKE(Choice::Value(42)); if (answer == 42 && value == 100) { return 0; } return 1 / (answer - answer); }", None),
        ("enum_type_fragment", "$macro TAKE(T, item)\nmatch(item) { T::Value(value) => { value } T::Empty => { 0 } }\n$end_macro\nenum Choice { Value(int64 value), Empty } int64 main() { int64 value = 100; int64 answer = $TAKE(Choice, Choice::Value(42)); if (answer == 42 && value == 100) { return 0; } return 1 / (answer - answer); }", None),
        ("destructure_hygiene", "$macro TAKE(item)\n{ auto (left, right) = item; yield left + right; }\n$end_macro\nint64 main() { int64 left = 100; int64 answer = $TAKE((40, 2)); if (answer == 42 && left == 100) { return 0; } return 1 / (answer - answer); }", None),
        ("generic_closure_template", "$macro MAKE\nfn<T>(T x) { x }\n$end_macro\nint64 main() { auto f = $MAKE; int64 value = f(42); if (value == 42) { return 0; } return 1 / (value - value); }", None),
        ("generic_closure_actual", "$macro ID(x)\nx\n$end_macro\nint64 main() { auto f = $ID(fn<T>(T x) { x }); int32 argument = 42; int32 value = f<int32>(argument); if (value == 42) { return 0; } return 1 / (value - value); }", None),
        ("generic_closure_capture", "$macro MAKE(x)\nfn<T> copy[x](T value) { cast<int64>(value) + x }\n$end_macro\nint64 main() { int64 bias = 40; auto f = $MAKE(bias); bias = 100; int64 value = f(2); if (value == 42) { return 0; } return 1 / (value - value); }", None),
        ("generic_closure_borrow", "$macro MAKE(x)\nfn<T>(T value) { cast<int64>(value) + x }\n$end_macro\nint64 main() { int64 bias = 40; auto f = $MAKE(bias); bias = 100; int64 value = f(2); if (value == 102) { return 0; } return 1 / (value - value); }", None),
        ("generic_capture_collision", "$macro ID(value)\nvalue\n$end_macro\nint64 main() { int64 x = 42; auto f = $ID(fn<T> copy[x](T x) { x }); return 0; }", "捕获变量与形参重名"),
        ("returning_generic_closure", "$macro MAKE\nfn() { fn<T>(T value) { value } }\n$end_macro\nint64 main() { auto f = $MAKE; auto g = f(); int64 value = g(42); if (value == 42) { return 0; } return 1 / (value - value); }", None),
        ("export_rebinds_later_macro", "$macro DECLARE(name, value)\nint64 name = value;\n$end_macro\n$macro GET(value)\nvalue\n$end_macro\nint64 main() { int64 value = 42; { $DECLARE(value, 10); int64 result = $GET(value); if (result != 10) { return 1 / (result - result); } } return 0; }", None),
        ("source_closure_local_shadow", "auto make() { int64 value = 42; return fn() { int64 value = 1; return value; }; } int64 main() { auto f = make(); int64 value = f(); if (value == 1) { return 0; } return 1 / (value - value); }", None),
        ("source_closure_pattern_shadow", "enum Choice { Value(int64 value), Empty } auto make() { int64 value = 100; return fn(Choice item) { return match(item) { Choice::Value(value) => { value } Choice::Empty => { 0 } }; }; } int64 main() { auto f = make(); int64 value = f(Choice::Value(42)); if (value == 42) { return 0; } return 1 / (value - value); }", None),
        ("source_closure_destructure_shadow", "auto make() { int64 value = 42; return fn() { auto (value, _) = (1, 2); return value; }; } int64 main() { auto f = make(); int64 value = f(); if (value == 1) { return 0; } return 1 / (value - value); }", None),
        ("runtime_control", "int64 main() { int32 value = 2; return 1 / (value - value); }", "运行期错误"),
    ]
    cases.append(("undefined_block_helper", "$macro VALUE\n{ yield helper; }\n$end_macro\nint64 main() { float64 helper = 1.0; float64 value = $VALUE; return 0; }", "没声明的变量：helper"))
    if options.default:
        cases.append(("builtin_helper_call", "$macro VALUE\n{ println(\"macro builtin\"); yield 42; }\n$end_macro\nint64 main() { return $VALUE - 42; }", None))
        cases.append(("undefined_vec_temp", "$macro vec<T:first>[first,...arrs]\n{\n    temp.push(first);\n    $for e in arrs\n    temp.push(e);\n    $end_for\n    yield temp;\n}\n$end_macro\nvoid main() { List<int64> temp; List<int64> array = $vec[1,2,3,4,5]; println(\"list={}\", array); }", "没声明的变量：temp"))
    with tempfile.TemporaryDirectory(prefix="vm-", dir=work) as directory:
        for name, source, error in cases:
            path = Path(directory) / (name + ".mgl")
            path.write_text(source + "\n", encoding="utf-8")
            result = subprocess.run([str(compiler), "run", str(path), *([] if options.default else ["--raw"]), "--out", str(Path(directory) / name)],
                                    cwd=ROOT, env=dict(os.environ, MAGEL_LANG="zh"), capture_output=True,
                                    text=True, encoding="utf-8", errors="replace", timeout=90)
            if error is None:
                assert result.returncode == 0 and not result.stderr, (name, result.returncode, result.stdout, result.stderr)
            else:
                assert result.returncode != 0 and error in result.stderr, (name, result.returncode, result.stderr)
                if name.startswith("undefined_"):
                    assert "分支结果类型必须一致" not in result.stderr, result.stderr
                    assert result.stderr.count("magel:") == 1, result.stderr
                if name == "arity":
                    at = source.rfind("$VALUE")
                    column = at - source.rfind("\n", 0, at)
                    assert f"第 4 行第 {column} 列" in result.stderr, result.stderr
                    assert "展开宏 $VALUE" in result.stderr and "定义于" in result.stderr, result.stderr
    print(f"AST 表达式宏 VM 接入回归通过：{len(cases)} 项", flush=True)


if __name__ == "__main__":
    main()
