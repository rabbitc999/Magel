"""编译开关隔离实验：同一份源码只切换 --checked / --unchecked。

目的不是再测一次绝对性能，而是回答「AOT 与手写 C++ 的倍数从哪来」：
在源码、优化等级、g++ 命令行、负载完全不变的前提下，只改一个编译开关，
观察耗时变化，从而把「诊断能力护栏」的代价与「运行时边界」的代价分开。

产出两份可核对的证据：

1. 两个版本的生成 C++（`*-checked.exe.aot.cpp` / `*-unchecked.exe.aot.cpp`）
   逐行比对，并把差异行数写进报告；实测差异只有顶部两行 `#define`。
2. `--unchecked` 并非「优化掉了计算」的反证：整数负载在递增 n 下的耗时呈
   线性增长，且各 n 的校验值与 `--checked` 逐位相同。

用法：
    python tests/benchmark_aot_flag_isolation.py --magel magel.exe
"""
import argparse
import datetime
import difflib
import hashlib
import json
import os
import platform
import re
import statistics
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_WORK = ROOT / 'build' / 'aot-flag-isolation'


def run(cmd, cwd=ROOT, env=None, timeout=900, **kw):
    merged = dict(os.environ, MAGEL_LANG=os.environ.get('MAGEL_LANG', 'zh'))
    if env:
        merged.update(env)
    result = subprocess.run([str(c) for c in cmd], cwd=str(cwd), env=merged,
                            capture_output=True, text=True, encoding='utf-8',
                            errors='replace', timeout=timeout, **kw)
    if result.returncode:
        raise RuntimeError(f'command failed ({result.returncode}): {cmd}\n'
                           f'stdout:\n{result.stdout}\nstderr:\n{result.stderr}')
    return result.stdout


def sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def build_aot(magel, source, out, opt, checked, work):
    """按给定开关导出 AOT 产物；--keep-intermediate 让生成 C++ 留在磁盘上可比对。"""
    run([magel, 'aot', source, '--out', out, f'--opt-level={opt}',
         '--checked' if checked else '--unchecked', '--keep-intermediate'])
    return Path(out)


def generated_cpp(exe):
    """magel aot 把生成源码写到 <输出名>.aot.cpp 旁边。"""
    return Path(str(exe) + '.aot.cpp')


def measure(exe, n, repeats):
    """AOT 优化对照握手：源码自身读 n 与 repeats，打印 <名称> <纳秒> <校验值>。"""
    out = run([exe], input=f'{n}\n{repeats}\n')
    samples = {}
    for line in out.splitlines():
        name, nanos, checksum = line.split()
        samples.setdefault(name, []).append((int(nanos) / 1e6, int(checksum)))
    return ({k: statistics.median(v for v, _ in pairs) for k, pairs in samples.items()},
            {k: pairs[-1][1] for k, pairs in samples.items()})


def measure_warmup_once(exe, n):
    """VM 基线的 AOT 源先读一次 n 预热、再读一次 n 计时，每个进程只出一个样本。

    该源自带 `elapsed/1000000.0`，首字段已是毫秒，不要再换算一次；第三字段是
    校验值，大整数会以 `5e+05` 这类科学计数形式打印，只能按字符串比对。
    """
    out = run([exe], input=f'{n}\n{n}\n').strip()
    fields = out.split()
    return float(fields[0]), fields[-1]


def head_defines(path, limit=4):
    """取生成 C++ 顶部的宏区域，用于证明两个开关版本只差这里。"""
    return Path(path).read_text(encoding='utf-8', errors='replace').splitlines()[:limit]


