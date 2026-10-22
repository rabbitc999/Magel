"""验证 Any 目标装箱、异构泛型容器、精确解包及资源和借用边界。"""
from pathlib import Path
import argparse
import os
import subprocess
ROOT = Path(__file__).resolve().parents[1]

def main():
    """用默认优化的 VM 与 checked VM 检查装箱的统一行为。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--magel', type=Path, default=ROOT / ('magel.exe' if os.name == 'nt' else 'magel'))
    args = parser.parse_args()
    compiler = args.magel.resolve()
    work = ROOT / 'build/magel-any-generics'
    work.mkdir(parents=True, exist_ok=True)
    cases = {'macro': ((ROOT / 'example/samples/any_generics.mgl').read_text(encoding='utf-8'), 'A=6 B=4\n1 2\n'), 'containers': ('int64 main()\n{\n    List<Any> list = [1, "two", true, 1.5];\n    list.push(\'A\');\n    Any character = list[4];\n    assert(tryGet<char>(&character).unwrap() == \'A\');\n    list.insert(1, "inserted");\n    Any text = list[1];\n    assert(tryGet<String>(&text).unwrap() == "inserted");\n    list[1] = false;\n    Any flag = list[1];\n    assert(tryGet<bool>(&flag).unwrap() == false);\n    Any number = list[0];\n    assert(isType<int64>(&number));\n    assert(!isType<float64>(&number));\n    assert(!tryGet<String>(&number).isValue());\n    List<Any> copied = list;\n    list.clear();\n    assert(copied.len() == 6);\n    Set<Any> values;\n    Any key = "key";\n    values.add(key);\n    values.add(key);\n    values.add(2);\n    values.add(false);\n    assert(values.len() == 3);\n    assert(values.has(key) != 0);\n    Map<Any, Any> map;\n    map.put(key, "before");\n    map.put(key, 7);\n    map.put(2, "two");\n    assert(map.len() == 2);\n    Any value = map[key];\n    assert(tryGet<int64>(&value).unwrap() == 7);\n    Map<String, Any> named = {"answer": 42, "text": "hello"};\n    Any answer = named["answer"];\n    assert(tryGet<int64>(&answer).unwrap() == 42);\n    Map<Any, int64> keys;\n    keys.put(key, 1);\n    assert(keys[key] == 1);\n    println("containers OK");\n    return 0;\n}', 'containers OK\n'), 'generic': ('struct Box<T> { T value; };\nT identity<T>(T value) { return value; }\nAny returned() { return "returned"; }\nAny resultBlock() { return { yield (9); }; }\nint64 calls = 0;\nString fresh() { calls = calls + 1; return "fresh" + " text"; }\nint64 main()\n{\n    Any value = identity<Any>(fresh());\n    assert(calls == 1);\n    assert(tryGet<String>(&value).unwrap() == "fresh text");\n    Box<Any> box;\n    box.value = 17;\n    assert(tryGet<int64>(&box.value).unwrap() == 17);\n    Option<Any> optional = Option<Any>::Value("optional");\n    Any payload = optional.unwrap();\n    assert(tryGet<String>(&payload).unwrap() == "optional");\n    value = returned();\n    assert(tryGet<String>(&value).unwrap() == "returned");\n    value = resultBlock();\n    assert(tryGet<int64>(&value).unwrap() == 9);\n    List<int64> numbers = [1, 2];\n    List<Any> mixed;\n    mixed.push(numbers);\n    numbers.push(3);\n    Any nested = mixed[0];\n    List<int64> extracted = tryGet<List<int64>>(&nested).unwrap();\n    assert(extracted.len() == 2);\n    int64 bias = 5;\n    let callback = int64 fn copy(int64 x) { x + bias };\n    mixed.push(callback);\n    Any callable = mixed[1];\n    let extractedCallback = tryGet<int64 fn(int64)>(&callable).unwrap();\n    assert(extractedCallback(3) == 8);\n    Any preserved = value;\n    value.clear();\n    assert(tryGet<int64>(&preserved).unwrap() == 9);\n    assert(value.isEmpty());\n    assert(!tryGet<int64>(&value).isValue());\n    value.set("again");\n    assert(isType<String>(&value));\n    println("generic OK");\n    return 0;\n}', 'generic OK\n')}
    cases['interfaces'] = ('interface Readable { int64 read(); };\ninterface Writable { void write(int64 value); };\nstruct Counter : Readable, Writable\n{\n    int64 value;\n    int64 read() { return self.value; }\n    void write(int64 value) { self.value = value; }\n};\nCounter global;\nReadable const& globalView() { return &global; }\nint64 consume(Readable const& value) { return value.read(); }\nReadable const& forward(Counter const& value) { return &value; }\nint64 main()\n{\n    Counter counter;\n    counter.value = 7;\n    Readable const& reader = &counter;\n    Writable mut& writer = &counter;\n    assert(reader.read() == 7);\n    writer.write(11);\n    assert(reader.read() == 11);\n    Readable const& copied = reader;\n    assert(copied.read() == 11);\n    assert(consume(&counter) == 11);\n    Readable const& forwarded = forward(&counter);\n    assert(forwarded.read() == 11);\n    global.value = 13;\n    Readable const& saved = globalView();\n    assert(saved.read() == 13);\n    println("interfaces OK");\n    return 0;\n}', 'interfaces OK\n')
    env = dict(os.environ, MAGEL_LANG='zh')
    for name, (code, expected) in cases.items():
        source = work / (name + '.mgl')
        source.write_text(code, encoding='utf-8')
        for backend in ('run',):
            output = work / (name + '-' + backend + ('.exe' if os.name == 'nt' else ''))
            result = subprocess.run([str(compiler), backend, str(source), '--out', str(output)], cwd=ROOT, env=env, capture_output=True, encoding='utf-8', timeout=240)
            assert result.returncode == 0, (name, backend, result.stdout, result.stderr)
            assert (result.returncode, result.stdout, result.stderr) == (0, expected, ''), (name, backend, result)
    errors = {'interface_missing': 'interface I{int64 read();};struct S{int64 read(){return 1;}};int64 main(){S s;I const& i=&s;return 0;}', 'interface_mutability': 'interface I{void write();};struct S:I{void write(){}};int64 main(){S original;const S s=original;I mut& i=&s;return 0;}', 'interface_escape': 'interface I{int64 read();};struct S:I{int64 read(){return 1;}};I const& bad(){S s;return &s;}int64 main(){return 0;}', 'interface_inner_scope': 'interface I{int64 read();};struct S:I{int64 read(){return 1;}};int64 main(){S outer;I const& i=&outer;{S inner;i=&inner;}return i.read();}', 'mixed_return_branches': 'Any bad(bool b){return if(b){1}else{"text"};}int64 main(){return 0;}', 'typed_list': 'int64 main(){List<int64> values;values.push("bad");return 0;}', 'local_struct': 'struct S{int64 x;};int64 main(){S s;List<Any> values;values.push(s);return 0;}', 'local_array': 'int64 main(){int64 a[2];List<Any> values;values.push(a);return 0;}', 'escape_list': 'List<Any> bad(){int64 x=1;List<Any> a;a.push(&x);return a;}int64 main(){return 0;}', 'escape_map': 'Map<String,Any> bad(){int64 x=1;Map<String,Any> a;a.put("x",&x);return a;}int64 main(){return 0;}', 'escape_return': 'Any bad(){int64 x=1;return &x;}int64 main(){return 0;}', 'readonly_clear': 'int64 main(){const Any a=1;a.clear();return 0;}', 'clear_arguments': 'int64 main(){Any a;a.clear(1);return 0;}', 'void_box': 'void nothing(){} int64 main(){List<Any> a;a.push(nothing());return 0;}'}
    for name, code in errors.items():
        source = work / (name + '.mgl')
        source.write_text(code, encoding='utf-8')
        result = subprocess.run([str(compiler), 'check', str(source)], cwd=ROOT, env=env, capture_output=True, encoding='utf-8', timeout=60)
        assert result.returncode != 0, (name, result.stdout, result.stderr)
    source = work / 'interface-readonly-runtime.mgl'
    source.write_text('interface I{void write();};struct S:I{int64 x;void write(){self.x=1;}};int64 main(){S s;I const& i=&s;i.write();return 0;}', encoding='utf-8')
    for backend in ('run',):
        output = work / ('interface-readonly-' + backend + ('.exe' if os.name == 'nt' else ''))
        result = subprocess.run([str(compiler), backend, str(source), '--out', str(output)], cwd=ROOT, env=env, capture_output=True, encoding='utf-8', timeout=240)
        assert result.returncode != 0 and '只读接收者' in result.stderr, result
if __name__ == '__main__':
    main()
