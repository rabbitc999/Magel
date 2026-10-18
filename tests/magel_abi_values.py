"""Compile a real C ABI library and verify exported MAGEL value/pointer conversions."""
from pathlib import Path
import json
import argparse
import os
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / "build" / "magel-abi-values"
EXE = ".exe" if os.name == "nt" else ""


def run(args, succeeds=True, env=None):
    result = subprocess.run(list(map(str, args)), cwd=ROOT, text=True,
                            encoding="utf-8", capture_output=True, env=env)
    assert (result.returncode == 0) == succeeds, result.stdout + result.stderr
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--targets", default="cpp-aot")
    targets = parser.parse_args().targets.split(",")
    assert all(t in ("cpp-aot", "interpreter") for t in targets)
    WORK.mkdir(parents=True, exist_ok=True)
    library = WORK / ("native.dll" if os.name == "nt" else "native.so")
    native = WORK / "native.c"
    fixture = ROOT / "doc" / "examples" / "abi_buffers"
    native.write_text((fixture / "native.c").read_text(encoding="utf-8"), encoding="utf-8")
    run(["gcc", "-shared", *(["-fPIC"] if os.name != "nt" else []), native, "-o", library])
    mc = WORK / "values.mgl"
    mc.write_text((fixture / "demo.mgl").read_text(encoding="utf-8").replace(
        '"./native.dll"', json.dumps(library.as_posix())), encoding="utf-8")
    prefix = WORK / "values"
    run([ROOT / "build/work/magel" / ("magel" + EXE), mc, "--emit", "--out", prefix])
    for target in targets:
        if target == "interpreter":
            continue
        out = WORK / target
        exe = WORK / ("abi-" + target + EXE)
        arguments = [ROOT / "build/magel-emit" / ("magel-emit" + EXE),
                     "--quad", prefix.with_suffix(".quad"), "--data", prefix.with_suffix(".data.bin"),
                     "--layout", prefix.with_suffix(".layout"), "--target", target, "--runtime-arena", "67108864"]
        run([*arguments, "--out", exe, "--cpp-output-dir", out])
        result = run([exe])
        assert result.stdout == "abi-values-ok\n", result.stdout
        assert result.stderr == "", result.stderr
        stats = run([exe, "--stats"])
        assert stats.stdout == "abi-values-ok\n", stats.stdout
        # AOT 接受 --stats，资源错误仍通过 stderr 判定。
        assert stats.stderr == "", stats.stderr
    compiler = ROOT / "build/work/magel" / ("magel" + EXE)
    import tempfile
    temp_root = Path(tempfile.gettempdir())
    before = set(temp_root.glob("magel-abi-*"))
    immediate = run([compiler, 'run', mc, "--out", WORK / "immediate"])
    assert immediate.stdout == "abi-values-ok\n" and immediate.stderr == "", immediate.stdout + immediate.stderr
    assert set(temp_root.glob("magel-abi-*")) == before, "ABI bridge temporary directory leaked"
    missing_compiler = {**os.environ, "MAGEL_ABI_CXX": str(WORK / "missing-compiler")}
    failed = run([compiler, 'run', mc, "--out", WORK / "missing"], succeeds=False, env=missing_compiler)
    assert "MAGEL_ABI_CXX" in failed.stderr, failed.stderr
    assert set(temp_root.glob("magel-abi-*")) == before, "failed bridge temporary directory leaked"
    scalar = WORK / "scalar.mgl"
    scalar.write_text('abi ' + json.dumps(library.as_posix()) +
                      ' { int32 identity(int32 value) = "identity"; }\n'
                      'int64 main() { std::lang::println("{}", identity((int32)7)); return 0; }\n', encoding="utf-8")
    small = run([compiler, 'run', scalar, "--out", WORK / "scalar"], env=missing_compiler)
    assert small.stdout == "7\n" and small.stderr == "", small.stdout + small.stderr
    for length, message in ((3, "returned null"), (-1, "buffer length"), (100000000, "buffer length")):
        bad = WORK / "bad-length.mgl"
        bad.write_text('abi ' + json.dumps(library.as_posix()) +
                       ' { int32[] numbers(int32 count) = "numbers" [count]; }\n'
                       f'int64 main() {{ int32 values[] = numbers((int32){length}); return 0; }}\n', encoding="utf-8")
        failed = run([compiler, 'run', bad, "--out", WORK / "bad-length"], succeeds=False)
        assert message in failed.stderr, failed.stderr
        assert set(temp_root.glob("magel-abi-*")) == before, "error bridge temporary directory leaked"
    bad = WORK / "null-reference.mgl"
    bad.write_text('struct Packet { int32 value; }; abi ' + json.dumps(library.as_posix()) +
                   ' { Packet mut& nothing() = "nothing"; }\n'
                   'int64 main() { Packet mut& value = nothing(); return 0; }\n', encoding="utf-8")
    failed = run([compiler, 'run', bad, "--out", WORK / "null-reference"], succeeds=False)
    assert "ABI returned null for nonnull reference" in failed.stderr, failed.stderr
    null_prefix = WORK / "null-reference"
    run([compiler, 'run', bad, "--emit", "--out", null_prefix])
    null_exe = WORK / ("null-reference" + EXE)
    run([ROOT / "build/magel-emit" / ("magel-emit" + EXE),
         "--quad", null_prefix.with_suffix(".quad"), "--data", null_prefix.with_suffix(".data.bin"),
         "--layout", null_prefix.with_suffix(".layout"), "--target", "cpp-aot", "--out", null_exe])
    failed = run([null_exe], succeeds=False)
    assert "ABI returned null for nonnull reference" in failed.stderr, failed.stderr
    for declaration in (
        'struct Bad { String text; }; abi LIBRARY { void f(Bad p); }',
        'struct Bad unknown { int32 value; }; abi LIBRARY { void f(Bad p); }',
        'abi LIBRARY { int32[] f(int32 count) = "f" [missing]; }',
        'abi LIBRARY { void f(int32 mut& mut& p); }',
    ):
        bad = WORK / "bad.mgl"
        bad.write_text(declaration.replace("LIBRARY", json.dumps(library.as_posix())) +
                       "\nint64 main() { return 0; }\n", encoding="utf-8")
        result = run([compiler, 'run', bad, "--emit", "--out", WORK / "bad"], succeeds=False)
        assert "abi" in result.stderr.lower() or "语法错误" in result.stderr, result.stderr
    print("MAGEL ABI 值、引用和非空返回检查通过")


if __name__ == "__main__":
    main()
