"""验证编译器一次收集多个独立错误，以及语义结束后的借用检查。"""
from pathlib import Path
import argparse
import json
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def main():
    """检查跨语句、跨函数、多阶段诊断及原始源码定位。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--magel", type=Path, default=ROOT / "magel.exe")
    compiler = parser.parse_args().magel.resolve()
    work = ROOT / "build/magel-analysis-diagnostics"
    work.mkdir(parents=True, exist_ok=True)
    sources = {
        "effects": ("int64 main(){\n int64 i=1,j=2;\n i=i++;\n j+=j++;\n return 0;\n}\n", {3: "MGL1301", 4: "MGL1301"}),
        "borrow": ("int64 mut& first(){\n int64 x=1;\n return &x;\n}\nint64 mut& second(){\n int64 y=2;\n return &y;\n}\nint64 main(){return 0;}\n", {3: "MGL1204", 7: "MGL1204"}),
        "mixed": ("int64 mut& first(){\n int64 x=1;\n x=x++;\n return &x;\n}\nint64 main(){\n missingOne();\n missingTwo();\n return 0;\n}\n", {3: "MGL1301", 4: "MGL1204", 7: "MGL1200", 8: "MGL1200"}),
    }
    for name, (text, expected) in sources.items():
        source = work / (name + ".mgl")
        source.write_text(text, encoding="utf-8")
        result = subprocess.run([str(compiler), "--diagnostics=json", str(source)], cwd=ROOT,
                                capture_output=True, encoding="utf-8", errors="replace", timeout=90)
        assert result.returncode != 0, (name, result)
        payload = json.loads(result.stdout)
        diagnostics = payload["diagnostics"]
        actual = {d["span"]["line"]: d["code"] for d in diagnostics}
        assert actual == expected, (name, actual, expected, diagnostics, result.stderr)
        for diagnostic in diagnostics:
            assert Path(diagnostic["span"]["file"]).resolve() == source.resolve(), diagnostic
            assert diagnostic["source"] == text.splitlines()[diagnostic["span"]["line"] - 1], diagnostic
        plain = subprocess.run([str(compiler), "--check", str(source)], cwd=ROOT,
                               capture_output=True, encoding="utf-8", errors="replace", timeout=90)
        assert plain.returncode != 0 and plain.stderr.count("magel：") == len(expected), plain
    print("多诊断验证通过：跨语句、跨函数、借用与语义混合错误及 JSON 定位")


if __name__ == "__main__":
    main()
