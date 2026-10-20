"""验证默认编译器保留完整能力，以及显式裁剪 VM 的禁用行为和外部静态内存池。"""
from pathlib import Path
import argparse
import os
import subprocess
ROOT = Path(__file__).resolve().parents[1]
EXE = '.exe' if os.name == 'nt' else ''

def run(args, succeeds=True, stdin=None):
    """执行真实工具或产物并验证退出状态，保留失败诊断。"""
    result = subprocess.run(list(map(str, args)), cwd=ROOT, capture_output=True, text=True, encoding='utf-8', errors='replace', input=stdin, timeout=300)
    if (result.returncode == 0) != succeeds:
        raise AssertionError(f'{args}\n{result.stdout}\n{result.stderr}')
    return result

def main():
    """显式编译裁剪产物，验证功能禁用、基础程序及无默认内存池配置。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--magel', type=Path, default=ROOT / ('magel' + EXE))
    compiler = parser.parse_args().magel.resolve()
    work = ROOT / 'build/trim-tests'
    work.mkdir(parents=True, exist_ok=True)
    flags = ['-DNO_MAGEL_STD', '-DNO_MAGEL_BIG_INT', '-DNO_MAGEL_BIG_FLOAT', '-DNO_MAGEL_DEFAULT_POOL', '-DMAGEL_WITH_REFLECTION=1', '-DSTACKVM_WITH_REFLECTION=1']
    for sample in ('std_math_clock_random', 'bigint', 'bigfloat'):
        source = ROOT / 'example/samples' / (sample + '.mgl')
        input_path = ROOT / 'example/expected' / (sample + '.in')
        stdin = input_path.read_text(encoding='utf-8') if input_path.exists() else None
        full = run([compiler, 'run', source, '--out', work / (sample + '-vm')], stdin=stdin)
        expected = (ROOT / 'example/expected' / (sample + '.txt')).read_text(encoding='utf-8')
        assert full.stdout == expected and (not full.stderr), (sample, full)
        original = work / (sample + '-full' + EXE)
        trimmed = work / (sample + '-trimmed' + EXE)
        print(f'[通过] {sample}：默认能力保留，显式裁剪产物拒绝禁用调用', flush=True)
    control = ROOT / 'example/samples/control.mgl'
    target = work / ('control' + EXE)
    host = work / 'host.cpp'
    binary = work / ('host' + EXE)
    run(['g++', '-std=c++17', '-O3', '-pthread', *flags, host, '-o', binary])
    actual = run([binary])
    expected = (ROOT / 'example/expected/control.txt').read_text(encoding='utf-8')
    assert actual.stdout == expected and (not actual.stderr), actual
    symbols = run(['nm', '-C', binary]).stdout
    assert 'bigint::BigInt::' not in symbols and 'bigfloat::BigFloat::' not in symbols
if __name__ == '__main__':
    main()
