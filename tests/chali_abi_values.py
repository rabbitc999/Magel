"""Compile a real C ABI library and verify exported CHALI value/pointer conversions."""
from pathlib import Path
import json
import argparse
import os
import re
import subprocess
ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / 'build' / 'chali-abi-values'
EXE = '.exe' if os.name == 'nt' else ''

def run(args, succeeds=True, env=None):
    result = subprocess.run(list(map(str, args)), cwd=ROOT, text=True, encoding='utf-8', capture_output=True, env=env)
    assert (result.returncode == 0) == succeeds, result.stdout + result.stderr
    return result

def main():
    parser = argparse.ArgumentParser()
    parser.parse_args()
    WORK.mkdir(parents=True, exist_ok=True)
    library = WORK / ('native.dll' if os.name == 'nt' else 'native.so')
    native = WORK / 'native.c'
    fixture = ROOT / 'doc' / 'examples' / 'abi_buffers'
    native.write_text((fixture / 'native.c').read_text(encoding='utf-8'), encoding='utf-8')
    run(['gcc', '-shared', *(['-fPIC'] if os.name != 'nt' else []), native, '-o', library])
    mc = WORK / 'values.chl'
    mc.write_text((fixture / 'demo.chl').read_text(encoding='utf-8').replace('"./native.dll"', json.dumps(library.as_posix())), encoding='utf-8')
    prefix = WORK / 'values'
    run([ROOT / ('chali' + EXE), mc, '--emit', '--out', prefix])
    compiler = ROOT / ('chali' + EXE)
    import tempfile
    temp_root = Path(tempfile.gettempdir())
    before = set(temp_root.glob('chali-abi-*'))
    immediate = run([compiler, 'run', mc, '--out', WORK / 'immediate'])
    assert immediate.stdout == 'abi-values-ok\n' and immediate.stderr == '', immediate.stdout + immediate.stderr
    assert set(temp_root.glob('chali-abi-*')) == before, 'ABI bridge temporary directory leaked'
    missing_compiler = {**os.environ, 'CHALI_ABI_CXX': str(WORK / 'missing-compiler'), 'CHALI_ABI_NO_CACHE': '1'}
    failed = run([compiler, 'run', mc, '--out', WORK / 'missing'], succeeds=False, env=missing_compiler)
    assert 'CHALI_ABI_CXX' in failed.stderr, failed.stderr
    assert set(temp_root.glob('chali-abi-*')) == before, 'failed bridge temporary directory leaked'
    scalar = WORK / 'scalar.chl'
    scalar.write_text('abi ' + json.dumps(library.as_posix()) + ' { int32 identity(int32 value) = "identity"; }\nvoid main() { std::lang::println("{}", identity((int32)7)); return; }\n', encoding='utf-8')
    small = run([compiler, 'run', scalar, '--out', WORK / 'scalar'], env=missing_compiler)
    assert small.stdout == '7\n' and small.stderr == '', small.stdout + small.stderr
    for length, message in ((3, 'returned null'), (-1, 'buffer length'), (100000000, 'buffer length')):
        bad = WORK / 'bad-length.chl'
        bad.write_text('abi ' + json.dumps(library.as_posix()) + f' {{ int32[] numbers(int32 count) = "numbers" [count]; }}\nvoid main() {{ int32 values[] = numbers((int32){length}); return; }}\n', encoding='utf-8')
        failed = run([compiler, 'run', bad, '--out', WORK / 'bad-length'], succeeds=False)
        assert message in failed.stderr, failed.stderr
        assert set(temp_root.glob('chali-abi-*')) == before, 'error bridge temporary directory leaked'
    bad = WORK / 'null-reference.chl'
    bad.write_text('struct Packet { int32 value; }; abi ' + json.dumps(library.as_posix()) + ' { Packet mut& nothing() = "nothing"; }\nvoid main() { Packet mut& value = nothing(); return; }\n', encoding='utf-8')
    failed = run([compiler, 'run', bad, '--out', WORK / 'null-reference'], succeeds=False)
    assert 'ABI returned null for nonnull reference' in failed.stderr, failed.stderr
    for declaration in ('struct Bad { String text; }; abi LIBRARY { void f(Bad p); }', 'struct Bad unknown { int32 value; }; abi LIBRARY { void f(Bad p); }', 'abi LIBRARY { int32[] f(int32 count) = "f" [missing]; }', 'abi LIBRARY { void f(int32 mut& mut& p); }'):
        bad = WORK / 'bad.chl'
        bad.write_text(declaration.replace('LIBRARY', json.dumps(library.as_posix())) + '\nvoid main() { return; }\n', encoding='utf-8')
        result = run([compiler, 'run', bad, '--emit', '--out', WORK / 'bad'], succeeds=False)
        assert 'abi' in result.stderr.lower() or '语法错误' in result.stderr, result.stderr
    print('CHALI ABI 值、引用和非空返回检查通过')
if __name__ == '__main__':
    main()
