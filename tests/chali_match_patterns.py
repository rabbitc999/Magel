"""验证 match 的或模式及两种左闭区间，覆盖 VM、VM 与禁止写法。"""
from pathlib import Path
import argparse
import os
import subprocess
ROOT = Path(__file__).resolve().parents[1]
EXE = '.exe' if os.name == 'nt' else ''

def main():
    """检查数值与字符串边界，保证左开模式被拒绝而普通元组仍可使用。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--chali', type=Path, default=ROOT / ('chali' + EXE))
    compiler = parser.parse_args().chali.resolve()
    work = ROOT / 'build/chali-match-patterns'
    work.mkdir(parents=True, exist_ok=True)

    def run(*args):
        return subprocess.run(list(map(str, args)), cwd=ROOT, capture_output=True, encoding='utf-8', errors='replace', timeout=240, env=dict(os.environ, CHALI_LANG='zh'))
    for name in ('match_or_string', 'match_range'):
        source = ROOT / 'example/samples' / (name + '.chl')
        expected = (ROOT / 'example/expected' / (name + '.txt')).read_text(encoding='utf-8')
        for backend in ('run',):
            output = work / (name + '-' + backend + EXE)
            result = run(compiler, backend, source, '--out', output)
            assert result.returncode == 0, result
            assert result.returncode == 0 and result.stdout == expected and (not result.stderr), result
    rejected = 0
    for name, subject, lo, hi in (('integer', '1', '0', '2'), ('float', '1.0', '0.0', '2.0'), ('string', '"m"', '"a"', '"z"')):
        for close in (')', ']'):
            source = work / (name + ('-open' if close == ')' else '-right-closed') + '.chl')
            source.write_text('void main(){match(' + subject + '){(' + lo + ',' + hi + close + '=>{let __boom = 1;} _=>{return;}}}\n', encoding='utf-8')
            result = run(compiler, '--check', source)
            assert result.returncode != 0 and result.stderr, result
            if close == ')':
                assert 'match 区间' in result.stderr or '语法错误' in result.stderr, result
            rejected += 1
    source = work / 'boundaries.chl'
    source.write_text('void main(){\n        let pair=(1,2);assert(pair.0==1 && pair.1==2);\n        match(0){std::lang::rangeEq(0,0)=>{} _=>{assert(false);}}\n        match(0){std::lang::range(0,0)=>{assert(false);} _=>{}}\n        match(1.0){_ if 0.0 <= 1.0 && 1.0 < 1.0 =>{assert(false);} _=>{}}\n        match(1.0){1.0=>{} _=>{assert(false);}}\n        match(""){""=>{} _=>{assert(false);}}\n        match(""){_ if false=>{assert(false);} _=>{}}\n        return;\n    }\n', encoding='utf-8')
    result = run(compiler, 'run', source, 'out', work / 'boundaries')
    assert result.returncode == 0 and (not result.stderr), result
if __name__ == '__main__':
    main()
