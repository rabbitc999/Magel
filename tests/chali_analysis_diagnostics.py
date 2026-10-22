"""验证编译器一次收集多个独立错误，以及语义结束后的借用检查。"""
from pathlib import Path
import argparse
import json
import os
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def main():
    """检查跨语句、跨函数、多阶段诊断及原始源码定位。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--chali", type=Path, default=ROOT / "ChaLi.exe")
    compiler = parser.parse_args().chali.resolve()
    work = ROOT / "build/chali-analysis-diagnostics"
    work.mkdir(parents=True, exist_ok=True)
    sources = {
        "effects": ("void main(){\n int64 i=1,j=2;\n i=i++;\n j+=j++;\n return;\n}\n", {3: "CHL1301", 4: "CHL1301"}),
        "borrow": ("int64 mut& first(){\n int64 x=1;\n return &x;\n}\nint64 mut& second(){\n int64 y=2;\n return &y;\n}\nvoid main(){return;}\n", {3: "CHL1204", 7: "CHL1204"}),
        "mixed": ("int64 mut& first(){\n int64 x=1;\n x=x++;\n return &x;\n}\nvoid main(){\n missingOne();\n missingTwo();\n return;\n}\n", {3: "CHL1301", 4: "CHL1204", 7: "CHL1200", 8: "CHL1200"}),
    }
    for name, (text, expected) in sources.items():
        source = work / (name + ".chl")
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
        assert plain.returncode != 0 and len(re.findall(r"^chali: ", plain.stderr, re.MULTILINE)) == len(expected), plain
        assert "chali：" not in plain.stderr, plain.stderr
    for language in ("zh", "en"):
        missing = subprocess.run([str(compiler), "--mode"], cwd=ROOT, env=dict(os.environ, CHALI_LANG=language),
                                 capture_output=True, encoding="utf-8", errors="replace", timeout=90)
        assert missing.returncode != 0 and missing.stderr.startswith("chali: "), (language, missing)
        assert "chali：" not in missing.stderr and "chali: chali:" not in missing.stderr, missing.stderr
    print("多诊断验证通过：跨语句、跨函数、借用与语义混合错误及 JSON 定位")


if __name__ == "__main__":
    main()
