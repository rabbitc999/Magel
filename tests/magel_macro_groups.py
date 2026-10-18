"""验证多括号宏经过完整前端后仍保留原始文件、行列与源码诊断。"""
from pathlib import Path
import json
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]
BINARY = ROOT / ("magel.exe" if (ROOT / "magel.exe").exists() else "magel")


def main():
    """逐项检查捕获体、同一行后缀、后续行和定义体中错误的位置。"""
    cases = [
        "$macro B{..Body}\nBody\n$end_macro\nvoid main() {\n    $B{\n        int64 value = missing;\n    }\n}\n",
        "$macro B{..Body}\nBody\n$end_macro\nvoid main() { $B{ int64 value = missing; } }\n",
        "$macro B{..Body}\nBody\n$end_macro\nvoid main() {\n    $B{\n        int64 value = 1;\n    } int64 next = missing;\n}\n",
        "$macro B{..Body}\nBody\n$end_macro\nvoid main() {\n    $B{\n        int64 value = 1;\n    }\n    int64 next = missing;\n}\n",
        "$macro B{..Body}\nBody int64 value = missing;\n$end_macro\nvoid main() { $B{} }\n",
        "$macro B{..Body}\nBody\n$end_macro\nvoid main() { $B{\n    /* first\n       second */ int64 value = missing;\n} }\n",
    ]
    with tempfile.TemporaryDirectory(prefix="macro-groups-", dir=ROOT / "tmp") as temporary:
        for index, source in enumerate(cases):
            path = Path(temporary) / f"diagnostic-{index}.mgl"
            path.write_text(source, encoding="utf-8")
            result = subprocess.run([str(BINARY), "check", str(path), "json"], cwd=ROOT,
                                    capture_output=True, text=True, encoding="utf-8", timeout=60)
            assert result.returncode != 0, result.stdout
            diagnostics = json.loads(result.stdout)["diagnostics"]
            assert len(diagnostics) == 1, diagnostics
            diagnostic = diagnostics[0]
            assert "missing" in diagnostic["message"], diagnostic
            before = source[:source.index("missing")]
            line = before.count("\n") + 1
            column = len(before.rsplit("\n", 1)[-1]) + 1
            span = diagnostic["span"]
            assert Path(span["file"]).resolve() == path.resolve(), diagnostic
            assert (span["line"], span["column"]) == (line, column), diagnostic
            assert diagnostic["source"] == source.splitlines()[line - 1], diagnostic
    print(f"多括号宏完整前端文件与行列映射：{len(cases)} 项通过")


if __name__ == "__main__":
    main()
