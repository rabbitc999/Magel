"""验证 Any、枚举、闭包、结构体、接口胖引用与错误边界，无外部依赖。"""
from pathlib import Path
import argparse
import os
import subprocess

ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / "build/magel-object-types"
EXE = ".exe" if os.name == "nt" else ""


def execute(*args):
    """收集中文诊断和资源泄漏信息，限制程序执行时间。"""
    return subprocess.run([str(x) for x in args], cwd=ROOT, env=dict(os.environ, MAGEL_LANG="zh"),
                          capture_output=True, encoding="utf-8", errors="replace", timeout=240)


def main():
    """三种执行路径验证正常组合，错误用例区分编译期与运行期。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--magel", type=Path, default=ROOT / ("magel" + EXE))
    parser.add_argument("--vm", type=Path)
    args = parser.parse_args()
    compiler = args.magel.resolve()
    WORK.mkdir(parents=True, exist_ok=True)
    source = ROOT / "example/samples/object_type_combinations.mgl"
    expected = "object type combinations OK\n"
    for backend in ("run", "aot"):
        output = WORK / (backend + EXE)
        result = execute(compiler, backend, source, "--out", output, *(["--opt-level=0"] if backend == "aot" else []))
        assert result.returncode == 0, (backend, result.stderr)
        if backend == "aot":
            result = execute(output)
        assert result.returncode == 0 and result.stdout == expected and not result.stderr, result
    if args.vm:
        prefix = WORK / "binary"
        result = execute(compiler, "svmb", source, "--out", prefix)
        assert result.returncode == 0, result.stderr
        result = execute(args.vm.resolve(), "run", prefix.with_suffix(".svmb"), "--data", prefix.with_suffix(".data.bin"), "--layout", prefix.with_suffix(".layout"))
        assert result.returncode == 0 and result.stdout.partition("\n\n;")[0] == expected.rstrip() and not result.stderr, result
        assert "没释放" not in result.stdout, result
    errors = {
        "local_struct": "struct S{String name;};int64 main(){S value;Any box;box.set(value);return 0;}",
        "local_enum": "enum E{Empty,Text(String value);};int64 main(){E value=E::Text(\"x\");Any box;box.set(value);return 0;}",
        "temporary_struct": "struct S{String name;};S make(){S value;return value;}int64 main(){Any box;box.set(make());return 0;}",
        "local_array": "int64 main(){int64 values[2];Any box;box.set(values);return 0;}",
        "runtime_array": "int64 main(){int64 values[]=new[2];Any box;box.set(values);return 0;}",
        "readonly_function": "int64 fn(int64) global;int64 add(int64 x){return x;}int64 main(){global=add;Any box;box.set(&global);(int64 fn(int64)) const& ref=box.getReference();ref=add;return 0;}",
        "readonly_object": "int64 main(){const Any box=1;box.set(2);return 0;}",
        "get_value_reference": "int64 global;int64 main(){Any box;box.set(&global);int64 const& ref=box.getValue();return 0;}",
        "get_reference_value": "int64 main(){Any box;box.set(1);int64 value=box.getReference();return 0;}",
        "optional_value": "int64 main(){Any box;Option<int64> value=box.getOptional();return 0;}",
        "escape_box": "Any bad(){int64 local=1;Any box;box.set(&local);return box;}int64 main(){return 0;}",
        "escape_box_copy": "Any bad(){int64 local=1;Any box;box.set(&local);return box.copy();}int64 main(){return 0;}",
        "escape_optional": "Option<int64 const&> bad(){int64 local=1;Any box;box.set(&local);return box.getOptional();}int64 main(){return 0;}",
        "escape_reference": "int64 const& bad(){int64 local=1;Any box;box.set(&local);return box.getReference();}int64 main(){return 0;}",
        "escape_forwarded": "Any wrap(int64 const& ref){Any box;box.set(&ref);return box;}Any bad(){int64 local=1;return wrap(&local);}int64 main(){return 0;}",
        "escape_container": "List<Any> bad(){int64 local=1;Any box;box.set(&local);List<Any> values;values.push(box);return values;}int64 main(){return 0;}",
        "escape_global": "Any global;int64 main(){int64 local=1;global.set(&local);return 0;}",
        "escape_inner_block": "int64 main(){Any box;{int64 local=1;box.set(&local);}return 0;}",
        "opaque_argument": "Any global;void store(Any box){global=box;}int64 main(){int64 local=1;Any box;box.set(&local);store(box);return 0;}",
        "opaque_function_argument": "Any global;void store(Any box){global=box;}int64 main(){int64 local=1;Any box;box.set(&local);void fn(Any) callback=store;callback(box);return 0;}",
        "opaque_variadic_argument": "Any global;void store(Any... boxes){global=boxes[0];}int64 main(){int64 local=1;Any box;box.set(&local);store(box);return 0;}",
        "opaque_enum_receiver": "Any global;Any save(Any box){global=box;return box;}int64 main(){int64 local=1;Any box;box.set(&local);Option<Any> item=Option<Any>::Value(box);item.transform(save);return 0;}",
        "opaque_struct_argument": "struct S{Any value;};S global;void save(S value){global=value;}int64 main(){int64 local=1;S value;value.value.set(&local);save(value);return 0;}",
    }
    for name, text in errors.items():
        invalid = WORK / (name + ".mgl")
        invalid.write_text(text + "\n", encoding="utf-8")
        result = execute(compiler, "--check", invalid)
        assert result.returncode != 0 and result.stderr, (name, result)
        if name.startswith(("escape_", "opaque_")):
            assert "MGL1204" in result.stderr, (name, result.stderr)
        if name.startswith("readonly_"):
            assert "只读" in result.stderr or "不可变" in result.stderr, (name, result.stderr)
    runtime_errors = {
        "scalar_mismatch": 'int64 main(){Any box;box.set("x");int64 value=box.getValue();return value;}',
        "function_mismatch": 'int64 add(int64 x){return x;}int64 main(){Any box;box.set(add);int64 fn(float64) value=box.getValue();return 0;}',
        "enum_mismatch": 'enum A{X;};enum B{Y;};A global;int64 main(){Any box;box.set(global);B value=box.getValue();return 0;}',
        "empty_value": 'int64 main(){Any box;int64 value=box.getValue();return value;}',
        "readonly_interface": 'interface I{void bump();};struct S:I{int64 value;void bump(){self.value=self.value+1;}};S global;int64 main(){Any box;box.set(&global);I const& view=box.getReference();view.bump();return 0;}',
    }
    for name, text in runtime_errors.items():
        invalid = WORK / (name + ".mgl")
        invalid.write_text(text + "\n", encoding="utf-8")
        result = execute(compiler, "run", invalid, "--out", WORK / name)
        assert result.returncode == 1 and "运行期错误" in result.stderr, (name, result)
        if name == "readonly_interface":
            assert "只读接收者不能调用修改方法" in result.stderr, result.stderr
        executable = WORK / (name + EXE)
        result = execute(compiler, "aot", invalid, "--out", executable, "--opt-level=0")
        assert result.returncode == 0, result.stderr
        result = execute(executable)
        assert result.returncode != 0 and result.stderr, (name, result)
    print(f"Any 类型组合：解释执行、AOT、{('svmb、' if args.vm else '')}{len(errors)} 项编译拒绝、{len(runtime_errors)} 项运行类型错误通过")


if __name__ == "__main__":
    main()
