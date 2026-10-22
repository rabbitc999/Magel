"""验证数组按值复制、只读引用、可变引用及引用来源的生命周期。"""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile
ROOT = Path(__file__).resolve().parents[1]

def main():
    """运行 VM 共用样例，并检查只读写入和引用逃逸诊断。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--magel', type=Path, default=ROOT / ('magel.exe' if os.name == 'nt' else 'magel'))
    compiler = parser.parse_args().magel.resolve()
    rejected = {'readonly': 'void f(int64[] & s) { s[0] = 1; }', 'const_reference': "void f(char[] const& s) { s[0] = 'x'; }", 'readonly_delete': 'void f(int64[] & s) { s.drop(); }', 'const_reference_delete': 'void f(char[] const& s) { s.drop(); }', 'readonly_increment': 'void f(int64[] & s) { s[0]++; }', 'readonly_forward': 'void change(int64[] mut& s) { s[0] = 1; } void f(int64[] & s) { change(s); }', 'readonly_element': 'void f(int64[] & s) { int64 mut& p = &s[0]; }', 'readonly_member': 'struct Cell { int64 value; }; void f(Cell[] const& s) { s[0].value = 1; }', 'readonly_member_reference': 'struct Cell { int64 value; }; void f(Cell[] const& s) { int64 mut& p = &s[0].value; }', 'readonly_foreach': 'void f(int64[] const& s) { for (let item in s) { item = 1; } }', 'escaped_array': 'int64[] & f() { int64 s[] = new[1]; return &s; }'}
    with tempfile.TemporaryDirectory(prefix='array-parameters-', dir=ROOT / 'build') as temporary:
        work = Path(temporary)
        for sample_name in ('array_parameter_values', 'fixed_array_values'):
            sample = ROOT / 'example/samples' / (sample_name + '.mgl')
            expected = (ROOT / 'example/expected' / (sample_name + '.txt')).read_text(encoding='utf-8')
            for backend in ('run',):
                target = work / (sample_name + '-' + backend + ('.exe' if os.name == 'nt' else ''))
                result = subprocess.run([str(compiler), backend, str(sample), '--out', str(target)], cwd=ROOT, capture_output=True, text=True, encoding='utf-8', timeout=240)
                assert result.returncode == 0, (sample_name, backend, result.stdout, result.stderr)
                assert result.returncode == 0 and result.stdout == expected and (not result.stderr), (sample_name, backend, result.stdout, result.stderr)
        for name, declaration in rejected.items():
            source = work / (name + '.mgl')
            source.write_text(declaration + '\nint64 main() { return 0; }\n', encoding='utf-8')
            result = subprocess.run([str(compiler), '--check', str(source)], cwd=ROOT, capture_output=True, text=True, encoding='utf-8', timeout=120)
            assert result.returncode != 0, (name, '错误的数组引用被接受')
            assert '语法' not in result.stderr and 'Syntax error' not in result.stderr, (name, result.stderr)
if __name__ == '__main__':
    main()
