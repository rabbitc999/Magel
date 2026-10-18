"""检查入口 main 接受 int64 或 void 返回类型，覆盖带参数入口及检查模式。"""
import argparse
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def main():
    """检查合法及非法入口，不依赖生成器或外部测试框架。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--magel", type=Path, default=ROOT / ("magel.exe" if os.name == "nt" else "magel"))
    args = parser.parse_args()
    work = ROOT / "build" / "entry-signature"
    work.mkdir(parents=True, exist_ok=True)
    env = dict(os.environ, MAGEL_LANG="zh")
    for params in ("", "std::lang::List<std::lang::String> args"):
        for result_type, body in (("int64", "return 0;"), ("int32", "return 0;"),
                                  ("uint64", "return 0;"), ("void", "return;"),
                                  ("float64", "return 0.0;"), ("bool", "return false;")):
            source = work / f"{result_type}-{'args' if params else 'empty'}.mgl"
            source.write_text(f"{result_type} main({params}){{{body}}}\n", encoding="utf-8")
            result = subprocess.run([str(args.magel.resolve()), "--check", str(source)], cwd=ROOT,
                                    env=env, capture_output=True, encoding="utf-8", errors="replace", timeout=120)
            if result_type in ("int64", "void"):
                assert result.returncode == 0, result.stderr
            else:
                assert result.returncode != 0 and "main 返回类型必须为 int64" in result.stderr, result
    print("入口签名：无参数及 List<String> 参数的 int64/void 正例、八项错误返回类型通过")


if __name__ == "__main__":
    main()
