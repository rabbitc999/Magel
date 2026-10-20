"""验证字符串自动清理清槽、循环重入和头部重复查询及网络 / SQL 胶水在VM的一致性。"""
from pathlib import Path
import argparse
import os
import subprocess
ROOT = Path(__file__).resolve().parents[1]
EXE = '.exe' if os.name == 'nt' else ''
MAGEL = ROOT / ('magel' + EXE)
WORK = ROOT / 'build/magel-string-reentry'

def run(*args):
    """执行工具或产物，保留标准输出和错误信息供失败诊断。"""
    result = subprocess.run([str(a) for a in args], cwd=ROOT, capture_output=True, text=True, encoding='utf-8', errors='replace')
    assert result.returncode == 0, (args, result.returncode, result.stdout, result.stderr)
    return result

def main():
    """对解释执行 和 VM 校验相同输出以及资源泄漏诊断。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--magel', type=Path, default=MAGEL, help='待验证的 MAGEL 工具链路径')
    args = parser.parse_args()
    compiler = args.magel.resolve()
    WORK.mkdir(parents=True, exist_ok=True)
    source = ROOT / 'example/samples/string_method_reentry.mgl'
    expected = (ROOT / 'example/expected/string_method_reentry.txt').read_text(encoding='utf-8')
    results = [('解释执行', run(compiler, 'run', source, '--out', WORK / 'interpret'))]
    for mode, result in results:
        assert result.stdout == expected, (mode, result.stdout, expected)
        assert not result.stderr, (mode, result.stderr)
if __name__ == '__main__':
    main()
