"""验证格式化保留多行字符串、插值字符串和注释中的原始内容。"""
import argparse
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def main():
    """比较格式化前后的执行结果，并检查二次格式化不再产生改动。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--magel', type=Path, default=ROOT / 'magel.exe')
    compiler = parser.parse_args().magel.resolve()
    work = ROOT / 'build/formatter-literals'
    work.mkdir(parents=True, exist_ok=True)
    source = work / 'multiline.mgl'
    source.write_text('''void main(){
String ordinary="first
  second";
String interpolated=f"first {1}
 second {2}";
/* comment {
   content }
*/
println("{}|{}",ordinary,interpolated);
}
''', encoding='utf-8')

    def invoke(*args):
        """执行格式化或 VM，失败时保留诊断。"""
        result = subprocess.run([str(compiler), *map(str, args)], cwd=ROOT,
                                capture_output=True, text=True, encoding='utf-8', timeout=60)
        assert result.returncode == 0, result
        return result.stdout

    before = invoke('run', source)
    invoke('fmt', source, '--write')
    formatted = source.read_bytes()
    assert 'first\n  second' in formatted.decode('utf-8')
    assert 'first {1}\n second {2}' in formatted.decode('utf-8')
    assert invoke('run', source) == before
    invoke('fmt', source, '--check')
    invoke('fmt', source, '--write')
    assert source.read_bytes() == formatted
    print('格式化保留多行字符串内容，执行结果与重复格式化均一致。')


if __name__ == '__main__':
    main()
