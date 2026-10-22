"""验证类型化元数据的真实 VM 导出；非空表来自 C++ 编译器模拟夹具。"""
from __future__ import annotations
import argparse
import os
from pathlib import Path
import subprocess
import tempfile
ROOT = Path(__file__).resolve().parents[1]
EXE = '.exe' if os.name == 'nt' else ''

def main() -> None:
    """运行两种真实配置及自包含导出查询，不把宿主夹具当作 CHALI 源码生成。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cxx', default='g++')
    parser.add_argument('--vm-on', type=Path, default=ROOT / 'build/reflection-vm-on' / ('chali-reflection-tests' + EXE))
    parser.add_argument('--vm-off', type=Path, default=ROOT / 'build/reflection-vm-off' / ('chali-reflection-tests' + EXE))
    args = parser.parse_args()
    checks = 0

    def run(command: list, *, success: bool=True):
        nonlocal checks
        result = subprocess.run(list(map(str, command)), cwd=ROOT, capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=240)
        assert (result.returncode == 0) == success, (command, result.stdout, result.stderr)
        checks += 1
        return result
    with tempfile.TemporaryDirectory(prefix='reflection-metadata-', dir=ROOT / 'build') as temporary:
        work = Path(temporary)
        for vm in (args.vm_on, args.vm_off):
            result = run([vm.resolve(), work])
            print(result.stdout.strip())
        plain = work / 'plain.chl'
        plain.write_text('int64 main(){return 0;}\n', encoding='utf-8')
        chali = ROOT / ('chali' + EXE)
        prefix = work / 'plain'
        run([chali, 'run', plain, '--emit', '--svmb', '--no-reflection', '--no-compress-symbols', '--out', prefix])
        assert b'MCRF' not in prefix.with_suffix('.svmb').read_bytes()
        run([chali, 'run', plain, '--reflection', '--emit', '--svmb', '--no-compress-symbols', '--out', prefix])
        assert prefix.with_suffix('.svmb').read_bytes().endswith(b'MCRF' + 1 .to_bytes(4, 'little') + bytes(28))
        print(f'反射元数据传输及导出：{checks} 项集成检查通过')
if __name__ == '__main__':
    main()
