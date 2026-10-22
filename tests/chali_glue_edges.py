"""验证标准库胶水在解释执行 和 VM 导出中的一致性。

先构建 chali/chali（或 ChaLi.exe）与 VM，再运行本文件。
"""
from pathlib import Path
import os
import subprocess
ROOT = Path(__file__).resolve().parents[1]
EXE = '.exe' if os.name == 'nt' else ''
WORK = ROOT / 'build' / 'chali-glue-edges'
CHALI = Path(os.environ.get('CHALI_COMPILER', ROOT / ('chali' + EXE))).resolve()

def run(*args, succeeds=True):
    result = subprocess.run([str(a) for a in args], cwd=ROOT, capture_output=True, text=True, encoding='utf-8', errors='replace')
    assert (result.returncode == 0) == succeeds, result.stdout + result.stderr
    return result

def main():
    WORK.mkdir(parents=True, exist_ok=True)
    source = ROOT / 'example/samples/std_glue_edges.chl'
    expected = (ROOT / 'example/expected/std_glue_edges.txt').read_text(encoding='utf-8')
    result = run(CHALI, 'run', source, '--out', WORK / 'vm')
    assert result.stdout == expected, result.stdout
    assert '程序结束' not in result.stderr, result.stderr
    prefix = WORK / 'edges'
    run(CHALI, 'run', source, '--emit', '--out', prefix)
    common = ['--quad', prefix.with_suffix('.quad'), '--data', prefix.with_suffix('.data.bin'), '--layout', prefix.with_suffix('.layout')]
    for macro in ('NO_CHALI_STD_MATH',):
        executable = WORK / (macro + EXE)
        module = 'std::math::' if macro.endswith('MATH') else 'std::text::'
    for expression in ('std::math::clamp(1, 2, 0)', 'std::math::clamp(1.0, 2.0, 0.0)', 'std::math::clamp(1.0, std::math::sqrt(-1.0), 2.0)'):
        invalid = WORK / 'invalid.chl'
        invalid.write_text('import std::math;\nvoid main() { ' + expression + '; return; }', encoding='utf-8')
        result = run(CHALI, 'run', invalid, '--out', WORK / 'invalid', succeeds=False)
        assert 'low 不能大于 high' in result.stdout + result.stderr, result.stdout + result.stderr
if __name__ == '__main__':
    main()
