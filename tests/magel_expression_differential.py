"""用固定随机种子生成数值表达式，以 Python 独立计算结果校验 MAGEL 解释与 AOT。"""
from pathlib import Path
import argparse
import os
import random
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
EXE = '.exe' if os.name == 'nt' else ''


def expressions():
    """生成有界整数表达式及预期值，排除溢出、零除数和非法移位。"""
    rng = random.Random(20261010)

    def arithmetic(depth):
        """递归生成算术表达式；除法按 MAGEL 规则向零截断。"""
        if depth == 0:
            value = rng.randint(-20, 20)
            return f'({value})', value
        left, a = arithmetic(depth - 1)
        right, b = arithmetic(depth - 1)
        op = rng.choice(('+', '-', '*', '/', '%'))
        if op in ('/', '%') and b == 0:
            right, b = '(1)', 1
        quotient = abs(a) // abs(b) * (-1 if (a < 0) != (b < 0) else 1) if b else 0
        value = {'+': a + b, '-': a - b, '*': a * b,
                 '/': quotient, '%': a - quotient * b}[op]
        return f'({left}{op}{right})', value

    cases = [arithmetic(3) for _ in range(128)]
    for _ in range(32):
        a, b = rng.randint(-1000, 1000), rng.randint(-1000, 1000)
        op = rng.choice(('&', '|', '^'))
        expected = {'&': a & b, '|': a | b, '^': a ^ b}[op]
        cases.append((f'(({a}){op}({b}))', expected))
    for _ in range(16):
        a, b = rng.randint(0, 1000), rng.randint(0, 7)
        op = rng.choice(('<<', '>>'))
        cases.append((f'({a}{op}{b})', a << b if op == '<<' else a >> b))
    for _ in range(16):
        left, a = arithmetic(2)
        right, b = arithmetic(2)
        op = rng.choice(('==', '!=', '<', '<=', '>', '>='))
        expected = {'==': a == b, '!=': a != b, '<': a < b,
                    '<=': a <= b, '>': a > b, '>=': a >= b}[op]
        cases.append((f'({left}{op}{right})', 'true' if expected else 'false'))
    return cases


def main():
    """生成临时 MAGEL 程序并分别验证解释执行 和 AOT。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--magel', type=Path, default=ROOT / ('magel' + EXE))
    args = parser.parse_args()
    compiler = args.magel.resolve()
    cases = expressions()
    (ROOT / 'build').mkdir(exist_ok=True)

    def run(command):
        """执行工具并要求成功且无资源诊断。"""
        result = subprocess.run(list(map(str, command)), cwd=ROOT, capture_output=True,
                                text=True, encoding='utf-8', errors='replace', timeout=240)
        assert result.returncode == 0 and not result.stderr, (command, result.stdout, result.stderr)
        return result

    with tempfile.TemporaryDirectory(prefix='magel-expression-audit-', dir=ROOT / 'build') as directory:
        work = Path(directory)
        source = work / 'expressions.mgl'
        checks = '\n'.join(f'    assert(({expr})==({value}));' for expr, value in cases)
        source.write_text('int64 main(){\n' + checks +
                          '\n    println("expression differential OK");\n    return 0;\n}\n',
                          encoding='utf-8')
        for backend in ('run', 'aot'):
            output = work / (backend + EXE)
            command = [compiler, backend, source, '--out', output]
            if backend != 'run':
                command.append('--opt-level=0')
            result = run(command)
            if backend != 'run':
                result = run([output])
            assert result.stdout == 'expression differential OK\n', (backend, result.stdout)
            print(f'[通过] {backend}：{len(cases)} 个固定种子表达式', flush=True)


if __name__ == '__main__':
    main()
