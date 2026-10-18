"""验证泛型单态化、嵌套组合、类型身份、推导与非法实例诊断。"""
from pathlib import Path
import argparse
import os
import subprocess
import shutil
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    """运行底层展开测试、解释执行、AOT 和非法泛型回归。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--magel", type=Path, default=ROOT / ("magel.exe" if os.name == "nt" else "magel"))
    args = parser.parse_args()
    compiler = args.magel.resolve()
    if os.name == "nt" and not compiler.is_file():
        compiler = compiler.with_suffix(".exe")
    env = dict(os.environ, MAGEL_LANG="zh")

    def run(*command, cwd=ROOT):
        return subprocess.run([str(x) for x in command], cwd=cwd, env=env,
                              capture_output=True, encoding="utf-8", errors="replace", timeout=240)

    with tempfile.TemporaryDirectory(prefix="magel-generic-monomorphization-") as folder:
        work = Path(folder)
        unit = work / ("unit.exe" if os.name == "nt" else "unit")
        result = run("g++", "-std=c++17", "-I" + str(ROOT / "source"), "-I" + str(ROOT / "library"), "-I" + str(ROOT / "library/preprocess"), "-I" + str(ROOT / "library/stackvm"),
                     ROOT / "tests/magel_generic_monomorphization_test.cpp", "-o", unit)
        assert result.returncode == 0, result.stderr
        result = run(unit)
        assert (result.returncode, result.stdout, result.stderr) == (0, "generic monomorphization unit tests OK\n", ""), result
        standalone = work / ("magel.exe" if os.name == "nt" else "magel")
        shutil.copy2(compiler, standalone)
        source = ROOT / "tests/fixtures/generic_monomorphization.mgl"
        for backend in ("run", "aot"):
            output = work / (backend + (".exe" if os.name == "nt" else ""))
            result = run(standalone if backend == "aot" else compiler, backend, source, "out", output,
                         cwd=work if backend == "aot" else ROOT)
            assert result.returncode == 0, (backend, result.stdout, result.stderr)
            if backend == "aot":
                result = run(output)
            assert (result.returncode, result.stdout, result.stderr) == (0, "generic monomorphization ok\n", ""), result
            print(backend + "：泛型单态化与嵌套组合通过", flush=True)
        host_output = work / ("host.exe" if os.name == "nt" else "host")
        result = run(standalone, "aot", ROOT / "tests/fixtures/generic_monomorphization_glue.mgl",
                     "--glue", ROOT / "tests/fixtures/generic_monomorphization_glue.cpp", "out", host_output, cwd=work)
        assert result.returncode == 0, (result.stdout, result.stderr)
        result = run(host_output)
        assert (result.returncode, result.stdout, result.stderr) == (0, "generic host symbols ok\n", ""), result
        print("AOT：复杂类型参数的宿主泛型绑定通过", flush=True)
        cases = {
            "too_many": ("struct G<T>{T value;};int64 main(){G<int64,String> g;return 0;}", "至多接受"),
            "duplicate_parameter": ("struct G<T,T>{T value;};", "重复的泛型参数"),
            "forward_default": ("struct G<T=V,V=int64>{T value;};", "只能引用前面的类型参数"),
            "self_default": ("struct G<T=T>{T value;};", "只能引用前面的类型参数"),
            "empty_default": ("struct G<T=>{T value;};", "默认类型为空"),
            "plain_constraint": ("interface I{int64 number();};struct G<T:I>{T value;};int64 main(){G<int64> g;return 0;}", "未实现接口"),
            "generic_constraint": ("interface I{int64 number();};struct B<T>{T value;};struct G<T:I>{T value;};int64 main(){G<B<int64>> g;return 0;}", "未实现接口"),
            "nested_constraint": ("interface I{int64 number();};struct B<T:I>{T value;};struct G<T>{B<T> value;};int64 main(){G<int64> g;return 0;}", "未实现接口"),
            "inference_conflict": ("T same<T>(T a,T b){return a;}int64 main(){same(1,\"bad\");return 0;}", "推导冲突"),
            "unknown_type": ("struct G<T>{T value;};int64 main(){G<Missing> g;return 0;}", "Missing"),
            "value_recursion": ("struct G<T>{G<T> value;};int64 main(){G<int64> g;return 0;}", "绕回了自己"),
            "growing_recursion": ("struct G<T>{G<List<T>> mut& next;};int64 main(){G<int64> g;return 0;}", "递归类型展开"),
            "recursive_default": ("struct G<T=G<>>{T value;};int64 main(){G<> g;return 0;}", "递归默认类型"),
            "readonly_generic": ("struct G<T>{T value;};void change<T>(G<T> const& g){g.value=1;}int64 main(){G<int64> g;change(&g);return 0;}", "只读"),
            "nested_borrow_escape": ("struct G<T>{T value;};G<G<int64 mut&>> bad(){int64 local=1;G<G<int64 mut&>> g;g.value.value=&local;return g;}", "生命周期"),
        }
        for name, (code, diagnostic) in cases.items():
            if "main()" not in code:
                code += "int64 main(){return 0;}"
            invalid = work / (name + ".mgl")
            invalid.write_text(code, encoding="utf-8")
            result = run(compiler, "check", invalid, "json")
            assert result.returncode != 0 and diagnostic in result.stdout + result.stderr, (name, result.returncode, result.stdout, result.stderr)
    print("泛型回归通过（底层展开、解释执行、AOT、" + str(len(cases)) + " 项错误诊断）")


if __name__ == "__main__":
    main()
