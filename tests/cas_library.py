"""重点构建并运行 caspp 全部独立测试，不扩展到其他库的全量回归。"""
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import json
import os
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def main():
    """串行构建后并行运行隔离的 CAS 测试进程，保留每项日志。"""
    output = ROOT / 'tmp/cas-library-tests'
    output.mkdir(parents=True, exist_ok=True)
    library = ROOT / 'library/caspp'
    with (output / 'build.log').open('w', encoding='utf-8') as log:
        subprocess.run(['make', '-C', str(library), '-j4', 'all'], stdout=log, stderr=subprocess.STDOUT, check=True)
    suffix = '.exe' if os.name == 'nt' else ''
    binaries = [(source.stem, library / 'build/tests' / (source.stem + suffix))
                for source in sorted((library / 'tests').glob('*.cpp'))]

    def run(entry):
        """运行一个独立测试，避免共享内存池和临时输出。"""
        name, binary = entry
        with (output / (name + '.log')).open('w', encoding='utf-8') as log:
            result = subprocess.run([str(binary)], cwd=library, stdout=log, stderr=subprocess.STDOUT, timeout=300)
        return {'name': name, 'exitCode': result.returncode}

    with ThreadPoolExecutor(max_workers=4) as pool:
        results = list(pool.map(run, binaries))
    report = {'tests': results, 'count': len(results), 'allPassed': all(row['exitCode'] == 0 for row in results)}
    (output / 'results.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(report, ensure_ascii=False))
    assert report['allPassed'], output


if __name__ == '__main__':
    main()
