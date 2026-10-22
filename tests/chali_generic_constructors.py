"""验证命名构造入口从实参推导泛型类型，以及二元组和重载歧义。"""
from pathlib import Path
import argparse
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    """使用真实 VM 验证标准 Pair、用户构造入口及推导失败诊断。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--chali', type=Path, default=ROOT / ('ChaLi.exe' if os.name == 'nt' else 'chali'))
    compiler = parser.parse_args().chali.resolve()
    cases = [
        ('void main() { let a = Pair::create("key", (int64)7); let b = Pair::create(("value", (int64)9)); let t = a.toTuple(); assert(t.0 == "key" && t.1 == 7 && b.second == 9); }', True),
        ('void main() { let p = std::lang::Pair::create((int64)1, "one"); assert(p.first == 1 && p.second == "one"); }', True),
        ('struct Cell<T> { T value; static Cell<T> create(T value) { self.value = value; } }; void main() { let c = Cell::create((int64)42); assert(c.value == 42); }', True),
        ('struct Choice<T,U> { static Choice<T,U> create(T a,U b) {} static Choice<T,U> create(U a,T b) {} }; void main() { let c = Choice::create((int64)1,"a"); }', False),
        ('void main() { let p = Pair::create(); }', False),
    ]
    with tempfile.TemporaryDirectory(prefix='chali-inferred-create-') as directory:
        work = Path(directory)
        for index, (code, success) in enumerate(cases):
            source = work / f'case{index}.chl'
            source.write_text(code, encoding='utf-8')
            result = subprocess.run([str(compiler), 'run', str(source), '--out', str(work / 'program')],
                                    env=dict(os.environ, CHALI_LANG='zh', CHALI_NO_STDIN_INPUT='1'),
                                    capture_output=True, encoding='utf-8', errors='replace', timeout=120)
            assert (result.returncode == 0) == success, (index, result.stdout, result.stderr)
            if success:
                assert not result.stdout and not result.stderr, (index, result.stdout, result.stderr)
            elif index == 3:
                assert '歧义' in result.stderr, result.stderr
    print('Generic create: Pair values, tuple, qualified name, user constructor and ambiguity passed')


if __name__ == '__main__':
    main()
