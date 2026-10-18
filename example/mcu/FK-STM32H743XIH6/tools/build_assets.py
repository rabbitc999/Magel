"""生成无反射字节码与内嵌启动程序，使用同一份程序进行 FatFS 与 Flash 验证。"""
import argparse
from pathlib import Path
import subprocess


def main():
    """调用宿主编译器，保留三份部署文件并生成只读 C++ 数组。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--magel', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)
    source = Path(__file__).resolve().parents[1] / 'programs/benchmark.mgl'
    prefix = args.out.resolve() / 'main'
    subprocess.run([str(args.magel.resolve()), 'svmb', str(source), '--no-reflection',
                    '--out', str(prefix)], check=True)
    declarations = ['#pragma once', '#include <cstddef>']
    for symbol, suffix in (('program', '.svmb'), ('data', '.data.bin'), ('layout', '.layout')):
        content = prefix.with_suffix(suffix).read_bytes()
        payload = ','.join(str(value) for value in content) or '0'
        declarations.append(f'alignas(8) static const unsigned char builtin_{symbol}[] = {{{payload}}};')
        declarations.append(f'static constexpr std::size_t builtin_{symbol}_size = {len(content)};')
    for name in ('hello', 'panic'):
        subprocess.run([str(args.magel.resolve()), 'svmb', str(source.with_name(name + '.mgl')),
                        '--no-reflection', '--out', str(args.out.resolve() / name)], check=True)
        for symbol, suffix in (('program', '.svmb'), ('data', '.data.bin'), ('layout', '.layout')):
            content = (args.out / name).with_suffix(suffix).read_bytes()
            payload = ','.join(str(value) for value in content) or '0'
            declarations.append(f'alignas(8) static const unsigned char builtin_{name}_{symbol}[] = {{{payload}}};')
            declarations.append(f'static constexpr std::size_t builtin_{name}_{symbol}_size = {len(content)};')
    (args.out / 'builtin_program.hpp').write_text('\n'.join(declarations) + '\n', encoding='utf-8')


if __name__ == '__main__':
    main()
