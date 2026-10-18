#!/usr/bin/env python3
"""验证 MAGEL 原始源码、宏、include、子模块与导出的行级调试信息。"""
import argparse
from pathlib import Path
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--magel', default='magel.exe')
    parser.add_argument('--exports', action='store_true')
    args = parser.parse_args()
    compiler = Path(args.magel).resolve()
    with tempfile.TemporaryDirectory(prefix='magel-lines-', ignore_cleanup_errors=True) as folder:
        root = Path(folder)
        source = root / 'main.mgl'
        def run(text, expected, extra=()):
            source.write_text(text, encoding='utf-8')
            result = subprocess.run([str(compiler), 'run', str(source), '--out', str(root/'out'), *extra], capture_output=True, timeout=20)
            error = result.stderr.decode('utf-8', errors='replace')
            assert result.returncode != 0 and 'panic' in error, error
            assert expected in error, error
            return error
        run('int64 main() {\n std::lang::panic("plain");\n return 0;\n}\n', 'main.mgl:2')
        source.write_text('int64 main() {\n int64 zero=0;\n return 1/zero;\n}\n', encoding='utf-8')
        raw = subprocess.run([str(compiler), 'run', str(source), '--raw', '--out', str(root/'raw')], capture_output=True, timeout=20)
        assert raw.returncode != 0 and 'main.mgl:3' in raw.stderr.decode('utf-8', errors='replace'), raw.stderr
        (root/'included.mgl').write_text('int64 included() {\n std::lang::panic("include");\n return 0;\n}\n', encoding='utf-8')
        run('%include "included.mgl"\nint64 main() { return included(); }\n', 'included.mgl:2')
        (root/'worker.mgl').write_text('module worker;\nint64 crash() {\n std::lang::panic("module");\n return 0;\n}\n', encoding='utf-8')
        run('import worker;\nint64 main() { return worker::crash(); }\n', 'worker.mgl:3')
        macro = '$macro CRASH(x)\n std::lang::panic(x);\n$end_macro\nint64 main() {\n $CRASH("macro")\n return 0;\n}\n'
        run(macro, 'main.mgl [宏 CRASH]:2')
        run('$macro INNER(x)\n std::lang::panic(x);\n$end_macro\n$macro OUTER(x)\n $INNER(x)\n$end_macro\nint64 main() {\n $OUTER("nested")\n return 0;\n}\n', 'main.mgl [宏 INNER]:2')
        run('$macro STEP(x)\n int64 temporary = x;\n$end_macro\nint64 main() {\n $STEP(1)\n std::lang::panic("after");\n return 0;\n}\n', 'main.mgl:6')
        run('$macro MANY(...values)\n$for item in values\n std::lang::panic(item);\n$end_for\n$end_macro\nint64 main() {\n $MANY("loop")\n return 0;\n}\n', 'main.mgl [宏 MANY]:3')
        run('$macro JOIN(left, right)\n left\n ##\n right\n$end_macro\nint64 joined_name=1;\nint64 main() {\n int64 value=$JOIN(joined_, name);\n std::lang::panic("paste");\n return value;\n}\n', 'main.mgl:9')
        run('$macro COMMENT(x)\n /* don\'t "\n comment */\n std::lang::panic(x);\n$end_macro\nint64 main() {\n $COMMENT("comment")\n return 0;\n}\n', 'main.mgl [宏 COMMENT]:4')
        run('int64 main() {\n std::lang::String text="hello\nworld";\n std::lang::panic("multiline");\n return 0;\n}\n', 'main.mgl:4')
        hidden = run(macro, 'macro', ['--compress-symbols', '--debug'])
        assert 'main.mgl' not in hidden and '调用栈' not in hidden and '出错位置' not in hidden, hidden
        if args.exports:
            source.write_text(macro, encoding='utf-8')
            for target in ('aot',):
                exe = root / (target+'.exe')
                built = subprocess.run([str(compiler), target, str(source), 'checked', '--execute-output', str(exe), '--debug', '--opt-level', '0'], capture_output=True, timeout=180)
                assert built.returncode == 0, built.stderr.decode('utf-8',errors='replace')
                result = subprocess.run([str(exe)],capture_output=True,timeout=20)
                error = result.stderr.decode('utf-8', errors='replace')
                assert result.returncode != 0 and 'main.mgl [宏 CRASH]:2' in error, error
        print('MAGEL original lines + include + module + macro + privacy' + (' + exports' if args.exports else '') + ': PASS')


if __name__ == '__main__':
    main()
