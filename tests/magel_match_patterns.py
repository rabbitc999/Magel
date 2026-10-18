"""验证 match 的或模式及两种左闭区间，覆盖 VM、AOT 与禁止写法。"""
from pathlib import Path
import argparse
import os
import subprocess

ROOT = Path(__file__).resolve().parents[1]
EXE = ".exe" if os.name == "nt" else ""


def main():
    """检查数值与字符串边界，保证左开模式被拒绝而普通元组仍可使用。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--magel", type=Path, default=ROOT / ("magel" + EXE))
    compiler = parser.parse_args().magel.resolve()
    work = ROOT / "build/magel-match-patterns"
    work.mkdir(parents=True, exist_ok=True)

    def run(*args):
        return subprocess.run(list(map(str, args)), cwd=ROOT, capture_output=True,
                              encoding="utf-8", errors="replace", timeout=240,
                              env=dict(os.environ, MAGEL_LANG="zh"))

    for name in ("match_or_string", "match_range"):
        source = ROOT / "example/samples" / (name + ".mgl")
        expected = (ROOT / "example/expected" / (name + ".txt")).read_text(encoding="utf-8")
        for backend in ("run", "aot"):
            output = work / (name + "-" + backend + EXE)
            result = run(compiler, backend, source, "--out", output)
            assert result.returncode == 0, result
            if backend == "aot": result = run(output)
            assert result.returncode == 0 and result.stdout == expected and not result.stderr, result

    rejected = 0
    for name, subject, lo, hi in (("integer", "1", "0", "2"),
                                  ("float", "1.0", "0.0", "2.0"),
                                  ("string", '\"m\"', '\"a\"', '\"z\"')):
        for close in (")", "]"):
            source = work / (name + ("-open" if close == ")" else "-right-closed") + ".mgl")
            source.write_text("int64 main(){match(" + subject + "){(" + lo + "," + hi + close +
                              "=>{return 1;} _=>{return 0;}}return 0;}\n", encoding="utf-8")
            result = run(compiler, "--check", source)
            assert result.returncode != 0 and result.stderr, result
            if close == ")": assert "match 区间仅支持" in result.stderr, result
            rejected += 1

    source = work / "boundaries.mgl"
    source.write_text('''int64 main(){
        auto pair=(1,2);assert(pair.0==1 && pair.1==2);
        match(0){[0,0]=>{} _=>{assert(false);}}
        match(0){[0,0)=>{assert(false);} _=>{}}
        match(1.0){[0.0,1.0)=>{assert(false);} _=>{}}
        match(1.0){[0.0,1.0]=>{} _=>{assert(false);}}
        match(""){["",""]=>{} _=>{assert(false);}}
        match(""){["","")=>{assert(false);} _=>{}}
        return 0;
    }\n''', encoding="utf-8")
    result = run(compiler, "run", source, "out", work / "boundaries")
    assert result.returncode == 0 and not result.stderr, result
    print(f"match 区间通过：数值/字符串样例 VM 与 AOT、端点及空区间、普通元组、{rejected} 个左开拒绝场景")


if __name__ == "__main__":
    main()
