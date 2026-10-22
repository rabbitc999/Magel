"""验证词法及语义诊断的源码摘录不含多余空竖线行。"""
from pathlib import Path
import argparse
import os
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]

def main():
    """检查中文非法字符、未声明名称和 TOML 根节点保存行为。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--chali", default=str(ROOT / "ChaLi.exe"))
    compiler = Path(parser.parse_args().chali).resolve()
    with tempfile.TemporaryDirectory(prefix="chali-layout-", dir=ROOT / "build") as folder:
        source = Path(folder) / "layout.chl"
        for text, message in [
            ("void main()\n{\n    中文测试\n}\n", "认不出来的字符"),
            ("void main()\n{\n    missingValue = 1;\n}\n", "missingValue"),
        ]:
            source.write_text(text, encoding="utf-8")
            result = subprocess.run([str(compiler), "--check", str(source)], cwd=ROOT,
                                    capture_output=True, text=True, encoding="utf-8",
                                    env=dict(os.environ, CHALI_LANG="zh"), timeout=90)
            diagnostic = result.stdout + result.stderr
            assert result.returncode != 0 and message in diagnostic, diagnostic
            assert not re.search(r"(?m)^\s*\|\s*$", diagnostic), diagnostic
            assert re.search(r"(?m)^\s*3\s*\|", diagnostic), diagnostic
            assert re.search(r"(?m)^\s*\|\s*~+", diagnostic), diagnostic
        source.write_text('''import std::toml;
void main()
{
    String original = "# header\\nanswer = 42 # inline\\n[table] # table\\nname = '# string'\\n";
    std::toml::TomlValue first = std::toml::parse(original).unwrap();
    String saved = first.dump();
    assert(saved == original);
    std::toml::TomlValue second = std::toml::parse(saved).unwrap();
    assert(second.dump() == original);
    println("TOML comments stable");
}
''', encoding="utf-8")
        result = subprocess.run([str(compiler), "run", str(source)], cwd=ROOT, capture_output=True,
                                text=True, encoding="utf-8", timeout=90)
        assert result.returncode == 0 and result.stdout == "TOML comments stable\n", result
    print("diagnostic layout and TOML comment round trip: PASS")

if __name__ == "__main__":
    main()
