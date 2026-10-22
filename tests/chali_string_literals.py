"""验证字面量数据合并、UTF-8 字节长度与 VM 字符串副本独立性。"""
import argparse
import os
import shutil
from pathlib import Path
import subprocess
import tempfile
ROOT = Path(__file__).resolve().parents[1]

def main():
    """运行独立 VM 编译器并检查重复字面量只占用一份生成期数据。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--chali', type=Path, default=ROOT / ('ChaLi.exe' if os.name == 'nt' else 'chali'))
    compiler = parser.parse_args().chali.resolve()
    if os.name == 'nt' and (not compiler.suffix) and compiler.with_suffix('.exe').is_file():
        compiler = compiler.with_suffix('.exe')
    with tempfile.TemporaryDirectory(prefix='string-literals-', dir=ROOT / 'build') as temporary:
        work = Path(temporary)
        source = ROOT / 'example/samples/string_literal_pool.chl'
        expected = (ROOT / 'example/expected/string_literal_pool.txt').read_text(encoding='utf-8')
        tools = work / 'tools'
        tools.mkdir()
        isolated_compiler = tools / compiler.name
        shutil.copy2(compiler, isolated_compiler)
        shadow = work / 'chali'
        (shadow / 'library/stackvm').mkdir(parents=True)
        (shadow / 'library/stackvm/native_text.cpp').write_text('#error unexpected_external_runtime\n', encoding='utf-8')
        for mode in ('run',):
            target = work / (mode + '')
            runner = isolated_compiler
            result = subprocess.run([str(runner), mode, str(source), '--out', str(target)], cwd=work, capture_output=True, text=True, encoding='utf-8', timeout=300)
            assert result.returncode == 0, (mode, result.stdout, result.stderr)
            assert result.returncode == 0 and result.stdout == expected and (not result.stderr), (mode, result.stdout, result.stderr)
            if mode == 'run':
                data = target.with_suffix('.data.bin').read_bytes()
                assert data.count('literal-pool-data-你好'.encode('utf-8')) == 1, '重复字面量未合并'
        for index, statement in enumerate(('"hello"[0] = \'H\';', '"hello"[0]++;', 'char mut& byte = &"hello"[0];')):
            invalid = work / f'readonly-{index}.chl'
            invalid.write_text('void main() { ' + statement + ' }\n', encoding='utf-8')
            result = subprocess.run([str(compiler), '--check', str(invalid)], cwd=ROOT, capture_output=True, text=True, encoding='utf-8', timeout=120)
            assert result.returncode != 0 and ('只读' in result.stderr or 'const&' in result.stderr), result.stderr
if __name__ == '__main__':
    main()
