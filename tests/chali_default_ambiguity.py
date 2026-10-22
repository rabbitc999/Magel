"""验证默认参数重载歧义的候选签名、省略参数和消歧提示。"""
from pathlib import Path
import argparse
import json
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    """检查各类调用的歧义诊断，同时验证显式传参与精确匹配仍可消歧。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--chali", type=Path, default=ROOT / ("ChaLi.exe" if os.name == "nt" else "chali"))
    compiler = parser.parse_args().chali.resolve()
    env = dict(os.environ, CHALI_LANG="zh")

    def run(*args):
        return subprocess.run([str(x) for x in args], cwd=ROOT, env=env,
                              capture_output=True, encoding="utf-8", errors="replace", timeout=60)

    cases = {
        "zero_args": 'int64 f(int64 number=1){return 1;}int64 f(String text="x"){return 2;}void main(){let __boom = f();}',
        "same_prefix": 'int64 f(int64 x,int64 number=1){return 1;}int64 f(int64 x,String text="x"){return 2;}void main(){let __boom = f(1);}',
        "exact_arity": 'int64 f(int64 x){return 1;}int64 f(int64 x,int64 number=1){return 2;}void main(){let __boom = f(1);}',
        "instance": 'struct S{int64 f(int64 number=1){return 1;}int64 f(String text="x"){return 2;}};void main(){S s;let __boom = s.f();}',
        "static": 'struct S{static int64 f(int64 number=1){return 1;}static int64 f(String text="x"){return 2;}};void main(){let __boom = S::f();}',
        "constructor": 'struct S{S(int64 number=1){}S(String text="x"){}};void main(){S s=new S();}',
        "variadic": 'int64 f(int64 number=1,int64... rest){return 1;}int64 f(String text="x"){return 2;}void main(){let __boom = f();}',
        "named_constructor": 'struct S{static S create(int64 number=1){}static S create(String text="x"){}};void main(){S s=S::create();}',
    }
    with tempfile.TemporaryDirectory(prefix="chali-default-ambiguity-") as directory:
        work = Path(directory)
        for name, code in cases.items():
            source = work / (name + ".chl")
            source.write_text(code, encoding="utf-8")
            result = run(compiler, "check", source, "json")
            assert result.returncode != 0, (name, result.stdout, result.stderr)
            diagnostics = json.loads(result.stdout)["diagnostics"]
            message = "\n".join(item["message"] for item in diagnostics)
            for expected in ("默认参数歧义", "候选签名", "省略：number", "请显式补充能区分候选的实参", "不会按省略默认参数的数量或声明顺序"):
                assert expected in message, (name, expected, message)
            signatures = message.split("候选签名：", 1)[1].split("；请", 1)[0]
            assert "$" not in signatures and "number = 默认值" in signatures, (name, signatures)
            expected_name = {"constructor": "S(", "named_constructor": "S::create(",
                             "static": "S::f(", "instance": "S.f("}.get(name, "f(")
            assert expected_name in signatures, (name, signatures)
            if name == "exact_arity":
                assert "未省略默认参数" in message, message
            else:
                assert "省略：text" in message, (name, message)
            if name == "variadic":
                assert "... rest" in message, message
        ordinary = work / "ordinary.chl"
        ordinary.write_text('int64 f(float64 x,int64 y){return 1;}int64 f(int64 x,float64 y){return 2;}void main(){let __boom = f(1,2);}', encoding="utf-8")
        result = run(compiler, "check", ordinary, "json")
        assert result.returncode != 0 and "多个重载同样匹配" in result.stdout and "默认参数歧义" not in result.stdout, result
        valid = work / "valid.chl"
        valid.write_text('int64 f(int64 x,int64 number=1){return 1;}int64 f(int64 x,String text="x"){return 2;}int64 pick(int64 x,int64 n=1){return 3;}int64 pick(float64 x,int64 n=1){return 4;}void main(){assert(f(1,2)==1 && f(1,"x")==2 && pick(1)==3 && pick(1.0)==4);}', encoding="utf-8")
        result = run(compiler, "run", valid, "out", work / "valid")
        assert (result.returncode, result.stdout, result.stderr) == (0, "", ""), result
    print("默认参数歧义诊断回归通过（8 类歧义、普通歧义兼容、显式消歧与匹配优先级）")


if __name__ == "__main__":
    main()
