"""验证匿名 fn 类型、旧词法删除、组合语法与三种执行路径。"""
from pathlib import Path
import argparse
import os
import subprocess

ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / "build/magel-function-types"
EXE = ".exe" if os.name == "nt" else ""


def execute(*args):
    """执行工具并捕获输出，固定中文诊断便于回归比较。"""
    env = dict(os.environ, MAGEL_LANG="zh")
    return subprocess.run([str(x) for x in args], cwd=ROOT, env=env,
                          capture_output=True, encoding="utf-8", errors="replace", timeout=240)


def main():
    """验证声明、嵌套签名、泛型推导、引用及不兼容代码的拒绝。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--magel", type=Path, default=ROOT / ("magel" + EXE))
    parser.add_argument("--vm", type=Path)
    args = parser.parse_args()
    compiler = args.magel.resolve()
    WORK.mkdir(parents=True, exist_ok=True)
    unit = WORK / ("unit" + EXE)
    result = execute("g++", "-std=c++17", "-I" + str(ROOT),
                     "-I" + str(ROOT / "source"), "-I" + str(ROOT / "library"),
                     "-I" + str(ROOT / "library/preprocess"), ROOT / "tests/magel_function_types_test.cpp", "-o", unit)
    assert result.returncode == 0, result.stderr
    result = execute(unit)
    assert result.returncode == 0 and result.stdout == "function type unit tests OK\n" and not result.stderr, result
    source = WORK / "combinations.mgl"
    source.write_text('''
import std::iter;
struct Holder<T> { T value; };
struct Handler { int64 fn(int64) callback; };
struct CallbackFactoryHolder<T> { (T fn(T)) fn(T fn(T),T) factory; };
enum Action { Empty, Run(int64 fn(int64) callback); };
int64 add(int64 value) { return value + 1; }
int64 fptr(int64 value) { return value + 2; }
struct fptrdef { int64 value; };
int64 apply(int64 fn(int64) callback, int64 value) { return callback(value); }
T applyGeneric<T>(T fn(T) callback, T value) { return callback(value); }
T identity<T>(T value) { return value; }
T fn(T) chooseCallback<T>(T fn(T) callback,T value) { callback(value);return callback; }
T invokeFactory<T>((T fn(T)) fn(T fn(T),T) factory,T fn(T) callback,T value) {
    T fn(T) selected=factory(callback,value);return selected(value);
}
String decorate(String value) { return value+"!"; }
int64 borrowCallback((int64 fn(int64)) const& operation,int64 value){return operation(value);}
T fn(T) makeGeneric<T>(T mut& bias) { return fn(T value) { value + bias }; }
int64 fn(int64) make(int64 mut& bias) { return fn(int64 value) { value + bias }; }
int64 nested(int64 fn(int64 fn(int64), int64) callback) { return callback(add, 7); }
int64 const& borrow(int64 const& value) { return value; }
int64 sum(int64... values) { int64 total=0; for(int64 x in values){total=total+x;} return total; }
int64[] numbers(){int64 values[]=new[2];values[0]=7;values[1]=8;return values;}
int64 main() {
    auto composeCallbacks = (int64 fn(int64)) fn(int64 fn(int64) a, int64 fn(int64) b) {
        int64 fn copy(int64 x) { a(b(x)) }
    };
    auto composed = composeCallbacks(add, fptr);
    assert(composed(5) == 8);
    auto composedCopy = composed;
    assert(composedCopy(7) == 10);
    auto inferredComposition = composeCallbacks(fn(x){x*5},fn(x){x-1});
    assert(inferredComposition(5) == 20);
    (int64 fn(int64)) fn(int64 fn(int64),int64) nestedFactory=chooseCallback<int64>;
    assert(nestedFactory(add,41)(9)==10);
    assert(invokeFactory<int64>(nestedFactory,add,9)==10);
    assert(invokeFactory(nestedFactory,add,9)==10);
    auto inferredCallback=chooseCallback(add,9);assert(inferredCallback(9)==10);
    CallbackFactoryHolder<int64> factoryHolder;factoryHolder.factory=chooseCallback<int64>;
    assert(factoryHolder.factory(add,41)(9)==10);
    Holder<(int64 fn(int64)) fn(int64 fn(int64),int64)> compoundHolder;
    compoundHolder.value=nestedFactory;assert(compoundHolder.value(add,41)(9)==10);
    (String fn(String)) fn(String fn(String),String) textFactory=chooseCallback<String>;
    assert(invokeFactory(textFactory,decorate,"ok")=="ok!");
    (int64[]) fn() factory=numbers;auto returnedArray=factory();assert(returnedArray[0]==7&&returnedArray[1]==8);
    int64 bias10=10,bias20=20,bias30=30,bias40=40,bias50=50,bias60=60,bias70=70,bias80=80,bias85=85,bias90=90,bias95=95;
    int64 fn(int64,int64) combine=int64 fn(int64 left,int64 right){left+right};
    assert(combine(2,3)==5);
    void fn() noOperation=void fn(){};noOperation();
    int64 fn(int64) callback = add;
    assert(callback(4)==5 && apply(callback,5)==6);
    assert(nested(apply)==8 && applyGeneric(fn(x){x+3},4)==7);
    auto higher=fn(int64 fn(int64) operation,int64 x){operation(x)};
    assert(higher(add,8)==9);
    auto captured=make(&bias10); assert(captured(3)==13);
    auto inferred=identity(captured); assert(inferred(3)==13);
    auto specialized=makeGeneric<int64>(&bias20); assert(specialized(2)==22);
    Handler handler; handler.callback=captured; assert(handler.callback(5)==15);
    int64 fn(int64) table[2]; table[0]=add;table[1]=captured;
    assert(table[0](1)==2 && table[1](1)==11);
    int64 fn(int64) dynamic[]=new[2]; dynamic[0]=make(&bias90);dynamic[1]=add;
    assert(dynamic[0](2)==92 && dynamic[1](2)==3);
    Holder<int64 fn(int64)> box; box.value=make(&bias30); assert(box.value(2)==32);
    List<int64 fn(int64)> callbacks; callbacks.push(make(&bias40)); callbacks.push(add);
    assert(callbacks[0](2)==42 && callbacks[1](2)==3);
    Map<int64,int64 fn(int64)> dispatch; dispatch.put(1,make(&bias50));
    assert(dispatch[1](2)==52 && make(&bias60)(3)==63);
    bool skipped=false && make(&bias70)(2)==72; assert(!skipped);
    Option<int64 fn(int64)> optional=Option<int64 fn(int64)>::Value(captured);
    auto extracted=optional.unwrap(); assert(extracted(4)==14);
    Action action=Action::Run(captured); Action copy=action;
    match(copy){Action::Run(operation)=>{assert(operation(8)==18);} Action::Empty=>{assert(false);}}
    (int64 fn(int64),String) pair=(captured,"ok");
    auto first=pair.0; assert(first(6)==16 && pair.1=="ok");
    int64 value=9;
    int64 & fn(int64 &) reader=borrow;
    int64 const& read=reader(&value); assert(read==9);
    (int64 fn(int64)) mut& writable=&callback;
    (int64 fn(int64)) & readonly=&callback;
    writable=make(&bias80); assert(callback(3)==83 && readonly(4)==84);
    int64 fn(int64) snapshot=readonly;
    writable=add; assert(callback(3)==4 && snapshot(3)==83);
    assert(borrowCallback(&callback,5)==6);
    writable=make(&bias85);
    List<(int64 fn(int64)) const&> borrowedFunctions; borrowedFunctions.push(&callback);
    assert(borrowedFunctions[0](5)==90 && callback(1)==86);
    (int64 fn(int64)) mut& arrayReference=&table[0];arrayReference=make(&bias95);
    assert(table[0](2)==97);
    int64 fn(int64...) variadic=sum; assert(variadic(1,2,3)==6);
    fptrdef ordinary; ordinary.value=12; int64 fptrdef=3;
    assert(fptr(2)==4 && ordinary.value==12 && fptrdef==3);
    assert(__typeTag(String fn(String))==__typeTag(std::lang::String fn(std::lang::String)));
    assert(__typeTag(int64 fn(int64))==__typeTag(int64 fn(int64)));
    assert(std::iter::range(1,4).map(captured).fold(0,fn(acc,x){acc+x})==36);
    std::lang::println("function types OK"); return 0;
}
''', encoding="utf-8")
    expected = "function types OK\n"
    for backend in ("run", "aot"):
        target = WORK / (backend + EXE)
        result = execute(compiler, backend, source, "--out", target)
        assert result.returncode == 0, (backend, result.stderr)
        if backend == "aot":
            result = execute(target)
        assert result.returncode == 0 and result.stdout == expected and not result.stderr, result
    if args.vm:
        prefix = WORK / "binary"
        result = execute(compiler, "svmb", source, "--out", prefix)
        assert result.returncode == 0, result.stderr
        result = execute(args.vm.resolve(), "run", prefix.with_suffix(".svmb"),
                         "--data", prefix.with_suffix(".data.bin"), "--layout", prefix.with_suffix(".layout"))
        assert result.returncode == 0 and result.stdout.partition("\n\n;")[0] == expected.rstrip() and not result.stderr, result
        assert "没释放" not in result.stdout, result
    for name, text in {
        "legacy-function": "int64 main(){fn(int64)->int64 f;return 0;}",
        "legacy-typed-lambda": "int64 main(){auto f=fn(int64 x)->int64{x+1};return f(1);}",
        "legacy-inferred-lambda": "int64 main(){auto f=fn(x)->auto{x+1};return f(1);}",
        "legacy-nested-function": "struct Box<T>{T value;};Box<fn(int64)->int64> value;int64 main(){return 0;}",
    }.items():
        legacy = WORK / (name + ".mgl")
        legacy.write_text(text, encoding="utf-8")
        result = execute(compiler, "--check", legacy)
        assert result.returncode != 0, (name, result)
    errors = {
        "composition_parameter_escape": "void bad(){auto combine=(int64 fn(int64)) fn(int64 fn(int64) a,int64 fn(int64) b){int64 fn(int64 x){a(b(x))}};}",
        "nested_factory_signature": "int64 add(int64 x){return x+1;}int64 fn(int64) choose(int64 fn(int64) callback,int64 value){return callback;}(float64 fn(float64)) fn(int64 fn(int64),int64) bad=choose;",
        "old_declaration": "fptr callback(int64 value) -> int64;",
        "old_alias": "fptrdef Callback(int64 value) -> int64;",
        "named_parameter": "int64 fn(int64 value) callback;",
        "missing_arrow": "fn(int64) int64 callback;",
        "missing_return": "callback fn(int64);",
        "variadic_not_last": "int64 fn(int64..., int64) callback;",
        "signature_mismatch": "int64 add(int64 x){return x;} int64 fn(float64) callback=add;",
        "parameter_signature": "int64 add(int64 x){return x;} int64 apply(int64 fn(float64) callback){return 0;} int64 bad(){return apply(add);}",
        "return_signature": "int64 add(int64 x){return x;} int64 fn(float64) bad(){return add;}",
        "field_signature": "int64 add(int64 x){return x;} struct S{int64 fn(float64) callback;}; void bad(){S s;s.callback=add;}",
        "variadic_signature": "int64 add(int64 x){return x;} int64 fn(int64...) callback=add;",
        "readonly_function_reference": "int64 add(int64 x){return x;} void bad(){int64 fn(int64) callback=add;(int64 fn(int64)) const& ref=&callback;ref=add;}",
        "function_reference_escape": "int64 add(int64 x){return x;} (int64 fn(int64)) const& bad(){int64 fn(int64) callback=add;return &callback;}",
        "function_reference_uninitialized": "void bad(){(int64 fn(int64)) mut& reference;}",
        "wrong_argument": "int64 add(int64 x){return x;} int64 bad(){int64 fn(int64) callback=add;return callback(true);}",
        "const_to_mut": "int64 const& borrow(int64 const& x){return x;} int64 mut& fn(int64 mut&) callback=borrow;",
    }
    for name, code in errors.items():
        invalid = WORK / (name + ".mgl")
        invalid.write_text(code + "\nint64 main(){return 0;}\n", encoding="utf-8")
        result = execute(compiler, "--check", invalid)
        assert result.returncode != 0 and result.stderr, (name, result)
    print(f"匿名函数类型：解释执行、AOT、{('svmb、' if args.vm else '')}{len(errors)} 项拒绝检查通过")


if __name__ == "__main__":
    main()
