"""验证非空引用在 VM 与 AOT 中的读写和递归行为。"""
import os
from magel_glue_edges import ROOT, MAGEL, EMITTER, EXE, run


def main(names=("reference", "foreach_reference", "tree", "lang_float_bytes", "reference_glue_optional", "fourier")):
    work = ROOT / "build/magel-references"
    work.mkdir(parents=True, exist_ok=True)
    for name in names:
        source = ROOT / f"example/samples/{name}.mgl"
        expected = (ROOT / f"example/expected/{name}.txt").read_text(encoding="utf-8")
        prefix = work / name
        result = run(MAGEL, 'run', source, "--out", prefix, "--step-limit", "1000000")
        assert result.stdout == expected and not result.stderr, (name, result.stdout, result.stderr)
        run(MAGEL, 'run', source, "--emit", "--out", prefix)
        args = ["--quad", prefix.with_suffix(".quad"), "--data", prefix.with_suffix(".data.bin"),
                "--layout", prefix.with_suffix(".layout")]
        glue = ["--glue", ROOT / "tests/fixtures/magel_optional_reference_glue.cpp"] if name == "reference_glue_optional" else []
        for target in ("cpp-aot",):
            executable = work / (name + "-" + target + EXE)
            run(EMITTER, *args, *glue, "--target", target, "--out", executable,
                "--cpp-output-dir", work / (name + "-aot"))
            result = run(executable)
            assert result.stdout == expected and not result.stderr, (name, target, result.stdout, result.stderr)
    checkUnboundField()
    print("引用：自动读写、非空语法、只读转换、成员访问、斐波那契与递归树解释与 AOT验证通过")


def checkUnboundField():
    work = ROOT / "build/magel-references"
    work.mkdir(parents=True, exist_ok=True)
    source = ROOT / "example/samples-error/reference_unbound_field.mgl"
    prefix = work / "unbound-field"
    result = run(MAGEL, 'run', source, "--out", prefix, succeeds=False)
    assert "引用尚未绑定有效元素" in result.stderr, result.stderr
    run(MAGEL, 'run', source, "--emit", "--out", prefix)
    executable = work / ("unbound-field" + EXE)
    run(EMITTER, "--quad", prefix.with_suffix(".quad"), "--data", prefix.with_suffix(".data.bin"),
        "--layout", prefix.with_suffix(".layout"), "--target", "cpp-aot", "--out", executable,
        "--magel-dir", work / "missing-runtime")
    result = run(executable, succeeds=False)
    assert "引用尚未绑定有效元素" in result.stderr, result.stderr


if __name__ == "__main__":
    main()
