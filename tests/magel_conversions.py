"""显式/隐式转换协议的解释与 AOT、只读性及值语义回归。"""
from pathlib import Path
import argparse
import os
import json
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
COMPILER = ROOT / 'magel.exe'


class ConversionTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='magel-conversions-')
        self.addCleanup(self.temp.cleanup)
        self.work = Path(self.temp.name)

    def run_program(self, code, expected=''):
        source = self.work / 'main.mgl'
        source.write_text(code, encoding='utf-8')
        for backend in ('run', 'aot'):
            with self.subTest(backend=backend):
                output = self.work / (backend + ('.exe' if os.name == 'nt' else ''))
                command = [str(COMPILER), backend, str(source), 'out', str(output)]
                if backend != 'run':
                    command += ['opt', '0']
                result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True,
                                        encoding='utf-8', errors='replace', timeout=240)
                self.assertEqual(result.returncode, 0, (result.stdout, result.stderr))
                if backend != 'run':
                    result = subprocess.run([str(output)], capture_output=True, text=True,
                                            encoding='utf-8', errors='replace', timeout=60)
                self.assertEqual((result.returncode, result.stdout, result.stderr), (0, expected, ''))

    def test_protocol_selection(self):
        self.run_program((ROOT / 'example/samples/conversion_interfaces.mgl').read_text(encoding='utf-8'),
                         'conversions: implicit=7 explicit=107 generic=9\n')

    def test_string_and_source_lifetime(self):
        self.run_program('''
int64 drops = 0;
struct Source : ExplicitConvert<String>, Drop {
    String text;
    String explicitConvert() { return self.text; }
    void drop() { drops = drops + 1; }
};
struct Automatic : ImplicitConvert<String> {
    String text;
    String implicitConvert() { return self.text; }
};
Source make() { Source s; s.text="source"; return s; }
String use(String value) { return value; }
String returned(Automatic const& value) { return value; }
void variadic(String... values) { assert(values[0]=="automatic"); assert(values[1]=="automatic"); }
int64 main() {
    Source named=make();
    String explicit=cast<String>(named);
    explicit[0]='X'; assert(named.text=="source");
    for(int64 i=0;i<20;i=i+1) {
        String temporary=cast<String>(make());
        assert(temporary=="source");
    }
    assert(drops==20);
    Automatic value;value.text="automatic";
    String first=value; String second=use(value); String third=returned(&value);
    first[0]='X';assert(value.text=="automatic");
    first=value;assert(first==second && second==third);
    List<String> items;items.push(value);assert(items[0]=="automatic");
    assert(value.text=="automatic");
    return 0;
}''')

    def test_aggregate_values(self):
        self.run_program('''
struct Target { String text; List<int64> numbers; };
struct Source : ImplicitConvert<Target>, ExplicitConvert<Target> {
    String text;
    Target implicitConvert() { Target t;t.text=self.text;t.numbers.push(7);return t; }
    Target explicitConvert() { Target t;t.text=self.text;t.numbers.push(8);return t; }
};
Target returned(Source const& s) { return s; }
int64 consume(Target t) { assert(t.text=="hello");return t.numbers[0]; }
int64 main() {
    Source s;s.text="hello";
    Target a=s;Target b=s;Target c=cast<Target>(s);
    a.text[0]='X';a.numbers[0]=10;
    assert(b.text=="hello" && b.numbers[0]==7 && c.numbers[0]==8);
    a=s;assert(a.text=="hello" && a.numbers[0]==7);
    Target r=returned(&s);assert(r.numbers[0]==7);
    assert(consume(s)==7);
    List<Target> items;items.push(s);assert(items[0].numbers[0]==7);
    Map<String,Target> literal={"key":s};assert(literal["key"].numbers[0]==7);
    assert(s.text=="hello");
    return 0;
}''')

    def test_object_results(self):
        self.run_program('''
struct Source : ImplicitConvert<Any>, ExplicitConvert<Any> {
    Any implicitConvert() { Any value=7;return value; }
    Any explicitConvert() { Any value=8;return value; }
};
Any returned(Source const& source) { return source; }
int64 consume(Any value) { return (int64)value.getValue(); }
int64 main() {
    Source source;
    Any a=source;assert((int64)a.getValue()==7);
    a=source;assert((int64)a.getValue()==7);
    Any b=cast<Any>(source);assert((int64)b.getValue()==8);
    Any c=returned(&source);assert((int64)c.getValue()==7);
    assert(consume(source)==7);
    List<Any> items;items.push(source);Any item=items[0];
    assert((int64)item.getValue()==7);
    return 0;
}''')

    def test_compile_time_rejections(self):
        cases = {
            'explicit_only': 'struct S:ExplicitConvert<int64>{int64 explicitConvert(){return 1;}};int64 main(){S s;int64 x=s;return 0;}',
            'wrong_signature': 'struct S:ImplicitConvert<int64>{String implicitConvert(){return "x";}};int64 main(){return 0;}',
            'missing_method': 'struct S:ImplicitConvert<int64>{int64 value;};int64 main(){return 0;}',
            'no_chain': 'struct S:ImplicitConvert<int32>{int32 implicitConvert(){return (int32)1;}};int64 main(){S s;int64 x=s;return 0;}',
            'readonly_implicit': 'struct S:ImplicitConvert<int64>{int64 n;int64 implicitConvert(){self.n=self.n+1;return self.n;}};int64 main(){S original;const S s=original;int64 x=s;return 0;}',
            'readonly_indirect_explicit': 'struct S:ExplicitConvert<int64>{int64 n;int64 explicitConvert(){self.n=self.n+1;return self.n;}int64 read(){return (int64)self;}};int64 main(){S original;const S s=original;return s.read();}',
            'readonly_indirect_implicit': 'struct S:ImplicitConvert<int64>{int64 n;int64 implicitConvert(){self.n=self.n+1;return self.n;}int64 read(){return self;}};int64 main(){S original;const S s=original;return s.read();}',
            'readonly_indirect_assignment': 'struct S:ImplicitConvert<int64>{int64 n;int64 implicitConvert(){self.n=self.n+1;return self.n;}int64 read(){int64 x=0;x=self;return x;}};int64 main(){S original;const S s=original;return s.read();}',
            'readonly_indirect_alias': 'struct S:ExplicitConvert<int64>{int64 n;int64 explicitConvert(){self.n=self.n+1;return self.n;}};struct W{S field;int64 read(){S mut& alias=&self.field;return (int64)alias;}};int64 main(){W original;const W w=original;return w.read();}',
            'object_local_address': 'struct S{int64 n;};int64 main(){S s;Any o;o.set(s);return 0;}',
            'readonly_explicit': 'struct S:ExplicitConvert<int64>{int64 n;int64 explicitConvert(){self.n=self.n+1;return self.n;}};int64 main(){S s;S const& r=&s;int64 x=(int64)r;return 0;}',
        }
        for name, code in cases.items():
            with self.subTest(case=name):
                path = self.work / (name + '.mgl')
                path.write_text(code, encoding='utf-8')
                result = subprocess.run([str(COMPILER), 'check', str(path), 'json'], capture_output=True,
                                        text=True, encoding='utf-8', errors='replace', timeout=30)
                self.assertNotEqual(result.returncode, 0, (name, result.stdout, result.stderr))
                diagnostics = json.loads(result.stdout)['diagnostics']
                self.assertTrue(diagnostics)
                if name.startswith('readonly_'):
                    self.assertIn('修改方法', diagnostics[0]['message'])
                self.assertFalse(path.with_suffix('.quad').exists())


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--magel', type=Path, default=COMPILER)
    args, remaining = parser.parse_known_args()
    COMPILER = args.magel.resolve()
    unittest.main(argv=[__file__, *remaining], verbosity=2)
