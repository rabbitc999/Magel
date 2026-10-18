"""验证 ABI 返回值所有权、同步回调、桥复用和有限头文件导入。"""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    """构建原生测试库并通过实际编译器验证 ABI 合约。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--magel', type=Path, default=ROOT / 'magel.exe')
    parser.add_argument('--cc', default='gcc')
    args = parser.parse_args()
    compiler = args.magel.resolve()
    with tempfile.TemporaryDirectory(prefix='magel abi contracts ') as directory:
        work = Path(directory)
        env = dict(os.environ, MAGEL_ABI_CACHE_DIR=str(work / 'cache'))

        def invoke(command):
            """执行外部命令，失败时保留完整诊断。"""
            result = subprocess.run(list(map(str, command)), cwd=ROOT, env=env,
                                    capture_output=True, timeout=600)
            assert result.returncode == 0, (command, result.stdout, result.stderr)
            return result.stdout

        library = work / ('native.dll' if os.name == 'nt' else 'native.so')
        invoke([args.cc, '-shared', '-fPIC', ROOT / 'doc/examples/abi_contracts/native.c', '-o', library])
        source = work / 'demo.mgl'
        source.write_text((ROOT / 'doc/examples/abi_contracts/demo.mgl').read_text(encoding='utf-8')
                          .replace('./native.dll', library.as_posix()), encoding='utf-8')
        assert invoke([compiler, 'run', source]).strip() == b'ABI contracts OK'
        bridge = work / ('bridge.dll' if os.name == 'nt' else 'bridge.so')
        assert invoke([compiler, 'run', source, '--abi-prepare', '--abi-write-bridge', bridge]) == b''
        env['MAGEL_ABI_CXX'] = 'missing-magel-cxx-for-cache-test'
        assert invoke([compiler, 'run', source]).strip() == b'ABI contracts OK'
        assert invoke([compiler, 'run', source, '--abi-bridge', bridge, '--abi-no-cache']).strip() == b'ABI contracts OK'
        env.pop('MAGEL_ABI_CXX')
        imported = work / 'imported.mgl'
        invoke([compiler, 'gen', 'abi', ROOT / 'doc/examples/abi_contracts/import.h',
                '--library', library.as_posix(), '--out', imported])
        with imported.open('a', encoding='utf-8') as stream:
            stream.write('\nint64 main(){assert(c_identity((int32)7)==C_FIRST);'
                         'auto cb=int32 fn(int32 x){return x;};'
                         'assert(c_apply(cb,(int32)3)==7);return 0;}\n')
        invoke([compiler, 'run', imported])
        failure = work / 'callback-failure.mgl'
        failure.write_text('abi "' + library.as_posix() + '" { '
                           'String owned(int32 fn(int32) cb) = "callback_owned" ["free_owned"]; } '
                           'int64 main(){auto cb=int32 fn(int32 x){panic("callback failure");return x;};'
                           'String text=owned(cb);return 0;}', encoding='utf-8')
        marker = work / 'released.txt'
        env['MAGEL_ABI_FREE_MARKER'] = str(marker)
        result = subprocess.run([str(compiler), 'run', str(failure)], cwd=ROOT, env=env,
                                capture_output=True, timeout=600)
        assert result.returncode != 0 and b'ABI callback invocation failed' in result.stderr + result.stdout, result
        assert marker.read_text().strip() == 'free'
        print('ABI ownership, callbacks, cache, precompiled bridge and import: OK')


if __name__ == '__main__':
    main()
