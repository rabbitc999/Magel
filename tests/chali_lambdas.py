"""验证 lambda 回调上下文、引用、资源所有权与错误拒绝，不依赖第三方包。"""
from pathlib import Path
import argparse
import os
import subprocess
ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / 'build/chali-lambdas'
EXE = '.exe' if os.name == 'nt' else ''

def execute(*args):
    """执行编译器或程序并收集完整输出。"""
    env = os.environ.copy()
    env['CHALI_LANG'] = 'zh'
    return subprocess.run([str(arg) for arg in args], cwd=ROOT, capture_output=True, env=env, encoding='utf-8', errors='replace', timeout=180)

def main():
    """运行 VM、独立字节码加载及签名和推导拒绝场景。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--chali', type=Path, default=ROOT / ('chali' + EXE))
    parser.add_argument('--cxx', default='g++')
    parser.add_argument('--vm', type=Path, help='可选的 StackVM 可执行文件，用于二进制加载回归')
    args = parser.parse_args()
    compiler = args.chali.resolve()
    WORK.mkdir(parents=True, exist_ok=True)
    unit = WORK / ('lambda-unit' + EXE)
    result = execute(args.cxx, '-std=c++17', '-I' + str(ROOT), '-I' + str(ROOT / 'source'), '-I' + str(ROOT / 'library'), '-I' + str(ROOT / 'library/preprocess'), ROOT / 'tests/chali_lambda_test.cpp', *[ROOT / 'source' / name for name in ('chali_generic.cpp', 'chali_lambda.cpp', 'chali_generic_closure.cpp', 'syntax/source_text.cpp', 'syntax/constant_expression.cpp')], '-o', unit)
    assert result.returncode == 0, result.stderr
    result = execute(unit)
    assert result.returncode == 0 and result.stdout == 'lambda unit tests OK\n' and (not result.stderr), result
    source = ROOT / 'example/samples/lambda_callbacks.chl'
    expected = (ROOT / 'example/expected/lambda_callbacks.txt').read_text(encoding='utf-8')
    result = execute(compiler, 'run', source, '--out', WORK / 'interpreted')
    assert result.returncode == 0 and result.stdout == expected and (not result.stderr), result
    executable = WORK / ('callbacks' + EXE)
    captures = ROOT / 'example/samples/lambda_captures.chl'
    result = execute(compiler, 'run', captures, '--out', WORK / 'captures-interpreted')
    assert result.returncode == 0 and result.stdout == (ROOT / 'example/expected/lambda_captures.txt').read_text(encoding='utf-8') and (not result.stderr), result
    captured_executable = WORK / ('captures' + EXE)
    modes = ROOT / 'tests/fixtures/lambda_capture_modes.chl'
    for backend in ('run',):
        executable = WORK / ('modes-' + backend + EXE)
        result = execute(compiler, backend, modes, '--out', executable)
        assert result.returncode == 0, result
        assert result.returncode == 0 and result.stdout == 'fn capture modes OK\n' and (not result.stderr), result
    if args.vm:
        prefix = WORK / 'captures-binary'
        result = execute(compiler, 'svmb', captures, '--out', prefix)
        assert result.returncode == 0, result.stderr
        result = execute(args.vm.resolve(), 'run', prefix.with_suffix('.svmb'), '--data', prefix.with_suffix('.data.bin'), '--layout', prefix.with_suffix('.layout'))
        assert result.returncode == 0 and result.stdout.partition('\n\n;')[0] == (ROOT / 'example/expected/lambda_captures.txt').read_text(encoding='utf-8').rstrip('\n') and (not result.stderr), result
        assert '没释放' not in result.stdout, result
    (WORK / 'callbacks_module.chl').write_text('module callbacks_module;\nimport std::iter;\nint64 offset=5; int64 adjust(int64 value){return value+offset;}\nint64 compute(){return std::iter::range(1,4).map(fn(value){adjust(value)}).fold(0,fn(acc,value){acc+value});}\n', encoding='utf-8')
    source = WORK / 'module_main.chl'
    source.write_text('import callbacks_module;\nvoid main(){std::lang::println("{}",callbacks_module::compute());return;}\n', encoding='utf-8')
    result = execute(compiler, 'run', source, '--out', WORK / 'module-interpret')
    assert result.returncode == 0 and result.stdout == '21\n' and (not result.stderr), result
    successes = {'const_capture_read': 'const int64 n=1;let f=int64 fn(){n};let __boom = f();', 'readonly_capture_read': 'int64 n=1;int64 const& r=&n;let f=int64 fn(){r};let __boom = f();', 'readonly_match_read': 'match(Option<int64>::Value(10)){Option<int64>::Value(n)=>{let f=int64 fn(){n};let __boom = f();} Option<int64>::None=>{}}', 'local_capture': 'int64 bias=10; let f=fn(int64 x){x+bias}; let __boom = f(1);', 'inferred_capture': 'int64 bias=10; let __boom = apply(fn(x){x+bias},1);', 'global_shadow': 'int64 x=10; let __boom = apply(fn(value){value+x},1);', 'destructure_shadow': 'let (x,y)=(10,20); let f=fn(){x}; let __boom = f();', 'destructure_callback': 'let (x,y)=(10,20); let __boom = apply(fn(value){value+x},1);'}
    successes.update({'reference_capture': 'int64 value=1; int64 mut& ref=&value; let f=fn(){ref}; let __boom = f();'})
    successes.update({'mutate_capture': 'int64 value=1; let f=int64 fn mut(){value+=1;return value;}; let __boom = f();'})
    successes.update({'aggregate_reference_capture': 'int64 x=1;let pair=(&x,2);let f=fn(){pair.1};let __boom = f();'})
    successes.update({'reference_list_capture': 'int64 value=1; List<int64 mut&> refs;refs.push(&value);let f=fn(){refs.len()};let __boom = f();'})
    successes.update({'mutate_aggregate_capture': 'let pair=(1,2);let f=int64 fn mut(){pair=(3,4);return pair.0;};let __boom = f();'})
    for name, body in successes.items():
        source = WORK / (name + '.chl')
        source.write_text('int64 x=100; int64 apply(int64 fn(int64) cb,int64 x){return cb(x);}void main(){' + body + '}\n', encoding='utf-8')
        result = execute(compiler, '--check', source)
        assert result.returncode == 0 and (not result.stderr), (name, result)
    failures = {'default_capture_write': 'int64 n=1;let f=int64 fn(){n+=1;return n;};let __boom = f();', 'const_capture_write': 'int64 n=1;let f=int64 fn const(){n=2;return n;};let __boom = f();', 'const_capture_method': 'List<int64> values;let f=void fn(){values.push(2);};f();', 'default_reference_write': 'int64 n=1;int64 mut& r=&n;let f=void fn(){r=2;};f();', 'mixed_unlisted_write': 'int64 n=1;int64 m=2;let f=void fn copy[n](){m=3;};f();', 'capture_ambiguous_default': 'int64 a=1;int64 b=2;let f=int64 fn copy[a,b] mut const(int64 x){a+b+x};let __boom = f(1);', 'capture_ambiguous_default_reverse': 'int64 a=1;let f=int64 fn copy[a] const mut(int64 x){a+x};let __boom = f(1);', 'capture_ambiguous_groups': 'int64 a=1;int64 b=2;let f=int64 fn copy[a,b] mut[a](int64 x){a+b+x};let __boom = f(1);', 'capture_duplicate_same_mode': 'int64 n=1;let f=int64 fn copy[n,n](){n};let __boom = f();', 'capture_duplicate': 'int64 n=1;let f=int64 fn copy[n] mut[n](){n};let __boom = f();', 'capture_unknown': 'let f=int64 fn copy[missing](){1};let __boom = f();', 'capture_parameter_conflict': 'int64 n=1;let f=int64 fn copy[n](int64 n){n};let __boom = f(1);', 'move_pointer_read': 'int64 n=2;int64 mut& r=&n;let f=int64 fn move[r](){r};let __boom = r;', 'move_read': 'int64 n=1;let f=int64 fn move(){n};let __boom = n;', 'move_recapture': 'int64 n=1;let f=int64 fn move(){n};let g=int64 fn copy(){n};let __boom = g();', 'move_unused_list': 'int64 n=1;let f=int64 fn move[n](){2};let __boom = n;', 'move_const_capture': 'const int64 n=1;let f=int64 fn move(){n};let __boom = f();', 'readonly_match_capture': 'match(Option<int64>::Value(10)){Option<int64>::Value(x)=>{let f=int64 fn mut(){x};let __boom = f();} Option<int64>::None=>{}}', 'arity': 'let __boom = apply(fn(x,y) { x+y }, 1);', 'typed_mismatch': 'let __boom = apply(bool fn(bool x) { x }, 1);', 'result_mismatch': 'let __boom = apply(bool fn(int64 x) { true }, 1);', 'missing_return': 'let f = fn(int64 x) { int64 y = x; };', 'empty_auto': 'let f = fn() {};', 'unknown': 'return apply(fn(x) { unavailable+x }, 1);'}
    for name, body in failures.items():
        source = WORK / (name + '.chl')
        source.write_text('int64 x=100;\nint64 apply(int64 fn(int64) cb,int64 x){return cb(x);}\nvoid main(){' + body + '}\n', encoding='utf-8')
        result = execute(compiler, '--check', source)
        assert result.returncode != 0 and result.stderr, (name, result)
        assert 'terminate called' not in result.stderr and 'Segmentation' not in result.stderr, (name, result)
        if 'capture' in name:
            assert '捕获' in result.stderr or '只读' in result.stderr or 'const' in result.stderr or ('CHL1205' in result.stderr), (name, result)
    readonly = WORK / 'readonly.chl'
    rejected_sources = {'copy_reference_escape': 'int64 fn() bad(){int64 n=1;int64 mut& r=&n;return int64 fn copy(){r};}void main(){}', 'move_reference_escape': 'int64 fn() bad(){int64 n=1;int64 mut& r=&n;return int64 fn move(){r};}void main(){}', 'const_capture': 'void main(){const int64 value=1;let f=fn mut(){value};let __boom = f();}', 'readonly_capture': 'void main(){int64 number=1;int64 const& value=&number;let f=fn mut(){value};let __boom = f();}', 'local_capture_escape': 'int64 fn() bad(){int64 value=1;return fn(){value};}void main(){}', 'parameter_capture_escape': 'int64 fn() bad(int64 value){return fn(){value};}void main(){}', 'scope_capture_escape': 'void main(){int64 fn() f=fn(){1};{int64 value=2;f=fn(){value};}let __boom = f();}', 'aggregate_capture_escape': 'struct Holder{int64 fn() f;};Holder bad(){int64 value=1;Holder h;h.f=fn(){value};return h;}void main(){}'}
    for name, text in rejected_sources.items():
        source = WORK / (name + '.chl')
        source.write_text(text, encoding='utf-8')
        result = execute(compiler, '--check', source)
        assert result.returncode != 0 and any((word in result.stderr for word in ('捕获', '只读', 'const', '生命周期', '局部'))), (name, result)
    readonly.write_text('import std::iter;\nvoid main(){std::lang::List<int64> values;values.push(1);values.iter().forEach(void fn(int64 const& value){value=2;});return;}\n', encoding='utf-8')
    result = execute(compiler, '--check', readonly)
    assert result.returncode != 0 and '只读' in result.stderr, result
if __name__ == '__main__':
    main()
