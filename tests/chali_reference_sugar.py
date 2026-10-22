"""验证默认只读引用语法糖的类型身份、权限及生命周期。"""
from pathlib import Path
import argparse
import subprocess
import os
ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / 'build/chali-reference-sugar'

def execute(*args):
    """执行编译器或产物，记录诊断并限制运行时间。"""
    return subprocess.run([str(arg) for arg in args], cwd=ROOT, capture_output=True, encoding='utf-8', errors='replace', timeout=180)

def main():
    """检查合法组合与只读、非空、逃逸拒绝路径。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--chali', type=Path, default=ROOT / 'ChaLi.exe')
    compiler = parser.parse_args().chali.resolve()
    WORK.mkdir(parents=True, exist_ok=True)
    cases = {'write': ('void main(){int64 x=1;int64 & a=&x;a=2;}', '只读'), 'field': ('struct S{int64 x;};void main(){S s;S & a=&s;a.x=2;}', '只读'), 'container': ('void main(){List<int64> xs;List<int64> & a=&xs;a.push(2);}', '只读'), 'mutable_argument': ('void write(int64 mut& a){a=2;}void main(){int64 x=1;int64 & a=&x;write(a);}', 'const&'), 'escape': ('int64 & bad(){int64 x=1;return &x;}void main(){}', 'CHL1204'), 'unbound': ('void main(){int64 & a;}', '初始化')}
    for name, (text, expected) in cases.items():
        source = WORK / (name + '.chl')
        source.write_text(text, encoding='utf-8')
        result = execute(compiler, '--check', source)
        assert result.returncode != 0 and expected in result.stderr, (name, result)
    source = ROOT / 'example/samples/reference_sugar.chl'
    result = execute(compiler, 'run', source, '--out', WORK / 'interpret')
    assert result.returncode == 0 and result.stdout == 'reference sugar OK\n' and (not result.stderr), result
    output = WORK / ('native.exe' if os.name == 'nt' else 'native')
if __name__ == '__main__':
    main()
