"""验证内部整数区间迭代器：范围检查、闭区间溢出、惰性遍历与闭包组合。"""
from pathlib import Path
import argparse
import os
import subprocess
ROOT = Path(__file__).resolve().parents[1]
EXE = '.exe' if os.name == 'nt' else ''

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--chali', type=Path, default=ROOT / ('chali' + EXE))
    compiler = parser.parse_args().chali.resolve()
    work = ROOT / 'build/foreach-interval-tests'
    work.mkdir(parents=True, exist_ok=True)

    def run(*args):
        return subprocess.run(list(map(str, args)), cwd=ROOT, capture_output=True, encoding='utf-8', errors='replace', timeout=240, env=dict(os.environ, CHALI_LANG='zh'))
    source = ROOT / 'example/samples/foreach_interval.chl'
    for backend in ('run',):
        output = work / (backend + EXE)
        result = run(compiler, backend, source, 'out', output)
        assert result.returncode == 0, result
        assert result.returncode == 0 and result.stdout == 'for intervals OK\n' and (not result.stderr), result
    rejected = {'int8_255': 'for(int8 x in std::lang::rangeEq<int8>(0,255)){}', 'int8_256': 'for(int8 x in std::lang::rangeEq<int8>(0,256)){}', 'uint8_256': 'for(uint8 x in std::lang::rangeEq<uint8>(0,256)){}', 'uint8_half_256': 'for(uint8 x in std::lang::range<uint8>(0,256)){}', 'int8_below': 'for(int8 x in std::lang::rangeEq<int8>(-129,0)){}', 'uint8_negative': 'for(uint8 x in std::lang::rangeEq<uint8>(-1,2)){}', 'signed64_overflow': 'for(int64 x in std::lang::rangeEq<int64>(0,18446744073709551615)){}', 'constant_expression': 'for(int8 x in std::lang::rangeEq<int8>(0,100+100)){}', 'named_constant': 'const int64 stop=255;for(int8 x in std::lang::rangeEq<int8>(0,stop)){}', 'reversed': 'for(int8 x in std::lang::rangeEq<int8>(10,0)){}', 'non_integer': 'for(float64 x in std::lang::rangeEq<float64>(0,1)){}', 'float_endpoint': 'for(int8 x in std::lang::rangeEq<int8>(0.0,1.0)){}', 'string_endpoint': 'for(x in std::lang::rangeEq("a","b")){}', 'reference_variable': 'for(int8 mut& x in std::lang::rangeEq<int8 mut&>(0,1)){}', 'borrow_escape': 'int8 fn() saved=int8 fn(){return (int8)0;};for(int8 x in std::lang::rangeEq<int8>(0,1)){saved=int8 fn(){x};}', 'left_open': 'for(x in (0,1)){}', 'left_open_right_closed': 'for(x in (0,1]){}'}
    for name, body in rejected.items():
        source = work / (name + '.chl')
        source.write_text('void main(){' + body + '}\n', encoding='utf-8')
        result = run(compiler, '--check', source)
        assert result.returncode != 0 and result.stderr, (name, result)
        assert 'terminate called' not in result.stderr, (name, result)
    for name, body in {'dynamic_overflow': 'int64 stop=256;for(uint8 x in std::lang::rangeEq<uint8>(0,stop)){}', 'dynamic_reverse': 'int64 start=4;int64 stop=2;for(int8 x in std::lang::rangeEq<int8>(start,stop)){}'}.items():
        source = work / (name + '.chl')
        source.write_text('void main(){' + body + '}\n', encoding='utf-8')
        for backend in ('run',):
            output = work / (name + '-' + backend + EXE)
            result = run(compiler, backend, source, 'out', output)
            assert result.returncode != 0 and 'for-in 区间' in result.stdout + result.stderr, (name, backend, result)
if __name__ == '__main__':
    main()