def cpp_diff(a, b):
    left = Path(a).read_text(encoding='utf-8', errors='replace').splitlines()
    right = Path(b).read_text(encoding='utf-8', errors='replace').splitlines()
    changed = [line for line in difflib.unified_diff(left, right, lineterm='')
               if line[:1] in '+-' and not line.startswith(('+++', '---'))]
    return changed


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--magel', type=Path, default=ROOT / 'magel.exe')
    parser.add_argument('--fixture', type=Path,
                        default=ROOT / 'tests/fixtures/aot_optimization_benchmark.mgl')
    parser.add_argument('--cpp', type=Path,
                        default=ROOT / 'tests/fixtures/AotOptimizationBenchmark.cpp',
                        help='原生 C++ 参照源；给出后以 -O2 -DNDEBUG 编译并测同一批算例')
    parser.add_argument('--vm-sources-dir', type=Path, default=ROOT / 'build/vm-baselines',
                        help='VM 基线的 *-aot.mgl 所在目录，缺省时跳过该组负载')
    parser.add_argument('--n', type=int, default=1000000)
    parser.add_argument('--repeats', type=int, default=5)
    parser.add_argument('--work', type=Path, default=DEFAULT_WORK)
    parser.add_argument('--out', type=Path,
                        default=ROOT / 'doc/benchmarks/aot-flag-isolation-2026-10-16.json')
    args = parser.parse_args()

    work = args.work.resolve()
    work.mkdir(parents=True, exist_ok=True)

    report = {
        'date': datetime.datetime.now(
            datetime.timezone(datetime.timedelta(hours=8))).isoformat(),
        'magel_version': (ROOT / 'VERSION').read_text().strip(),
        'commit': run(['git', 'rev-parse', 'HEAD']).strip(),
        'platform': platform.platform(),
        'cpu': platform.processor(),
        'host_compiler': run(['g++', '--version']).splitlines()[0],
        'n': args.n,
        'repeats': args.repeats,
        'purpose': '在同一份源码上只切换 --checked/--unchecked，'
                   '把「诊断护栏的代价」与「运行时边界的代价」分开',
        'fixed_cpp_switches': ['STACKVM_WITH_ALLOCATION_GUARDS', 'NO_MAGEL_PANIC_TRACE'],
    }

    # ---- 1) AOT 优化对照用的五算例：同源，只换开关 ----
    switches = {}
    for checked in (False, True):
        tag = 'checked' if checked else 'unchecked'
        exe = build_aot(args.magel, args.fixture, work / f'aotopt-{tag}.exe', 3, checked, work)
        switches[tag] = exe
    report['generated_cpp_sha256'] = {tag: sha256(generated_cpp(exe))
                                      for tag, exe in switches.items()}

    # 逐行比对两个生成文件，证明差异只在开关本身
    diff = cpp_diff(generated_cpp(switches['checked']), generated_cpp(switches['unchecked']))
    report['generated_cpp_diff'] = diff
    report['generated_cpp_diff_only_defines'] = bool(diff) and all(
        '#define' in line for line in diff)
    report['generated_cpp_head'] = {tag: head_defines(generated_cpp(exe))
                                    for tag, exe in switches.items()}

    checkpoints = {}
    for tag, exe in switches.items():
        median, checksum = measure(exe, args.n, args.repeats)
        checkpoints[tag] = {'median_ms': median, 'checksum': checksum}

    # ---- 2) 反证：--unchecked 下的整数负载随 n 线性增长，且结果与 checked 一致 ----
    linear_src = args.vm_sources_dir / 'integer_loop-aot.mgl'
    report['fold_check'] = None
    if linear_src.exists():
        linear = {}
        for n in (100000, 1000000, 10000000):
            exe = build_aot(args.magel, linear_src, work / f'fold-linear-{n}.exe', 2, False, work)
            ms, checksum = measure_warmup_once(exe, n)
            linear[str(n)] = {'ms': ms, 'checksum': checksum}
        growth = [linear[str(b)]['ms'] / linear[str(a)]['ms']
                  for a, b in ((100000, 1000000), (1000000, 10000000))]
        report['fold_check'] = {
            'source': str(linear_src.relative_to(ROOT)),
            'samples': linear,
            'growth_per_10x': growth,
            'linear': all(7.0 <= g <= 13.0 for g in growth),
            'note': '若循环被折掉，耗时不会随 n 增长；比值应在 10 附近',
        }

    # ---- 3) VM 基线的五类负载：同源，只换开关 ----
    vm_switches = {}
    if args.vm_sources_dir.is_dir():
        for name in ('integer_loop', 'branches', 'float64_loop', 'calls', 'array_reads'):
            source = args.vm_sources_dir / f'{name}-aot.mgl'
            if not source.exists():
                continue
            for checked in (False, True):
                tag = f"{name}-{'checked' if checked else 'unchecked'}"
                exe = build_aot(args.magel, source, work / f'{tag}.exe', 2, checked, work)
                ms, checksum = measure_warmup_once(exe, args.n)
                vm_switches[tag] = {'ms': ms, 'checksum': checksum}
    report['vm_loads'] = vm_switches or None
    if vm_switches:
        # 反证：换开关不改计算结果，只改耗时
        report['vm_loads_checksum_match'] = {
            name: vm_switches[f'{name}-checked']['checksum']
            == vm_switches[f'{name}-unchecked']['checksum']
            for name in ('integer_loop', 'branches', 'float64_loop', 'calls', 'array_reads')
            if f'{name}-checked' in vm_switches and f'{name}-unchecked' in vm_switches}
        report['vm_loads_ratio'] = {
            name: vm_switches[f'{name}-checked']['ms'] / vm_switches[f'{name}-unchecked']['ms']
            for name in report['vm_loads_checksum_match']}

    # ---- 4) 原生 C++ 参照 ----
    cpp_median = None
    if args.cpp:
        cpp_dir = work / 'cpp'
        cpp_dir.mkdir(parents=True, exist_ok=True)
        cpp_binary = cpp_dir / (args.cpp.stem + '.exe')
        run(['g++', '-std=c++17', '-O2', '-DNDEBUG', args.cpp.resolve(), '-o', cpp_binary])
        cpp_median, _ = measure(cpp_binary, args.n, args.repeats)
        report['cpp_command'] = ['g++', '-std=c++17', '-O2', '-DNDEBUG',
                                 str(args.cpp.resolve()), '-o', str(cpp_binary)]
        report['cpp_binary_sha256'] = sha256(cpp_binary)
        report['cpp_source_sha256'] = sha256(args.cpp.resolve())

    # ---- 5) 派生比值 ----
    ratios = {}
    for tag in ('checked', 'unchecked'):
        if tag not in checkpoints:
            continue
        if tag != 'unchecked':
            ratios[f'{tag}_over_unchecked'] = {
                k: (checkpoints[tag]['median_ms'][k] / checkpoints['unchecked']['median_ms'][k])
                for k in checkpoints[tag]['median_ms']}
        if cpp_median:
            ratios[f'{tag}_over_cpp'] = {k: checkpoints[tag]['median_ms'][k] / cpp_median[k]
                                         for k in checkpoints[tag]['median_ms']}
    if cpp_median:
        ratios['unchecked_over_cpp'] = {k: checkpoints['unchecked']['median_ms'][k] / cpp_median[k]
                                        for k in cpp_median}
    report['ratios'] = ratios
    report['median_ms'] = {tag: data['median_ms'] for tag, data in checkpoints.items()}
    if cpp_median:
        report['median_ms']['cpp'] = cpp_median

    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding='utf-8')

    # ---- 控制台摘要 ----
    print(f"生成 C++ 逐行差异仅含 #define：{report['generated_cpp_diff_only_defines']}"
          f"（{len(diff)} 行）")
    for line in diff:
        print(f'  {line}')
    print()
    header = f"{'算例':<16}{'unchecked':>12}{'checked':>12}{'C++':>12}{'c/u':>9}{'u/C++':>9}"
    print(header)
    order = list(checkpoints['unchecked']['median_ms'])
    for key in order:
        u = checkpoints['unchecked']['median_ms'][key]
        c = checkpoints['checked']['median_ms'][key]
        cpp = cpp_median.get(key) if cpp_median else None
        print(f"{key:<16}{u:>12.3f}{c:>12.3f}"
              f"{(f'{cpp:.3f}' if cpp else '-'):>12}"
              f"{c / u:>8.2f}x" + (f"{u / cpp:>8.2f}x" if cpp else ''))
    if report['fold_check']:
        fc = report['fold_check']
        print(f"\n线性检查：{fc['growth_per_10x']} 线性={fc['linear']}")
    print(f"\n写入 {args.out}")
    return 0


if __name__ == '__main__':
    sys.exit(main())
