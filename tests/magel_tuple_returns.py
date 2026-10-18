"""验证逗号返回、泛型元组和已有左值的解构赋值。"""
from pathlib import Path
import argparse
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    """在解释器和 AOT 中验证元组值语义及非法赋值诊断。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--magel", type=Path, default=ROOT / ("magel.exe" if os.name == "nt" else "magel"))
    compiler = parser.parse_args().magel.resolve()
    env = dict(os.environ, MAGEL_LANG="zh")

    def run(*command):
        return subprocess.run([str(x) for x in command], cwd=ROOT, env=env,
                              capture_output=True, encoding="utf-8", errors="replace", timeout=240)

    with tempfile.TemporaryDirectory(prefix="magel-tuple-returns-") as folder:
        work = Path(folder)
        for backend in ("run", "aot"):
            output = work / (backend + (".exe" if os.name == "nt" else ""))
            result = run(compiler, backend, ROOT / "tests/fixtures/tuple_returns.mgl", "out", output)
            assert result.returncode == 0, (backend, result.stdout, result.stderr)
            if backend == "aot":
                result = run(output)
            assert (result.returncode, result.stdout, result.stderr) == (0, "tuple returns ok\n", ""), result
            print(backend + "：多值返回与泛型元组通过", flush=True)
        # 括号目标的交换也必须先保存完整右值，覆盖字符串复制和泛型返回。
        for backend, no_opt in (("run", False), ("run", True), ("aot", False)):
            output = work / ("swap-" + backend + ("-no-opt" if no_opt else "") + (".exe" if os.name == "nt" else ""))
            previous = env.get("MAGEL_NO_OPT")
            if no_opt:
                env["MAGEL_NO_OPT"] = "1"
            result = run(compiler, backend, ROOT / "example/samples/tuple_swap.mgl", "out", output)
            if previous is None:
                env.pop("MAGEL_NO_OPT", None)
            else:
                env["MAGEL_NO_OPT"] = previous
            assert result.returncode == 0, (backend, no_opt, result.stdout, result.stderr)
            if backend == "aot":
                result = run(output)
            assert (result.returncode, result.stdout, result.stderr) == (0, "tuple swap OK\n", ""), result
            print(backend + (" 关闭优化" if no_opt else "") + "：括号元组交换通过", flush=True)
        cases = {
            "parenthesized_arity": ("int64 a,b;(a,b)=(1,2,3);", "数量"),
            "parenthesized_const": ("const int64 a=1;int64 b;(a,b)=(2,3);", "不可变"),
            "arity": ("int64 a,b;a,b=(1,2,3);", "数量"),
            "shape": ("int64 a,b,c;a,(b,c)=(1,2);", "形状"),
            "not_tuple": ("int64 a,b;a,b=1;", "必须是元组"),
            "const_target": ("const int64 a=1;int64 b;a,b=(2,3);", "不可变"),
            "readonly": ("int64 a=1;int64 const& r=&a;int64 b;r,b=(2,3);", "只读"),
            "type_mismatch": ("int64 a,b;a,b=(1,\"wrong\");", "不能隐式转换"),
            "invalid_target": ("int64 b;1,b=(1,2);", "赋值"),
            "unknown_target": ("int64 b;missing,b=(1,2);", "missing"),
            "return_mismatch": ("", "不能隐式转换"),
            "missing_assignment": ("int64 a,b;a,b;", "必须使用"),
            "tuple_readonly": ("(int64,int64) pair=(1,2);int64 b;pair.0,b=(3,4);", "元组"),
            "return_borrow": ("", "生命周期"),
            "capture_write": ("const int64 a=0;int64 b;auto f=void fn(){a,b=(1,2);};f();", "不可变"),
        }
        for name, (body, diagnostic) in cases.items():
            code = "int64 main(){" + body + "return 0;}"
            if name == "return_mismatch":
                code = "(int64,String) bad(){return 1,2;}int64 main(){return 0;}"
            if name == "return_borrow":
                code = "(int64 mut&,int64) bad(){int64 local=1;return &local,1;}int64 main(){return 0;}"
            source = work / (name + ".mgl")
            source.write_text(code, encoding="utf-8")
            result = run(compiler, "check", source, "json")
            assert result.returncode != 0 and diagnostic in result.stdout + result.stderr, (name, result.returncode, result.stdout, result.stderr)
    print("元组返回回归通过（解释执行、AOT、" + str(len(cases)) + " 项错误诊断）")


if __name__ == "__main__":
    main()
