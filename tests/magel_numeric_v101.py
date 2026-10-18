"""v1.0.1 数值、数组安全与 float32 后缀的跨后端回归。"""
from pathlib import Path
import argparse
import os
import subprocess
import tempfile
import sys

ROOT = Path(__file__).resolve().parents[1]
EXE = '.exe' if os.name == 'nt' else ''


def main():
    """对解释器和 AOT 的各优化级别检查数值结果与异常行为。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--magel', type=Path, default=ROOT / ('magel' + EXE))
    args = parser.parse_args()
    compiler = args.magel.resolve()
    sys.path.insert(0, str(ROOT / 'tools'))
    from magel_lsp import inferred_type
    from magel_api import TOKEN
    for literal in ('1f', '1.25f', '1e2f', '1.25e-2f'):
        assert [m.group() for m in TOKEN.finditer(literal)] == [literal]
        assert inferred_type([], literal, '') == 'float32'
    assert inferred_type([], '1.25', '') == 'float64'
    assert inferred_type([], '0xff', '') == 'int64'
    print('[通过] 编辑器识别 f 后缀类型与科学计数法', flush=True)
    env = dict(os.environ, MAGEL_LANG='zh', MAGEL_NO_STDIN_INPUT='1')
    with tempfile.TemporaryDirectory(prefix='magel-numeric-101-') as directory:
        work = Path(directory)

        def invoke(command):
            """执行单条命令并保留中文诊断。"""
            return subprocess.run(list(map(str, command)), cwd=ROOT, env=env,
                                  capture_output=True, text=True, encoding='utf-8',
                                  errors='replace', timeout=240)

        def check(result, success, message=''):
            """断言退出状态及关键输出。"""
            assert (result.returncode == 0) == success, (result.returncode, result.stdout, result.stderr)
            assert message in result.stdout + result.stderr, (message, result.stdout, result.stderr)

        sample = ROOT / 'example/samples/numeric_v101.mgl'
        for backend in ('run', 'aot'):
            for level in range(3):
                output = work / (f'{backend}-{level}' + EXE)
                result = invoke([compiler, backend, sample, '--out', output, f'--opt-level={level}'])
                check(result, True)
                if backend == 'aot':
                    result = invoke([output])
                check(result, True, 'numeric v1.0.1 OK')
                assert not result.stderr, result.stderr
                print(f'[通过] {backend} opt={level} 数值、f 后缀及 loop', flush=True)

        for name, value in [('string', '"x"'), ('float', '1.9'), ('bool', 'true')]:
            source = work / (name + '.mgl')
            source.write_text(f'int64 main() {{ int64 a[]=new[{value}]; delete a; return 0; }}', encoding='utf-8')
            check(invoke([compiler, '--check', source]), False, '数组长度必须是整数')
            print(f'[通过] 拒绝 {name} 数组长度', flush=True)

        # 输入长度从运行时读取，确保优化器不能把异常路径提前折叠。
        source = work / 'invalid-allocation.mgl'
        source.write_text('''int64 main() {
            int64 n=inputInt(); int64 a[]=new[n];
            println("unexpected allocation: {}", a.len()); delete a; return 0;
        }''', encoding='utf-8')
        for backend in ('run', 'aot'):
            output = work / ('invalid-' + backend + EXE)
            if backend == 'aot':
                check(invoke([compiler, backend, source, '--out', output, '--opt-level=0']), True)
            for length, diagnostic in [(-1, '数组长度为负数或分配字节数溢出'),
                                       (-4294967295, '数组长度为负数或分配字节数溢出'),
                                       (9223372036854775807, '数组长度为负数或分配字节数溢出'),
                                       (4294967297, '堆内存不足'), (536870913, '堆内存不足')]:
                command = [compiler, backend, source, '--out', output] if backend == 'run' else [output]
                result = subprocess.run(list(map(str, command)), cwd=ROOT, env=dict(env, MAGEL_NO_STDIN_INPUT='0'), input=f'{length}\n',
                                        capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=30)
                check(result, False, diagnostic)
                assert 'unexpected allocation' not in result.stdout, result.stdout
                print(f'[通过] {backend} 拒绝数组长度 {length}', flush=True)

        # 有 break 的循环确实可能落到函数末尾，不能误放行缺少返回值。
        for body in ('loop { break; };', 'loop { if (true) { break; } else { return 1; } };'):
            source = work / 'missing-return.mgl'
            source.write_text('int64 f(){' + body + '} int64 main(){return 0;}', encoding='utf-8')
            check(invoke([compiler, '--check', source]), False, '不是所有路径都有 return')
        print('[通过] 循环可退出时仍检查缺失返回路径', flush=True)

        # 无后缀不能自动缩窄成 float32；后缀不是忽略类型的装饰。
        source = work / 'no-suffix-narrowing.mgl'
        source.write_text('float32 f(float32 x){return x;} int64 main(){auto x=f(1.25);return 0;}', encoding='utf-8')
        check(invoke([compiler, '--check', source]), False)
        print('[通过] 无后缀 float64 不隐式缩窄', flush=True)


if __name__ == '__main__':
    main()
