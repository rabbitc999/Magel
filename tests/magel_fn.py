"""验证 fn 匿名函数、旧 lambda 拒绝诊断及函数类型语法。"""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    """在解释与 AOT 后端验证匿名函数推导和捕获行为。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--magel', type=Path, default=ROOT / ('magel.exe' if os.name == 'nt' else 'magel'))
    parser.add_argument('--front-end-only', action='store_true')
    options = parser.parse_args()
    compiler = options.magel.resolve()
    env = dict(os.environ, MAGEL_LANG='zh', MAGEL_NO_STDIN_INPUT='1')
    with tempfile.TemporaryDirectory(prefix='magel-fn-') as directory:
        def invoke(command):
            """执行编译或测试程序，捕获可比较的输出。"""
            return subprocess.run(list(map(str, command)), cwd=ROOT, env=env,
                                  capture_output=True, text=True, encoding='utf-8',
                                  errors='replace', timeout=240)

        for name in ('fn_auto', 'fn_capture_modes', 'lambda_auto', 'lambda_capture_modes'):
            expected = (ROOT / 'example/expected' / (name + '.txt')).read_text(encoding='utf-8')
            for level in (0, 2):
                result = invoke([compiler, 'run', ROOT / 'example/samples' / (name + '.mgl'), f'--opt-level={level}'])
                assert (result.returncode, result.stdout, result.stderr) == (0, expected, ''), (name, result)
                print(f'[通过] {name} run O{level}', flush=True)
            if not options.front_end_only and name.startswith('fn_'):
                output = Path(directory) / (name + ('.exe' if os.name == 'nt' else ''))
                result = invoke([compiler, 'aot', ROOT / 'example/samples' / (name + '.mgl'), '--out', output])
                assert result.returncode == 0, (name, result.stdout, result.stderr)
                result = invoke([output])
                assert (result.returncode, result.stdout, result.stderr) == (0, expected, ''), (name, result)
                print(f'[通过] {name} AOT 默认优化', flush=True)
        legacy = Path(directory) / 'legacy.mgl'
        for spelling in ('lambda(int64 x){x+1}', 'int64 lambda(int64 x){return x+1;}',
                         'lambda copy(int64 x){return x+1;}', 'lambda move[x](){return x;}'):
            legacy.write_text('int64 main(){int64 x=1;auto f=' + spelling + ';return 0;}', encoding='utf-8')
            result = invoke([compiler, 'run', legacy])
            assert result.returncode != 0, (spelling, result.stdout, result.stderr)
        legacy.write_text('int64 main(){int64 lambda=3;assert(lambda==3);return 0;}', encoding='utf-8')
        result = invoke([compiler, 'run', legacy])
        assert result.returncode == 0, (result.stdout, result.stderr)
        for keyword in ('fn', 'lambda'):
            for fragment in (
                'int64 x=1; auto f=KEY const(){x+=1;return x;};',
                'int64 x=1; auto f=KEY copy[x] move[x](){return x;};',
                'auto f=KEY(int64 x) -> int64 {return x;};',
            ):
                source = Path(directory) / 'invalid.mgl'
                source.write_text('int64 main(){' + fragment.replace('KEY', keyword) + 'return 0;}', encoding='utf-8')
                result = invoke([compiler, '--check', source])
                assert result.returncode != 0, (keyword, fragment, result.stdout, result.stderr)
            print(f'[通过] {keyword} 非法捕获与箭头返回类型诊断', flush=True)


if __name__ == '__main__':
    main()
