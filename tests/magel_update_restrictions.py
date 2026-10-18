"""检查完整表达式的对象冲突、独立更新与目标位置只求值一次。"""
from pathlib import Path
import argparse
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_MC = ROOT / "build/work/magel" / ("magel.exe" if os.name == "nt" else "magel")
MESSAGE = "MGL1301"


def main():
    """验证同目标冲突、不同目标更新以及位置求值的运行行为。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--magel", type=Path, default=DEFAULT_MC)
    args = parser.parse_args()
    compiler = args.magel.resolve()
    work = ROOT / "build/magel-update-restrictions"
    work.mkdir(parents=True, exist_ok=True)
    prefix = "struct Pair{int64 x;int64 y;};\nint64 sum(int64 x,int64 y) { return x+y; }\nint64 main() { int64 a=3,b=2,i=1; int64 data[4]; "
    suffix = " return 0; }\n"
    cases = []
    legal = []
    for operator in ("+=", "-=", "*=", "/=", "%=", "&=", "|=", "^=", "<<=", ">>="):
        for update in ("i++", "++i", "i--", "--i"):
            cases.append((f"{operator} {update}", f"i {operator} {update};"))
            legal.append(f"i=3; a {operator} {update};")
    cases.extend([
        ("自赋值", "i=i++;"),
        ("重复更新", "a=i++ + ++i;"),
        ("读写冲突", "a=i++ + i;"),
        ("函数实参", "sum(i++, i++);"),
        ("下标依赖", "data[i++]=i;"),
        ("别名", "int64 mut& r=&i; a=i++ + r++;"),
        ("元素冲突", "a=data[0]++ + data[0]++;"),
        ("元素别名", "int64 mut& r=&data[0]; a=r++ + data[0]++;"),
        ("动态下标", "a=data[i]++ + data[b]++;"),
        ("字段冲突", "Pair p; a=p.x++ + p.x++;"),
        ("字段别名", "Pair p; int64 mut& r=&p.x; a=r++ + p.x++;"),
        ("短路", "bool ready=false && (i++ > ++i);"),
    ])
    legal.extend([
        "a=(i++)+(b++); std::lang::assert(a==3 && i==2 && b==3);",
        "data[i++]=1; std::lang::assert(i==2 && data[1]==1);",
        "data[i++]+=2; std::lang::assert(i==2 && data[1]==2);",
        "data[i++]++; std::lang::assert(i==2 && data[1]==1);",
        "a=data[0]++ + data[1]++; std::lang::assert(data[0]==1 && data[1]==1);",
        "int64 mut& r=&data[0]; a=r++ + data[1]++; std::lang::assert(data[0]==1 && data[1]==1);",
        "Pair p; int64 mut& r=&p.x; a=r++ + p.y++; std::lang::assert(p.x==1 && p.y==1);",
        "sum(i++, b+=2); std::lang::assert(i==2 && b==4);",
        "bool ready=false && (i++ > (a+=2)); std::lang::assert(i==1 && a==3);",
        "i++; a+=i; --i; b*=2;",
        "int64 first=i++, second=(a+=2);",
        "for(i+=1; i++<5; a+=1){b++; a+=b;}",
    ])
    with tempfile.TemporaryDirectory(prefix="case-", dir=work) as directory:
        directory = Path(directory)
        source = directory / "case.mgl"
        def compile_case(body):
            source.write_text(prefix + body + suffix, encoding="utf-8")
            return subprocess.run([str(compiler), 'run', str(source), "--out", str(directory / "out")],
                                  cwd=ROOT, capture_output=True, text=True, encoding="utf-8",
                                  errors="replace", timeout=60)
        for label, body in cases:
            result = compile_case(body)
            assert result.returncode != 0 and MESSAGE in result.stderr, (label, result.returncode, result.stdout, result.stderr)
        for body in legal:
            result = compile_case(body)
            assert result.returncode == 0, (body, result.stdout, result.stderr)
    print(f"[通过] {len(cases)} 种同目标冲突均拒绝；{len(legal)} 种独立目标或表达式边界均接受")


if __name__ == "__main__":
    main()
