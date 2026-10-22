"""验证行输入及文件文本接口的 VM、错误与边界行为。"""
from pathlib import Path
import json
import os
import subprocess
import tempfile
ROOT = Path(__file__).resolve().parents[1]
COMPILER = ROOT / ('ChaLi.exe' if os.name == 'nt' else 'chali')

def main():
    """真实 stdin 保留空格和空行；文件预算、零字节与关闭错误沿用 Result。"""
    with tempfile.TemporaryDirectory(prefix='chali-text-readers-') as folder:
        work = Path(folder)
        env = dict(os.environ, CHALI_LANG='zh', CHALI_NO_STDIN_INPUT='0')

        def run(code, stdin='', arguments=(), options=()):
            file = work / 'main.chl'
            file.write_text(code, encoding='utf-8')
            command = [str(COMPILER), 'run', str(file), '--out', str(work / 'output'), *options]
            if arguments:
                command += ['--', *map(str, arguments)]
            return subprocess.run(command, input=stdin, cwd=work, env=env, capture_output=True, encoding='utf-8', timeout=60)
        code = (ROOT / 'example/samples/std_read_lines.chl').read_text(encoding='utf-8')
        expected = '[hello world]\n[\n中文 空格\nlast]\n'
        for _ in range(1):
            result = run(code, 'hello world\r\n\r\n中文 空格\r\nlast')
            assert result.returncode == 0 and result.stdout.replace('\r\n', '\n') == expected, (result.stdout, result.stderr)
            result = run('void main(){assert(readLines(0)=="");assert(readLines(9)=="one\\n\\ntwo");assert(readLine()=="");}', 'one\n\ntwo')
            assert result.returncode == 0, result.stderr
            result = run((ROOT / 'example/samples/std_file_strings.chl').read_text(encoding='utf-8'), arguments=[work / 'file.txt'])
            assert result.returncode == 0 and result.stdout.strip() == 'file strings ok', result.stderr
        result = run('void main(){assert(readLine()=="one");assert(readLines(2)=="two\\nthree");}', options=['--input', 'one two three'])
        assert result.returncode == 0, result.stderr
        result = run('void main(){readLines(-1);}')
        assert result.returncode != 0 and 'nonnegative' in result.stderr, result.stderr
        (work / 'raw.bin').write_bytes(b'ab\x00cd')
        (work / 'exact.txt').write_bytes(b'abcd')
        code = 'import std::io;\nvoid main(){\n FileStream exact=FileStream::open("exact.txt","rb").take();\n assert(exact.readAllString(4).unwrap()=="abcd");exact.close();\n FileStream short=FileStream::open("exact.txt","rb").take();assert(short.readAllString(3).isErr());short.close();\n FileStream zero=FileStream::open("raw.bin","rb").take();assert(zero.readString(5).isErr());zero.close();\n FileStream zeroLine=FileStream::open("raw.bin","rb").take();assert(zeroLine.readLine().isErr());zeroLine.close();\n FileStream bad=FileStream::open("exact.txt","rb").take();assert(bad.readString(-1).isErr());bad.close();assert(bad.readLine().isErr());\n return;\n}'
        result = run(code)
        assert result.returncode == 0, result.stderr
        result = subprocess.run([str(COMPILER), 'man', 'api', '--json', 'std::lang::readLines'], capture_output=True, encoding='utf-8')
        item = json.loads(result.stdout)['symbols'][0]
        assert item['parameters'][0]['name'] == 'N' and '非负' in item['parameters'][0]['documentation'], item
if __name__ == '__main__':
    main()
