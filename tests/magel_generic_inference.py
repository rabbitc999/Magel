"""验证后置泛型、枚举上下文与泛型回调在解释器及两种 C++ 后端中的一致性。"""
import os
from magel_glue_edges import ROOT, MAGEL, EMITTER, EXE, run


def main():
    work = ROOT / "build/magel-generic-inference"
    work.mkdir(parents=True, exist_ok=True)
    for name in ("generic_inference", "function_pointer_glue", "cpp_glue"):
        source = ROOT / f"example/samples/{name}.mgl"
        expected = (ROOT / f"example/expected/{name}.txt").read_text(encoding="utf-8")
        prefix = work / name
        result = run(MAGEL, 'run', source, "--out", prefix)
        assert result.stdout == expected and not result.stderr, (name, result.stdout, result.stderr)
        run(MAGEL, 'run', source, "--emit", "--out", prefix)
        common = ["--quad", prefix.with_suffix(".quad"), "--data", prefix.with_suffix(".data.bin"),
                  "--layout", prefix.with_suffix(".layout")]
        glue = [] if name == "generic_inference" else ["--glue", ROOT / (
            "tests/fixtures/magel_generic_callback_glue.cpp" if name == "function_pointer_glue"
            else "source/glue/cpp_glue.cpp")]
        for target in ("cpp-aot",):
            executable = work / (name + "-" + target + EXE)
            run(EMITTER, *common, *glue, "--target", target, "--out", executable,
                "--cpp-output-dir", work / (name + "-aot"))
            result = run(executable)
            assert result.stdout == expected and not result.stderr, (name, target, result.stdout, result.stderr)
        print(name + "：解释器、AOT 一致", flush=True)


if __name__ == "__main__":
    main()
