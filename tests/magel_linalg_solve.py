"""验证线性代数求解、数组互转与索引在解释执行 和 AOT 导出中的一致性。

先构建 magel/magel（或 magel.exe）与 magel-emit，再运行本文件。
"""
from pathlib import Path
import os
import subprocess

ROOT = Path(__file__).resolve().parents[1]
EXE = ".exe" if os.name == "nt" else ""
WORK = ROOT / "build" / "magel-linalg-solve"
MAGEL = ROOT / "build/work/magel" / ("magel" + EXE)
EMITTER = ROOT / "build/magel-emit" / ("magel-emit" + EXE)


def run(*args, succeeds=True):
    result = subprocess.run([str(a) for a in args], cwd=ROOT, capture_output=True,
                            text=True, encoding="utf-8", errors="replace")
    assert (result.returncode == 0) == succeeds, (result.returncode, args, result.stdout, result.stderr)
    return result


def main():
    WORK.mkdir(parents=True, exist_ok=True)
    source = ROOT / "example/samples/std_linalg_solve_glue.mgl"
    expected = (ROOT / "example/expected/std_linalg_solve_glue.txt").read_text(encoding="utf-8")
    result = run(MAGEL, 'run', source, "--out", WORK / "vm")
    assert result.stdout == expected, result.stdout
    assert "程序结束" not in result.stderr, result.stderr
    prefix = WORK / "edges"
    # VM 检查全部整数特化；导出覆盖双精度、单精度及 int64，控制单个 C++ 翻译单元大小。
    exportSource = WORK / "export.mgl"
    text = source.read_text(encoding="utf-8")
    for scalar in ("int8", "int16", "int32", "uint8", "uint16", "uint32", "uint64"):
        text = text.replace("    checkInteger<" + scalar + ">();", "")
    exportSource.write_text(text, encoding="utf-8")
    run(MAGEL, 'run', exportSource, "--emit", "--out", prefix)
    common = ["--quad", prefix.with_suffix(".quad"), "--data", prefix.with_suffix(".data.bin"),
              "--layout", prefix.with_suffix(".layout")]
    for target in ("cpp-aot",):
        executable = WORK / (target + EXE)
        run(EMITTER, *common, "--target", target, "--out", executable,
            "--cpp-output-dir", WORK / "aot")
        result = run(executable)
        assert result.stdout == expected, (target, result.stdout)
        assert "程序结束" not in result.stderr, result.stderr
    executable = WORK / ("trimmed" + EXE)
    run("g++", "-std=c++17", "-O0", "-DNO_MAGEL_STD_LINALG",
        WORK / "aot" / ("cpp-aot" + EXE + ".aot.cpp"), "-o", executable)
    result = run(executable, succeeds=False)
    assert "std::linalg::__matrix" in result.stdout + result.stderr
    for setup, message in (
        ("a.set(0, 0, 0.0);", "奇异或近奇异矩阵"),
        ("a.set(0, 0, 0.000000000001);", "奇异或近奇异矩阵"),
        ("a.rows = 2;", "维度不匹配"),
        ("a.rows = -1; a.cols = -1;", "维度不匹配"),
        ("a.values.clear();", "维度不匹配"),
        ("std::linalg::Vector<float64> col = a[-1];", "列索引越界"),
        ("std::linalg::Vector<float64> col = a[1];", "列索引越界"),
        ("float64 value = rhs[1];", "越界"),
        ("float64 raw[2]; std::linalg::Matrix<float64> bad = std::linalg::Matrix<float64>::fromArray(raw, 2, 2, 2);", "维度不匹配"),
    ):
        invalid = WORK / "invalid.mgl"
        invalid.write_text("import std::linalg;\nint64 main() { "
            "std::linalg::Matrix<float64> a = std::linalg::Matrix<float64>::identity(1); "
            "std::linalg::Vector<float64> rhs = std::linalg::Vector<float64>::zeros(1); "
            + setup + " std::linalg::Vector<float64> x = a.solve(rhs); return 0; }",
            encoding="utf-8")
        result = run(MAGEL, 'run', invalid, "--out", WORK / "invalid", succeeds=False)
        assert message in result.stdout + result.stderr, result.stdout + result.stderr
    print("线性代数检查通过：VM、AOT、裁剪、奇异矩阵与非法尺寸")


if __name__ == "__main__":
    main()
