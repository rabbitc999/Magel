"""验证 Any 目标装箱、异构泛型容器、精确解包及资源和借用边界。"""
from pathlib import Path
import argparse
import os
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def main():
    """用默认优化的 VM 与 checked AOT 检查装箱的统一行为。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--magel', type=Path, default=ROOT / ('magel.exe' if os.name == 'nt' else 'magel'))
    args = parser.parse_args()
    compiler = args.magel.resolve()
    work = ROOT / 'build/magel-any-generics'
    work.mkdir(parents=True, exist_ok=True)
    cases = {
        'macro': ((ROOT / 'example/samples/any_generics.mgl').read_text(encoding='utf-8'), 'A=6 B=4\n1 2\n'),
        'containers': ('''int64 main()
{
    List<Any> list = {1, "two", true, 1.5};
    list.push('A');
    Any character = list[4];
    assert(tryGet<char>(&character).unwrap() == 'A');
    list.insert(1, "inserted");
    Any text = list[1];
    assert(tryGet<String>(&text).unwrap() == "inserted");
    list[1] = false;
    Any flag = list[1];
    assert(tryGet<bool>(&flag).unwrap() == false);
    Any number = list[0];
    assert(isType<int64>(&number));
    assert(!isType<float64>(&number));
    assert(!tryGet<String>(&number).isValue());
    List<Any> copied = list;
    list.clear();
    assert(copied.len() == 6);
    Set<Any> values;
    Any key = "key";
    values.add(key);
    values.add(key);
    values.add(2);
    values.add(false);
    assert(values.len() == 3);
    assert(values.has(key) != 0);
    Map<Any, Any> map;
    map.put(key, "before");
    map.put(key, 7);
    map.put(2, "two");
    assert(map.len() == 2);
    Any value = map[key];
    assert(tryGet<int64>(&value).unwrap() == 7);
    Map<String, Any> named = {"answer": 42, "text": "hello"};
    Any answer = named["answer"];
    assert(tryGet<int64>(&answer).unwrap() == 42);
    Map<Any, int64> keys;
    keys.put(key, 1);
    assert(keys[key] == 1);
    println("containers OK");
    return 0;
}''', 'containers OK\n'),
        'generic': ('''struct Box<T> { T value; };
T identity<T>(T value) { return value; }
Any returned() { return "returned"; }
Any resultBlock() { return { yield (9); }; }
int64 calls = 0;
String fresh() { calls = calls + 1; return "fresh" + " text"; }
int64 main()
{
    Any value = identity<Any>(fresh());
    assert(calls == 1);
    assert(tryGet<String>(&value).unwrap() == "fresh text");
    Box<Any> box;
    box.value = 17;
    assert(tryGet<int64>(&box.value).unwrap() == 17);
    Option<Any> optional = Option<Any>::Value("optional");
    Any payload = optional.unwrap();
    assert(tryGet<String>(&payload).unwrap() == "optional");
    value = returned();
    assert(tryGet<String>(&value).unwrap() == "returned");
    value = resultBlock();
    assert(tryGet<int64>(&value).unwrap() == 9);
    List<int64> numbers = {1, 2};
    List<Any> mixed;
    mixed.push(numbers);
    numbers.push(3);
    Any nested = mixed[0];
    List<int64> extracted = tryGet<List<int64>>(&nested).unwrap();
    assert(extracted.len() == 2);
    int64 bias = 5;
    auto callback = int64 fn copy(int64 x) { x + bias };
    mixed.push(callback);
    Any callable = mixed[1];
    auto extractedCallback = tryGet<int64 fn(int64)>(&callable).unwrap();
    assert(extractedCallback(3) == 8);
    Any preserved = value;
    value.clear();
    assert(tryGet<int64>(&preserved).unwrap() == 9);
    assert(value.isEmpty());
    assert(!tryGet<int64>(&value).isValue());
    value.set("again");
    assert(isType<String>(&value));
    println("generic OK");
    return 0;
}''', 'generic OK\n'),
    }
    cases['interfaces'] = ('''interface Readable { int64 read(); };
interface Writable { void write(int64 value); };
struct Counter : Readable, Writable
{
    int64 value;
    int64 read() { return self.value; }
    void write(int64 value) { self.value = value; }
};
Counter global;
Readable const& globalView() { return &global; }
int64 consume(Readable const& value) { return value.read(); }
Readable const& forward(Counter const& value) { return &value; }
int64 main()
{
    Counter counter;
    counter.value = 7;
    Readable const& reader = &counter;
    Writable mut& writer = &counter;
    assert(reader.read() == 7);
    writer.write(11);
    assert(reader.read() == 11);
    Readable const& copied = reader;
    assert(copied.read() == 11);
    assert(consume(&counter) == 11);
    Readable const& forwarded = forward(&counter);
    assert(forwarded.read() == 11);
    global.value = 13;
    Readable const& saved = globalView();
    assert(saved.read() == 13);
    println("interfaces OK");
    return 0;
}''', 'interfaces OK\n')
    env = dict(os.environ, MAGEL_LANG='zh')
    for name, (code, expected) in cases.items():
        source = work / (name + '.mgl')
        source.write_text(code, encoding='utf-8')
        for backend in ('run', 'aot'):
            output = work / (name + '-' + backend + ('.exe' if os.name == 'nt' else ''))
            result = subprocess.run([str(compiler), backend, str(source), '--out', str(output)], cwd=ROOT,
                                    env=env, capture_output=True, encoding='utf-8', timeout=240)
            assert result.returncode == 0, (name, backend, result.stdout, result.stderr)
            if backend == 'aot':
                result = subprocess.run([str(output)], cwd=ROOT, env=env, capture_output=True,
                                        encoding='utf-8', timeout=60)
            assert (result.returncode, result.stdout, result.stderr) == (0, expected, ''), (name, backend, result)
    errors = {
        'interface_missing': 'interface I{int64 read();};struct S{int64 read(){return 1;}};int64 main(){S s;I const& i=&s;return 0;}',
        'interface_mutability': 'interface I{void write();};struct S:I{void write(){}};int64 main(){S original;const S s=original;I mut& i=&s;return 0;}',
        'interface_escape': 'interface I{int64 read();};struct S:I{int64 read(){return 1;}};I const& bad(){S s;return &s;}int64 main(){return 0;}',
        'interface_inner_scope': 'interface I{int64 read();};struct S:I{int64 read(){return 1;}};int64 main(){S outer;I const& i=&outer;{S inner;i=&inner;}return i.read();}',
        'mixed_return_branches': 'Any bad(bool b){return if(b){1}else{"text"};}int64 main(){return 0;}',
        'typed_list': 'int64 main(){List<int64> values;values.push("bad");return 0;}',
        'local_struct': 'struct S{int64 x;};int64 main(){S s;List<Any> values;values.push(s);return 0;}',
        'local_array': 'int64 main(){int64 a[2];List<Any> values;values.push(a);return 0;}',
        'escape_list': 'List<Any> bad(){int64 x=1;List<Any> a;a.push(&x);return a;}int64 main(){return 0;}',
        'escape_map': 'Map<String,Any> bad(){int64 x=1;Map<String,Any> a;a.put("x",&x);return a;}int64 main(){return 0;}',
        'escape_return': 'Any bad(){int64 x=1;return &x;}int64 main(){return 0;}',
        'readonly_clear': 'int64 main(){const Any a=1;a.clear();return 0;}',
        'clear_arguments': 'int64 main(){Any a;a.clear(1);return 0;}',
        'void_box': 'void nothing(){} int64 main(){List<Any> a;a.push(nothing());return 0;}',
    }
    for name, code in errors.items():
        source = work / (name + '.mgl')
        source.write_text(code, encoding='utf-8')
        result = subprocess.run([str(compiler), 'check', str(source)], cwd=ROOT, env=env,
                                capture_output=True, encoding='utf-8', timeout=60)
        assert result.returncode != 0, (name, result.stdout, result.stderr)
    # 接口的具体实现到运行期才确定，沿用分派器对只读接收者的动态保护。
    source = work / 'interface-readonly-runtime.mgl'
    source.write_text('interface I{void write();};struct S:I{int64 x;void write(){self.x=1;}};int64 main(){S s;I const& i=&s;i.write();return 0;}', encoding='utf-8')
    for backend in ('run', 'aot'):
        output = work / ('interface-readonly-' + backend + ('.exe' if os.name == 'nt' else ''))
        result = subprocess.run([str(compiler), backend, str(source), '--out', str(output)], cwd=ROOT,
                                env=env, capture_output=True, encoding='utf-8', timeout=240)
        if backend == 'aot':
            assert result.returncode == 0, result.stderr
            result = subprocess.run([str(output)], cwd=ROOT, env=env, capture_output=True,
                                    encoding='utf-8', timeout=60)
        assert result.returncode != 0 and '只读接收者' in result.stderr, result
    print(f'Any 泛型：{len(cases)} 组 VM/AOT 程序与 {len(errors)} 个编译错误边界、接口只读运行保护通过。')


if __name__ == '__main__':
    main()
