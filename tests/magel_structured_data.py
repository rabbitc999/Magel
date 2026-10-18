from pathlib import Path
"""验证 JSON/TOML 副本、MAGEL 胶水、导出与裁剪。"""
import os
from magel_glue_edges import ROOT, MAGEL, EMITTER, EXE, run


def main():
    work = ROOT / "build/structured-data"
    work.mkdir(parents=True, exist_ok=True)
    for name in ("json", "toml"):
        for file in (name + ".hpp", name + ".cpp", "bind.hpp", "test_" + name + ".cpp"):
            assert (ROOT / "library" / name / file).read_bytes() == (ROOT / name / file).read_bytes(), file
        executable = work / ("test-" + name + EXE)
        run("g++", "-std=c++17", "-O2", ROOT / f"{name}/test_{name}.cpp",
            ROOT / f"{name}/{name}.cpp", "-o", executable)
        run(executable)
    source = ROOT / "example/samples/std_structured_data.mgl"
    expected = (ROOT / "example/expected/std_structured_data.txt").read_text(encoding="utf-8")
    prefix = work / "data"
    result = run(MAGEL, 'run', source, "--out", prefix)
    assert result.stdout == expected and not result.stderr, (result.stdout, result.stderr)
    run(MAGEL, 'run', source, "--emit", "--out", prefix)
    arguments = ["--quad", prefix.with_suffix(".quad"), "--data", prefix.with_suffix(".data.bin"),
                 "--layout", prefix.with_suffix(".layout")]
    asset_target = work / ("data" + "-assets" + EXE)
    run(EMITTER, *arguments, "--target", "cpp-aot", "--out", asset_target,
        "--cpp-output", "--magel-dir", work / "missing-runtime")
    cpp = Path(str(asset_target) + ".aot.cpp")
    for target in ("cpp-aot",):
        executable = work / (target + EXE)
        run(EMITTER, *arguments, "--target", target, "--out", executable,
            "--cpp-output-dir", work / "aot")
        result = run(executable)
        assert result.stdout == expected and not result.stderr, (target, result.stdout, result.stderr)
    for name in ("JSON", "TOML"):
        executable = work / ("trim-" + name + EXE)
        run("g++", "-std=c++17", "-O0", *(["-Wa,-mbig-obj"] if os.name == "nt" else []), "-DNO_MAGEL_STD_" + name, cpp, "-o", executable)
        result = run(executable, succeeds=False)
        assert "std::" + name.lower() + "::" in result.stdout + result.stderr
    result = run(MAGEL, 'run', ROOT / "example/samples-error/structured_closed.mgl",
                 "--out", work / "closed", succeeds=False)
    assert "结构化数据：文档已关闭或节点无效" in result.stderr
    print("JSON/TOML：库副本、VM、AOT、内嵌资源与裁剪验证通过")


if __name__ == "__main__":
    main()
