"""验证函数默认参数的绑定、求值时机、资源与错误诊断。"""
from pathlib import Path
import argparse
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    """运行解释与 AOT 回归，并检查非法默认参数。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--magel", type=Path, default=ROOT / ("magel.exe" if os.name == "nt" else "magel"))
    args = parser.parse_args()
    compiler = args.magel.resolve()
    env = dict(os.environ, MAGEL_LANG="zh")

    def run(*command):
        return subprocess.run([str(x) for x in command], cwd=ROOT, env=env,
                              capture_output=True, encoding="utf-8", errors="replace", timeout=240)

    with tempfile.TemporaryDirectory(prefix="magel-default-arguments-") as folder:
        work = Path(folder)
        source = ROOT / "tests/fixtures/default_arguments.mgl"
        for backend in ("run", "aot"):
            output = work / (backend + (".exe" if os.name == "nt" else ""))
            result = run(compiler, backend, source, "out", output)
            assert result.returncode == 0, (backend, result.stdout, result.stderr)
            if backend != "run":
                result = run(output)
            assert (result.returncode, result.stdout, result.stderr) == (0, "defaults ok\n", ""), result
        cases = {
            "invalid_expression_unused": ('int64 f(int64 a=true+1){return a;}', "不能隐式混用"),
            "required_after_default": ('int64 f(int64 a=1,int64 b){return b;}', "默认参数后不能有必填参数"),
            "wrong_type_unused": ('int64 f(int64 a="bad"){return a;}', "类型"),
            "parameter_reference": ('int64 f(int64 a,int64 b=a){return b;}', "不能引用形参"),
            "unknown_name": ('int64 f(int64 a=missing){return a;}', "missing"),
            "missing_required": ('int64 f(int64 a,int64 b=2){return a+b;} int64 main(){return f();}', "重载"),
            "too_many": ('int64 f(int64 a=2){return a;} int64 main(){return f(1,2);}', "重载"),
            "ambiguous": ('int64 f(int64 a=1){return a;} int64 f(String a="x"){return 2;} int64 main(){return f();}', "多个重载"),
            "function_pointer": ('int64 f(int64 a=1){return a;} int64 main(){int64 fn(int64) p=f;return p();}', "实参个数"),
            "enum_payload": ('enum E{Value(int64 a=1);};', "载荷不能使用默认参数"),
            "interface": ('interface I{int64 f(int64 a=1);};', "接口方法暂不支持默认参数"),
            "self_reference": ('struct S{int64 x;int64 f(int64 a=self.x){return a;}};', "不能引用 self"),
        }
        for name, (code, diagnostic) in cases.items():
            if "main()" not in code:
                code += "int64 main(){return 0;}"
            invalid = work / (name + ".mgl")
            invalid.write_text(code, encoding="utf-8")
            result = run(compiler, "check", invalid, "json")
            assert result.returncode != 0 and diagnostic in result.stdout + result.stderr, (name, result.stdout, result.stderr)
    print("函数默认参数回归通过（解释执行、AOT、12 项错误诊断）")


if __name__ == "__main__":
    main()
