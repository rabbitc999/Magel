"""验证自增减、复合赋值、值语义、引用、迭代器、链式调用及名称解析在解释执行与 AOT 中的一致性与资源清理。"""
from pathlib import Path
import argparse
import os
import subprocess

ROOT = Path(__file__).resolve().parents[1]
EXE = ".exe" if os.name == "nt" else ""
MAGEL = ROOT / "build/work/magel" / ("magel" + EXE)
WORK = ROOT / "build/magel-reference-values"


def run(*args):
    """执行工具或产物，保留标准输出和错误信息供失败诊断。"""
    result = subprocess.run([str(a) for a in args], cwd=ROOT, capture_output=True,
                            text=True, encoding="utf-8", errors="replace")
    assert result.returncode == 0, (args, result.returncode, result.stdout, result.stderr)
    return result


def main():
    """对解释执行 和 AOT 校验相同输出以及资源泄漏诊断。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--magel", type=Path, default=MAGEL, help="待验证的 MAGEL 工具链路径")
    parser.add_argument("--samples", nargs="+", default=["reference_sugar", "reference_combinations", "reference_lifetimes", "ownership_unary_edges", "ownership_operator_results", "ownership_exit_edges", "static_new", "conversion_interfaces", "constant_expressions", "destructuring_guards", "generic_inherited_constraint", "compound_assignment", "increment_decrement", "reference_value_types", "pointer_sort", "p1_reference_ownership", "p2_tuple_readonly", "collection_literals", "chained_temporaries", "chain_expression_cleanup", "iter_pipeline", "lambda_callbacks", "lambda_auto", "import_names", "core_name_shadow", "deprecated_identifiers"], help="指定本次验证的综合样例")
    args = parser.parse_args()
    compiler = args.magel.resolve()
    WORK.mkdir(parents=True, exist_ok=True)
    if "lambda_callbacks" in args.samples and "lambda_captures" not in args.samples:
        args.samples.append("lambda_captures")
    for sample in args.samples:
        source = ROOT / f"example/samples/{sample}.mgl"
        expected = (ROOT / f"example/expected/{sample}.txt").read_text(encoding="utf-8")
        flags = ["--threads"] if (ROOT / f"example/expected/{sample}.threads").is_file() else []
        results = [("解释执行", run(compiler, 'run', source, "--out", WORK / (sample + "-interpret"), *flags))]
        for mode in ("aot",):
            executable = WORK / (sample + "-" + mode + EXE)
            glue = {"function_pointer_glue": ROOT / "tests/fixtures/magel_generic_callback_glue.cpp",
                    "cpp_glue": ROOT / "source/glue/cpp_glue.cpp"}.get(sample)
            run(compiler, mode, source, "--out", executable, "--opt-level=0", *flags,
                *(["--glue", glue] if glue else []))
            results.append((mode, run(executable)))
        for mode, result in results:
            assert result.stdout == expected, (mode, result.stdout, expected)
            assert not result.stderr, (mode, result.stderr)
        print(f"[通过] {sample}：解释执行、AOT", flush=True)
    print(f"综合样例解释执行与 AOT 检查通过（{len(args.samples)} 个）：{', '.join(args.samples)}")


if __name__ == "__main__":
    main()
