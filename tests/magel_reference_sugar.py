"""验证默认只读引用语法糖的类型身份、权限及生命周期。"""
from pathlib import Path
import argparse
import subprocess
import os

ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / "build/magel-reference-sugar"


def execute(*args):
    """执行编译器或产物，记录诊断并限制运行时间。"""
    return subprocess.run([str(arg) for arg in args], cwd=ROOT, capture_output=True,
                          encoding="utf-8", errors="replace", timeout=180)


def main():
    """检查合法组合与只读、非空、逃逸拒绝路径。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--magel", type=Path, default=ROOT / "magel.exe")
    compiler = parser.parse_args().magel.resolve()
    WORK.mkdir(parents=True, exist_ok=True)
    cases = {
        "write": ("int64 main(){int64 x=1;int64 & a=&x;a=2;return 0;}", "只读"),
        "field": ("struct S{int64 x;};int64 main(){S s;S & a=&s;a.x=2;return 0;}", "只读"),
        "container": ("int64 main(){List<int64> xs;List<int64> & a=&xs;a.push(2);return 0;}", "只读"),
        "mutable_argument": ("void write(int64 mut& a){a=2;}int64 main(){int64 x=1;int64 & a=&x;write(a);return 0;}", "const&"),
        "escape": ("int64 & bad(){int64 x=1;return &x;}int64 main(){return 0;}", "MGL1204"),
        "unbound": ("int64 main(){int64 & a;return 0;}", "初始化"),
    }
    for name, (text, expected) in cases.items():
        source = WORK / (name + ".mgl")
        source.write_text(text, encoding="utf-8")
        result = execute(compiler, "--check", source)
        assert result.returncode != 0 and expected in result.stderr, (name, result)
    source = ROOT / "example/samples/reference_sugar.mgl"
    result = execute(compiler, "run", source, "--out", WORK / "interpret")
    assert result.returncode == 0 and result.stdout == "reference sugar OK\n" and not result.stderr, result
    output = WORK / ("native.exe" if os.name == "nt" else "native")
    result = execute(compiler, "aot", source, "--out", output, "--opt-level=0")
    assert result.returncode == 0, result
    result = execute(output)
    assert result.returncode == 0 and result.stdout == "reference sugar OK\n" and not result.stderr, result
    print(f"引用语法糖通过：{len(cases)} 项拒绝，组合样例解释 / AOT")


if __name__ == "__main__":
    main()
