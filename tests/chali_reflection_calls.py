"""CHALI 调用桥接真实VM回归，覆盖全部调用形式与精确类型拒绝。"""
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
    parser.add_argument('--fixtures', nargs='+', choices=('calls', 'operators', 'defaults'), default=['calls', 'operators', 'defaults'])
    args = parser.parse_args()
    checks = 0

    def run(command, ok=True):
        nonlocal checks
        result = subprocess.run(list(map(str, command)), cwd=ROOT, capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=240)
        assert (result.returncode == 0) == ok, (command, result.stdout, result.stderr)
        checks += 1
        return result
    with tempfile.TemporaryDirectory(prefix='reflection-calls-', dir=ROOT / 'build') as directory:
        work = Path(directory)
        sample = ROOT / 'example/reflection/calls.chl'
        rejected = run([args.chali, 'run', sample, '--no-reflection', '--check'], False)
        assert '未开启' in rejected.stderr, rejected.stderr
        for body in ('std::reflect::callStatic<Unknown>("missing",args)', 'std::reflect::callStatic<Box<T>>("missing",args)'):
            source = work / 'invalid.chl'
            source.write_text('import std::reflect;\nstruct Box<T>{T value;};\nvoid main(){List<std::reflect::ReflectValue> args;' + body + ';return;}\n', encoding='utf-8')
            run([args.chali, 'run', source, '--reflection', '--check'], False)
        for body in ('std::reflect::__reflect_actualType<Reader>(reader)', 'std::reflect::__reflect_unary<Reader,int64>(reader,"read")'):
            source = work / 'internal.chl'
            source.write_text('import std::reflect;\ninterface Reader {int64 read();};\nint64 probe(Reader const& reader){return ' + body + ';}\nvoid main(){return;}\n', encoding='utf-8')
            rejected = run([args.chali, 'run', source, '--reflection', '--check'], False)
            assert '仅供编译器' in rejected.stderr, rejected.stderr
        source = work / 'ordinary.chl'
        source.write_text('void main(){int64 string=3;assert(string==3);}\n', encoding='utf-8')
        run([args.chali, 'run', source, '--hard-realtime', '--check'])
        hard = run([args.chali, 'run', sample, '--reflection', '--hard-realtime', '--check'], False)
        assert '硬实时' in hard.stderr, hard.stderr
        for fixture in args.fixtures:
            sample = ROOT / ('example/reflection/' + fixture + '.chl')
            for backend in ('',):
                output = work / (fixture + (backend or 'direct') + EXE)
                command = [args.chali, *['run'], sample, '--reflection', '--out', output]
                result = run(command)
                if backend:
                    result = run([output])
                assert result.stdout == 'reflection ' + fixture + ' OK\n' and (not result.stderr), (backend, result.stdout, result.stderr)
        print(f'CHALI 调用桥接：{checks} 项检查通过')
if __name__ == '__main__':
    main()
