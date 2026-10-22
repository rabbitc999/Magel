"""验证 Any、枚举、闭包、结构体、接口胖引用与错误边界，无外部依赖。"""
from pathlib import Path
import argparse
import os
import subprocess
ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / 'build/chali-object-types'
EXE = '.exe' if os.name == 'nt' else ''

def execute(*args):
    """收集中文诊断和资源泄漏信息，限制程序执行时间。"""
    return subprocess.run([str(x) for x in args], cwd=ROOT, env=dict(os.environ, CHALI_LANG='zh'), capture_output=True, encoding='utf-8', errors='replace', timeout=240)

def main():
    """三种执行路径验证正常组合，错误用例区分编译期与运行期。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--chali', type=Path, default=ROOT / ('chali' + EXE))
    parser.add_argument('--vm', type=Path)
    args = parser.parse_args()
    compiler = args.chali.resolve()
    WORK.mkdir(parents=True, exist_ok=True)
    source = ROOT / 'example/samples/object_type_combinations.chl'
    expected = 'object type combinations OK\n'
    for backend in ('run',):
        output = WORK / (backend + EXE)
        result = execute(compiler, backend, source, '--out', output, *[])
        assert result.returncode == 0, (backend, result.stderr)
        assert result.returncode == 0 and result.stdout == expected and (not result.stderr), result
    if args.vm:
        prefix = WORK / 'binary'
        result = execute(compiler, 'svmb', source, '--out', prefix)
        assert result.returncode == 0, result.stderr
        result = execute(args.vm.resolve(), 'run', prefix.with_suffix('.svmb'), '--data', prefix.with_suffix('.data.bin'), '--layout', prefix.with_suffix('.layout'))
        assert result.returncode == 0 and result.stdout.partition('\n\n;')[0] == expected.rstrip() and (not result.stderr), result
        assert '没释放' not in result.stdout, result
    errors = {'local_struct': 'struct S{String name;};void main(){S value;Any box;box.set(value);}', 'local_enum': 'enum E{Empty,Text(String value);};void main(){E value=E::Text("x");Any box;box.set(value);}', 'temporary_struct': 'struct S{String name;};S make(){S value;return value;}void main(){Any box;box.set(make());}', 'local_array': 'void main(){int64 values[2];Any box;box.set(values);}', 'runtime_array': 'void main(){int64 values[]=new[2];Any box;box.set(values);}', 'readonly_function': 'int64 fn(int64) global;int64 add(int64 x){return x;}void main(){global=add;Any box;box.set(&global);(int64 fn(int64)) const& ref=box.getReference();ref=add;}', 'readonly_object': 'void main(){const Any box=1;box.set(2);}', 'get_value_reference': 'int64 global;void main(){Any box;box.set(&global);int64 const& ref=box.getValue();}', 'get_reference_value': 'void main(){Any box;box.set(1);int64 value=box.getReference();}', 'optional_value': 'void main(){Any box;Option<int64> value=box.getOptional();}', 'escape_box': 'Any bad(){int64 local=1;Any box;box.set(&local);return box;}void main(){}', 'escape_box_copy': 'Any bad(){int64 local=1;Any box;box.set(&local);return box.copy();}void main(){}', 'escape_optional': 'Option<int64 const&> bad(){int64 local=1;Any box;box.set(&local);return box.getOptional();}void main(){}', 'escape_reference': 'int64 const& bad(){int64 local=1;Any box;box.set(&local);return box.getReference();}void main(){}', 'escape_forwarded': 'Any wrap(int64 const& ref){Any box;box.set(&ref);return box;}Any bad(){int64 local=1;return wrap(&local);}void main(){}', 'escape_container': 'List<Any> bad(){int64 local=1;Any box;box.set(&local);List<Any> values;values.push(box);return values;}void main(){}', 'escape_global': 'Any global;void main(){int64 local=1;global.set(&local);}', 'escape_inner_block': 'void main(){Any box;{int64 local=1;box.set(&local);}}', 'opaque_argument': 'Any global;void store(Any box){global=box;}void main(){int64 local=1;Any box;box.set(&local);store(box);}', 'opaque_function_argument': 'Any global;void store(Any box){global=box;}void main(){int64 local=1;Any box;box.set(&local);void fn(Any) callback=store;callback(box);}', 'opaque_variadic_argument': 'Any global;void store(Any... boxes){global=boxes[0];}void main(){int64 local=1;Any box;box.set(&local);store(box);}', 'opaque_enum_receiver': 'Any global;Any save(Any box){global=box;return box;}void main(){int64 local=1;Any box;box.set(&local);Option<Any> item=Option<Any>::Value(box);item.transform(save);}', 'opaque_struct_argument': 'struct S{Any value;};S global;void save(S value){global=value;}void main(){int64 local=1;S value;value.value.set(&local);save(value);}'}
    for name, text in errors.items():
        invalid = WORK / (name + '.chl')
        invalid.write_text(text + '\n', encoding='utf-8')
        result = execute(compiler, '--check', invalid)
        assert result.returncode != 0 and result.stderr, (name, result)
        if name.startswith(('escape_', 'opaque_')):
            assert 'CHL1204' in result.stderr, (name, result.stderr)
        if name.startswith('readonly_'):
            assert '只读' in result.stderr or '不可变' in result.stderr, (name, result.stderr)
    runtime_errors = {'scalar_mismatch': 'void main(){Any box;box.set("x");int64 value=box.getValue();let __boom = value;}', 'function_mismatch': 'int64 add(int64 x){return x;}void main(){Any box;box.set(add);int64 fn(float64) value=box.getValue();}', 'enum_mismatch': 'enum A{X;};enum B{Y;};A global;void main(){Any box;box.set(global);B value=box.getValue();}', 'empty_value': 'void main(){Any box;int64 value=box.getValue();let __boom = value;}', 'readonly_interface': 'interface I{void bump();};struct S:I{int64 value;void bump(){self.value=self.value+1;}};S global;void main(){Any box;box.set(&global);I const& view=box.getReference();view.bump();}'}
    for name, text in runtime_errors.items():
        invalid = WORK / (name + '.chl')
        invalid.write_text(text + '\n', encoding='utf-8')
        result = execute(compiler, 'run', invalid, '--out', WORK / name)
        assert result.returncode == 1 and '运行期错误' in result.stderr, (name, result)
        if name == 'readonly_interface':
            assert '只读接收者不能调用修改方法' in result.stderr, result.stderr
        executable = WORK / (name + EXE)
if __name__ == '__main__':
    main()
