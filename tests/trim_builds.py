"""验证默认编译器保留完整能力，以及显式裁剪 VM 的禁用行为和外部静态内存池。"""
from pathlib import Path
import argparse
import os
import subprocess
ROOT = Path(__file__).resolve().parents[1]
EXE = '.exe' if os.name == 'nt' else ''

def run(args, succeeds=True, stdin=None):
    """执行真实工具或产物并验证退出状态，保留失败诊断。"""
    result = subprocess.run(list(map(str, args)), cwd=ROOT, capture_output=True, text=True, encoding='utf-8', errors='replace', input=stdin, timeout=300, env=dict(os.environ, CHALI_LANG='zh'))
    if (result.returncode == 0) != succeeds:
        raise AssertionError(f'{args}\n{result.stdout}\n{result.stderr}')
    return result

def main():
    """显式编译裁剪产物，验证功能禁用、基础程序及无默认内存池配置。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--chali', type=Path, default=ROOT / ('chali' + EXE))
    compiler = parser.parse_args().chali.resolve()
    work = ROOT / 'build/trim-tests'
    work.mkdir(parents=True, exist_ok=True)
    flags = ['-DNO_CHALI_STD', '-DNO_CHALI_BIG_INT', '-DNO_CHALI_BIG_FLOAT', '-DNO_CHALI_DEFAULT_POOL', '-DCHALI_WITH_REFLECTION=1', '-DSTACKVM_WITH_REFLECTION=1']
    for sample in ('std_math_clock_random', 'bigint', 'bigfloat'):
        source = ROOT / 'example/samples' / (sample + '.chl')
        input_path = ROOT / 'example/expected' / (sample + '.in')
        stdin = input_path.read_text(encoding='utf-8') if input_path.exists() else None
        full = run([compiler, 'run', source, '--out', work / (sample + '-vm')], stdin=stdin)
        expected = (ROOT / 'example/expected' / (sample + '.txt')).read_text(encoding='utf-8')
        assert full.stdout == expected and (not full.stderr), (sample, full)
        print(f'[通过] {sample}：默认编译器能力保留', flush=True)
    binary = work / ('vm-trim' + EXE)
    run(['g++', '-std=c++17', '-O3', '-pthread', '-ffunction-sections', '-fdata-sections',
         '-Wl,--gc-sections', '-DCHALI_CUSTOM_ALLOCATOR=1', '-DSTACKVM_WITH_DEFAULT_HEAP=0',
         '-DSTACKVM_WITH_THREADS=0', *flags, '-Isource', '-Ilibrary', '-Isource/stackvm',
         ROOT / 'tests/vm_trim.cpp', *[ROOT / 'source/stackvm' / name for name in (ROOT / 'source/stackvm/core_sources.txt').read_text().splitlines()], '-o', binary])
    actual = run([binary])
    assert actual.stdout == 'trimmed VM and explicit static runtime pool passed\n' and not actual.stderr, actual
    symbols = run(['nm', '-C', binary]).stdout
    assert 'bigint::BigInt::' not in symbols and 'bigfloat::BigFloat::' not in symbols
    print('VM 裁剪、禁用入口、显式静态运行时池与大数符号检查通过', flush=True)
if __name__ == '__main__':
    main()
