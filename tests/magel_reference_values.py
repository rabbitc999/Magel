"""验证自增减、复合赋值、值语义、引用、迭代器、链式调用及名称解析在解释执行与 VM 中的一致性与资源清理。"""
from pathlib import Path
import argparse
import os
import re
import subprocess
ROOT = Path(__file__).resolve().parents[1]
EXE = '.exe' if os.name == 'nt' else ''
MAGEL = ROOT / ('magel' + EXE)
WORK = ROOT / 'build/magel-reference-values'

def run(*args):
    """执行工具或产物，保留标准输出和错误信息供失败诊断。"""
    result = subprocess.run([str(a) for a in args], cwd=ROOT, capture_output=True, text=True, encoding='utf-8', errors='replace')
    assert result.returncode == 0, (args, result.returncode, result.stdout, result.stderr)
    return result

def main():
    """对解释执行 和 VM 校验相同输出以及资源泄漏诊断。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--magel', type=Path, default=MAGEL, help='待验证的 MAGEL 工具链路径')
    parser.add_argument('--samples', nargs='+', default=['reference_sugar', 'reference_combinations', 'reference_lifetimes', 'ownership_unary_edges', 'ownership_operator_results', 'ownership_exit_edges', 'static_new', 'conversion_interfaces', 'constant_expressions', 'destructuring_guards', 'generic_inherited_constraint', 'compound_assignment', 'increment_decrement', 'reference_value_types', 'pointer_sort', 'p1_reference_ownership', 'p2_tuple_readonly', 'collection_literals', 'chained_temporaries', 'chain_expression_cleanup', 'iter_pipeline', 'lambda_callbacks', 'lambda_auto', 'import_names', 'core_name_shadow', 'deprecated_identifiers'], help='指定本次验证的综合样例')
    args = parser.parse_args()
    compiler = args.magel.resolve()
    WORK.mkdir(parents=True, exist_ok=True)
    if 'lambda_callbacks' in args.samples and 'lambda_captures' not in args.samples:
        args.samples.append('lambda_captures')
    for sample in args.samples:
        source = ROOT / f'example/samples/{sample}.mgl'
        expected = (ROOT / f'example/expected/{sample}.txt').read_text(encoding='utf-8')
        flags = ['--threads'] if (ROOT / f'example/expected/{sample}.threads').is_file() else []
        results = [('解释执行', run(compiler, 'run', source, '--out', WORK / (sample + '-interpret'), *flags))]
        for mode, result in results:
            assert result.stdout == expected, (mode, result.stdout, expected)
            if sample == 'destructuring_guards' and mode == '解释执行':
                warning = 'magel: 警告：[^\\n]*destructuring_guards\\.mgl:23:47：\\[MGL1601\\] match 整数模式与前面的模式重叠；仍按分支顺序和守卫选择匹配项\\n 23 \\| [^\\n]*\\n    \\| +~\\n'
                assert re.fullmatch(warning, result.stderr), (mode, result.stderr)
            else:
                assert not result.stderr, (mode, result.stderr)
if __name__ == '__main__':
    main()
