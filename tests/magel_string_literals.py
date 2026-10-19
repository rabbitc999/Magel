"""验证字面量数据合并、UTF-8 字节长度与 VM/AOT 字符串副本独立性。"""
import argparse
import os
import shutil
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    """运行两种后端并检查重复字面量只占用一份生成期数据。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--magel", type=Path, default=ROOT / ("magel.exe" if os.name == "nt" else "magel"))
    compiler = parser.parse_args().magel.resolve()
    if os.name == "nt" and not compiler.suffix and compiler.with_suffix(".exe").is_file():
        compiler = compiler.with_suffix(".exe")
    with tempfile.TemporaryDirectory(prefix="string-literals-", dir=ROOT / "build") as temporary:
        work = Path(temporary)
        source = ROOT / "example/samples/string_literal_pool.mgl"
        expected = (ROOT / "example/expected/string_literal_pool.txt").read_text(encoding="utf-8")
        tools = work / "tools"
        tools.mkdir()
        isolated_compiler = tools / compiler.name
        shutil.copy2(compiler, isolated_compiler)
        shadow = work / "magel"
        (shadow / "source/magel-emit").mkdir(parents=True)
        (shadow / "source/magel-emit/aot.cpp").write_text("// repository marker\n", encoding="utf-8")
        (shadow / "library/stackvm/parts").mkdir(parents=True)
        (shadow / "library/stackvm/parts/machine_natives.cpp").write_text(
            '#error unexpected_external_runtime\n', encoding="utf-8")
        for mode in ("run", "aot"):
            target = work / (mode + (".exe" if os.name == "nt" and mode == "aot" else ""))
            runner = isolated_compiler if mode == "aot" else compiler
            result = subprocess.run([str(runner), mode, str(source), "--out", str(target)],
                                    cwd=work, capture_output=True, text=True, encoding="utf-8", timeout=300)
            assert result.returncode == 0, (mode, result.stdout, result.stderr)
            if mode == "aot":
                result = subprocess.run([str(target)], capture_output=True, text=True, encoding="utf-8", timeout=60)
            assert result.returncode == 0 and result.stdout == expected and not result.stderr, (mode, result.stdout, result.stderr)
            if mode == "run":
                data = target.with_suffix(".data.bin").read_bytes()
                assert data.count("literal-pool-data-你好".encode("utf-8")) == 1, "重复字面量未合并"
        for index, statement in enumerate((
                '"hello"[0] = \'H\';',
                '"hello"[0]++;',
                'char mut& byte = &"hello"[0];')):
            invalid = work / f"readonly-{index}.mgl"
            invalid.write_text("int64 main() { " + statement + " return 0; }\n", encoding="utf-8")
            result = subprocess.run([str(compiler), "--check", str(invalid)], cwd=ROOT,
                                    capture_output=True, text=True, encoding="utf-8", timeout=120)
            assert result.returncode != 0 and ("只读" in result.stderr or "const&" in result.stderr), result.stderr
    print("字面量合并：VM/AOT、只读字节借用、UTF-8 长度、值复制、常量去重与相邻仓库隔离通过")


if __name__ == "__main__":
    main()
