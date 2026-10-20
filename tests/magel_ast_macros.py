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
    compiler = parser.parse_args().magel.resolve()
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
        ("capture", "$macro VALUE\nhelper\n$end_macro\nint64 main() { int32 helper = 1; return $VALUE; }", "定义处绑定尚未接入"),
        ("update_conflict", "$macro TWICE(x)\nx + x\n$end_macro\nint64 main() { int32 a = 0; return $TWICE(++a); }", "MGL1301"),
        ("runtime_control", "int64 main() { int32 value = 2; return 1 / (value - value); }", "运行期错误"),
    ]
    with tempfile.TemporaryDirectory(prefix="vm-", dir=work) as directory:
        for name, source, error in cases:
            path = Path(directory) / (name + ".mgl")
            path.write_text(source + "\n", encoding="utf-8")
            result = subprocess.run([str(compiler), "run", str(path), "--raw", "--out", str(Path(directory) / name)],
                                    cwd=ROOT, env=dict(os.environ, MAGEL_LANG="zh"), capture_output=True,
                                    text=True, encoding="utf-8", errors="replace", timeout=90)
            if error is None:
                assert result.returncode == 0 and not result.stderr, (name, result.returncode, result.stdout, result.stderr)
            else:
                assert result.returncode != 0 and error in result.stderr, (name, result.returncode, result.stderr)
    print(f"AST 表达式宏 VM 接入回归通过：{len(cases)} 项", flush=True)


if __name__ == "__main__":
    main()
