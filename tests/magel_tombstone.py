"""验证墓碑幂等、显式清扫、跨清扫存活对象及 VM 一致性。"""
from pathlib import Path
import argparse
import os
import subprocess
ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / 'build/tombstone'

def run(*args):
    """执行工具并保留完整失败上下文。"""
    return subprocess.run([str(arg) for arg in args], cwd=ROOT, capture_output=True, encoding='utf-8', errors='replace', timeout=240)

def main():
    """对标量、结构体和数组分别验证清扫前后释放。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--magel', type=Path, default=ROOT / 'magel.exe')
    args = parser.parse_args()
    compiler = args.magel.resolve()
    WORK.mkdir(parents=True, exist_ok=True)
    cases = {'scalar': ('void dispose(int64 mut& value) { delete value; }', 'int64 mut& value = heap int64;'), 'struct': ('struct Item { int64 number; }; void dispose(Item mut& value) { delete value; }', 'Item mut& value = heap Item;'), 'array': ('void dispose(int64[] mut& value) { delete value; }', 'int64 value[] = new[4];'), 'string_array': ('void dispose(String[] mut& value) { delete value; }', 'String value[] = new[2];value[0]="owned";')}
    for kind, (helper, allocation) in cases.items():
        for swept in (False, True):
            source = WORK / f'{kind}-{swept}.mgl'
            argument = '&value' if kind.endswith('array') else 'value'
            body = f'dispose({argument}); dispose({argument});'
            if swept:
                body += f'if (std::lang::sweep() < 1) {{ panic("missing tombstone"); }} dispose({argument});'
            source.write_text(helper + '\nint64 main() { ' + allocation + body + 'std::lang::println("ok"); return 0; }\n', encoding='utf-8')
            commands = [('VM', [compiler, 'run', source, '--out', WORK / 'vm'])]
            exe = WORK / (source.stem + ('.exe' if os.name == 'nt' else ''))
            for backend, command in commands:
                result = run(*command)
                if swept:
                    assert result.returncode != 0 and 'MGL1205' in result.stderr, (kind, backend, result)
                else:
                    assert result.returncode == 0 and result.stdout == 'ok\n' and (not result.stderr), (kind, backend, result)
    source = WORK / 'live.mgl'
    source.write_text('void dispose(int64 mut& value) { delete value; }\nint64 main() { int64 mut& live = heap int64; std::lang::sweep(); *live = 42; dispose(live); dispose(live); if (std::lang::sweep() < 1) { panic("missing live tombstone"); } if (std::lang::sweep() != 0) { panic("nonempty sweep"); } std::lang::println("live ok"); return 0; }\n', encoding='utf-8')
    result = run(compiler, 'run', source)
    assert result.returncode == 0 and result.stdout == 'live ok\n' and (not result.stderr), result
    print('[通过] 跨清扫活跃分配、后续幂等释放与空清扫', flush=True)
    source = WORK / 'merged.mgl'
    source.write_text('int64 main() { int64 tiny[] = new[1]; delete tiny; {\n    int64 i=0;\n    while (i<300) {\n int64 large[] = new[262144]; delete large; \n        i++;\n    }\n} std::lang::println("merged ok"); return 0; }\n', encoding='utf-8')
    output = WORK / ('merged' + ('.exe' if os.name == 'nt' else ''))
if __name__ == '__main__':
    main()
