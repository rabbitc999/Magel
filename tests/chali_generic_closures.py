"""通过 VM 验证泛型闭包的类型推导、特化、捕获状态与结构体成员。"""
import argparse
from pathlib import Path
import subprocess
import tempfile


def main():
    """运行成功用例并要求非法的类型推导、特化与借用使用被编译器拒绝。"""
    parser = argparse.ArgumentParser()
    parser.add_argument('--chali', type=Path, default=Path('ChaLi.exe'))
    compiler = parser.parse_args().chali.resolve()
    accepted = {
        'identity': ('''int64 main() {
            let f = fn<T>(T x) { x };
            assert(f(3) == 3);
            assert(f("hello") == "hello");
            assert(f<int64>(4) == 4);
            return 0;
        }''', ''),
        'specialization': ('''int64 main() {
            let f = fn<T, U>(T x, U y) { 1 };
            f<T, int64> = fn(T x, int64 y) { 2 };
            f<int64, int64> = fn(int64 x, int64 y) { 3 };
            assert(f(true, false) == 1);
            assert(f(true, 2) == 2);
            assert(f(1, 2) == 3);
            return 0;
        }''', ''),
        'shared_capture': ('''int64 main() {
            int64 state = 0;
            let f = fn<T> copy[state](T x) {
                state += 1;
                return state;
            };
            f<int64> = fn(int64 x) {
                state += 10;
                return state;
            };
            assert(f(true) == 1);
            assert(f(2) == 11);
            assert(f("x") == 12);
            assert(state == 0);
            let g = f;
            assert(g(3) == 22);
            assert(f(false) == 13);
            return 0;
        }''', ''),
        'member': ('''struct S {
            let f = fn<T>(T x) { x };
            f<int64> = fn(int64 x) { x + 1 };
        };
        int64 main() {
            S s;
            assert(s.f(2) == 3);
            assert(s.f<String>("member") == "member");
            S other = s;
            assert(other.f(true));
            return 0;
        }''', ''),
        'generic_parameter': ('''int64 apply<F>(F f) { return f(3); }
        struct Box<F> { F value; };
        int64 boxed<F>(F f) {
            Box<F> box;
            box.value = f;
            return box.value(5);
        }
        int64 main() {
            let f = fn<T>(T x) { x + 1 };
            assert(apply(f) == 4);
            assert(boxed(f) == 6);
            let plain = int64 fn(int64 x) { x * 2 };
            assert(apply(plain) == 6);
            return 0;
        }''', ''),
        'borrow': ('''int64 main() {
            int64 state = 1;
            let f = fn<T> mut[state](T x) {
                state += 1;
                return state;
            };
            assert(f(true) == 2);
            assert(f(3) == 3);
            assert(state == 3);
            return 0;
        }''', ''),
    }
    accepted.update({
        'full_resolves_overlap': ('''int64 main() {
            let f = fn<T, U>(T x, U y) { 0 };
            f<T, int64> = fn(T x, int64 y) { 1 };
            f<int64, U> = fn(int64 x, U y) { 2 };
            f<int64, int64> = fn(int64 x, int64 y) { 3 };
            assert(f(1, 2) == 3);
            return 0;
        }''', ''),
        'member_capture': ('''int64 initial = 4;
        struct S {
            let f = fn<T> copy[initial](T x) {
                initial += 1;
                return initial;
            };
        };
        int64 main() {
            int64 initial = 99;
            S first;
            S second;
            assert(first.f(1) == 5);
            assert(first.f(true) == 6);
            assert(second.f(1) == 5);
            assert(initial == 99);
            return 0;
        }''', ''),
        'void': ('''int64 main() {
            int64 count = 0;
            let f = fn<T> mut[count](T x) { count += 1; };
            f(1);
            f(true);
            assert(count == 2);
            return 0;
        }''', ''),
        'generic_reference': ('''int64 apply<F>(F const& f) { return f(3); }
        int64 main() {
            int64 offset = 2;
            let f = fn<T> copy[offset](T x) { x + offset };
            assert(apply(&f) == 5);
            return 0;
        }''', ''),
    })
    accepted.update({
        'outer_struct': ('''struct S<T> {
            let f = fn<U>(T value, U ignored) { value };
        };
        int64 main() {
            S<int64> numbers;
            S<String> text;
            assert(numbers.f(7, true) == 7);
            assert(text.f("outer", 1) == "outer");
            return 0;
        }''', ''),
        'outer_function': ('''let make<T>(T value) {
            let callback = fn<U> copy[value](U ignored) { value };
            return callback;
        }
        int64 main() {
            let number = make(7);
            let text = make("outer");
            assert(number(true) == 7);
            assert(text(1) == "outer");
            return 0;
        }''', ''),
        'generic_explicit_call': ('''int64 apply<F>(F operation) { return operation<int64>(3); }
        int64 main() {
            let f = fn<T>(T value) { value + 1 };
            assert(apply(f) == 4);
            return 0;
        }''', ''),
        'specialization_parameter_shadow': ('''int64 main() {
            int64 state = 5;
            let f = fn<T> copy[state](T value) { state + 1 };
            f<int64> = fn(int64 state) { state + 2 };
            assert(f(true) == 6);
            assert(f(3) == 5);
            return 0;
        }''', ''),
    })
    accepted.update({
        'comments': ('''int64 main() {
            let f = fn</* type */ T>(T x) {
                x // retain the return expression
            };
            assert(f(3) == 3);
            let g = f;
            assert(g<int64>(4) == 4);
            return 0;
        }''', ''),
        'default_type': ('''int64 main() {
            let f = fn<T, U = int64>(T x) { cast<U>(x) };
            assert(f(3) == 3);
            return 0;
        }''', ''),
        'mutable_reference': ('''int64 main() {
            let f = fn<T>(T mut& value) {
                value += 1;
                return value;
            };
            int64 value = 1;
            assert(f(&value) == 2);
            assert(value == 2);
            return 0;
        }''', ''),
    })
    accepted.update({
        'typed_struct': ('struct A<T> { T fn(T, int32) a; int32 fn(T, int32) b; };\nint32 add(int32 x, int32 c) { return x + c; }\nint32 mul(int32 x, int32 c) { return x * c; }\nint64 main() { A<int32> x; x.a = add; x.b = mul; println("{} {}", x.a(3, 4), x.b(3, 4)); return 0; }', '7 12'),
        'typed_local': ('int32 A<T>(int32 c) { T fn(T, int32) a; int32 fn(T, int32) b; a = fn(T x, int32 y) { x + cast<T>(y) }; b = int32 fn(T x, int32 y) { cast<int32>(x) * y }; println("{} {}", a(cast<T>(3), c), b(cast<T>(3), c)); return 0; } int64 main() { int32 c = 4; return A<int32>(c); }', '7 12'),
        'typed_enum_payload': ('enum A<T> { Pair(T fn(T, int32) a, int32 fn(T, int32) b) };\nint32 add(int32 x, int32 c) { return x + c; }\nint32 mul(int32 x, int32 c) { return x * c; }\nint64 main() { A<int32> x = A<int32>::Pair(add, mul); match(x) { Pair(a, b) => { println("{} {}", a(3, 4), b(3, 4)); } } return 0; }', '7 12'),
        'typed_opcode': ('opcode A { new=__std_bint_from_int; drop=__std_bint_drop; copy=__std_bint_copy; int32 fn(int32, int32) a; int32 fn(int32, int32) b; };\nint32 add(int32 x, int32 c) { return x + c; }\nint32 mul(int32 x, int32 c) { return x * c; }\nint64 main() { A x; x.a = add; x.b = mul; println("{} {}", x.a(3, 4), x.b(3, 4)); return 0; }', '7 12'),
        'typed_struct_capture': ('struct A<T> { T fn(T, int32) a; int32 fn(T, int32) b; };\nint64 main() { int32 state = 10; A<int32> x; x.a = int32 fn copy(int32 v, int32 c) { state += c; return v + state; }; let copied = x; int32 v = 3; int32 four = 4; int32 one = 1; println("{} {} {}", x.a(v, four), copied.a(v, one), x.a(v, one)); return 0; }', '17 14 18'),
        'typed_opcode_capture': ('opcode A { new=__std_bint_from_int; drop=__std_bint_drop; copy=__std_bint_copy; int32 fn(int32, int32) a; int32 fn(int32, int32) b; };\nint64 main() { int32 state = 10; A x = A::new(0); x.a = int32 fn copy(int32 v, int32 c) { state += c; return v + state; }; let copied = x; int32 v = 3; int32 four = 4; int32 one = 1; println("{} {} {}", x.a(v, four), copied.a(v, one), x.a(v, one)); return 0; }', '17 14 18'),
    })
    rejected = {
        'duplicate': '''void main() {
            let f = fn<T>(T x) { x };
            f<int64> = fn(int64 x) { x };
            f<int64> = fn(int64 x) { x + 1 };
        }''',
        'ambiguous': '''void main() {
            let f = fn<T, U>(T x, U y) { 0 };
            f<T, int64> = fn(T x, int64 y) { 1 };
            f<int64, U> = fn(int64 x, U y) { 2 };
            f(1, 2);
        }''',
        'conflicting_inference': '''void main() {
            let f = fn<T>(T x, T y) { x };
            f(1, true);
        }''',
        'missing_parameter': '''void main() {
            let f = fn<T, U>(T x) { x };
            f(1);
        }''',
        'moved_source': '''void main() {
            String text = "captured";
            let f = fn<T>(T x) { text.len() };
            let owner = String fn move[text]() { text };
            f(1);
        }''',
    }
    rejected['explicit_moved_source'] = rejected['moved_source'].replace('f(1);', 'f<int64>(1);')
    rejected['readonly_capture'] = '''void main() {
        int64 state = 0;
        let f = fn<T>(T value) { state += 1; return state; };
        f(1);
    }'''
    rejected.update({
        'typed_enum_members': 'enum A<T> { Empty, Some(T item); T fn(T, int32) a; int32 fn(T, int32) b; };\nint32 add(int32 x, int32 c) { return x + c; }\nint32 mul(int32 x, int32 c) { return x * c; }\nint64 main() { A<int32> x = A<int32>::Empty; x.a = add; x.b = mul; println("{} {}", x.a(3, 4), x.b(3, 4)); return 0; }',
        'typed_opcode_generic': 'opcode A<T> { new=__std_bint_from_int; drop=__std_bint_drop; copy=__std_bint_copy; T fn(T, int32) a; int32 fn(T, int32) b; };\nint32 add(int32 x, int32 c) { return x + c; }\nint32 mul(int32 x, int32 c) { return x * c; }\nint64 main() { A<int32> x; x.a = add; x.b = mul; println("{} {}", x.a(3, 4), x.b(3, 4)); return 0; }',
    })
    with tempfile.TemporaryDirectory(prefix='chali-generic-closures-') as directory:
        work = Path(directory)
        for name, (source, output) in accepted.items():
            path = work / f'{name}.chl'
            path.write_text(source, encoding='utf-8')
            result = subprocess.run([str(compiler), 'run', str(path), '--out', str(work / name)],
                                    capture_output=True, text=True, encoding='utf-8', errors='replace')
            assert result.returncode == 0, (name, result.stdout, result.stderr)
            assert result.stdout.strip() == output, (name, result.stdout)
        for name, source in rejected.items():
            path = work / f'{name}.chl'
            path.write_text(source, encoding='utf-8')
            result = subprocess.run([str(compiler), str(path), '--check'],
                                    capture_output=True, text=True, encoding='utf-8', errors='replace')
            assert result.returncode == 1, (name, result.stdout, result.stderr)
    print(f'泛型闭包：{len(accepted)} 个 VM 用例、{len(rejected)} 个拒绝用例通过。')


if __name__ == '__main__':
    main()
