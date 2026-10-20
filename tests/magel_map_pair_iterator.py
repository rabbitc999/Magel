"""仅用 VM 验证 Map 迭代元素为 Pair，并拒绝元组、键和值元素类型。"""
import argparse
from pathlib import Path
import subprocess
import tempfile


def main():
    """验证借用快照和消费迭代的 Pair 类型与嵌套所有权。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--magel', default='magel.exe')
    compiler = str(Path(parser.parse_args().magel).resolve())
    root = Path(__file__).resolve().parents[1]
    with tempfile.TemporaryDirectory(prefix='magel-map-pair-') as directory:
        work = Path(directory)
        result = subprocess.run([compiler, 'run', str(root / 'example/samples/map_pair_iterator.mgl'),
                                 'out', str(work / 'success')], capture_output=True,
                                encoding='utf-8', errors='replace', timeout=180)
        assert result.returncode == 0 and result.stdout.strip() == 'Map Pair iterator OK', result.stdout + result.stderr
        assert not result.stderr, result.stderr
        for kind in ['(String, int64)', 'String', 'int64']:
            source = work / 'invalid.mgl'
            source.write_text('import std::iter;\nint64 main() { Map<String, int64> values = {"a": 1}; '
                              + 'for (' + kind + ' entry in values.iter()) {} return 0; }', encoding='utf-8')
            result = subprocess.run([compiler, '--check', str(source)], capture_output=True,
                                    encoding='utf-8', errors='replace', timeout=90)
            assert result.returncode != 0 and 'Iterator<R>' in result.stdout + result.stderr, result.stdout + result.stderr
        source.write_text('int64 main() { Pair<String, int64> value = Pair<String, int64>::from(("a", 1)); return 0; }', encoding='utf-8')
        result = subprocess.run([compiler, '--check', str(source)], capture_output=True,
                                encoding='utf-8', errors='replace', timeout=90)
        assert result.returncode != 0, result.stdout + result.stderr
    print('Map Pair iterator: VM ownership, create/toTuple and 4 rejection checks passed')


if __name__ == '__main__':
    main()
