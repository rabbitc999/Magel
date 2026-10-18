"""static T create 的构造语义、非强制使用、资源所有权与反射在解释执行与 AOT 中的回归。"""
from pathlib import Path
import argparse
import json
import os
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
COMPILER = ROOT / 'magel.exe'
WITH_REFLECTION = False


class ConstructorTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='magel-constructors-')
        self.addCleanup(self.temp.cleanup)
        self.work = Path(self.temp.name)

    def program(self, code, expected='', reflection=False):
        source = self.work / 'main.mgl'
        source.write_text(code, encoding='utf-8')
        for backend in ('run', 'aot'):
            with self.subTest(backend=backend):
                output = self.work / (backend + ('.exe' if os.name == 'nt' else ''))
                command = [str(COMPILER), backend, str(source), 'out', str(output)]
                if backend != 'run':
                    command += ['opt', '0']
                if reflection:
                    command += ['reflect']
                result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True,
                                        encoding='utf-8', errors='replace', timeout=240)
                self.assertEqual(result.returncode, 0, (result.stdout, result.stderr))
                if backend != 'run':
                    result = subprocess.run([str(output)], capture_output=True, text=True,
                                            encoding='utf-8', errors='replace', timeout=60)
                self.assertEqual((result.returncode, result.stdout, result.stderr), (0, expected, ''))

    def test_value_lifetime(self):
        self.program((ROOT / 'example/samples/static_new.mgl').read_text(encoding='utf-8'),
                     (ROOT / 'example/expected/static_new.txt').read_text(encoding='utf-8'))

    def test_early_return_and_default(self):
        self.program('''
struct Value {
    int64 number;
    static Value create(bool early) {
        self.number=1;
        if(early) { return; }
        self.number=2;
    }
};
struct Partial {
    int64 number;
    static Partial create(bool set) { if(set) { self.number=7; } }
};
int64 main(){
    Value unused;assert(unused.number==0);
    Value a=Value::create(true);Value b=Value::create(false);
    assert(a.number==1 && b.number==2);
    for(int64 i=0;i<10;i=i+1) {
        Partial first=Partial::create(true);assert(first.number==7);
        Partial second=Partial::create(false);assert(second.number==0);
    }
    return 0;
}''')

    def test_reflection(self):
        if not WITH_REFLECTION:
            self.skipTest('使用 --reflection 验证已开启反射宏的编译器')
        self.program('''
import std::reflect;
@Reflect struct Value {
    String text;
    static Value create(String text) { self.text=text; }
};
@Reflect enum State {
    Empty, Text(String text);
    static State create(String text) { self=State::Text(text); }
};
int64 main(){
    List<std::reflect::ReflectValue> args;
    args.push(std::reflect::ReflectValue::Text("owned"));
    Value value=std::reflect::construct<Value>(args).unwrap();
    assert(value.text=="owned");
    assert(args[0].asText().unwrap()=="owned");
    auto info=std::reflect::getStructInfo<Value>().unwrap();
    assert(info.methods.len()==1);
    assert(info.methods[0].isConstructor && info.methods[0].isStatic);
    assert(info.methods[0].function.name=="new");
    State state=std::reflect::construct<State>(args).unwrap();
    match(state){
        State::Text(text)=>{assert(text=="owned");}
        State::Empty=>{assert(false);}
    }
    args[0]=std::reflect::ReflectValue::I64(7);
    assert(std::reflect::construct<Value>(args).isErr());
    return 0;
}''', reflection=True)

    def test_repeated_ownership(self):
        self.program('''
struct Owned : Drop {
    static int64 drops;
    String text;
    List<String> items;
    static Owned create(String text) {
        self.text=text;
        self.items.push(text);
    }
    void drop(){Owned::drops=Owned::drops+1;}
};
Owned make(String text){return Owned::create(text);}
int64 read(Owned value){return value.items.len();}
int64 main(){
    String source="resource";
    for(int64 i=0;i<256;i=i+1){
        {
            Owned value=Owned::create(source);
            value=make(source);
            assert(read(value)==1 && source=="resource");
        }
        assert(Owned::drops==(i+1)*3);
    }
    assert(read(Owned::create(source))==1);
    assert(Owned::drops==770);
    return 0;
}''')

    def test_chaining_and_add(self):
        self.program('''
struct Value : Drop, Add<Value,int64> {
    static int64 builds;
    static int64 drops;
    String text;
    int64 number;
    static Value create(int64 number) {
        self.text="live";self.number=number;Value::builds=Value::builds+1;
    }
    int64 readValue(){assert(self.text=="live");return self.number;}
    void touch(){self.number=self.number+1;}
    String const& borrow(){return &self.text;}
    Value leftAdd(int64 amount){return Value::create(self.number+amount);}
    void drop(){Value::drops=Value::drops+1;}
};
int64 main(){
    for(int64 i=0;i<64;i=i+1){
        int64 before=Value::drops;
        int64 number=Value::create(i).readValue();
        assert(number==i && Value::drops==before+1);
        Value::create(i).touch();
        assert(Value::drops==before+2);
        int64 a=3;
        Value sum=Value::create(i)+a;
        assert(sum.number==i+3 && sum.text=="live");
        assert(Value::drops==before+3);
        int64 result=(Value::create(i)+4).readValue();
        assert(result==i+4 && Value::drops==before+5);
    }
    assert(Value::builds==384 && Value::drops==384);
    {
        String const& borrowed=Value::create(9).borrow();
        assert(borrowed=="live" && Value::drops==384);
    }
    assert(Value::drops==385);
    bool skipped=false && Value::create(0).readValue()>0;
    assert(!skipped && Value::builds==385 && Value::drops==385);
    return 0;
}''')

    def test_two_calls_then_add(self):
        self.program('''
int64 trace=0;
int64 calls=0;
struct Chain : Drop, Closeable, Add<Chain,int64> {
    int64 stage;
    int64 number;
    String text;
    List<int64> items;
    Chain f2(){
        calls=calls+1;
        assert(trace==0 && self.text=="seed" && self.items.len()==1);
        Chain result;
        result.stage=2;result.number=self.number+2;
        result.text=self.text+"2";result.items=self.items;result.items.push(2);
        assert(self.text=="seed" && self.items.len()==1);
        return result;
    }
    Chain leftAdd(int64 amount){
        calls=calls+1;
        assert(trace==0 && self.text=="seed2" && self.items.len()==2);
        Chain result;
        result.stage=3;result.number=self.number+amount;
        result.text=self.text+"3";result.items=self.items;result.items.push(3);
        assert(self.text=="seed2" && self.items.len()==2);
        return result;
    }
    int64 read(){return self.number;}
    String const& borrow(){return &self.text;}
    void close(){self.text=self.text+"#";}
    void drop(){
        assert(self.text[len(self.text)-1]=='#');
        trace=trace*10+self.stage;
    }
};
Chain f1(int64 number){
    calls=calls+1;
    Chain result;result.stage=1;result.number=number;result.text="seed";
    result.items.push(1);return result;
}
Chain const& borrowed(Chain const& value){return &value;}
struct Borrowed : Drop {
    static int64 drops;
    String text;
    void drop(){Borrowed::drops=Borrowed::drops+1;}
};
struct Owner : Add<Borrowed const&,int64> {
    Borrowed value;
    Borrowed const& leftAdd(int64 unused){return &self.value;}
};
int64 main(){
    int64 a=7;
    for(int64 i=0;i<64;i=i+1){
        trace=0;
        int64 before=calls;
        {
            Chain result=f1(i).f2()+a;
            assert(calls==before+3 && trace==21);
            assert(result.number==i+9 && result.text=="seed23");
            assert(result.items.len()==3 && result.items[0]==1 && result.items[2]==3);
        }
        assert(trace==213);
        trace=0;
        int64 number=(f1(i).f2()+a).read();
        assert(number==i+9 && trace==321);
        trace=0;
        f1(i).f2()+a;
        assert(trace==321);
        trace=0;
        bool skipped=false && (f1(i).f2()+a).read()>0;
        assert(!skipped && trace==0 && calls==before+9);
    }
    trace=0;
    {
        String const& text=(f1(0).f2()+a).borrow();
        assert(text=="seed23" && trace==0);
    }
    assert(trace==321);
    trace=0;
    {
        Chain source=f1(10);
        {
            Chain result=borrowed(&source).f2()+a;
            assert(trace==2 && result.number==19);
            assert(source.text=="seed" && source.items.len()==1);
        }
        assert(trace==23 && source.text=="seed");
    }
    assert(trace==231);
    {
        Owner owner;owner.value.text="alive";
        owner+1;
        assert(Borrowed::drops==0 && owner.value.text=="alive");
    }
    assert(Borrowed::drops==1);
    return 0;
}''')

    def test_rejections(self):
        cases = {
            'wrong_return_type': 'struct V{static int64 create(){}};int64 main(){return 0;}',
            'void_return_type': 'struct V{static void create(){}};int64 main(){return 0;}',
            'return_value': 'struct V{static V create(){V v;return v;}};int64 main(){return 0;}',
            'wrong_argument': 'struct V{static V create(int64 n){}};int64 main(){V v=V::create("x");return 0;}',
            'variadic': 'struct V{static V create(int64... n){}};int64 main(){return 0;}',
            'ordinary_static_self': 'struct V{int64 n;static void reset(){self.n=1;}};int64 main(){return 0;}',
            'discarded': 'struct V{static V create(){}};int64 main(){V::create();return 0;}',
            'discarded_parentheses': 'struct V{static V create(){}};int64 main(){(V::create());return 0;}',
            'discarded_generic': 'struct V<T>{static V<T> create(){}};int64 main(){V<int64>::create();return 0;}',
            'discarded_enum': 'enum V{Empty;static V create(){}};int64 main(){V::create();return 0;}',
        }
        for name, code in cases.items():
            with self.subTest(case=name):
                source = self.work / (name + '.mgl')
                source.write_text(code, encoding='utf-8')
                result = subprocess.run([str(COMPILER), 'check', str(source), 'json'],
                                        capture_output=True, text=True, encoding='utf-8',
                                        errors='replace', timeout=30)
                self.assertNotEqual(result.returncode, 0, (result.stdout, result.stderr))
                self.assertTrue(json.loads(result.stdout)['diagnostics'])
                if name.startswith('discarded'):
                    self.assertIn('构造结果必须被接收', result.stdout)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--magel', type=Path, default=COMPILER)
    parser.add_argument('--reflection', action='store_true')
    args, remaining = parser.parse_known_args()
    COMPILER = args.magel.resolve()
    WITH_REFLECTION = args.reflection
    unittest.main(argv=[__file__, *remaining], verbosity=2)
