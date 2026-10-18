"""逐库验证新语法及胶水在解释器、AOT 中的输出一致性。"""
import os
import subprocess
import sys
from magel_glue_edges import ROOT, MAGEL, EMITTER, EXE

SAMPLES = (
    "std_binary", "std_structured_data", "std_linalg_solve_glue",
    "std_complex_fft", "std_numerics_number_theory", "std_math_clock_random",
    "std_parse_random_path_memory", "std_collections_algorithms", "std_algorithms_extra",
    "std_glue_edges", "std_io", "std_file_result", "regex_limits",
    "typed_messages_shared", "thread_messages", "coroutine_typed_messages", "interrupt_arguments",
    "std_process_probe", "std_net", "string_method_reentry", "iter_pipeline",
)


def main():
    work = ROOT / "build/magel-library-migration"
    (work / "tmp").mkdir(parents=True, exist_ok=True)

    def run(*args):
        result = subprocess.run(list(map(str, args)), cwd=work, capture_output=True,
                                text=True, encoding="utf-8", errors="replace", timeout=300)
        assert result.returncode == 0, (result.returncode, args, result.stdout, result.stderr)
        return result

    for name in (tuple(sys.argv[1:]) or SAMPLES):
        source = ROOT / f"example/samples/{name}.mgl"
        expected = (ROOT / f"example/expected/{name}.txt").read_text(encoding="utf-8")
        threads = ["--threads"] if (ROOT / f"example/expected/{name}.threads").exists() else []
        prefix = work / name
        result = run(MAGEL, 'run', source, *threads, "-I", ROOT, "--out", prefix)
        assert result.stdout == expected and not result.stderr, (name, result.stdout, result.stderr)
        if name == "std_linalg_solve_glue":
            # 全整数宽度由解释器验证；导出沿用专项测试的三种代表标量，控制翻译单元大小。
            text = source.read_text(encoding="utf-8")
            for scalar in ("int8", "int16", "int32", "uint8", "uint16", "uint32", "uint64"):
                text = text.replace("    checkInteger<" + scalar + ">();", "")
            source = work / "linalg-export.mgl"
            source.write_text(text, encoding="utf-8")
        run(MAGEL, 'run', source, "-I", ROOT, "--emit", "--out", prefix)
        common = [*threads, "--quad", prefix.with_suffix(".quad"), "--data", prefix.with_suffix(".data.bin"),
                  "--layout", prefix.with_suffix(".layout")]
        for target in ("cpp-aot",):
            executable = work / (name + "-" + target + EXE)
            run(EMITTER, *common, "--target", target, "--out", executable,
                "--cpp-output-dir", work / (name + "-aot"))
            result = run(executable)
            assert result.stdout == expected and not result.stderr, (name, target, result.stdout, result.stderr)
        print(name + "：解释器、AOT 通过", flush=True)


if __name__ == "__main__":
    main()
