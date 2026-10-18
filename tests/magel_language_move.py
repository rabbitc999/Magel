"""验证 std::lang::move 的资源所有权转移语义、别名清理与 const 引用 drop 限制。"""
from pathlib import Path
import argparse
import os
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    """同时检查解释执行、AOT 与编译期错误。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--magel", type=Path, default=ROOT / ("magel.exe" if os.name == "nt" else "magel"))
    compiler = parser.parse_args().magel.resolve()
    if os.name == "nt" and not compiler.is_file():
        compiler = compiler.with_suffix(".exe")
    env = dict(os.environ, MAGEL_LANG="zh")

    def run(*args, cwd=ROOT):
        return subprocess.run([str(arg) for arg in args], cwd=cwd, env=env,
                              capture_output=True, encoding="utf-8", errors="replace", timeout=240)

    with tempfile.TemporaryDirectory(prefix="magel-language-move-") as directory:
        work = Path(directory)
        standalone = work / ("magel.exe" if os.name == "nt" else "magel")
        shutil.copy2(compiler, standalone)
        for backend in ("run", "aot"):
            output = work / (backend + (".exe" if os.name == "nt" else ""))
            invocation = compiler if backend == "run" else standalone
            result = run(invocation, backend, ROOT / "tests/fixtures/language_move.mgl", "out", output,
                         cwd=ROOT if backend == "run" else work)
            assert result.returncode == 0, (backend, result.stdout, result.stderr)
            if backend == "aot": result = run(output)
            assert (result.returncode, result.stdout, result.stderr) == (0, "move ok\n", ""), result
        for backend in ("run", "aot"):
            output = work / ("file-" + backend + (".exe" if os.name == "nt" else ""))
            result = run(compiler, backend, ROOT / "tests/fixtures/move_file.mgl", "out", output)
            assert result.returncode == 0, (backend, result.stdout, result.stderr)
            if backend == "aot": result = run(output)
            assert (result.returncode, result.stdout, result.stderr) == (0, "file move ok\n", ""), result
            assert (ROOT / "build/move-file.bin").read_bytes() == b"moved"
            assert (ROOT / "build/move-stream.bin").read_bytes() == b"stream"
            assert (ROOT / "build/move-closure-file.bin").read_bytes() == b"closure"
            assert (ROOT / "build/move-closure-stream.bin").read_bytes() == b"owned"
        prefix = "struct S : Drop { int64 value; void drop() {} };"
        cases = {
            "const_drop": ("void f(S const& p){p.drop();}", "不能调用 drop"),
            "const_dereference": ("void f(S const& p){(*p).drop();}", "不能调用 drop"),
            "const_field": ("struct B{S item;};void f(B const& p){p.item.drop();}", "不能调用 drop"),
            "const_interface": ("void f(Drop const& p){p.drop();}", "不能调用 drop"),
            "const_alias": ("void f(){S s;S const& p=&s;p.drop();}", "不能调用 drop"),
            "const_value": ("void f(){S original;const S s=original;s.drop();}", "不能调用 drop"),
            "const_move": ("void f(S const& p){S s=std::lang::move(p);}", "只接受可写引用"),
            "value_move": ("void f(){S s;S result=std::lang::move(s);}", "只接受可写引用"),
            "const_move_method": ("struct B{S item;S take(){return std::lang::move(&self.item);}};void f(B const& p){S s=p.take();}", "只读接收者"),
            "borrow_escape": ("struct Borrowed{int64 const& value;};Borrowed f(){int64 local=1;Borrowed s;s.value=&local;return std::lang::move(&s);}", "MGL1204"),
            "any_escape": ("Any f(){int64 local=1;Any box=&local;return move(&box);}", "MGL1204"),
            "any_global": ("Any global;void f(){int64 local=1;Any box=&local;global=move(&box);}", "MGL1204"),
            "nested_any_escape": ("struct B{Any value;};B f(){int64 local=1;B box;box.value.set(&local);return move(&box);}", "MGL1204"),
            "redeclare_live": ("void f(){S s;S s;}", "不能重复声明"),
            "redeclare_live_string": ('void f(){String a="1";String a="2";}', "不能重复声明"),
            "redeclare_branch": ("void f(bool flag){S s;if(flag){S t=std::lang::move(&s);}S s;}", "不能重复声明"),
            "redeclare_field": ("struct B{S item;};void f(){B b;S t=std::lang::move(&b.item);B b;}", "不能重复声明"),
            "redeclare_reset": ("void f(){S s;S t=std::lang::move(&s);s=t;S s;}", "不能重复声明"),
            "redeclare_old_alias": ("void f(){S s;S mut& p=&s;S t=std::lang::move(&s);S s;int64 x=p.value;}", "MGL1205"),
            "scalar_read": ("void f(){int64 a=1;int64 b=move(&a);int64 c=a;}", "MGL1205"),
            "scalar_repeat": ("void f(){int64 a=1;int64 b=move(&a);int64 c=move(&a);}", "MGL1205"),
            "string_read": ('void f(){String a="x";String b=move(&a);println(a);}', "MGL1205"),
            "arity": ("void f(){std::lang::move();}", "需要一个"),
        }
        for name, (body, expected) in cases.items():
            source = work / (name + ".mgl")
            source.write_text(prefix + body + "int64 main(){return 0;}", encoding="utf-8")
            result = run(compiler, "check", source, "json")
            assert result.returncode != 0 and expected in result.stdout + result.stderr, (name, result)
        repeated = work / "repeated.mgl"
        repeated.write_text(prefix + "int64 main(){S s;S a=std::lang::move(&s);S b=std::lang::move(&s);return 0;}", encoding="utf-8")
        result = run(compiler, "run", repeated, "out", work / "repeated")
        assert result.returncode != 0 and "源对象已经销毁" in result.stdout + result.stderr, result
        mutable = work / "mutable.mgl"
        mutable.write_text("int64 move(int64 value){return value+1;}int64 count=0;struct S:Drop{void drop(){count=count+1;}};int64 main(){{S s;s.drop();}assert(count==1 && move(3)==4);return 0;}", encoding="utf-8")
        result = run(compiler, "run", mutable, "out", work / "mutable")
        assert (result.returncode, result.stdout, result.stderr) == (0, "", ""), result
    print("std::lang::move 回归通过（解释执行、AOT、别名与嵌套清理、重复移动、移动后访问诊断）")


if __name__ == "__main__":
    main()
