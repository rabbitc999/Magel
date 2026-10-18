"""new 关键字语法清理的回归（v1.0.0）。

旧写法（裸 new; / 裸 new(args) / 工厂名 new）已删除，改为显式写出类型：
  heap T           ≡ 原 new;
  new T(args)      ≡ 原 new(args);（类型显式写出）
  heap new T(args) ≡ 原 new(args);（堆上构造）
  Type::create(...) 工厂方法由 new 改名为 create

本测试同时覆盖：
  * 新写法（heap T / new T(args) / ::create / heap new T(args) / new [n]）VM 与 AOT 双后端结果一致；
  * 旧写法（= new; / = new(...) / Type::new(...)）现在 magel check 必须报错。

VM 与 AOT 双后端结果一致。
"""
from pathlib import Path
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
COMPILER = Path(os.environ.get('MAGEL_TEST_COMPILER', str(ROOT / ('magel.exe' if os.name == 'nt' else 'magel'))))

CASES = {
    # 糖 heap T：堆上分配 T（类型由左值推导），等价于原 new;
    'heap_sugar': r'''
struct Point { int64 x; int64 y; }
int64 main() {
    Point mut& p = heap Point;
    p.x = 3; p.y = 4;
    assert(p.x == 3 && p.y == 4);
    println("heap sugar ok");
    return 0;
}
''',
    # 糖 new T(args)：按目标类型构造；工厂方法改名 create
    'new_ctor_sugar': r'''
struct Point { int64 x; int64 y; static Point create(int64 x, int64 y) { self.x = x; self.y = y; } }
int64 main() {
    Point mut& p = new Point(3, 4);
    assert(p.x == 3 && p.y == 4);
    Point q = Point::create(5, 6);
    assert(q.x == 5 && q.y == 6);
    println("new ctor sugar ok");
    return 0;
}
''',
    # 糖 heap new T(args)：堆上构造（左值为 T mut& 时即堆构造）
    'heap_new_sugar': r'''
struct Point { int64 x; int64 y; static Point create(int64 x, int64 y) { self.x = x; self.y = y; } }
int64 main() {
    Point mut& p = heap new Point(3, 4);
    assert(p.x == 3 && p.y == 4);
    println("heap new sugar ok");
    return 0;
}
''',
    # 保留 new [n] 数组分配（元素类型由左值推导）
    'array_sugar': r'''
int64 main() {
    int64 a[] = new [5];
    a[0] = 7;
    assert(a[0] == 7);
    println("new array ok");
    return 0;
}
''',
}

ERRORS = {
    # 裸 new; 已删除，必须报错
    'old_bare_new': ('struct Point { int64 x; int64 y; }\nint64 main() { Point mut& p = new; return 0; }', '分号'),
    # 裸 new(args) 已删除，必须报错
    'old_bare_new_args': ('struct Point { int64 x; int64 y; }\nint64 main() { Point mut& p = new(7); return 0; }', '左圆括号'),
    # 工厂名 new 已改名为 create，Type::new 必须报错
    'old_factory_new': ('struct Point { int64 x; int64 y; }\nint64 main() { Point q = Point::new(3, 4); return 0; }', '找不到匹配的重载'),
}


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
            result = subprocess.run([str(COMPILER), 'run', str(path), 'out', str(folder / (name + '-vm'))],
                                    capture_output=True, text=True, encoding='utf-8', env=env, timeout=90)
            assert result.returncode == 0, (name, result.stdout, result.stderr)
            if os.environ.get('MAGEL_NEW_VM_ONLY'):
                print(name + ': VM 通过', flush=True)
                continue
            executable = folder / (name + ('.exe' if os.name == 'nt' else ''))
            build = subprocess.run([str(COMPILER), 'aot', str(path), 'out', str(executable), 'opt', '0'],
                                   capture_output=True, text=True, encoding='utf-8', env=env, timeout=180)
            assert build.returncode == 0, (name, build.stdout, build.stderr)
            aot = subprocess.run([str(executable)], capture_output=True, text=True, encoding='utf-8', timeout=30)
            assert aot.returncode == 0 and aot.stdout == result.stdout, (name, aot.stdout, aot.stderr, result.stdout)
            print(name + ': VM / AOT 通过', flush=True)
        if not os.environ.get('MAGEL_NEW_CASE'):
            for name, (source, fragment) in ERRORS.items():
                path = folder / (name + '.mgl')
                path.write_text(source, encoding='utf-8')
                result = subprocess.run([str(COMPILER), 'check', str(path)],
                                        capture_output=True, text=True, encoding='utf-8', env=env, timeout=90)
                assert result.returncode != 0 and fragment in result.stdout + result.stderr, (name, result.stdout, result.stderr)
            print('new 语法清理：新写法 VM/AOT 通过，旧写法诊断到位。')


if __name__ == '__main__':
    main()
