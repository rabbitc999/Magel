"""static T create 的构造语义、非强制使用、资源所有权与反射在解释执行与 VM 中的回归。"""
from pathlib import Path
import argparse
import json
import os
import subprocess
import tempfile
import unittest
ROOT = Path(__file__).resolve().parents[1]
COMPILER = ROOT / 'ChaLi.exe'
WITH_REFLECTION = True

class ConstructorTests(unittest.TestCase):

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='chali-constructors-')
        self.addCleanup(self.temp.cleanup)
        self.work = Path(self.temp.name)

    def program(self, code, expected='', reflection=False):
        source = self.work / 'main.chl'
        source.write_text(code, encoding='utf-8')
        for backend in ('run',):
            with self.subTest(backend=backend):
                output = self.work / (backend + ('.exe' if os.name == 'nt' else ''))
                command = [str(COMPILER), backend, str(source), 'out', str(output)]
                if backend != 'run':
                    command += ['opt', '0']
                if reflection:
                    command += ['reflect']
                result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=240)
                self.assertEqual(result.returncode, 0, (result.stdout, result.stderr))
                if backend != 'run':
                    result = subprocess.run([str(output)], capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=60)
                self.assertEqual((result.returncode, result.stdout, result.stderr), (0, expected, ''))

    def test_value_lifetime(self):
        self.program((ROOT / 'example/samples/static_new.chl').read_text(encoding='utf-8'), (ROOT / 'example/expected/static_new.txt').read_text(encoding='utf-8'))

    def test_early_return_and_default(self):
        self.program('\nstruct Value {\n    int64 number;\n    static Value create(bool early) {\n        self.number=1;\n        if(early) { return; }\n        self.number=2;\n    }\n};\nstruct Partial {\n    int64 number;\n    static Partial create(bool set) { if(set) { self.number=7; } }\n};\nvoid main(){\n    Value unused;assert(unused.number==0);\n    Value a=Value::create(true);Value b=Value::create(false);\n    assert(a.number==1 && b.number==2);\n    {\n        int64 i=0;\n        while (i<10) {\n\n        Partial first=Partial::create(true);assert(first.number==7);\n        Partial second=Partial::create(false);assert(second.number==0);\n    \n            i=i+1;\n        }\n    }\n    return;\n}')

    def test_reflection(self):
        if not WITH_REFLECTION:
            self.skipTest('使用 --reflection 验证已开启反射宏的编译器')
        self.program('\nimport std::reflect;\n@Reflect struct Value {\n    String text;\n    static Value create(String text) { self.text=text; }\n};\n@Reflect enum State {\n    Empty, Text(String text);\n    static State create(String text) { self=State::Text(text); }\n};\nvoid main(){\n    List<std::reflect::ReflectValue> args;\n    args.push(std::reflect::ReflectValue::Text("owned"));\n    Value value=std::reflect::construct<Value>(args).unwrap();\n    assert(value.text=="owned");\n    assert(args[0].asText().unwrap()=="owned");\n    let info=std::reflect::getStructInfo<Value>().unwrap();\n    int64 constructors=0;\n    for(let method in info.methods){\n        if(method.isConstructor){\n            assert(method.isStatic && method.function.name=="new");\n            constructors=constructors+1;\n        }\n    }\n    assert(constructors==1);\n    State state=std::reflect::construct<State>(args).unwrap();\n    match(state){\n        State::Text(text)=>{assert(text=="owned");}\n        State::Empty=>{assert(false);}\n    }\n    args[0]=std::reflect::ReflectValue::I64(7);\n    assert(std::reflect::construct<Value>(args).isErr());\n    return;\n}', reflection=True)

    def test_repeated_ownership(self):
        self.program('\nstruct Owned : Drop {\n    static int64 drops;\n    String text;\n    List<String> items;\n    static Owned create(String text) {\n        self.text=text;\n        self.items.push(text);\n    }\n    void drop(){Owned::drops=Owned::drops+1;}\n};\nOwned make(String text){return Owned::create(text);}\nint64 read(Owned value){return value.items.len();}\nvoid main(){\n    String source="resource";\n    {\n        int64 i=0;\n        while (i<256) {\n\n        {\n            Owned value=Owned::create(source);\n            value=make(source);\n            assert(read(value)==1 && source=="resource");\n        }\n        assert(Owned::drops==(i+1)*3);\n    \n            i=i+1;\n        }\n    }\n    assert(read(Owned::create(source))==1);\n    assert(Owned::drops==770);\n    return;\n}')

    def test_chaining_and_add(self):
        self.program('\nstruct Value : Drop, Add<Value,int64> {\n    static int64 builds;\n    static int64 drops;\n    String text;\n    int64 number;\n    static Value create(int64 number) {\n        self.text="live";self.number=number;Value::builds=Value::builds+1;\n    }\n    int64 readValue(){assert(self.text=="live");return self.number;}\n    void touch(){self.number=self.number+1;}\n    String const& borrow(){return &self.text;}\n    Value leftAdd(int64 amount){return Value::create(self.number+amount);}\n    void drop(){Value::drops=Value::drops+1;}\n};\nvoid main(){\n    {\n        int64 i=0;\n        while (i<64) {\n\n        int64 before=Value::drops;\n        int64 number=Value::create(i).readValue();\n        assert(number==i && Value::drops==before+1);\n        Value::create(i).touch();\n        assert(Value::drops==before+2);\n        int64 a=3;\n        Value sum=Value::create(i)+a;\n        assert(sum.number==i+3 && sum.text=="live");\n        assert(Value::drops==before+3);\n        int64 result=(Value::create(i)+4).readValue();\n        assert(result==i+4 && Value::drops==before+5);\n    \n            i=i+1;\n        }\n    }\n    assert(Value::builds==384 && Value::drops==384);\n    {\n        String const& borrowed=Value::create(9).borrow();\n        assert(borrowed=="live" && Value::drops==384);\n    }\n    assert(Value::drops==385);\n    bool skipped=false && Value::create(0).readValue()>0;\n    assert(!skipped && Value::builds==385 && Value::drops==385);\n    return;\n}')

    def test_two_calls_then_add(self):
        self.program('\nint64 trace=0;\nint64 calls=0;\nstruct Chain : Drop, Closeable, Add<Chain,int64> {\n    int64 stage;\n    int64 number;\n    String text;\n    List<int64> items;\n    Chain f2(){\n        calls=calls+1;\n        assert(trace==0 && self.text=="seed" && self.items.len()==1);\n        Chain result;\n        result.stage=2;result.number=self.number+2;\n        result.text=self.text+"2";result.items=self.items;result.items.push(2);\n        assert(self.text=="seed" && self.items.len()==1);\n        return result;\n    }\n    Chain leftAdd(int64 amount){\n        calls=calls+1;\n        assert(trace==0 && self.text=="seed2" && self.items.len()==2);\n        Chain result;\n        result.stage=3;result.number=self.number+amount;\n        result.text=self.text+"3";result.items=self.items;result.items.push(3);\n        assert(self.text=="seed2" && self.items.len()==2);\n        return result;\n    }\n    int64 read(){return self.number;}\n    String const& borrow(){return &self.text;}\n    void close(){self.text=self.text+"#";}\n    void drop(){\n        assert(self.text[len(self.text)-1]==\'#\');\n        trace=trace*10+self.stage;\n    }\n};\nChain f1(int64 number){\n    calls=calls+1;\n    Chain result;result.stage=1;result.number=number;result.text="seed";\n    result.items.push(1);return result;\n}\nChain const& borrowed(Chain const& value){return &value;}\nstruct Borrowed : Drop {\n    static int64 drops;\n    String text;\n    void drop(){Borrowed::drops=Borrowed::drops+1;}\n};\nstruct Owner : Add<Borrowed const&,int64> {\n    Borrowed value;\n    Borrowed const& leftAdd(int64 unused){return &self.value;}\n};\nvoid main(){\n    int64 a=7;\n    {\n        int64 i=0;\n        while (i<64) {\n\n        trace=0;\n        int64 before=calls;\n        {\n            Chain result=f1(i).f2()+a;\n            assert(calls==before+3 && trace==21);\n            assert(result.number==i+9 && result.text=="seed23");\n            assert(result.items.len()==3 && result.items[0]==1 && result.items[2]==3);\n        }\n        assert(trace==213);\n        trace=0;\n        int64 number=(f1(i).f2()+a).read();\n        assert(number==i+9 && trace==321);\n        trace=0;\n        f1(i).f2()+a;\n        assert(trace==321);\n        trace=0;\n        bool skipped=false && (f1(i).f2()+a).read()>0;\n        assert(!skipped && trace==0 && calls==before+9);\n    \n            i=i+1;\n        }\n    }\n    trace=0;\n    {\n        String const& text=(f1(0).f2()+a).borrow();\n        assert(text=="seed23" && trace==0);\n    }\n    assert(trace==321);\n    trace=0;\n    {\n        Chain source=f1(10);\n        {\n            Chain result=borrowed(&source).f2()+a;\n            assert(trace==2 && result.number==19);\n            assert(source.text=="seed" && source.items.len()==1);\n        }\n        assert(trace==23 && source.text=="seed");\n    }\n    assert(trace==231);\n    {\n        Owner owner;owner.value.text="alive";\n        owner+1;\n        assert(Borrowed::drops==0 && owner.value.text=="alive");\n    }\n    assert(Borrowed::drops==1);\n    return;\n}')

    def test_rejections(self):
        cases = {'wrong_return_type': 'struct V{static int64 create(){}};void main(){}', 'void_return_type': 'struct V{static void create(){}};void main(){}', 'return_value': 'struct V{static V create(){V v;return v;}};void main(){}', 'wrong_argument': 'struct V{static V create(int64 n){}};void main(){V v=V::create("x");}', 'variadic': 'struct V{static V create(int64... n){}};void main(){}', 'ordinary_static_self': 'struct V{int64 n;static void reset(){self.n=1;}};void main(){}', 'discarded': 'struct V{static V create(){}};void main(){V::create();}', 'discarded_parentheses': 'struct V{static V create(){}};void main(){(V::create());}', 'discarded_generic': 'struct V<T>{static V<T> create(){}};void main(){V<int64>::create();}', 'discarded_enum': 'enum V{Empty;static V create(){}};void main(){V::create();}'}
        for name, code in cases.items():
            with self.subTest(case=name):
                source = self.work / (name + '.chl')
                source.write_text(code, encoding='utf-8')
                result = subprocess.run([str(COMPILER), 'check', str(source), 'json'], capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=30)
                self.assertNotEqual(result.returncode, 0, (result.stdout, result.stderr))
                self.assertTrue(json.loads(result.stdout)['diagnostics'])
                if name.startswith('discarded'):
                    self.assertIn('构造结果必须被接收', result.stdout)
if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--chali', type=Path, default=COMPILER)
    parser.add_argument('--reflection', action='store_true', default=True)
    args, remaining = parser.parse_known_args()
    COMPILER = args.chali.resolve()
    WITH_REFLECTION = args.reflection
    unittest.main(argv=[__file__, *remaining], verbosity=2)
