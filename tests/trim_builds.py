"""Magel 裁剪构建回归：需要默认/裁剪的 magel 与 magel-emit 先完成构建。"""
from pathlib import Path
import argparse
import os
import subprocess

ROOT = Path(__file__).resolve().parents[1]
EXE = ".exe" if os.name == "nt" else ""


def run(args, succeeds=True):
    """执行命令并验证成功/失败状态，保留诊断用于定位。"""
    result = subprocess.run(list(map(str, args)), cwd=ROOT, capture_output=True,
                            text=True, encoding="utf-8", errors="replace")
    if (result.returncode == 0) != succeeds:
        raise AssertionError(f"{args}\n{result.stdout}\n{result.stderr}")
    return result


def main():
    """验证移除行为、保留能力及单文件导出的裁剪定义。"""
    parser = argparse.ArgumentParser()
    parser.add_argument("--magel", default=str(ROOT / ("magel" + EXE)))
    args = parser.parse_args()
    work = ROOT / "build/trim-tests"
    work.mkdir(parents=True, exist_ok=True)
    for sample in ["std_math_clock_random", "bigint", "bigfloat"]:
        result = run([args.magel, ROOT / "example/samples" / (sample + ".mgl"),
                      "--out", work / sample], succeeds=False)
        assert result.stderr.strip(), sample
    control = ROOT / "example/samples/control.mgl"
    run([args.magel, control, "--out", work / "control-run"])
    run([args.magel, control, "--emit", "--out", work / "control"])
    target = work / ("control" + EXE)
    run([ROOT / "build/magel-emit" / ("magel-emit" + EXE), "--quad", work / "control.quad",
         "--data", work / "control.data.bin", "--layout", work / "control.layout", "--out", target, "--cpp-output"])
    host = work / "host.cpp"
    host.write_text('#define MAGEL_NO_MAIN\n#include "control' + EXE + '.aot.cpp"\n'
                    '#include <cassert>\n'
                    'int main() { alignas(256) static unsigned char pool[2*1024*1024]; '
                    'std::size_t peak=0; int result=runMagel(0,nullptr,false,100000,pool,sizeof(pool),&peak); '
                    'assert(result==0 && peak>0); '
                    'assert(!Memory::defaultRuntimeAllocatorIfCreated()); }\n', encoding="utf-8")
    binary = work / ("host" + EXE)
    flags = ["-DNO_MAGEL_STD", "-DNO_MAGEL_BIG_INT",
             "-DNO_MAGEL_DEFAULT_POOL"]
    run(["g++", "-std=c++17", "-O0", "-pthread", *flags, host, "-o", binary])
    run([binary])
    symbols = run(["nm", "-C", binary]).stdout
    assert "bigint::BigInt::" not in symbols and "bigfloat::BigFloat::" not in symbols
    print("Magel trim checks passed")


if __name__ == "__main__":
    main()
