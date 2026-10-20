"""验证自定义分配器访问护栏，以及真实 MAGEL 非法访问的 panic、源码行与调用栈。"""
from pathlib import Path
import argparse
import os
import subprocess
ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / 'build/magel-memory-guard'

def execute(*args):
    """执行工具，保留诊断，并设置超时以发现 panic 未终止程序的问题。"""
    return subprocess.run([str(arg) for arg in args], cwd=ROOT, capture_output=True, encoding='utf-8', errors='replace', timeout=240, env=dict(os.environ, MAGEL_LANG='zh'))

def main():
    """先检查自定义堆契约，再检查解释与 VM 的实际错误位置。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--magel', type=Path, default=ROOT / 'magel.exe')
    parser.add_argument('--cxx', default='g++')
    args = parser.parse_args()
    compiler = args.magel.resolve()
    WORK.mkdir(parents=True, exist_ok=True)
    suffix = '.exe' if os.name == 'nt' else ''
    unit = WORK / ('unit' + suffix)
    result = execute(args.cxx, '-std=c++17', '-pthread', '-DMAGEL_CUSTOM_ALLOCATOR=1', '-DMAGEL_WITH_REFLECTION=1', '-DSTACKVM_WITH_REFLECTION=1', '-Ilibrary', '-Isource', '-Ilibrary/stackvm', 'tests/magel_memory_guard_test.cpp', 'build/magel-stackvm-core/libstackvm.a', '-lm', '-o', unit)
    assert result.returncode == 0, result
    result = execute(unit)
    assert result.returncode == 0 and result.stdout == 'custom heap memory guard OK\n', result
    unchecked_unit = WORK / ('unit-unchecked' + suffix)
    result = execute(args.cxx, '-std=c++17', '-pthread', '-DMAGEL_CUSTOM_ALLOCATOR=1', '-DSTACKVM_WITH_BIGINT=0', '-DSTACKVM_WITH_BIGFLOAT=0', '-DSTACKVM_WITH_ALLOCATION_GUARDS=0', '-DMAGEL_WITH_REFLECTION=1', '-DSTACKVM_WITH_REFLECTION=1', '-Ilibrary', '-Isource', '-Ilibrary/stackvm', 'tests/magel_memory_guard_test.cpp', 'library/stackvm/stackvm.cpp', '-lm', '-o', unchecked_unit)
    assert result.returncode == 0, result
    result = execute(unchecked_unit)
    assert result.returncode == 0 and result.stdout == 'custom heap unchecked OK\n', result
    cases = {'read': ('int64', 'int64 leaf(int64 & p) { return p; }', 'return outer(stale);'), 'write': ('int64', 'int64 leaf(int64 mut& p) { p = 9; return 0; }', 'return outer(stale);'), 'float': ('float64', 'int64 leaf(float64 & p) { float64 value = p; return 0; }', 'return outer(stale);'), 'char': ('char', 'int64 leaf(char & p) { char value = p; return 0; }', 'return outer(stale);'), 'char_write': ('char', "int64 leaf(char mut& p) { p = 'A'; return 0; }", 'return outer(stale);')}
    for name, (type_name, leaf, body) in cases.items():
        permission = 'mut&' if name in ('write', 'char_write') else '&'
        source = WORK / (name + '.mgl')
        source.write_text(leaf + '\nint64 outer(' + type_name + ' ' + permission + ' p) { return leaf(p); }\nvoid dispose(' + type_name + ' mut& p) { delete p; }\nint64 main() {\n' + type_name + ' mut& owner = heap ' + type_name + ';\n' + type_name + ' ' + permission + ' stale = owner;\ndispose(owner);\n' + body + '\n}\n', encoding='utf-8')
        result = execute(compiler, '--lang', 'zh', 'run', source, '--out', WORK / (name + '-interpret'))
        results = [('解释执行', result)]
        for backend, result in results:
            assert result.returncode != 0 and 'panic：' in result.stderr, (name, backend, result)
            assert '调用栈' in result.stderr and 'leaf' in result.stderr and ('outer' in result.stderr), (name, backend, result)
            assert source.name + ':1' in result.stderr, (name, backend, result)
            assert 'Segmentation' not in result.stderr and 'terminate called' not in result.stderr, result
    result = execute(compiler, '--lang', 'zh', 'run', source, '--compress-symbols', '--out', WORK / 'hidden-interpret')
    assert result.returncode != 0 and 'panic：' in result.stderr and ('调用栈' not in result.stderr), result
    safe = ROOT / 'example/samples/reference_sugar.mgl'
    expected = 'reference sugar OK\n'
    print('内存访问护栏：自定义堆、VM panic、源码位置和调用栈通过')

if __name__ == '__main__':
    main()
