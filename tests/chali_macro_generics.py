"""验证宏类型参数只从声明的静态来源或期望类型推导。"""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    """执行显式类型、来源推导、结果推导及不足诊断用例。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--chali", type=Path, default=ROOT / "ChaLi.exe")
    parser.add_argument("--default", action="store_true", help="使用含标准库的默认编译路径")
    options = parser.parse_args()
    compiler = options.chali.resolve()
    cases = [
        ("explicit", "$macro ID<T>(x)\ncast<T>(x)\n$end_macro\nvoid main() { int32 a = $ID<int32>(42); let __boom = a - 42; }", None),
        ("argument", "$macro ID<T:x>(x)\ncast<T>(x)\n$end_macro\nvoid main() { int32 x = 42; let a = $ID(x); let __boom = a - 42; }", None),
        ("expected", "$macro ID<T>(x)\ncast<T>(x)\n$end_macro\nvoid main() { int32 a = $ID(42); let __boom = a - 42; }", None),
        ("return_expected", "$macro ID<T>(x)\ncast<T>(x)\n$end_macro\nint32 test() { return $ID(42); } void main() { let __boom = test() - 42; }", None),
        ("explicit_wins", "$macro ID<T:x>(x)\ncast<T>(x)\n$end_macro\nvoid main() { int64 x = 42; int32 a = $ID<int32>(x); let __boom = a - 42; }", None),
        ("no_execution", "$macro ZERO<T:x>(x)\ncast<T>(0)\n$end_macro\nint64 counter = 0; int64 next() { counter += 1; return 42; } void main() { int64 a = $ZERO(next()); let __boom = counter + a; }", None),
        ("statement", "$macro SET<T:x>(name, x)\nT name = x;\n$end_macro\nvoid main() { int32 x = 42; $SET(value, x); let __boom = value - 42; }", None),
        ("reference_type", "$macro ID<T:x>(x)\ncast<T>(x)\n$end_macro\nvoid main() { int32 x = 42; let p = $ID(&x); let __boom = *p - 42; }", None),
        ("nested", "$macro ID<T:x>(x)\ncast<T>(x)\n$end_macro\nvoid main() { int32 x = 42; let y = $ID($ID(x)); let __boom = y - 42; }", None),
        ("closure_type", "$macro ID<T:x>(x)\ncast<T>(x)\n$end_macro\nvoid main() { let f = $ID(fn(int64 value) { value + 40 }); let __boom = f(2) - 42; }", None),
        ("unused_semantics", "$macro ZERO<T>(x)\ncast<T>(0)\n$end_macro\nvoid main() { int32 value = $ZERO(cast<Missing>(1)); let __boom = value; }", None),
        ("ordinary_generic_expression", "$macro ID(x)\nx\n$end_macro\nT identity<T>(T value) { return value; } void main() { int32 value = 42; let __boom = $ID(identity<int32>(value)) - 42; }", None),
        ("generic_type_qualifier", "$macro ID(x)\nx\n$end_macro\nstruct Box<T> { static int64 get() { return 42; } } void main() { let __boom = $ID(Box<int32>::get()) - 42; }", None),
        ("outer_generic_type", "$macro ID(x)\nx\n$end_macro\nT outer<T>(T value) { return $ID(cast<T>(value)); } void main() { int32 value = 42; let __boom = outer<int32>(value) - 42; }", None),
        ("outer_generic_explicit", "$macro ID<T>(x)\ncast<T>(x)\n$end_macro\nT outer<T>(T value) { return $ID<T>(value); } void main() { int32 value = 42; let __boom = outer<int32>(value) - 42; }", None),
        ("no_fallback", "$macro ID<T>(x)\ncast<T>(x)\n$end_macro\nvoid main() { let a = $ID(42); let __boom = a; }", "缺少静态类型约束"),
        ("bad_source", "$macro ID<T:y>(x)\ncast<T>(x)\n$end_macro\nvoid main() { let __boom = $ID(42); }", "推导来源不是形参"),
        ("wrong_count", "$macro ID<T>(x)\ncast<T>(x)\n$end_macro\nvoid main() { let __boom = $ID<int64,int32>(42); }", "类型实参数量"),
    ]
    work = ROOT / "build/macro-hygiene"
    work.mkdir(parents=True, exist_ok=True)
    executed = 0
    with tempfile.TemporaryDirectory(prefix="generic-", dir=work) as directory:
        for name, source, error in cases:
            if not options.default and name in {"ordinary_generic_expression", "generic_type_qualifier", "outer_generic_type", "outer_generic_explicit"}:
                continue
            executed += 1
            if error is None:
                marker = "let __boom = "
                at = source.rfind(marker)
                end = source.index(";", at)
                source = source[:at] + "macroTestZero(" + source[at + len(marker):end] + ")" + source[end:]
                source += "\nint64 macroTestZero(int64 value) { if (value == 0) { return 0; } return 1 / (value - value); }\n"
            path = Path(directory) / (name + ".chl")
            path.write_text(source + "\n", encoding="utf-8")
            result = subprocess.run([str(compiler), "run", str(path), *([] if options.default else ["--raw"]), "--out", str(Path(directory) / name)],
                                    cwd=ROOT, env=dict(os.environ, CHALI_LANG="zh"), capture_output=True,
                                    text=True, encoding="utf-8", errors="replace", timeout=90)
            if error is None:
                assert result.returncode == 0 and not result.stderr, (name, result.returncode, result.stdout, result.stderr)
            else:
                assert result.returncode != 0 and error in result.stderr, (name, result.returncode, result.stderr)
    print(f"宏静态类型推导 VM 回归通过：{executed} 项", flush=True)


if __name__ == "__main__":
    main()
