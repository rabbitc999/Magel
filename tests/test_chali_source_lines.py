"""验证 CHALI 原始源码、宏、导入模块、子模块与导出的行级调试信息。"""
import argparse
from pathlib import Path
import subprocess
import tempfile

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--chali', default='ChaLi.exe')
    args = parser.parse_args()
    compiler = Path(args.chali).resolve()
    with tempfile.TemporaryDirectory(prefix='chali-lines-', ignore_cleanup_errors=True) as folder:
        root = Path(folder)
        source = root / 'main.chl'

        def run(text, expected, extra=()):
            source.write_text(text, encoding='utf-8')
            result = subprocess.run([str(compiler), 'run', str(source), '--out', str(root / 'out'), *extra], capture_output=True, timeout=20)
            error = result.stderr.decode('utf-8', errors='replace')
            assert result.returncode != 0 and 'panic' in error, error
            assert expected in error, error
            return error
        run('int64 main() {\n std::lang::panic("plain");\n return 0;\n}\n', 'main.chl:2')
        source.write_text('int64 main() {\n int64 zero=0;\n return 1/zero;\n}\n', encoding='utf-8')
        raw = subprocess.run([str(compiler), 'run', str(source), '--raw', '--out', str(root / 'raw')], capture_output=True, timeout=20)
        assert raw.returncode != 0 and 'main.chl:3' in raw.stderr.decode('utf-8', errors='replace'), raw.stderr
        (root / 'included.chl').write_text('module included;\nint64 included() {\n std::lang::panic("include");\n return 0;\n}\n', encoding='utf-8')
        run('import included;\nint64 main() { return included::included(); }\n', 'included.chl:3')
        (root / 'worker.chl').write_text('module worker;\nint64 crash() {\n std::lang::panic("module");\n return 0;\n}\n', encoding='utf-8')
        run('import worker;\nint64 main() { return worker::crash(); }\n', 'worker.chl:3')
        macro = '$macro CRASH(x)\n std::lang::panic(x);\n$end_macro\nint64 main() {\n $CRASH("macro");\n return 0;\n}\n'
        run(macro, 'main.chl:5')
        run('$macro INNER(x)\n std::lang::panic(x);\n$end_macro\n$macro OUTER(x)\n $INNER(x);\n$end_macro\nint64 main() {\n $OUTER("nested");\n return 0;\n}\n', 'main.chl:8')
        run('$macro STEP(x)\n int64 temporary = x;\n$end_macro\nint64 main() {\n $STEP(1);\n std::lang::panic("after");\n return 0;\n}\n', 'main.chl:6')
        run('$macro MANY(...values)\n$for item in values\n std::lang::panic(item);\n$end_for\n$end_macro\nint64 main() {\n $MANY("loop");\n return 0;\n}\n', 'main.chl:7')
        run('$macro JOIN(left, right)\n $concat(left, right)\n\n\n$end_macro\nint64 joined_name=1;\nint64 main() {\n int64 value=$JOIN(joined_, name);\n std::lang::panic("paste");\n return value;\n}\n', 'main.chl:9')
        run('$macro COMMENT(x)\n /* don\'t "\n comment */\n std::lang::panic(x);\n$end_macro\nint64 main() {\n $COMMENT("comment");\n return 0;\n}\n', 'main.chl:7')
        run('int64 main() {\n std::lang::String text="hello\nworld";\n std::lang::panic("multiline");\n return 0;\n}\n', 'main.chl:4')
        hidden = run(macro, 'macro', ['--compress-symbols', '--debug'])
        assert 'main.chl' not in hidden and '调用栈' not in hidden and ('出错位置' not in hidden), hidden
        print('CHALI original lines + imported module + macro + privacy' + ': PASS')
if __name__ == '__main__':
    main()
