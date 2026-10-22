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
    parser.add_argument('--chali', type=Path, default=ROOT / ('ChaLi.exe' if os.name == 'nt' else 'chali'))
    args = parser.parse_args()
    compiler = args.chali.resolve()
    with tempfile.TemporaryDirectory(prefix='opcode-std-', dir=ROOT / 'build') as directory:
        work = Path(directory)

        def run(source, success=True, threads=False):
            result = subprocess.run([str(compiler), 'run', str(source), '--out', str(work / 'output'),
                                     *(['--threads'] if threads else [])], cwd=ROOT,
                env=dict(os.environ, CHALI_LANG='zh'), capture_output=True, text=True, encoding='utf-8', timeout=180)
            assert (result.returncode == 0) == success, (result.stdout, result.stderr)
            return result

        result = run(ROOT / 'example/samples/opcode_std_storage.chl')
        assert result.stdout == 'opcode standard storage OK\n' and not result.stderr, result
        async_result = run(ROOT / 'example/samples/std_net_async.chl', threads=True)
        assert async_result.stdout == 'nonblocking network OK\n' and not async_result.stderr, async_result
        for body, marker in [
            ('import std::net; void main(){TcpSocket a; TcpSocket b=a;}', 'NonCopyable'),
            ('import std::net; void main(){TcpListener a; TcpListener b=a;}', 'NonCopyable'),
            ('import std::image; void main(){Image a; int64 handle=a.opcode;}', '内部句柄'),
            ('import std::image; void reject(Image const& image){image.setPixel(0,0,1,2,3,4);} void main(){}', '只读'),
            ('import std::linalg; void reject(ScalarArray<float64> const& values){values.clear();} void main(){}', '只读'),
            ('import std::net; void reject(TcpSocket const& socket){socket.receiveAsync((int32)1);} void main(){}', '只读'),
            ('int64 __std_user(){return 1;} void main(){}', '__std_'),
        ]:
            source = work / 'rejected.chl'
            source.write_text(body.replace('; ', ';\n', 1) if body.startswith('import ') else body, encoding='utf-8')
            rejected = run(source, False)
            assert marker in rejected.stderr, rejected.stderr
    data = json.loads((ROOT / 'tools/chali_stdlib_api.json').read_text(encoding='utf-8'))
    types = {r['name']: r['kind'] for r in data['symbols']}
    assert types['Image'] == 'opcode' and types['TcpSocket'] == 'opcode' and types['TcpListener'] == 'opcode'
    print('standard opcode storage: VM ownership, scope cleanup, status isolation and API index passed')


if __name__ == '__main__':
    main()
