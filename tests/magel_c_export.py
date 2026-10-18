"""验证 C 导出只允许 AOT，并从纯 C 客户端验证共享库。"""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    """验证前端拒绝 VM 路径及 AOT 上下文接口。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--magel', type=Path, default=ROOT / 'magel.exe')
    parser.add_argument('--front-end-only', action='store_true')
    parser.add_argument('--gcc-toolchain', default='g++')
    parser.add_argument('--cc', default='gcc')
    args = parser.parse_args()
    compiler = args.magel.resolve()
    source = ROOT / 'doc/examples/abi_contracts/exports.mgl'
    env = dict(os.environ, MAGEL_LANG='zh', MAGEL_NO_STDIN_INPUT='1')

    def invoke(command):
        """执行工具链命令并捕获诊断。"""
        result = subprocess.run(list(map(str, command)), cwd=ROOT, env=env,
                                capture_output=True, timeout=600)
        for field in ('stdout', 'stderr'):
            value = getattr(result, field)
            try:
                value = value.decode('utf-8')
            except UnicodeDecodeError:
                value = value.decode('gb18030', errors='replace')
            setattr(result, field, value)
        return result

    result = invoke([compiler, 'check', source])
    assert result.returncode == 0, (result.stdout, result.stderr)
    with tempfile.TemporaryDirectory(prefix='magel-c-export-') as directory:
        output = Path(directory)
        for mode in ('run', 'svmb'):
            result = invoke([compiler, mode, source, '--out', output / mode])
            assert result.returncode != 0, (mode, result.stdout, result.stderr)
            assert 'C 函数导出仅支持 AOT' in result.stdout + result.stderr, result
            assert not (output / (mode + '.svmb')).exists()
            assert 'library main must not execute' not in result.stdout + result.stderr
            print(f'[通过] {mode} 拒绝 C 导出', flush=True)
        ordinary = output / 'ordinary.mgl'
        ordinary.write_text('int64 main(){println("VM OK");return 0;}', encoding='utf-8')
        result = invoke([compiler, 'run', ordinary])
        assert (result.returncode, result.stdout.strip()) == (0, 'VM OK'), result
        library_only = output / 'library.mgl'
        library_only.write_text('export int32 identity(int32 x){return x;}', encoding='utf-8')
        result = invoke([compiler, 'check', library_only])
        assert result.returncode == 0, result
        result = invoke([compiler, 'run', library_only])
        assert result.returncode != 0 and 'C 函数导出仅支持 AOT' in result.stdout + result.stderr, result
        if args.front_end_only:
            return
        library = output / ('counter.dll' if os.name == 'nt' else 'counter.so')
        result = invoke([compiler, 'aot', source, '--shared', '--out', library,
                         '--gcc-toolchain', args.gcc_toolchain])
        assert result.returncode == 0, (result.stdout, result.stderr)
        client = output / ('client.exe' if os.name == 'nt' else 'client')
        result = invoke([args.cc, ROOT / 'doc/examples/abi_contracts/client.c',
                         '-I', output, library, '-o', client])
        assert result.returncode == 0, (result.stdout, result.stderr)
        result = invoke([client])
        assert (result.returncode, result.stdout.strip()) == (0, 'C exports OK'), result
        print('[通过] 纯 C 调用 AOT 共享库与独立上下文', flush=True)


if __name__ == '__main__':
    main()
