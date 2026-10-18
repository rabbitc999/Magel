"""验证引用返回与跨作用域保存的生命周期检查，不依赖第三方包。"""
from pathlib import Path
import argparse
import json
import os
import subprocess

ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / "build/magel-reference-escape"


def execute(*args):
    """运行工具并保留诊断和资源清理输出。"""
    return subprocess.run([str(arg) for arg in args], cwd=ROOT, capture_output=True,
                          encoding="utf-8", errors="replace", timeout=180)


def main():
    """运行正常生命周期及独立的逃逸拒绝场景。"""
    parser = argparse.ArgumentParser()
    parser.add_argument("--magel", type=Path, default=ROOT / "magel.exe")
    args = parser.parse_args()
    compiler = args.magel.resolve()
    WORK.mkdir(parents=True, exist_ok=True)
    cases = {
        "local": "int64 mut& bad(){int64 x=1;return &x;}",
        "alias": "int64 mut& bad(){int64 x=1;int64 mut& a=&x;int64 const& b=a;return (int64 mut&)b;}",
        "value_parameter": "int64 mut& bad(int64 x){return &x;}",
        "string_parameter": "String const& bad(String x){return &x;}",
        "struct_parameter": "struct S{int64 x;};int64 mut& bad(S s){return &s.x;}",
        "field": "struct S{int64 x;};int64 mut& bad(){S s;return &s.x;}",
        "identity": "int64 mut& identity(int64 mut& x){return x;}int64 mut& bad(){int64 x=1;return identity(&x);}",
        "option": "Option<int64 mut&> bad(){int64 x=1;return Option<int64 mut&>::Value(&x);}",
        "option_alias": "Option<int64 mut&> bad(){int64 x=1;Option<int64 mut&> a=Option<int64 mut&>::Value(&x);return a;}",
        "option_unwrap": "int64 mut& bad(){int64 x=1;Option<int64 mut&> a=Option<int64 mut&>::Value(&x);return a.unwrap();}",
        "tuple": "(int64 mut&,int64) bad(){int64 x=1;return (&x,2);}",
        "global_store": "Option<int64 mut&> saved;void bad(){int64 x=1;saved=Option<int64 mut&>::Value(&x);}",
        "parameter_store": "Option<int64 mut&> saved;void bad(int64 mut& x){saved=Option<int64 mut&>::Value(x);}",
        "outer_store": "void bad(){Option<int64 mut&> saved;{int64 x=1;saved=Option<int64 mut&>::Value(&x);}}",
        "output_store": "void bad(Option<int64 mut&> mut& saved){int64 x=1;saved=Option<int64 mut&>::Value(&x);}",
        "reference_field": "struct Box{int64 mut& value;};Box bad(){int64 x=1;Box b;b.value=&x;return b;}",
        "outer_field": "struct Box{int64 mut& value;};void bad(){Box b;{int64 x=1;b.value=&x;}}",
        "rebind": "void bad(){int64 x=1;int64 mut& p=&x;{int64 y=2;p=&y;}}",
        "destructure": "int64 const& bad(){int64 x=1;(int64 const&,int64) pair=(&x,2);auto (p,n)=pair;return p;}",
        "match": "int64 mut& bad(){int64 x=1;Option<int64 mut&> a=Option<int64 mut&>::Value(&x);match(a){Option<int64 mut&>::Value(p)=>{return p;}Option<int64 mut&>::None=>{return &global;}}return &global;}int64 global=0;",
        "list_push": "List<int64 mut&> bad(){int64 x=1;List<int64 mut&> values;values.push(&x);return values;}",
        "outer_list": "void bad(){List<int64 mut&> values;{int64 x=1;values.push(&x);}}",
        "list_index": "List<int64 mut&> bad(){int64 x=1;List<int64 mut&> values;values.push(&global);values[0]=&x;return values;}int64 global=0;",
        "allocated_payload": "struct Box{int64 mut& value;Box(int64 mut& x){self.value=&x;}};Box mut& bad(){int64 x=1;Box mut& b=new(&x);return b;}",
        "temporary_receiver": "struct S{int64 x;int64 mut& reference(){return &self.x;}};S make(){S x;return x;}int64 mut& bad(){return make().reference();}",
        "field_alias": "struct Box{int64 mut& value;};Box bad(){int64 x=1;Box b;Box mut& p=&b;p.value=&x;return b;}",
        "list_alias": "List<int64 mut&> bad(){int64 x=1;List<int64 mut&> values;List<int64 mut&> mut& p=&values;p.push(&x);return values;}",
    }
    for name, source in cases.items():
        path = WORK / (name + ".mgl")
        path.write_text(source + "\nint64 main(){return 0;}\n", encoding="utf-8")
        result = execute(compiler, "--check", path)
        assert result.returncode != 0 and "MGL1204" in result.stderr, (name, result)
        result = execute(compiler, "--diagnostics=json", path)
        diagnostic = json.loads(result.stdout)["diagnostics"][0]
        assert result.returncode != 0 and diagnostic["code"] == "MGL1204", (name, result)
        assert Path(diagnostic["span"]["file"]).name == path.name, (name, diagnostic)
        assert diagnostic["span"]["line"] == 1 and diagnostic["span"]["column"] >= 1, (name, diagnostic)
    normal = ROOT / "example/samples/reference_lifetimes.mgl"
    result = execute(compiler, "run", normal, "--out", WORK / "normal-interpret")
    assert result.returncode == 0 and result.stdout == "reference lifetimes OK\n" and not result.stderr, result
    output = WORK / ("normal-aot.exe" if os.name == "nt" else "normal-aot")
    result = execute(compiler, "aot", normal, "--out", output, "--opt-level=0")
    assert result.returncode == 0, result
    result = execute(output)
    assert result.returncode == 0 and result.stdout == "reference lifetimes OK\n" and not result.stderr, result
    print(f"引用生命周期验证通过：{len(cases)} 个拒绝场景，正常样例解释/AOT")


if __name__ == "__main__":
    main()
