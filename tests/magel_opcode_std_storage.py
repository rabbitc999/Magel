"""验证标准库 opcode 的深复制、套接字独占、自动释放与内部入口边界。"""
from pathlib import Path
import argparse
import json
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    """执行真实 VM 程序并确认独占资源和私有句柄的编译期限制。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--magel', type=Path, default=ROOT / ('magel.exe' if os.name == 'nt' else 'magel'))
    args = parser.parse_args()
    compiler = args.magel.resolve()
    with tempfile.TemporaryDirectory(prefix='opcode-std-', dir=ROOT / 'build') as directory:
        work = Path(directory)

        def run(source, success=True, threads=False):
            result = subprocess.run([str(compiler), 'run', str(source), '--out', str(work / 'output'),
                                     *(['--threads'] if threads else [])], cwd=ROOT,
                env=dict(os.environ, MAGEL_LANG='zh'), capture_output=True, text=True, encoding='utf-8', timeout=180)
            assert (result.returncode == 0) == success, (result.stdout, result.stderr)
            return result

        result = run(ROOT / 'example/samples/opcode_std_storage.mgl')
        assert result.stdout == 'opcode standard storage OK\n' and not result.stderr, result
        async_result = run(ROOT / 'example/samples/std_net_async.mgl', threads=True)
        assert async_result.stdout == 'nonblocking network OK\n' and not async_result.stderr, async_result
        for body, marker in [
            ('import std::net; int64 main(){TcpSocket a; TcpSocket b=a; return 0;}', 'NonCopyable'),
            ('import std::net; int64 main(){TcpListener a; TcpListener b=a; return 0;}', 'NonCopyable'),
            ('import std::image; int64 main(){Image a; int64 handle=a.opcode; return 0;}', '内部句柄'),
            ('import std::image; void reject(Image const& image){image.setPixel(0,0,1,2,3,4);} int64 main(){return 0;}', '只读'),
            ('import std::linalg; void reject(ScalarArray<float64> const& values){values.clear();} int64 main(){return 0;}', '只读'),
            ('import std::net; void reject(TcpSocket const& socket){socket.receiveAsync((int32)1);} int64 main(){return 0;}', '只读'),
            ('int64 __std_user(){return 1;} int64 main(){return 0;}', '__std_'),
        ]:
            source = work / 'rejected.mgl'
            source.write_text(body.replace('; ', ';\n', 1) if body.startswith('import ') else body, encoding='utf-8')
            rejected = run(source, False)
            assert marker in rejected.stderr, rejected.stderr
    data = json.loads((ROOT / 'tools/magel_stdlib_api.json').read_text(encoding='utf-8'))
    types = {r['name']: r['kind'] for r in data['symbols']}
    assert types['Image'] == 'opcode' and types['TcpSocket'] == 'opcode' and types['TcpListener'] == 'opcode'
    print('standard opcode storage: VM ownership, scope cleanup, status isolation and API index passed')


if __name__ == '__main__':
    main()
