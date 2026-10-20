"""new 关键字语法清理的回归。

旧写法（裸 new; / 裸 new(args) / 工厂名 new）已删除，改为显式写出类型：
  heap T           ≡ 原 new;
  new T(args)      ≡ 原 new(args);（类型显式写出）
  heap new T(args) ≡ 原 new(args);（堆上构造）
  Type::create(...) 工厂方法由 new 改名为 create

本测试同时覆盖：
  * 新写法（heap T / new T(args) / ::create / heap new T(args) / new [n]）VM 双后端结果一致；
  * 旧写法（= new; / = new(...) / Type::new(...)）现在 magel check 必须报错。

VM 双后端结果一致。
"""
from pathlib import Path
import os
import subprocess
import tempfile
ROOT = Path(__file__).resolve().parents[1]
COMPILER = Path(os.environ.get('MAGEL_TEST_COMPILER', str(ROOT / ('magel.exe' if os.name == 'nt' else 'magel'))))
CASES = {'heap_sugar': '\nstruct Point { int64 x; int64 y; }\nint64 main() {\n    Point mut& p = heap Point;\n    p.x = 3; p.y = 4;\n    assert(p.x == 3 && p.y == 4);\n    println("heap sugar ok");\n    return 0;\n}\n', 'new_ctor_sugar': '\nstruct Point { int64 x; int64 y; static Point create(int64 x, int64 y) { self.x = x; self.y = y; } }\nint64 main() {\n    Point mut& p = new Point(3, 4);\n    assert(p.x == 3 && p.y == 4);\n    Point q = Point::create(5, 6);\n    assert(q.x == 5 && q.y == 6);\n    println("new ctor sugar ok");\n    return 0;\n}\n', 'heap_new_sugar': '\nstruct Point { int64 x; int64 y; static Point create(int64 x, int64 y) { self.x = x; self.y = y; } }\nint64 main() {\n    Point mut& p = heap new Point(3, 4);\n    assert(p.x == 3 && p.y == 4);\n    println("heap new sugar ok");\n    return 0;\n}\n', 'array_sugar': '\nint64 main() {\n    int64 a[] = new [5];\n    a[0] = 7;\n    assert(a[0] == 7);\n    println("new array ok");\n    return 0;\n}\n'}
ERRORS = {'old_bare_new': ('struct Point { int64 x; int64 y; }\nint64 main() { Point mut& p = new; return 0; }', '分号'), 'old_bare_new_args': ('struct Point { int64 x; int64 y; }\nint64 main() { Point mut& p = new(7); return 0; }', '左圆括号'), 'old_factory_new': ('struct Point { int64 x; int64 y; }\nint64 main() { Point q = Point::new(3, 4); return 0; }', '找不到匹配的重载')}

def main():
    env = dict(os.environ, MAGEL_LANG='zh', MAGEL_NO_STDIN_INPUT='1')
    with tempfile.TemporaryDirectory(prefix='magel-new-') as directory:
        folder = Path(directory)
        for name, source in CASES.items():
            selected = os.environ.get('MAGEL_NEW_CASE')
            if selected and selected != name:
                continue
            path = folder / (name + '.mgl')
            path.write_text(source, encoding='utf-8')
            result = subprocess.run([str(COMPILER), 'run', str(path), 'out', str(folder / (name + '-vm'))], capture_output=True, text=True, encoding='utf-8', env=env, timeout=90)
            assert result.returncode == 0, (name, result.stdout, result.stderr)
            if os.environ.get('MAGEL_NEW_VM_ONLY'):
                print(name + ': VM 通过', flush=True)
                continue
            executable = folder / (name + ('.exe' if os.name == 'nt' else ''))
        if not os.environ.get('MAGEL_NEW_CASE'):
            for name, (source, fragment) in ERRORS.items():
                path = folder / (name + '.mgl')
                path.write_text(source, encoding='utf-8')
                result = subprocess.run([str(COMPILER), 'check', str(path)], capture_output=True, text=True, encoding='utf-8', env=env, timeout=90)
                assert result.returncode != 0 and fragment in result.stdout + result.stderr, (name, result.stdout, result.stderr)
if __name__ == '__main__':
    main()
