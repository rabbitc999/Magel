"""CHALI 类型化查询真实VM回归，包含缺失、遮蔽、所有权与功能开关。"""
from pathlib import Path
import argparse
import os
import subprocess
import tempfile
ROOT = Path(__file__).resolve().parents[1]
EXE = '.exe' if os.name == 'nt' else ''

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--chali', type=Path, default=ROOT / ('chali' + EXE))
    args = parser.parse_args()
    checks = 0

    def run(command, ok=True):
        nonlocal checks
        result = subprocess.run(list(map(str, command)), cwd=ROOT, capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=240)
        assert (result.returncode == 0) == ok, (command, result.stdout, result.stderr)
        checks += 1
        return result
    with tempfile.TemporaryDirectory(prefix='reflection-queries-', dir=ROOT / 'build') as directory:
        work = Path(directory)
        sample = ROOT / 'example/reflection/queries.chl'
        rejected = run([args.chali, 'run', sample, '--no-reflection', '--check'], False)
        assert '未开启' in rejected.stderr, rejected.stderr
        for body in ('std::reflect::getTypeId<Unknown>()', 'std::reflect::getTypeId<Box<T>>()'):
            source = work / 'invalid.chl'
            source.write_text('import std::reflect;\nstruct Box<T>{T value;};\nvoid main(){' + body + ';return;}\n', encoding='utf-8')
            run([args.chali, 'run', source, '--reflection', '--check'], False)
        for backend in ('',):
            output = work / ((backend or 'direct') + EXE)
            command = [args.chali, *['run'], sample, '--reflection', '--out', output]
            result = run(command)
            if backend:
                result = run([output])
            assert result.stdout == 'reflection queries OK\n' and (not result.stderr), (backend, result.stdout, result.stderr)
        print(f'CHALI 类型化查询：{checks} 项检查通过')
if __name__ == '__main__':
    main()
