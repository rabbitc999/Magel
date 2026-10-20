"""通过 VM 验证泛型闭包的类型推导、特化、捕获状态与结构体成员。"""
import argparse
from pathlib import Path
import subprocess
import tempfile


def main():
    """运行成功用例并要求非法的类型推导、特化与借用使用被编译器拒绝。"""
    parser = argparse.ArgumentParser()
    parser.add_argument('--magel', type=Path, default=Path('magel.exe'))
    compiler = parser.parse_args().magel.resolve()
    accepted = {
        'identity': ('''int64 main() {
            auto f = fn<T>(T x) { x };
            assert(f(3) == 3);
            assert(f("hello") == "hello");
            assert(f<int64>(4) == 4);
            return 0;
        }''', ''),
        'specialization': ('''int64 main() {
            auto f = fn<T, U>(T x, U y) { 1 };
            f<T, int64> = fn(T x, int64 y) { 2 };
            f<int64, int64> = fn(int64 x, int64 y) { 3 };
            assert(f(true, false) == 1);
            assert(f(true, 2) == 2);
            assert(f(1, 2) == 3);
            return 0;
        }''', ''),
        'shared_capture': ('''int64 main() {
            int64 state = 0;
            auto f = fn<T> copy[state](T x) {
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
            auto g = f;
            assert(g(3) == 22);
            assert(f(false) == 13);
            return 0;
        }''', ''),
        'member': ('''struct S {
            auto f = fn<T>(T x) { x };
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
            auto f = fn<T>(T x) { x + 1 };
            assert(apply(f) == 4);
            assert(boxed(f) == 6);
            auto plain = int64 fn(int64 x) { x * 2 };
            assert(apply(plain) == 6);
            return 0;
        }''', ''),
        'borrow': ('''int64 main() {
            int64 state = 1;
            auto f = fn<T> mut[state](T x) {
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
            auto f = fn<T, U>(T x, U y) { 0 };
            f<T, int64> = fn(T x, int64 y) { 1 };
            f<int64, U> = fn(int64 x, U y) { 2 };
            f<int64, int64> = fn(int64 x, int64 y) { 3 };
            assert(f(1, 2) == 3);
            return 0;
        }''', ''),
        'member_capture': ('''int64 initial = 4;
        struct S {
            auto f = fn<T> copy[initial](T x) {
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
            auto f = fn<T> mut[count](T x) { count += 1; };
            f(1);
            f(true);
            assert(count == 2);
            return 0;
        }''', ''),
        'generic_reference': ('''int64 apply<F>(F const& f) { return f(3); }
        int64 main() {
            int64 offset = 2;
            auto f = fn<T> copy[offset](T x) { x + offset };
            assert(apply(&f) == 5);
            return 0;
        }''', ''),
    })
    accepted.update({
        'outer_struct': ('''struct S<T> {
            auto f = fn<U>(T value, U ignored) { value };
        };
        int64 main() {
            S<int64> numbers;
            S<String> text;
            assert(numbers.f(7, true) == 7);
            assert(text.f("outer", 1) == "outer");
            return 0;
        }''', ''),
        'outer_function': ('''auto make<T>(T value) {
            auto callback = fn<U> copy[value](U ignored) { value };
            return callback;
        }
        int64 main() {
            auto number = make(7);
            auto text = make("outer");
            assert(number(true) == 7);
            assert(text(1) == "outer");
            return 0;
        }''', ''),
        'generic_explicit_call': ('''int64 apply<F>(F operation) { return operation<int64>(3); }
        int64 main() {
            auto f = fn<T>(T value) { value + 1 };
            assert(apply(f) == 4);
            return 0;
        }''', ''),
        'specialization_parameter_shadow': ('''int64 main() {
            int64 state = 5;
            auto f = fn<T> copy[state](T value) { state + 1 };
            f<int64> = fn(int64 state) { state + 2 };
            assert(f(true) == 6);
            assert(f(3) == 5);
            return 0;
        }''', ''),
    })
    accepted.update({
        'comments': ('''int64 main() {
            auto f = fn</* type */ T>(T x) {
                x // retain the return expression
            };
            assert(f(3) == 3);
            auto g = f;
            assert(g<int64>(4) == 4);
            return 0;
        }''', ''),
        'default_type': ('''int64 main() {
            auto f = fn<T, U = int64>(T x) { cast<U>(x) };
            assert(f(3) == 3);
            return 0;
        }''', ''),
        'mutable_reference': ('''int64 main() {
            auto f = fn<T>(T mut& value) {
                value += 1;
                return value;
            };
            int64 value = 1;
            assert(f(&value) == 2);
            assert(value == 2);
            return 0;
        }''', ''),
    })
    rejected = {
        'duplicate': '''void main() {
            auto f = fn<T>(T x) { x };
            f<int64> = fn(int64 x) { x };
            f<int64> = fn(int64 x) { x + 1 };
        }''',
        'ambiguous': '''void main() {
            auto f = fn<T, U>(T x, U y) { 0 };
            f<T, int64> = fn(T x, int64 y) { 1 };
            f<int64, U> = fn(int64 x, U y) { 2 };
            f(1, 2);
        }''',
        'conflicting_inference': '''void main() {
            auto f = fn<T>(T x, T y) { x };
            f(1, true);
        }''',
        'missing_parameter': '''void main() {
            auto f = fn<T, U>(T x) { x };
            f(1);
        }''',
        'moved_source': '''void main() {
            String text = "captured";
            auto f = fn<T>(T x) { text.len() };
            auto owner = String fn move[text]() { text };
            f(1);
        }''',
    }
    rejected['explicit_moved_source'] = rejected['moved_source'].replace('f(1);', 'f<int64>(1);')
    rejected['readonly_capture'] = '''void main() {
        int64 state = 0;
        auto f = fn<T>(T value) { state += 1; return state; };
        f(1);
    }'''
    with tempfile.TemporaryDirectory(prefix='magel-generic-closures-') as directory:
        work = Path(directory)
        for name, (source, output) in accepted.items():
            path = work / f'{name}.mgl'
            path.write_text(source, encoding='utf-8')
            result = subprocess.run([str(compiler), 'run', str(path), '--out', str(work / name)],
                                    capture_output=True, text=True, encoding='utf-8', errors='replace')
            assert result.returncode == 0, (name, result.stdout, result.stderr)
            assert result.stdout.strip() == output, (name, result.stdout)
        for name, source in rejected.items():
            path = work / f'{name}.mgl'
            path.write_text(source, encoding='utf-8')
            result = subprocess.run([str(compiler), str(path), '--check'],
                                    capture_output=True, text=True, encoding='utf-8', errors='replace')
            assert result.returncode == 1, (name, result.stdout, result.stderr)
    print(f'泛型闭包：{len(accepted)} 个 VM 用例、{len(rejected)} 个拒绝用例通过。')


if __name__ == '__main__':
    main()
