"""验证线程超时等待不消费句柄、完成回收和原始失败诊断。"""

import argparse
import os
from pathlib import Path
import subprocess
import tempfile


def main():
    """在独立临时目录运行普通及压缩符号程序，检查等待边界。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--chali', default='chali')
    args = parser.parse_args()
    compiler = Path(args.chali).resolve()
    if not compiler.exists():
        compiler = compiler.with_suffix('.exe')
    with tempfile.TemporaryDirectory(prefix='chali-thread-wait-') as folder:
        directory = Path(folder)
        source = directory / 'wait.chl'

        def run(text, compressed=False):
            """写入本次程序，以真实编译器运行并返回 UTF-8 诊断。"""
            source.write_text(text, encoding='utf-8')
            return subprocess.run(
                [str(compiler), 'run', str(source), '--threads',
                 '--compress-symbols' if compressed else '--no-compress-symbols'],
                cwd=directory, capture_output=True, encoding='utf-8',
                errors='replace', timeout=30, env=dict(os.environ, CHALI_LANG='zh'),
            )

        prelude = '''import std::thread;
import std::clock;
void worker() {
    String value = std::thread::receive();
    println("worker:{}", value);
}
'''
        body = '''void main() {
    Thread task = std::thread::create(worker);
    if (std::thread::tryWait(task)) { let __boom = 11; }
    int64 started = std::clock::nowNanos();
    if (std::thread::waitFor(task, 20)) { let __boom = 12; }
    if (std::clock::nowNanos() - started < 20000000) { let __boom = 15; }
    std::thread::send(task, "alive");
    if (!std::thread::waitFor(task, 9223372036854775807)) { let __boom = 13; }
    println("done");

}
'''
        for compressed in (False, True):
            result = run(prelude + body, compressed)
            assert result.returncode == 0, (result.stdout, result.stderr)
            assert result.stdout.count('worker:alive') == 1, result.stdout
            assert 'done' in result.stdout, result.stdout

            # 超时后既可以再次限时等待，也可以沿用无期限 wait。
            result = run(prelude + body.replace(
                'if (!std::thread::waitFor(task, 9223372036854775807)) { return 13; }',
                'std::thread::wait(task);'), compressed)
            assert result.returncode == 0, result.stderr
            assert result.stdout.count('worker:alive') == 1, result.stdout

            for expression, error in (
                ('std::thread::waitFor(task, -1)', '不能为负数'),
                ('std::thread::waitFor(std::thread::current(), 0)', '无效、自身或已等待'),
                ('std::thread::waitFor(invalid, 0)', '无效、自身或已等待'),
            ):
                result = run(prelude + '''void main() {
    Thread task = std::thread::create(worker);
    Thread invalid;
    ''' + expression + ''';

}
''', compressed)
                assert result.returncode != 0 and error in result.stderr, result.stderr

            result = run('''import std::thread;
void leaf() { std::lang::panic("wait failure"); }
void main() {
    Thread task = std::thread::create(leaf);
    std::thread::waitFor(task, 5000);

}
''', compressed)
            assert result.returncode != 0 and '线程 1' in result.stderr and 'wait failure' in result.stderr, result.stderr
            assert 'leaf' in result.stderr or compressed, result.stderr

            result = run('''import std::thread;
void worker() {}
void main() {
    Thread task = std::thread::create(worker);
    if (!std::thread::waitFor(task, 5000)) { let __boom = 14; }
    std::thread::tryWait(task);

}
''', compressed)
            assert result.returncode != 0 and '已等待' in result.stderr, result.stderr
    print('线程超时等待回归通过')


if __name__ == '__main__':
    main()
