"""检查返回控制表达式、局部具名闭包与字段派生的语义边界。"""
from pathlib import Path
import argparse
import json
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
BINARY = Path(os.environ.get("MAGEL_BIN", str(ROOT / ("magel.exe" if os.name == "nt" else "magel"))))


def run(arguments, timeout=60):
    """以真实编译器运行验证场景，返回完整的退出状态与诊断。"""
    return subprocess.run([str(BINARY), *map(str, arguments)], cwd=ROOT, capture_output=True,
                          text=True, encoding="utf-8", timeout=timeout)


def main():
    """验证正常程序，以及即使不调用也必须拒绝的类型和生命周期错误。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--aot", action="store_true", help="同时验证默认 checked、O3 的 AOT 输出")
    options = parser.parse_args()
    for name in ("return_expression", "named_function", "derive"):
        result = run(["run", ROOT / f"example/samples/{name}.mgl"])
        expected = (ROOT / f"example/expected/{name}.txt").read_text().strip()
        assert result.returncode == 0 and result.stdout.strip() == expected, (name, result.stdout, result.stderr)
        if options.aot:
            output = ROOT / "tmp" / (name + "-sugar-aot.exe")
            compiled = run(["aot", ROOT / f"example/samples/{name}.mgl", "--out", output], timeout=300)
            assert compiled.returncode == 0, (name, compiled.stdout, compiled.stderr)
            executed = subprocess.run([str(output)], capture_output=True, text=True, encoding="utf-8", timeout=60)
            assert executed.returncode == 0 and executed.stdout.strip() == expected, (name, executed.stdout, executed.stderr)
    errors = [
        ('int64[] bad(){return {int64 values[]=new[1];defer {println("{}",values[0]);}yield (values);};} void main(){}', "延迟清理仍借用"),
        ("T bad<T>(T value){return if(true){int32 small=1;yield (small);}else{yield (value);};} void main(){bad<int64>(2);}", "类型必须一致"),
        ("T bad<T>(T value){return {return value;};} void main(){bad<int64>(2);}", "禁止嵌套 return"),
        ("int64 bad(){ return { return 1; }; } void main(){}", "禁止嵌套 return"),
        ("int64 bad(){ return loop { break level foo; }; } void main(){}", "break level 后只能跟 all"),
        ("int64 bad(){ return loop { break level all extra; }; } void main(){}", "MGL1001"),
        ("int64 bad(bool yes){return if(yes){int32 small=1;yield (small);}else{int64 large=2;yield (large);};} void main(){}", "类型必须一致"),
        ("int64 bad(bool yes){return {if(yes){yield (1);}};} void main(){}", "每条正常路径"),
        ("int64 const& bad(){return {int64 local=1;yield (&local);};} void main(){}", "MGL1204"),
        ('String bad(){return {String local="owned";defer {println("{}",local);}yield (local);};} void main(){}', "延迟清理仍借用"),
        ("void main(){int64 x=1;int64 bad(){x=2;return x;}bad();}", "MGL1203"),
        ("int64 fn() bad(){int64 x=1;int64 read(){return x;}return read;} void main(){}", "MGL1204"),
        ("void main(){int64 value=1 + int64 bad(){return 2;};}", None),
        ("struct Plain {int64 x;} #derive(Equal) struct Box<T>{T value;} void main(){Box<Plain> box;}", "未实现 Equal"),
        ("#derive(Drop)\nstruct Bad {int64 value;} void main(){}", "禁止生成 Drop"),
        ("#derive(ToString)\nstruct Bad {int64 value;} void main(){}", "只支持 Equal"),
        ("struct Inner {int64 value;} #derive(Equal) struct Bad {Inner inner;} void main(){}", "未实现 Equal"),
        ("struct Inner {int64 value;} #derive(Hash) struct Bad {Inner inner;} void main(){}", "未实现 Hash"),
        ("struct Inner {int64 value;} #derive(Clone) struct Bad {Inner inner;} void main(){}", "未实现 Clone"),
        ("struct Inner {int64 value;} #derive(Equal) enum Bad {Value(Inner inner)} void main(){}", "未实现 Equal"),
    ]
    (ROOT / "tmp").mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="declaration-sugars-", dir=ROOT / "tmp") as directory:
        for index, (source, message) in enumerate(errors):
            path = Path(directory) / f"bad-{index}.mgl"
            path.write_text(source, encoding="utf-8")
            result = run(["check", path, "json"])
            assert result.returncode != 0, (source, result.stdout)
            if message:
                diagnostics = json.loads(result.stdout)["diagnostics"]
                assert any(message in item["message"] or message == item["code"] for item in diagnostics), (source, diagnostics)
    print(f"声明语法糖：3 个正常程序、{len(errors)} 个语义与生命周期错误通过")


if __name__ == "__main__":
    main()
