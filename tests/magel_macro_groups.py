"""在真实 VM 中验证卫生宏的分组、变长片段及调用来源。"""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    """运行分组及来源回归，并核对错误来自编译器而非运行期。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--magel", type=Path, default=ROOT / "magel.exe")
    parser.add_argument("--default", action="store_true", help="使用含标准库的默认编译路径")
    options = parser.parse_args()
    compiler = options.magel.resolve()
    cases = [
        ("groups", "$macro ADD[a](b){c}\na + b + c\n$end_macro\nint64 main() { return $ADD[1](2){3} - 6; }", None),
        ("group_shape", "$macro A[a]\na\n$end_macro\nint64 main() { return $A(0); }", "参数组"),
        ("empty_groups", "$macro A[](){}\n0\n$end_macro\nint64 main() { return $A[](){}; }", None),
        ("variadic_empty", "$macro ADD(...xs)\n$for x in xs\nx;\n$end_for\n0\n$end_macro\nint64 main() { return $ADD(); }", None),
        ("for_hygiene", "$macro ADD(total, ...xs)\n$for x in xs\nint64 temp = x; total += temp;\n$end_for\n$end_macro\nint64 main() { int64 temp = 100; int64 total = 0; $ADD(total, 1, 2, 3); return total + temp - 106; }", None),
        ("for_match", "$macro ADD(total, ...xs)\n$for x in xs\n$match x is expr\ntotal += x;\n$end_match\n$end_for\n$end_macro\nint64 main() { int64 total = 0; $ADD(total, 1, 2); return total - 3; }", None),
        ("variadic_not_last", "$macro A(...xs, y)\n0\n$end_macro\nint64 main() { return 0; }", "末尾"),
        ("for_non_variadic", "$macro A(xs)\n$for x in xs\nx;\n$end_for\n$end_macro\nint64 main() { $A(1); return 0; }", "变长"),
        ("source_line", "$macro LINE\n$line\n$end_macro\nint64 main() { return $LINE - 4; }", None),
        ("nested_line", "$macro LINE\n$line\n$end_macro\n$macro OUTER\n$LINE\n$end_macro\nint64 main() { return $OUTER - 7; }", None),
        ("forward_line", "$macro ID(x)\nx\n$end_macro\nint64 main() { return $ID($line) - 4; }", None),
        ("stringify", "$macro STR(x)\n$stringify(x)\n$end_macro\nint64 main() { String s = $STR(1 /*keep*/ + 2); if (s == \"1 /*keep*/ + 2\") { return 0; } return 1; }", None),
        ("stringify_rest", "$macro STR(...xs)\n$stringify(xs)\n$end_macro\nint64 main() { String text = $STR(a, /*keep*/ b); if (text == \"a, /*keep*/ b\") { return 0; } return 1; }", None),
        ("stringify_empty_rest", "$macro STR(...xs)\n$stringify(xs)\n$end_macro\nint64 main() { String text = $STR(); if (text == \"\") { return 0; } return 1; }", None),
        ("concat", "int64 ab = 42; int64 main() { return $concat(a, b) - 42; }", None),
        ("concat_reject", "int64 main() { return $concat(a, +); }", "单个标识符"),
        ("source_module", "module demo;\n$macro MODULE\n$module\n$end_macro\nint64 main() { String s = $MODULE; if (s == \"demo\") { return 0; } return 1; }", None),
        ("imported_macro", "module lib;\nint64 helper = 42;\n$macro VALUE\nhelper\n$end_macro\nmodule app;\nimport lib;\nint64 main() { int64 helper = 1; return $VALUE - 42; }", None),
        ("forward_rest", "$macro INNER(out, ...xs)\n$for x in xs\nout += x;\n$end_for\n$end_macro\n$macro OUTER(out, ...xs)\n$INNER(out, xs);\n$end_macro\nint64 main() { int64 out = 0; $OUTER(out, 1, 2, 3); return out - 6; }", None),
        ("replace", "$replace ADD(a, b) a + b\nint64 main() { return $ADD(40, 2) - 42; }", None),
        ("replace_string", "$replace URL() \"https://example\" // keep comment\nint64 main() { String s = $URL(); if (s == \"https://example\") { return 0; } return 1; }", None),
        ("type_parameter", "$macro DECLARE(T, name, value)\nT name = value;\n$end_macro\nint64 main() { $DECLARE(int32, value, 42); return value - 42; }", None),
        ("variadic_call", "$macro ADD(...xs)\nadd(xs)\n$end_macro\nint64 add(int64 a, int64 b) { return a + b; } int64 main() { return $ADD(40, 2) - 42; }", None),
        ("variadic_tuple", "$macro PAIR(first, ...rest)\n(first, rest)\n$end_macro\nint64 main() { auto pair = $PAIR(1 + 1, 2 + 2); return pair.0 * 10 + pair.1 - 24; }", None),
        ("empty_variadic_call", "$macro CALL(...xs)\nzero(xs)\n$end_macro\nint64 zero() { return 0; } int64 main() { return $CALL(); }", None),
        ("type_required", "$macro DECLARE(T, name)\nT name;\n$end_macro\nint64 main() { $DECLARE(1 + 2, value); return 0; }", "类型位置"),
        ("type_iteration", "$macro ADD(out, ...Types)\n$for T in Types\nT temp = 1; out += temp;\n$end_for\n$end_macro\nint64 main() { int64 out = 0; $ADD(out, int32, int64); return out - 2; }", None),
        ("control_inside_block", "$macro ADD(...xs)\n{ int64 total = 0;\n$for x in xs\ntotal += x;\n$end_for\nyield total; }\n$end_macro\nint64 main() { int64 value = $ADD(40, 2); return value - 42; }", None),
        ("type_qualifier", "$macro CALL(T)\nT::get()\n$end_macro\nstruct Box { static int64 get() { return 42; } } int64 main() { return $CALL(Box) - 42; }", None),
    ]
    work = ROOT / "build/macro-hygiene"
    work.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="groups-", dir=work) as directory:
        for name, source, error in cases:
            if options.default and name == "imported_macro":
                (Path(directory) / "lib.mgl").write_text("module lib;\nint64 helper = 42;\n$macro VALUE\nhelper\n$end_macro\n", encoding="utf-8")
                source = "module app;\nimport lib;\nint64 main() { int64 helper = 1; return $VALUE - 42; }"
            if error is None:
                at = source.rfind("return ")
                end = source.index(";", at)
                source = source[:at] + "return macroTestZero(" + source[at + 7:end] + ")" + source[end:]
                source += "\nint64 macroTestZero(int64 value) { if (value == 0) { return 0; } return 1 / (value - value); }\n"
            path = Path(directory) / (name + ".mgl")
            path.write_text(source + "\n", encoding="utf-8")
            result = subprocess.run([str(compiler), "run", str(path), *([] if options.default else ["--raw"]), "--out", str(Path(directory) / name)],
                                    cwd=ROOT, env=dict(os.environ, MAGEL_LANG="zh"), capture_output=True,
                                    text=True, encoding="utf-8", errors="replace", timeout=90)
            if error is None:
                assert result.returncode == 0 and not result.stderr, (name, result.returncode, result.stdout, result.stderr)
            else:
                assert result.returncode != 0 and error in result.stderr, (name, result.returncode, result.stderr)
    print(f"宏参数组与来源 VM 回归通过：{len(cases)} 项", flush=True)


if __name__ == "__main__":
    main()
