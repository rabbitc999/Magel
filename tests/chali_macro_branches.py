"""验证生成解析器的五类完整片段、编译期分支及 ECMAScript 原文匹配。"""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    """运行真实 VM，检查分支顺序、未选中语义隔离、原文及正则错误。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--chali", type=Path, default=ROOT / ("ChaLi.exe" if os.name == "nt" else "chali"))
    parser.add_argument("--default", action="store_true", help="使用含标准库的默认编译路径")
    options = parser.parse_args()
    compiler = options.chali.resolve()
    choose = "$match A is ident\n11\n$else is expr\n22\n$else is type\n33\n$else is block\n44\n$else is stmt\n55\n$else\n66\n$end_match"
    cases = [
        ("ident", choose, "notDeclared", 11),
        ("expr", choose, "1 + 2", 22),
        ("type", choose, "int64", 33),
        ("unknown_generic_type", choose, "Unknown<int64, int32, int8>", 33),
        ("lowercase_generic_type", choose, "unknown<int64>", 33),
        ("block", "$match A is block\n44\n$else\n0\n$end_match", "{ int64 value = 1; }", 44),
        ("stmt", choose, "return 0;", 55),
        ("partial_expr", choose, "1 +", 66),
        ("full_ident", "$match A is ident\n1\n$else\n0\n$end_match", "a + b", 0),
        ("branch_order", "$match A is expr\n1\n$else is ident\n2\n$end_match", "missing", 1),
        ("inactive_semantics", "$match A is ident\n1\n$else\nmissingFunction();\nint64 invalid = unknown;\n0\n$end_match", "anything", 1),
        ("inactive_closure", "$match A is ident\n1\n$else\nlet unused = fn(int64 x) { missingFunction(); return x; };\n0\n$end_match", "anything", 1),
        ("regex_digits", '$regex A is "[0-9]+"\n1\n$else\n0\n$end_regex', "  123  ", 1),
        ("regex_full", '$regex A is "[0-9]+"\n1\n$else\n0\n$end_regex', "abc123", 0),
        ("regex_quotes", '$regex A is "[0-9]+"\n1\n$else\n0\n$end_regex', '"123"', 0),
        ("regex_backref", '$regex A is "([a-z])\\\\1"\n1\n$else\n0\n$end_regex', "aa", 1),
        ("regex_lookahead", '$regex A is "(?=a)a"\n1\n$else\n0\n$end_regex', "a", 1),
        ("regex_decode_once", '$regex A is "\\\\d+"\n1\n$else\n0\n$end_regex', "42", 1),
        ("regex_comment", '$regex A is "a /\\\\*keep\\\\*/ b"\n1\n$else\n0\n$end_regex', "a /*keep*/ b", 1),
        ("regex_newline", '$regex A is "[\\\\s\\\\S]+"\n1\n$else\n0\n$end_regex', "a\nb", 1),
    ]
    work = ROOT / "build/macro-hygiene"
    work.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="branches-", dir=work) as folder:
        directory = Path(folder)

        def execute(name, source):
            """独立编译一个分支用例，隔离所有中间产物。"""
            path = directory / (name + ".chl")
            path.write_text(source + "\n", encoding="utf-8")
            return subprocess.run([str(compiler), "run", str(path), *([] if options.default else ["--raw"]), "--out", str(directory / name)],
                                  cwd=ROOT, env=dict(os.environ, CHALI_LANG="zh"), capture_output=True,
                                  text=True, encoding="utf-8", errors="replace", timeout=90)

        for name, body, argument, expected in cases:
            source = "$macro CHOOSE(A)\n" + body + "\n$end_macro\nvoid main() { int64 result = $CHOOSE(" + argument + "); if (result == " + str(expected) + ") { return; } let __boom = 1 / (result - result); }"
            result = execute(name, source)
            assert result.returncode == 0 and not result.stderr, (name, result.returncode, result.stdout, result.stderr)
        source = "$macro EMIT(A)\n$match A is stmt\nA;\n$end_match\n$end_macro\nvoid main() { $EMIT(return;); }"
        result = execute("emit_statement", source)
        assert result.returncode == 0 and not result.stderr, result.stderr
        source = "$macro EMPTY(A)\n$match A is stmt\nA;\n$end_match\n$end_macro\nvoid main() { $EMPTY(1 +); return; }"
        result = execute("no_match", source)
        assert result.returncode == 0 and not result.stderr, result.stderr
        result = execute("invalid_regex", '$macro BAD(A)\n$regex A is "["\n1\n$else\n0\n$end_regex\n$end_macro\nvoid main() { let __boom = $BAD(123); }')
        assert result.returncode != 0 and "宏正则模式非法" in result.stderr, result.stderr
    print(f"宏语法分类与正则分支：{len(cases) + 3} 项 VM 回归通过", flush=True)


if __name__ == "__main__":
    main()
