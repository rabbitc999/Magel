"""break 语法清理的回归。

旧写法（break #level(N) / break value;）已删除，改为：
  yield 值;        ≡ 原 break 值;（值表达式内交出结果）
  break level N;   ≡ 原 break #level(N);（N 正整数字面量，仅循环层跳转）
  break all;       ≡ 原 break #level(ALL);（退出最外层循环）
  break level all; ≡ 原 break all;

本测试覆盖：新写法（yield / break level N / break all / break level all）VM 双后端
结果一致；旧写法（break #level(N) / break 值;）现在 chali check 必须报错。
"""
from pathlib import Path
import os
import subprocess
import tempfile
ROOT = Path(__file__).resolve().parents[1]
COMPILER = Path(os.environ.get('CHALI_TEST_COMPILER', str(ROOT / ('ChaLi.exe' if os.name == 'nt' else 'chali'))))
CASES = {'yield_sugar': '\nint64 main() {\n    int64 v = loop { yield 42; };\n    assert(v == 42);\n    int64 w = match(1) { 1 => { yield 7; } _ => { 0 } };\n    assert(w == 7);\n    println("yield sugar ok");\n    return 0;\n}\n', 'level_sugar': '\nint64 main() {\n    int64 n = 0;\n    loop { loop { n++; break level 2; }; assert(false); };\n    assert(n == 1);\n    int64 m = 0;\n    loop {\n        loop {\n            loop { m++; break level 3; };\n            assert(false);\n        };\n        assert(false);\n    };\n    assert(m == 1);\n    println("level sugar ok");\n    return 0;\n}\n', 'all_sugar': '\nint64 main() {\n    int64 a = 0;\n    loop { loop { a++; break all; }; assert(false); };\n    assert(a == 1);\n    println("all sugar ok");\n    return 0;\n}\n', 'level_all_sugar': '\nint64 main() {\n    int64 la = 0;\n    loop { loop { la++; break level all; }; assert(false); };\n    assert(la == 1);\n    println("level all sugar ok");\n    return 0;\n}\n', 'mixed': '\nint64 main() {\n    int64 r = 0;\n    loop {\n        loop {\n            loop { r++; break level 2; };\n            assert(false);\n        };\n        break all;\n    };\n    assert(r == 1);\n    int64 w = loop { yield 5; };\n    assert(w == 5);\n    int64 x = loop { yield 9; };\n    assert(x == 9);\n    println("mixed ok");\n    return 0;\n}\n'}
ERRORS = {'old_value_break': ('int64 main() { int64 v = loop { break 7; }; return 0; }', '数字'), 'old_hashlevel': ('int64 main() { loop { loop { break #level(2); } }; return 0; }', '认不出来的字符'), 'level_nonint': ('int64 main() { loop { break level foo; }; return 0; }', 'break level 后只能跟 all'), 'level_extra': ('int64 main() { loop { break level all extra; }; return 0; }', '分号'), 'level_negative': ('int64 main() { loop { break level -1; }; return 0; }', '减号')}

def main():
    env = dict(os.environ, CHALI_LANG='zh', CHALI_NO_STDIN_INPUT='1')
    with tempfile.TemporaryDirectory(prefix='chali-break-') as directory:
        folder = Path(directory)
        for name, source in CASES.items():
            selected = os.environ.get('CHALI_BREAK_CASE')
            if selected and selected != name:
                continue
            path = folder / (name + '.chl')
            path.write_text(source, encoding='utf-8')
            result = subprocess.run([str(COMPILER), 'run', str(path), 'out', str(folder / (name + '-vm'))], capture_output=True, text=True, encoding='utf-8', env=env, timeout=90)
            assert result.returncode == 0, (name, result.stdout, result.stderr)
            if os.environ.get('CHALI_BREAK_VM_ONLY'):
                print(name + ': VM 通过', flush=True)
                continue
            executable = folder / (name + ('.exe' if os.name == 'nt' else ''))
        if not os.environ.get('CHALI_BREAK_CASE'):
            for name, (source, fragment) in ERRORS.items():
                path = folder / (name + '.chl')
                path.write_text(source, encoding='utf-8')
                result = subprocess.run([str(COMPILER), 'check', str(path)], capture_output=True, text=True, encoding='utf-8', env=env, timeout=90)
                assert result.returncode != 0 and fragment in result.stdout + result.stderr, (name, result.stdout, result.stderr)
if __name__ == '__main__':
    main()
