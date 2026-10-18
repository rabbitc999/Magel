"""验证标准库胶水在解释执行 和 AOT 导出中的一致性。

先构建 magel/magel（或 magel.exe）与 magel-emit，再运行本文件。
"""
from pathlib import Path
import os
import subprocess

ROOT = Path(__file__).resolve().parents[1]
EXE = ".exe" if os.name == "nt" else ""
WORK = ROOT / "build" / "magel-glue-edges"
MAGEL = Path(os.environ.get('MAGEL_COMPILER', ROOT / ("magel" + EXE))).resolve()
EMITTER = ROOT / "build/magel-emit" / ("magel-emit" + EXE)


def run(*args, succeeds=True):
    result = subprocess.run([str(a) for a in args], cwd=ROOT, capture_output=True,
                            text=True, encoding="utf-8", errors="replace")
    assert (result.returncode == 0) == succeeds, result.stdout + result.stderr
    return result


def main():
    WORK.mkdir(parents=True, exist_ok=True)
    source = ROOT / "example/samples/std_glue_edges.mgl"
    expected = (ROOT / "example/expected/std_glue_edges.txt").read_text(encoding="utf-8")
    result = run(MAGEL, 'run', source, "--out", WORK / "vm")
    assert result.stdout == expected, result.stdout
    assert "程序结束" not in result.stderr, result.stderr
    prefix = WORK / "edges"
    run(MAGEL, 'run', source, "--emit", "--out", prefix)
    common = ["--quad", prefix.with_suffix(".quad"), "--data", prefix.with_suffix(".data.bin"),
              "--layout", prefix.with_suffix(".layout")]
    for target in ("cpp-aot",):
        executable = WORK / (target + EXE)
        run(EMITTER, *common, "--target", target, "--out", executable,
            "--cpp-output-dir", WORK / "aot")
        result = run(executable)
        assert result.stdout == expected, (target, result.stdout)
        assert "程序结束" not in result.stderr, result.stderr
    # 模块裁剪：只对仍有宿主胶水的模块有意义。std::text / std::path / std::numerics /
    # std::numtheory 已整体迁到 .mgl（宿主绑定删净），没有可裁剪的胶水，故不再列入。
    for macro in ("NO_MAGEL_STD_MATH",):
        executable = WORK / (macro + EXE)
        run("g++", "-std=c++17", "-O0", "-D" + macro,
            WORK / "aot" / ("cpp-aot" + EXE + ".aot.cpp"), "-o", executable)
        result = run(executable, succeeds=False)
        module = "std::math::" if macro.endswith("MATH") else "std::text::"
        assert module in result.stdout + result.stderr, result.stdout + result.stderr
    for expression in ("std::math::clamp(1, 2, 0)",
                       "std::math::clamp(1.0, 2.0, 0.0)",
                       "std::math::clamp(1.0, std::math::sqrt(-1.0), 2.0)"):
        invalid = WORK / "invalid.mgl"
        invalid.write_text("import std::math;\nint64 main() { " + expression +
                           "; return 0; }", encoding="utf-8")
        result = run(MAGEL, 'run', invalid, "--out", WORK / "invalid", succeeds=False)
        assert "low 不能大于 high" in result.stdout + result.stderr, result.stdout + result.stderr
    print("标准库胶水边界检查通过：VM、AOT、模块裁剪、非法夹取区间")


if __name__ == "__main__":
    main()

