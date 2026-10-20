"""仅通过 VM 核验标准错误流、文件格式化打印及写入失败。"""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile


def main():
    """分别捕获 stdout、stderr 和文件字节，验证格式与资源错误互不混淆。"""
    parser = argparse.ArgumentParser()
    parser.add_argument('--magel', type=Path, default=Path('magel.exe'))
    parser.add_argument('--vm', type=Path)
    options = parser.parse_args()
    compiler = options.magel.resolve()
    with tempfile.TemporaryDirectory(prefix='magel-print-streams-') as folder:
        work = Path(folder)
        sequence = 0

        def run(source, success=True):
            """在独立工作目录运行源码，并保留失败诊断。"""
            nonlocal sequence
            sequence += 1
            path = work / f'case_{sequence}.mgl'
            path.write_text(source, encoding='utf-8')
            result = subprocess.run([str(compiler), 'run', str(path), '--out', str(work / f'out_{sequence}')],
                                    cwd=work, capture_output=True, encoding='utf-8', errors='replace',
                                    env=dict(os.environ, MAGEL_LANG='zh'), timeout=120)
            assert (result.returncode == 0) == success, (result.returncode, result.stdout, result.stderr)
            return result

        result = run('''void main() {
            print("stdout");
            assert(eprint("err={2:x}:{1:2F}", 1.25, 255) == 0);
            std::lang::eprintln(" 中文 {{}} {}", 7);
            String literal = "{unchanged}";
            eprint(literal);
            eprintln();
            println(" done");
        }''')
        assert result.stdout == 'stdout done\n', result.stdout
        assert result.stderr == 'err=ff:1.25 中文 {} 7\n{unchanged}\n', result.stderr
        if options.vm:
            prefix = work / 'standalone'
            emitted = subprocess.run([str(compiler), 'svmb', str(work / 'case_1.mgl'), '--out', str(prefix)],
                                     cwd=work, capture_output=True, encoding='utf-8', timeout=120)
            assert emitted.returncode == 0, emitted.stdout + emitted.stderr
            standalone = subprocess.run([str(options.vm.resolve()), 'run', str(prefix.with_suffix('.svmb')),
                                         '--data', str(prefix.with_suffix('.data.bin')),
                                         '--layout', str(prefix.with_suffix('.layout'))],
                                        cwd=work, capture_output=True, encoding='utf-8', timeout=120)
            assert standalone.returncode == 0, standalone.stdout + standalone.stderr
            assert standalone.stdout.partition('\n\n;')[0] == result.stdout.rstrip('\n'), standalone.stdout
            assert standalone.stderr == result.stderr, standalone.stderr

        result = run('''import std::io;
        void main() {
            FileStream stream = FileStream::open("stream.txt", "wb").take();
            assert(stream.print("{}:{:x}", 12, 255).unwrap() == 5);
            assert(stream.println(" {}", "中文").unwrap() == 8);
            assert(stream.println().unwrap() == 1);
            String literal = "{raw}";
            assert(stream.print(literal).unwrap() == 5);
            assert(stream.flush().isOk());
            stream.close();
            assert(stream.print("closed").isErr());
            assert(stream.println("closed").isErr());
            FileStream readonly = FileStream::open("stream.txt", "rb").take();
            assert(readonly.println("no").isErr());
            readonly.close();
            File raw = fileOpen("raw.txt", "wb").take();
            assert(raw.print("{:X}", 255) == 2);
            assert(raw.println(" {}", 7) == 3);
            assert(filePrint(&raw, "{{}}{}", 9) == 3);
            assert(std::io::filePrintln(&raw) == 1);
            fileClose(&raw);
        }''')
        assert not result.stdout and not result.stderr, (result.stdout, result.stderr)
        assert (work / 'stream.txt').read_bytes() == '12:ff 中文\n\n{raw}'.encode('utf-8')
        assert (work / 'raw.txt').read_bytes() == b'FF 7\n{}9\n'

        result = run('''import std::io;
        int64 eprint(int64 value) { return value + 2; }
        int64 filePrint(int64 value) { return value + 4; }
        struct Printer { int64 print(int64 value) { return value + 3; } };
        void main() { Printer item; assert(eprint(4) == 6); assert(item.print(4) == 7); assert(filePrint(4) == 8); }''')
        assert not result.stdout and not result.stderr
        result = run('''import std::io;
        void main() {
            auto filePrint = fn(int64 value) { value + 4 };
            auto filePrintln = fn(int64 value) { value + 5 };
            auto eprintln = fn(int64 value) { value + 6 };
            assert(filePrint(1) == 5);
            assert(filePrintln(1) == 6);
            assert(eprintln(1) == 7);
        }''')
        assert not result.stdout and not result.stderr
        result = run('''import std::io;
        void main() {
            auto filePrint = fn<T>(T value) { value };
            auto eprint = fn<T>(T value) { value };
            assert(filePrint(5) == 5);
            assert(eprint("value") == "value");
        }''')
        assert not result.stdout and not result.stderr
        for source, reason in [
            ('void main() { eprintln("{}", 1, 2); }', '没有被占位符用到'),
            ('import std::io;\nvoid main() { FileStream s = FileStream::open("bad.txt", "wb").take(); s.print("{:bad}", 1); }', '格式仅支持'),
        ]:
            result = run(source, success=False)
            assert reason in result.stderr and not result.stdout, result.stderr
        result = run('''import std::io;
        void main() {
            File file = fileOpen("closed.txt", "wb").take(); fileClose(&file); file.println("bad");
        }''', success=False)
        assert 'fwrite' in result.stderr and '已经释放过了' in result.stderr and not result.stdout, result.stderr
    print('VM 打印流：stdout/stderr 分离、格式化、文件字节与关闭/只读错误通过。')


if __name__ == '__main__':
    main()
