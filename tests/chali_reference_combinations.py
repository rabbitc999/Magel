"""验证引用生命周期与泛型、枚举、回调、容器和构造链的组合边界。"""
from pathlib import Path
import argparse
import json
import os
import subprocess
ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / 'build/chali-reference-combinations'

def execute(*args):
    """执行工具或产物，并保留失败诊断及运行时资源清理输出。"""
    return subprocess.run([str(arg) for arg in args], cwd=ROOT, capture_output=True, encoding='utf-8', errors='replace', timeout=180)

def main():
    """检查组合拒绝路径及解释执行、VM 的正常行为。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--chali', type=Path, default=ROOT / 'ChaLi.exe')
    compiler = parser.parse_args().chali.resolve()
    WORK.mkdir(parents=True, exist_ok=True)
    cases = {'generic_box': '\nstruct Box<T> { T value; };\nT relay<T>(T value) { return value; }\nBox<int64 mut&> bad() {\n    int64 local = 1;\n    Box<int64 mut&> box;\n    box.value = &local;\n    return relay(box);\n}', 'generic_enum': '\nenum Choice<T> { Empty, Found(T value); };\nChoice<int64 mut&> bad() {\n    int64 local = 1;\n    return Choice<int64 mut&>::Found(&local);\n}', 'nested_result': '\nResult<Option<int64 mut&>, String> bad() {\n    int64 local = 1;\n    return Result<Option<int64 mut&>, String>::Ok(Option<int64 mut&>::Value(&local));\n}', 'callback_argument': '\n\nint64 mut& invoke(int64 mut& fn(int64 mut&) callback, int64 mut& value) { return callback(value); }\nint64 mut& bad() {\n    int64 local = 1;\n    int64 mut& fn(int64 mut&) callback = int64 mut& fn(int64 mut& value) { return value; };\n    return invoke(callback, &local);\n}', 'lambda_local': '\nvoid main() {\n    let callback = int64 mut& fn() {\n        int64 local = 1;\n        return &local;\n    };\n    return;\n}', 'lambda_value_parameter': '\nvoid main() {\n    let callback = int64 mut& fn(int64 value) { return &value; };\n    return;\n}', 'generic_tuple': '\nT relay<T>(T value) { return value; }\n(int64 const&, int64) bad() {\n    int64 local = 1;\n    (int64 const&, int64) pair = (&local, 2);\n    return relay(pair);\n}', 'cast_option': '\nOption<int64 mut&> bad() {\n    int64 local = 1;\n    int64 const& read = &local;\n    return Option<int64 mut&>::Value((int64 mut&)read);\n}', 'constructor_chain': '\nstruct Entry {\n    String name;\n    static Entry create(String name) { self.name = name; }\n    String const& borrow() { return &self.name; }\n};\nString const& bad() {\n    return Entry::create("temporary").borrow();\n}', 'static_factory_chain': '\nstruct Entry {\n    String name;\n    static Entry make() { Entry entry; entry.name = "temporary"; return entry; }\n    String const& borrow() { return &self.name; }\n};\nString const& bad() { return Entry::make().borrow(); }\n', 'enum_method_match': '\nint64 global = 0;\nenum Entry {\n    Empty, Ref(int64 const& value);\n    int64 const& borrow() {\n        match (self) {\n            Entry::Ref(p) => { return p; }\n            Entry::Empty => { return &global; }\n        }\n        return &global;\n    }\n};\nint64 const& bad() { int64 local = 1; return Entry::Ref(&local).borrow(); }\n', 'generic_list': '\nT relay<T>(T value) { return value; }\nList<int64 mut&> bad() {\n    int64 local = 1;\n    List<int64 mut&> values;\n    values.push(&local);\n    return relay(values);\n}', 'nested_alias': '\nstruct Box<T> { T value; };\nvoid bad() {\n    Box<Option<int64 mut&>> box;\n    Box<Option<int64 mut&>> mut& alias = &box;\n    {\n        int64 local = 1;\n        alias.value = Option<int64 mut&>::Value(&local);\n    }\n}', 'iterator_span': '\nimport std::iter;\nstd::iter::Iter<int64 const&> bad() {\n    int64 local[2];\n    local[0] = 1;\n    return std::iter::fromSpan<int64>(&local[0], 2);\n}'}
    for name, source in cases.items():
        source = source.lstrip('\n')
        if 'void main()' not in source:
            source += '\nvoid main() { }\n'
        path = WORK / (name + '.chl')
        path.write_text(source, encoding='utf-8')
        result = execute(compiler, '--check', path)
        assert result.returncode != 0 and 'CHL1204' in result.stderr, (name, result)
        result = execute(compiler, '--diagnostics=json', path)
        assert result.returncode != 0, (name, result)
        diagnostic = json.loads(result.stdout)['diagnostics'][0]
        assert diagnostic['code'] == 'CHL1204', (name, diagnostic)
        span = diagnostic['span']
        assert Path(span['file']).name == path.name, (name, diagnostic)
        lines = source.splitlines()
        assert 1 <= span['line'] <= len(lines) and span['column'] >= 1, (name, diagnostic)
        assert diagnostic['source'].strip() == lines[span['line'] - 1].strip(), (name, diagnostic)
        print(f'[通过] 拒绝：{name}', flush=True)
    source = ROOT / 'example/samples/reference_combinations.chl'
    expected = (ROOT / 'example/expected/reference_combinations.txt').read_text(encoding='utf-8')
    result = execute(compiler, 'run', source, '--out', WORK / 'normal-interpret')
    assert result.returncode == 0 and result.stdout == expected and (not result.stderr), result
    module = WORK / 'borrowed_module.chl'
    module.write_text('module borrowed_module;\nint64 shared = 70;\nT relay<T>(T value) { return value; }\nint64 const& sharedReference() { return &borrowed_module::shared; }\n', encoding='utf-8')
    source = WORK / 'module_main.chl'
    source.write_text('import borrowed_module;\nvoid main() {\nint64 value = 3;\nint64 mut& p = borrowed_module::relay(&value);\np = 4;\nassert(value == 4 && borrowed_module::sharedReference() == 70);\nprintln("module reference combinations OK");\nreturn;\n}\n', encoding='utf-8')
    result = execute(compiler, 'run', source, '--out', WORK / 'module-interpret')
    assert result.returncode == 0 and result.stdout == 'module reference combinations OK\n' and (not result.stderr), result
    module = WORK / 'bad_borrowed_module.chl'
    source = WORK / 'bad_module_main.chl'
    module.write_text('module bad_borrowed_module;\n' + cases['constructor_chain'].lstrip('\n'), encoding='utf-8')
    source.write_text('import bad_borrowed_module;\nvoid main() { return; }\n', encoding='utf-8')
    result = execute(compiler, '--check', source)
    assert result.returncode != 0 and 'CHL1204' in result.stderr, result
    result = execute(compiler, '--diagnostics=json', source)
    assert result.returncode != 0, result
    diagnostic = json.loads(result.stdout)['diagnostics'][0]
    assert diagnostic['code'] == 'CHL1204' and Path(diagnostic['span']['file']).name == module.name, diagnostic
    assert diagnostic['span']['line'] == 8 and diagnostic['span']['column'] >= 1, diagnostic
    assert diagnostic['source'].strip() == 'return Entry::create("temporary").borrow();', diagnostic
if __name__ == '__main__':
    main()
