"""对已确认的数值边界、拥有值传播和并发数据映像问题执行定向回归。"""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]


def run_native(cxx, work, sanitize):
    """直接编译实际库单元，验证精确数值、溢出拒绝与跨编译器模板兼容。"""
    executable = work / ('native-safety.exe' if os.name == 'nt' else 'native-safety')
    sources = [ROOT / 'tests/chali_native_safety.cpp']
    for pattern in ['library/bignum/bigint*.cpp', 'library/bignum/bigfloat*.cpp', 'library/simple_ast/ast*.cpp']:
        sources.extend(sorted(ROOT.glob(pattern)))
    sources += [ROOT / name for name in ['library/simple_ast/simple_ast.cpp', 'library/json/json.cpp', 'library/toml/toml.cpp', 'library/toml/toml_source.cpp']]
    flags = ['-std=c++17', '-O1' if sanitize else '-O2', '-DCHALI_WITH_REFLECTION=1', '-DSTACKVM_WITH_REFLECTION=1']
    if sanitize:
        flags += ['-g', '-fsanitize=address,undefined,float-cast-overflow', '-fno-omit-frame-pointer', '-no-pie']
    subprocess.run([cxx, *flags, *map(str, sources), '-o', str(executable)], cwd=ROOT, check=True, timeout=300)
    result = subprocess.run([str(executable)], cwd=work, capture_output=True, text=True, encoding='utf-8', timeout=60)
    assert result.returncode == 0 and result.stdout == 'native adversarial boundaries passed\n' and not result.stderr, result


def run_owned(compiler, work, env):
    """重复成功、失败与容器载荷传播，要求输出正确并且没有资源残留诊断。"""
    result = subprocess.run([str(compiler), 'run', str(ROOT / 'tests/fixtures/compiler_safety_owned_try.chl'), '--out', str(work / 'owned')], cwd=work, capture_output=True, text=True, encoding='utf-8', errors='replace', env=env, timeout=180)
    assert result.returncode == 0 and result.stdout == 'owned propagation cleanup OK\n' and not result.stderr, result


def run_snapshot(compiler, work, env):
    """在优化转储管道阻塞期间覆盖磁盘映像，验证 VM 使用当前编译的内存快照。"""
    source = work / 'snapshot.chl'
    count = 2000
    source.write_text('void main()\n{\n' + '    println("snapshot-safe");\n' * count + '    return;\n}\n', encoding='utf-8')
    prefix = work / 'snapshot'
    process = subprocess.Popen([str(compiler), 'run', str(source), '--out', str(prefix)], cwd=work, stdout=subprocess.PIPE, stderr=subprocess.PIPE, env=dict(env, CHALI_DUMP_OPT='1'))
    try:
        deadline = time.monotonic() + 120
        while not prefix.with_suffix('.layout').exists():
            if process.poll() is not None:
                out, error = process.communicate()
                raise AssertionError((process.returncode, out, error))
            if time.monotonic() >= deadline:
                raise TimeoutError('等待数据映像写出超时')
            time.sleep(0.005)
        # 大量优化转储填满管道；此时数据文件已写出，但 VM 尚未装载数据。
        prefix.with_suffix('.data.bin').write_bytes(b'\0' * 16)
        prefix.with_suffix('.layout').write_bytes(b'')
        output, error = process.communicate(timeout=120)
        # Windows 控制台走文本模式给出 CRLF；比对前归一化，避免把换行差异当成失败。
        normalized = output.replace(b'\r\n', b'\n')
        assert process.returncode == 0 and normalized == b'snapshot-safe\n' * count, (process.returncode, normalized[:200], error[-2000:])
        assert b'AddressSanitizer' not in error and b'runtime error:' not in error, error[-2000:]
    finally:
        if process.poll() is None:
            process.kill()
            process.communicate()


def main():
    """执行针对性回归；可显式开启 sanitizer，不扩展随机探索。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--chali', type=Path, default=ROOT / ('ChaLi.exe' if os.name == 'nt' else 'chali'))
    parser.add_argument('--cxx', default='g++')
    parser.add_argument('--sanitize-native', action='store_true')
    parser.add_argument('--native-only', action='store_true')
    args = parser.parse_args()
    env = dict(os.environ, CHALI_LANG='zh', CHALI_NO_STDIN_INPUT='1', ASAN_OPTIONS='detect_leaks=1:halt_on_error=1', UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    with tempfile.TemporaryDirectory(prefix='chali-known-safety-') as directory:
        work = Path(directory)
        run_native(args.cxx, work, args.sanitize_native)
        if not args.native_only:
            run_owned(args.chali.resolve(), work, env)
            run_snapshot(args.chali.resolve(), work, env)
    print('已确认问题回归通过：数值边界、平台模板' + ('' if args.native_only else '、拥有值传播、数据快照'))


if __name__ == '__main__':
    main()
