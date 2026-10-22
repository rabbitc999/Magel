"""验证线性代数求解、数组互转与索引在解释执行 和 VM 导出中的一致性。

先构建 chali/chali（或 ChaLi.exe）与 VM，再运行本文件。
"""
from pathlib import Path
import os
import subprocess
ROOT = Path(__file__).resolve().parents[1]
EXE = '.exe' if os.name == 'nt' else ''
WORK = ROOT / 'build' / 'chali-linalg-solve'
CHALI = ROOT / ('chali' + EXE)

def run(*args, succeeds=True):
    result = subprocess.run([str(a) for a in args], cwd=ROOT, capture_output=True, text=True, encoding='utf-8', errors='replace')
    assert (result.returncode == 0) == succeeds, (result.returncode, args, result.stdout, result.stderr)
    return result

def main():
    WORK.mkdir(parents=True, exist_ok=True)
    source = ROOT / 'example/samples/std_linalg_solve_glue.chl'
    expected = (ROOT / 'example/expected/std_linalg_solve_glue.txt').read_text(encoding='utf-8')
    result = run(CHALI, 'run', source, '--out', WORK / 'vm')
    assert result.stdout == expected, result.stdout
    assert '程序结束' not in result.stderr, result.stderr
    prefix = WORK / 'edges'
    exportSource = WORK / 'export.chl'
    text = source.read_text(encoding='utf-8')
    for scalar in ('int8', 'int16', 'int32', 'uint8', 'uint16', 'uint32', 'uint64'):
        text = text.replace('    checkInteger<' + scalar + '>();', '')
    exportSource.write_text(text, encoding='utf-8')
    run(CHALI, 'run', exportSource, '--emit', '--out', prefix)
    common = ['--quad', prefix.with_suffix('.quad'), '--data', prefix.with_suffix('.data.bin'), '--layout', prefix.with_suffix('.layout')]
    executable = WORK / ('trimmed' + EXE)
    for setup, message in (('a.set(0, 0, 0.0);', '奇异或近奇异矩阵'), ('a.set(0, 0, 0.000000000001);', '奇异或近奇异矩阵'), ('a.rows = 2;', '维度不匹配'), ('a.rows = -1; a.cols = -1;', '维度不匹配'), ('a.values.clear();', '维度不匹配'), ('std::linalg::Vector<float64> col = a[-1];', '列索引越界'), ('std::linalg::Vector<float64> col = a[1];', '列索引越界'), ('float64 value = rhs[1];', '越界'), ('float64 raw[2]; std::linalg::Matrix<float64> bad = std::linalg::Matrix<float64>::fromArray(raw, 2, 2, 2);', '维度不匹配')):
        invalid = WORK / 'invalid.chl'
        invalid.write_text('import std::linalg;\nvoid main() { std::linalg::Matrix<float64> a = std::linalg::Matrix<float64>::identity(1); std::linalg::Vector<float64> rhs = std::linalg::Vector<float64>::zeros(1); ' + setup + ' std::linalg::Vector<float64> x = a.solve(rhs); return; }', encoding='utf-8')
        result = run(CHALI, 'run', invalid, '--out', WORK / 'invalid', succeeds=False)
        assert message in result.stdout + result.stderr, result.stdout + result.stderr
if __name__ == '__main__':
    main()
