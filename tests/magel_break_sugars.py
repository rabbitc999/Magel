"""break 语法清理的回归（v1.0.0）。

旧写法（break #level(N) / break value;）已删除，改为：
  yield 值;        ≡ 原 break 值;（值表达式内交出结果）
  break level N;   ≡ 原 break #level(N);（N 正整数字面量，仅循环层跳转）
  break all;       ≡ 原 break #level(ALL);（退出最外层循环）
  break level all; ≡ 原 break all;

本测试覆盖：新写法（yield / break level N / break all / break level all）VM 与 AOT 双后端
结果一致；旧写法（break #level(N) / break 值;）现在 magel check 必须报错。
"""
from pathlib import Path
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
COMPILER = Path(os.environ.get('MAGEL_TEST_COMPILER', str(ROOT / ('magel.exe' if os.name == 'nt' else 'magel'))))

CASES = {
    # yield 值; 交出结果
    'yield_sugar': r'''
int64 main() {
    int64 v = loop { yield 42; };
    assert(v == 42);
    int64 w = match(1) { 1 => { yield 7; } _ => { 0 } };
    assert(w == 7);
    println("yield sugar ok");
    return 0;
}
''',
    # break level N; 多层退出
    'level_sugar': r'''
int64 main() {
    int64 n = 0;
    loop { loop { n++; break level 2; }; assert(false); };
    assert(n == 1);
    int64 m = 0;
    loop {
        loop {
            loop { m++; break level 3; };
            assert(false);
        };
        assert(false);
    };
    assert(m == 1);
    println("level sugar ok");
    return 0;
}
''',
    # break all; 退出最外层循环
    'all_sugar': r'''
int64 main() {
    int64 a = 0;
    loop { loop { a++; break all; }; assert(false); };
    assert(a == 1);
    println("all sugar ok");
    return 0;
}
''',
    # break level all; 退出最外层循环
    'level_all_sugar': r'''
int64 main() {
    int64 la = 0;
    loop { loop { la++; break level all; }; assert(false); };
    assert(la == 1);
    println("level all sugar ok");
    return 0;
}
''',
    # 新旧混用（yield 值 + break level + break all）
    'mixed': r'''
int64 main() {
    int64 r = 0;
    loop {
        loop {
            loop { r++; break level 2; };
            assert(false);
        };
        break all;
    };
    assert(r == 1);
    int64 w = loop { yield 5; };
    assert(w == 5);
    int64 x = loop { yield 9; };
    assert(x == 9);
    println("mixed ok");
    return 0;
}
''',
}

ERRORS = {
    # break 值; 已删除，必须报错
    'old_value_break': ('int64 main() { int64 v = loop { break 7; }; return 0; }', '数字'),
    # break #level(N) 已删除，必须报错
    'old_hashlevel': ('int64 main() { loop { loop { break #level(2); } }; return 0; }', '认不出来的字符'),
    # break level 后必须跟 all（变量非法）
    'level_nonint': ('int64 main() { loop { break level foo; }; return 0; }', 'break level 后只能跟 all'),
    # break level all 后多 token
    'level_extra': ('int64 main() { loop { break level all extra; }; return 0; }', '分号'),
    # break level -1（负数）按值表达式解析，level 未声明
    'level_negative': ('int64 main() { loop { break level -1; }; return 0; }', '减号'),
}


def main():
    env = dict(os.environ, MAGEL_LANG='zh', MAGEL_NO_STDIN_INPUT='1')
    with tempfile.TemporaryDirectory(prefix='magel-break-') as directory:
        folder = Path(directory)
        for name, source in CASES.items():
            selected = os.environ.get('MAGEL_BREAK_CASE')
            if selected and selected != name:
                continue
            path = folder / (name + '.mgl')
            path.write_text(source, encoding='utf-8')
            result = subprocess.run([str(COMPILER), 'run', str(path), 'out', str(folder / (name + '-vm'))],
                                    capture_output=True, text=True, encoding='utf-8', env=env, timeout=90)
            assert result.returncode == 0, (name, result.stdout, result.stderr)
            if os.environ.get('MAGEL_BREAK_VM_ONLY'):
                print(name + ': VM 通过', flush=True)
                continue
            executable = folder / (name + ('.exe' if os.name == 'nt' else ''))
            build = subprocess.run([str(COMPILER), 'aot', str(path), 'out', str(executable), 'opt', '0'],
                                   capture_output=True, text=True, encoding='utf-8', env=env, timeout=180)
            assert build.returncode == 0, (name, build.stdout, build.stderr)
            aot = subprocess.run([str(executable)], capture_output=True, text=True, encoding='utf-8', timeout=30)
            assert aot.returncode == 0 and aot.stdout == result.stdout, (name, aot.stdout, aot.stderr, result.stdout)
            print(name + ': VM / AOT 通过', flush=True)
        if not os.environ.get('MAGEL_BREAK_CASE'):
            for name, (source, fragment) in ERRORS.items():
                path = folder / (name + '.mgl')
                path.write_text(source, encoding='utf-8')
                result = subprocess.run([str(COMPILER), 'check', str(path)],
                                        capture_output=True, text=True, encoding='utf-8', env=env, timeout=90)
                assert result.returncode != 0 and fragment in result.stdout + result.stderr, (name, result.stdout, result.stderr)
            print('break 语法清理：新写法 VM/AOT 通过，旧写法诊断到位。')


if __name__ == '__main__':
    main()
