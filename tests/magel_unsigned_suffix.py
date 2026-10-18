"""验证无符号 u 后缀的范围、负号诊断、常量及解释/AOT 一致性。"""
from pathlib import Path
import argparse
import os
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
EXE = '.exe' if os.name == 'nt' else ''


def main():
    """运行无符号字面量回归及非法写法检查。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--magel', type=Path, default=ROOT / ('magel' + EXE))
    args = parser.parse_args()
    compiler = args.magel.resolve()
    env = dict(os.environ, MAGEL_LANG='zh', MAGEL_NO_STDIN_INPUT='1')
    sys.path.insert(0, str(ROOT / 'tools'))
    from magel_api import TOKEN
    from magel_lsp import inferred_type
    for literal in ('1u', '18446744073709551615u', '0xffu', 'b_101u', 'b101u'):
        assert [m.group() for m in TOKEN.finditer(literal)] == [literal], literal
        assert inferred_type([], literal, '') == 'uint64', literal
    assert inferred_type([], '0xff', '') == 'int64'
    assert inferred_type([], '1f', '') == 'float32'
    with tempfile.TemporaryDirectory(prefix='magel-unsigned-') as directory:
        work = Path(directory)

        def invoke(command):
            """执行命令并捕获诊断，禁止等待交互输入。"""
            return subprocess.run(list(map(str, command)), cwd=ROOT, env=env,
                                  capture_output=True, text=True, encoding='utf-8',
                                  errors='replace', timeout=240)

        source = ROOT / 'example/samples/unsigned_suffix.mgl'
        for backend in ('run', 'aot'):
            for level in (0, 2):
                output = work / (f'{backend}-{level}' + EXE)
                result = invoke([compiler, backend, source, '--out', output, f'--opt-level={level}'])
                assert result.returncode == 0, (backend, level, result.stdout, result.stderr)
                if backend == 'aot': result = invoke([output])
                assert (result.returncode, result.stdout, result.stderr) == (0, 'unsigned suffix OK\n', ''), result
                print(f'[通过] {backend} opt={level} 无符号运算、上下文及常量', flush=True)

        def check(code, succeeds, diagnostic=''):
            """检查源程序是否按预期被接受或拒绝。"""
            source = work / 'check.mgl'
            source.write_text(code, encoding='utf-8')
            result = invoke([compiler, '--check', source])
            assert (result.returncode == 0) == succeeds, (code, result.stdout, result.stderr)
            assert diagnostic in result.stdout + result.stderr, (code, diagnostic, result.stdout, result.stderr)

        for expression in ('-1u', '-(1u)', '-+1u', '-0u', '-0xffu', '-b_1u'):
            check(f'int64 main(){{auto x={expression};return 0;}}', False, 'u 后缀整数字面量不能使用一元负号')
            print(f'[通过] 拒绝 {expression}', flush=True)
        for declaration in ('uint8 x=256u;', 'uint8 x=0x100u;', 'uint8 x=b_100000000u;',
                            'int64 x=1u;', 'uint8 x=1u;x=256u;'):
            check('int64 main(){' + declaration + 'return 0;}', False)
        check('int64 main(){auto x=18446744073709551616u;return 0;}', False, '超出 64 位')
        for expression in ('1.0u', '0x1_uu'):
            check(f'int64 main(){{auto x={expression};return 0;}}', False)
        check('''@annotation U(value:uint64=18446744073709551615u)
@U struct Tagged {};
@annotation Small(value:uint8)
@Small(255u) struct Narrow {};
int64 main(){return 0;}''', True)
        check('''@annotation Signed(value:int64)
@Signed(1u) struct Tagged {};
int64 main(){return 0;}''', False, 'u 后缀整数字面量只能用于无符号注解参数')
        check('''@annotation Small(value:uint8)
@Small(256u) struct Tagged {};
int64 main(){return 0;}''', False, '超出声明类型范围')
        print('[通过] 越界、换符号、非法后缀及注解参数检查', flush=True)


if __name__ == '__main__':
    main()
