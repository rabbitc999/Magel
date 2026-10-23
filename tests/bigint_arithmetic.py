"""用 Python 任意精度整数独立校验默认、NTT、Karatsuba 与两条除法路径。"""
from pathlib import Path
import argparse
import json
import random
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def cases():
    """生成固定种子的符号、进位、归一化和算法边界输入。"""
    rng = random.Random(20261021)
    values = [0, 1, -1, (1 << 32) - 1, 1 << 32, (1 << 64) - 1]
    for left in values:
        for right in values[1:]:
            yield left, right
    for width in [2, 3, 8, 31, 32, 55, 56, 57, 63, 64, 65, 95, 96, 97,
                  127, 128, 129, 191, 192, 193, 255, 256, 257, 383, 384, 385,
                  511, 512, 513, 767, 768, 769, 1023, 1024, 1025, 1536, 2048]:
        divisor = rng.getrandbits(width * 32 - 1) | (1 << (width * 32 - 1))
        yield divisor, divisor
        yield divisor + 1, divisor
        for rest in [0, 1, divisor - 1]:
            quotient = rng.getrandbits(width * 32 + 1)
            yield quotient * divisor + rest, divisor
        yield (1 << (width * 32)) - 1, (1 << (width * 32 - 31)) + 1
        yield rng.getrandbits(width * 32), rng.getrandbits(65) | 1
        if width >= 3:
            # 高两块估商无法识别最低块借位，必须走减乘后的回加校正。
            correction_divisor = (1 << (width * 32 - 1)) + 1
            yield correction_divisor * 2 - 1, correction_divisor
        if width in [257, 513, 1025, 2048]:
            for left_sign, right_sign in [(-1, 1), (1, -1), (-1, -1)]:
                yield (divisor * divisor + divisor - 1) * left_sign, divisor * right_sign
    for _ in range(600):
        width = rng.randrange(1, 150)
        left = rng.getrandbits(rng.randrange(1, width * 64 + 1))
        right = rng.getrandbits(rng.randrange(1, width * 32 + 1)) or 1
        yield left * rng.choice([-1, 1]), right * rng.choice([-1, 1])


def main():
    """编译差分入口并验证每条结果，保存可复核的测试摘要。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', type=Path, default=ROOT / 'tmp/bigint-arithmetic.exe')
    parser.add_argument('--portable', action='store_true', help='验证没有原生 128 位乘积的递归终点')
    args = parser.parse_args()
    binary = args.binary.resolve()
    binary.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run(['g++', '-std=c++17', '-O2', '-Wall', '-Wextra', '-D_GLIBCXX_ASSERTIONS',
                    *(['-DBIGINT_FORCE_PORTABLE_MULTIPLY'] if args.portable else []),
                    str(ROOT / 'tests/bigint_arithmetic.cpp'),
                    str(ROOT / 'source/bignum/bigint.cpp'), str(ROOT / 'source/bignum/bigint_multiplication.cpp'), str(ROOT / 'source/bignum/bigint_division.cpp'), str(ROOT / 'source/bignum/bigint_number_theory.cpp'), str(ROOT / 'source/bignum/bigint_bits.cpp'), str(ROOT / 'source/bignum/bigint_modular.cpp'), str(ROOT / 'source/bignum/bigint_conversion.cpp'), '-o', str(binary)], check=True)
    inputs = list(cases())
    rows = [(mode, left, right) for mode in range(3) for left, right in inputs]
    text = ''.join(f'{mode} {left:x} {right:x}\n' for mode, left, right in rows)
    result = subprocess.run([str(binary)], input=text, capture_output=True,
                            text=True, encoding='utf-8', timeout=240, check=True)
    lines = result.stdout.splitlines()
    assert len(lines) == len(rows), (len(lines), len(rows), result.stderr)
    for index, ((mode, left, right), line) in enumerate(zip(rows, lines)):
        product, quotient, remainder = [int(value, 16) for value in line.split()]
        expected = abs(left) // abs(right)
        if (left < 0) != (right < 0):
            expected = -expected
        assert (product, quotient, remainder) == (left * right, expected, left - expected * right), (index, mode, left, right)
    report = {'seed': 20261021, 'portable': args.portable, 'inputPairs': len(inputs), 'algorithmModes': 3,
              'cases': len(rows), 'integerComparisons': len(rows) * 3,
              'aliasAndSquareChecks': len(rows) * 2, 'status': 'passed'}
    (binary.parent / 'bigint-arithmetic-results.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(report, ensure_ascii=False))


if __name__ == '__main__':
    main()
