"""验证 lambda 回调上下文、引用、资源所有权与错误拒绝，不依赖第三方包。"""
from pathlib import Path
import argparse
import os
import subprocess

ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / "build/magel-lambdas"
EXE = ".exe" if os.name == "nt" else ""


def execute(*args):
    """执行编译器或程序并收集完整输出。"""
    env = os.environ.copy(); env['MAGEL_LANG'] = 'zh'
    return subprocess.run([str(arg) for arg in args], cwd=ROOT, capture_output=True, env=env,
                          encoding="utf-8", errors="replace", timeout=180)


def main():
    """运行正常样例的两个后端以及签名和推导拒绝场景。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--magel", type=Path, default=ROOT / ("magel" + EXE))
    parser.add_argument("--cxx", default="g++")
    parser.add_argument("--vm", type=Path, help="可选的 StackVM 可执行文件，用于二进制加载回归")
    args = parser.parse_args()
    compiler = args.magel.resolve()
    WORK.mkdir(parents=True, exist_ok=True)
    unit = WORK / ("lambda-unit" + EXE)
    result = execute(args.cxx, "-std=c++17", "-I" + str(ROOT),
                     "-I" + str(ROOT / "source"), "-I" + str(ROOT / "library"), "-I" + str(ROOT / "library/preprocess"), ROOT / "tests/magel_lambda_test.cpp", "-o", unit)
    assert result.returncode == 0, result.stderr
    result = execute(unit)
    assert result.returncode == 0 and result.stdout == "lambda unit tests OK\n" and not result.stderr, result
    source = ROOT / "example/samples/lambda_callbacks.mgl"
    expected = "lambda callbacks OK\n"
    result = execute(compiler, "run", source, "--out", WORK / "interpreted")
    assert result.returncode == 0 and result.stdout == expected and not result.stderr, result
    executable = WORK / ("callbacks" + EXE)
    result = execute(compiler, "aot", source, "--out", executable, "--opt-level=0")
    assert result.returncode == 0, result.stderr
    result = execute(executable)
    assert result.returncode == 0 and result.stdout == expected and not result.stderr, result
    captures = ROOT / "example/samples/lambda_captures.mgl"
    result = execute(compiler, "run", captures, "--out", WORK / "captures-interpreted")
    assert result.returncode == 0 and result.stdout == "lambda captures OK\n" and not result.stderr, result
    captured_executable = WORK / ("captures" + EXE)
    result = execute(compiler, "aot", captures, "--out", captured_executable, "--opt-level=0")
    assert result.returncode == 0, result.stderr
    result = execute(captured_executable)
    assert result.returncode == 0 and result.stdout == "lambda captures OK\n" and not result.stderr, result
    modes = ROOT / "tests/fixtures/lambda_capture_modes.mgl"
    for backend in ("run", "aot"):
        executable = WORK / ("modes-" + backend + EXE)
        result = execute(compiler, backend, modes, "--out", executable)
        assert result.returncode == 0, result
        if backend == "aot": result = execute(executable)
        assert result.returncode == 0 and result.stdout == "lambda capture modes OK\n" and not result.stderr, result
    if args.vm:
        prefix = WORK / "captures-binary"
        result = execute(compiler, "svmb", captures, "--out", prefix)
        assert result.returncode == 0, result.stderr
        result = execute(args.vm.resolve(), "run", prefix.with_suffix(".svmb"),
                         "--data", prefix.with_suffix(".data.bin"), "--layout", prefix.with_suffix(".layout"))
        assert result.returncode == 0 and result.stdout.partition("\n\n;")[0] == "lambda captures OK" and not result.stderr, result
        assert "没释放" not in result.stdout, result
    (WORK / "callbacks_module.mgl").write_text(
        "module callbacks_module;\nimport std::iter;\n"
        "int64 offset=5; int64 adjust(int64 value){return value+offset;}\n"
        "int64 compute(){return std::iter::range(1,4).map(fn(value){adjust(value)})"
        ".fold(0,fn(acc,value){acc+value});}\n", encoding="utf-8")
    source = WORK / "module_main.mgl"
    source.write_text("import callbacks_module;\nint64 main(){std::lang::println(\"{}\",callbacks_module::compute());return 0;}\n",
                      encoding="utf-8")
    result = execute(compiler, "run", source, "--out", WORK / "module-interpret")
    assert result.returncode == 0 and result.stdout == "21\n" and not result.stderr, result
    executable = WORK / ("module-aot" + EXE)
    result = execute(compiler, "aot", source, "--out", executable, "--opt-level=0")
    assert result.returncode == 0, result.stderr
    result = execute(executable)
    assert result.returncode == 0 and result.stdout == "21\n" and not result.stderr, result
    successes = {
        "const_capture_read": "const int64 n=1;auto f=int64 fn(){n};return f();",
        "readonly_capture_read": "int64 n=1;int64 const& r=&n;auto f=int64 fn(){r};return f();",
        "readonly_match_read": "match(Option<int64>::Value(10)){Option<int64>::Value(n)=>{auto f=int64 fn(){n};return f();} Option<int64>::None=>{return 0;}} return 0;",
        "local_capture": "int64 bias=10; auto f=fn(int64 x){x+bias}; return f(1);",
        "inferred_capture": "int64 bias=10; return apply(fn(x){x+bias},1);",
        "global_shadow": "int64 x=10; return apply(fn(value){value+x},1);",
        "destructure_shadow": "auto (x,y)=(10,20); auto f=fn(){x}; return f();",
        "destructure_callback": "auto (x,y)=(10,20); return apply(fn(value){value+x},1);",
    }
    successes.update({"reference_capture": "int64 value=1; int64 mut& ref=&value; auto f=fn(){ref}; return f();",})
    successes.update({"mutate_capture": "int64 value=1; auto f=int64 fn mut(){value+=1;return value;}; return f();",})
    successes.update({"aggregate_reference_capture": "int64 x=1;auto pair=(&x,2);auto f=fn(){pair.1};return f();",})
    successes.update({"reference_list_capture": "int64 value=1; List<int64 mut&> refs;refs.push(&value);auto f=fn(){refs.len()};return f();",})
    successes.update({"mutate_aggregate_capture": "auto pair=(1,2);auto f=int64 fn mut(){pair=(3,4);return pair.0;};return f();",})
    for name, body in successes.items():
        source = WORK / (name + '.mgl')
        source.write_text('int64 x=100; int64 apply(int64 fn(int64) cb,int64 x){return cb(x);}'
                          'int64 main(){' + body + '}\n', encoding='utf-8')
        result = execute(compiler, '--check', source)
        assert result.returncode == 0 and not result.stderr, (name, result)
    failures = {
        "default_capture_write": "int64 n=1;auto f=int64 fn(){n+=1;return n;};return f();",
        "const_capture_write": "int64 n=1;auto f=int64 fn const(){n=2;return n;};return f();",
        "const_capture_method": "List<int64> values;auto f=void fn(){values.push(2);};f();return 0;",
        "default_reference_write": "int64 n=1;int64 mut& r=&n;auto f=void fn(){r=2;};f();return 0;",
        "mixed_unlisted_write": "int64 n=1;int64 m=2;auto f=void fn copy[n](){m=3;};f();return 0;",
        "capture_ambiguous_default": "int64 a=1;int64 b=2;auto f=int64 fn copy[a,b] mut const(int64 x){a+b+x};return f(1);",
        "capture_ambiguous_default_reverse": "int64 a=1;auto f=int64 fn copy[a] const mut(int64 x){a+x};return f(1);",
        "capture_ambiguous_groups": "int64 a=1;int64 b=2;auto f=int64 fn copy[a,b] mut[a](int64 x){a+b+x};return f(1);",
        "capture_duplicate_same_mode": "int64 n=1;auto f=int64 fn copy[n,n](){n};return f();",
        "capture_duplicate": "int64 n=1;auto f=int64 fn copy[n] mut[n](){n};return f();",
        "capture_unknown": "auto f=int64 fn copy[missing](){1};return f();",
        "capture_parameter_conflict": "int64 n=1;auto f=int64 fn copy[n](int64 n){n};return f(1);",
        "move_pointer_read": "int64 n=2;int64 mut& r=&n;auto f=int64 fn move[r](){r};return r;",
        "move_read": "int64 n=1;auto f=int64 fn move(){n};return n;",
        "move_recapture": "int64 n=1;auto f=int64 fn move(){n};auto g=int64 fn copy(){n};return g();",
        "move_unused_list": "int64 n=1;auto f=int64 fn move[n](){2};return n;",
        "move_const_capture": "const int64 n=1;auto f=int64 fn move(){n};return f();",
        "readonly_match_capture": "match(Option<int64>::Value(10)){Option<int64>::Value(x)=>{auto f=int64 fn mut(){x};return f();} Option<int64>::None=>{return 0;}} return 0;",
        "arity": "return apply(fn(x,y) { x+y }, 1);",
        "typed_mismatch": "return apply(bool fn(bool x) { x }, 1);",
        "result_mismatch": "return apply(bool fn(int64 x) { true }, 1);",
        "missing_return": "auto f = fn(int64 x) { int64 y = x; }; return 0;",
        "empty_auto": "auto f = fn() {}; return 0;",
        "unknown": "return apply(fn(x) { unavailable+x }, 1);",
    }
    for name, body in failures.items():
        source = WORK / (name + ".mgl")
        source.write_text("int64 x=100;\nint64 apply(int64 fn(int64) cb,int64 x){return cb(x);}\n"
                          "int64 main(){" + body + "}\n", encoding="utf-8")
        result = execute(compiler, "--check", source)
        assert result.returncode != 0 and result.stderr, (name, result)
        assert "terminate called" not in result.stderr and "Segmentation" not in result.stderr, (name, result)
        if "capture" in name:
            assert "捕获" in result.stderr or "只读" in result.stderr or "const" in result.stderr or "MGL1205" in result.stderr, (name, result)
    readonly = WORK / "readonly.mgl"
    rejected_sources = {
        "copy_reference_escape": "int64 fn() bad(){int64 n=1;int64 mut& r=&n;return int64 fn copy(){r};}int64 main(){return 0;}",
        "move_reference_escape": "int64 fn() bad(){int64 n=1;int64 mut& r=&n;return int64 fn move(){r};}int64 main(){return 0;}",
        "event_capture_escape": "import std::event;\nint64 main(){int64 value=1;Timer timer=Timer::everyNanos(1);timer.onTick(void fn mut(int64 source){value+=source;});return 0;}",
        "interrupt_field_capture_escape": "import std::event;\nint64 main(){int64 value=1;Interrupt interrupt;interrupt.callback=void fn mut(int64 source){value+=source;};return 0;}",
        "event_binding_capture_escape": "import std::event;\nint64 main(){int64 value=1;Timer timer=Timer::everyNanos(1);std::event::bindEventArguments(timer.handle,void fn mut(int64 source,std::lang::List<std::lang::Any> args){value+=source;});return 0;}",
        "const_capture": "int64 main(){const int64 value=1;auto f=fn mut(){value};return f();}",
        "readonly_capture": "int64 main(){int64 number=1;int64 const& value=&number;auto f=fn mut(){value};return f();}",
        "local_capture_escape": "int64 fn() bad(){int64 value=1;return fn(){value};}int64 main(){return 0;}",
        "parameter_capture_escape": "int64 fn() bad(int64 value){return fn(){value};}int64 main(){return 0;}",
        "scope_capture_escape": "int64 main(){int64 fn() f=fn(){1};{int64 value=2;f=fn(){value};}return f();}",
        "aggregate_capture_escape": "struct Holder{int64 fn() f;};Holder bad(){int64 value=1;Holder h;h.f=fn(){value};return h;}int64 main(){return 0;}",
    }
    for name, text in rejected_sources.items():
        source = WORK / (name + ".mgl")
        source.write_text(text, encoding="utf-8")
        result = execute(compiler, "--check", source)
        assert result.returncode != 0 and any(word in result.stderr for word in ("捕获", "只读", "const", "生命周期", "局部")), (name, result)
    readonly.write_text("import std::iter;\nint64 main(){std::lang::List<int64> values;values.push(1);"
                        "values.iter().forEach(void fn(int64 const& value){value=2;});return 0;}\n", encoding="utf-8")
    result = execute(compiler, "--check", readonly)
    assert result.returncode != 0 and "只读" in result.stderr, result
    print(f"lambda 验证通过：提升器单测、组合及模块样例解释/AOT、{len(failures)+len(rejected_sources)+1} 个拒绝场景")


if __name__ == "__main__":
    main()
