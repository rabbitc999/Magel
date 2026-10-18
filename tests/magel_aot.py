"""End-to-end checks for Magel ``--target cpp-aot``.

Build ``magel`` and ``magel-emit`` first, then run::

    python tests/magel_aot.py

Covers: interpreter / ``--target cpp-aot`` output equality,
optimizer-level consistency, the default intermediate-file cleanup, the
``--keep-intermediate`` / ``--clean`` switches, failure paths and manifest
path-safety.  Small floating-point differences are tolerated on purpose.
"""

import argparse
import os
import re
from pathlib import Path
import shutil
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[1]
EXE = ".exe" if os.name == "nt" else ""
MAGEL = ROOT / ("magel" + EXE)
EMITTER = ROOT / "build" / "magel-emit" / ("magel-emit" + EXE)
WORK = ROOT / "tmp" / "magel-aot-tests"
SAMPLES = ROOT / "example" / "samples"

# 输出比对样例（不含输入 / 线程 / 仅驱动内置胶水的样例）。
EQUIVALENCE = [
    "aot_basic", "control", "recursion", "structs", "closeable", "containers",
    "string_text", "interface", "float", "match_range", "pointer",
    "option_result_methods", "math", "std_collections_algorithms",
    # std::compress 的 AOT 产物要把 lztext 源码一起内嵌，是新增模块里最容易断的一环。
    "std_compress",
]

# 运行时桥接线：协程 / 消息 / 线程 / 中断（解释器对照组）。
CONCURRENCY = [
    "coroutine", "coroutine_messages", "coroutine_typed_messages",
    "thread_messages", "typed_messages_shared",
    "interrupt", "interrupt_arguments", "interrupt_handle", "interrupt_thread",
]

# 这些样例句柄型运行时需要显式 `--threads`（和 check.sh 的 `<name>.threads` 一致）。
THREAD_SAMPLES = {"thread_messages", "typed_messages_shared", "interrupt_thread"}

NUMBER = re.compile(r"[-+]?(?:\d+\.\d*|\.\d+|\d+)(?:[eE][-+]?\d+)?")


def command(args, *, cwd=ROOT, succeeds=True, env=None):
    result = subprocess.run(
        [str(arg) for arg in args], cwd=cwd, text=True, env=env,
        encoding="utf-8", errors="replace", capture_output=True, check=False,
        timeout=600,
    )
    if succeeds is not None and (result.returncode == 0) != succeeds:
        raise AssertionError(
            f"command returned {result.returncode}: {' '.join(map(str, args))}\n"
            f"stdout:\n{result.stdout}\nstderr:\n{result.stderr}"
        )
    return result


def run(*args, **kwargs):
    return command(args, succeeds=None, **kwargs)


def emit(name, prefix):
    command([MAGEL, 'run', SAMPLES / f"{name}.mgl", "--emit", "--out", prefix])


def aot(prefix, out, extra=(), env=None, succeeds=True, threads=False):
    args = [
        EMITTER, "--quad", prefix.with_suffix(".quad"),
        "--data", prefix.with_suffix(".data.bin"),
        "--layout", prefix.with_suffix(".layout"),
        "--target", "cpp-aot", "--out", out, *extra,
    ]
    if threads:
        args.append("--threads")
    return command(args, succeeds=succeeds, env=env)


def close(a, b, tolerance=1e-6):
    """逐行比较；数字 token 允许小浮点误差。"""
    left, right = a.splitlines(), b.splitlines()
    if len(left) != len(right):
        return False
    for row_a, row_b in zip(left, right):
        if row_a == row_b:
            continue
        parts_a, parts_b = NUMBER.split(row_a), NUMBER.split(row_b)
        if parts_a != parts_b:
            return False
        numbers_a = NUMBER.findall(row_a)
        numbers_b = NUMBER.findall(row_b)
        if len(numbers_a) != len(numbers_b):
            return False
        for text_a, text_b in zip(numbers_a, numbers_b):
            try:
                float_a, float_b = float(text_a), float(text_b)
            except ValueError:
                return False
            if abs(float_a - float_b) > tolerance * max(1.0, abs(float_b)):
                return False
    return True


def interpreter(name, threads=False):
    args = [MAGEL, 'run', SAMPLES / f"{name}.mgl"]
    if threads:
        args.append("--threads")
    return command(args, succeeds=None).stdout


def check_equivalence(failures):
    print("== 输出一致性（解释 / cpp / cpp-aot）==")
    for name in EQUIVALENCE:
        prefix = WORK / name
        try:
            emit(name, prefix)
            expected = interpreter(name)
            aot(prefix, WORK / f"{name}.exe", ["--opt-level=0"])
            actual = run(WORK / f"{name}.exe").stdout
            if not close(actual, expected):
                failures.append(f"{name}: cpp-aot 与解释输出不一致")
                continue
            print(f"  [ok] {name}")
        except Exception as error:  # noqa: BLE001
            failures.append(f"{name}: {error}")

    print("== 运行时桥：协程 / 消息 / 线程 / 中断 ==")
    for name in CONCURRENCY:
        prefix = WORK / name
        threads = name in THREAD_SAMPLES
        try:
            emit(name, prefix)
            expected = interpreter(name, threads=threads)
            aot(prefix, WORK / f"{name}.exe", ["--opt-level=0"], threads=threads)
            actual = run(WORK / f"{name}.exe").stdout
            if not close(actual, expected):
                failures.append(f"{name}: 解释与 cpp-aot 输出不一致")
                continue
            print(f"  [ok] {name}")
        except Exception as error:  # noqa: BLE001
            failures.append(f"{name}: {error}")

    return failures


def check_opt_levels(failures):
    print("== 优化级别一致性 ==")
    name = "aot_basic"
    prefix = WORK / name
    emit(name, prefix)
    outputs = {}
    for level in ("0", "1", "2", "3", "s"):
        exe = WORK / f"{name}.O{level}.exe"
        try:
            aot(prefix, exe, [f"--opt-level={level}", "--keep-intermediate"])
            outputs[level] = run(exe).stdout
            log = exe.with_suffix(exe.suffix + ".aot.log").read_text(encoding="utf-8")
            expected_flag = {"0": "-O0", "1": "-O1", "2": "-O2", "3": "-O3", "s": "-Os"}[level]
            if expected_flag not in log:
                failures.append(f"opt {level}: 日志里没看到 {expected_flag}")
            print(f"  [ok] -O{level}")
        except Exception as error:  # noqa: BLE001
            failures.append(f"opt {level}: {error}")
    values = list(outputs.values())
    if values and any(not close(value, values[0]) for value in values):
        failures.append("不同优化级别输出不一致")
    # 裸编译器旗标也应透传/识别。
    try:
        aot(prefix, WORK / f"{name}.raw.exe", ["-O2", "--keep-intermediate"])
        print("  [ok] 裸 -O2")
    except Exception as error:  # noqa: BLE001
        failures.append(f"裸 -O2: {error}")


def intermediate_paths(exe):
    return [
        Path(str(exe) + ".aot.cpp"),
        Path(str(exe) + (".aot.obj" if os.name == "nt" else ".aot.o")),
        Path(str(exe) + ".aot.log"),
    ]


def check_cleanup(failures):
    print("== 中间文件清理 ==")
    name = "aot_basic"
    prefix = WORK / name
    emit(name, prefix)

    # 默认：成功后删中间文件，exe 保留且能跑。
    exe = WORK / "clean.exe"
    try:
        aot(prefix, exe, ["--opt-level=0"])
        leftovers = [path for path in intermediate_paths(exe) if path.exists()]
        manifest = exe.parent / ".magel-aot-manifest"
        if leftovers or manifest.exists():
            failures.append(f"默认清理留下了 {leftovers} 或清单")
        elif not exe.exists() or run(exe).stdout == "":
            failures.append("默认清理后 exe 丢失或不能运行")
        else:
            print("  [ok] 默认删除中间文件")
    except Exception as error:  # noqa: BLE001
        failures.append(f"默认清理: {error}")

    # --keep-intermediate：全部保留。
    exe = WORK / "keep.exe"
    try:
        aot(prefix, exe, ["--opt-level=0", "--keep-intermediate"])
        missing = [path for path in intermediate_paths(exe) if not path.exists()]
        if missing:
            failures.append(f"--keep-intermediate 缺少 {missing}")
        elif not run(exe).stdout:
            failures.append("--keep-intermediate 产物不能运行")
        else:
            print("  [ok] --keep-intermediate 保留")
    except Exception as error:  # noqa: BLE001
        failures.append(f"--keep-intermediate: {error}")

    # --clean：只清理，不重新编译。
    try:
        command([EMITTER, "--target", "cpp-aot", "--clean", "--out", exe])
        if any(path.exists() for path in intermediate_paths(exe)) or \
                (exe.parent / ".magel-aot-manifest").exists():
            failures.append("--clean 没清干净")
        elif not exe.exists():
            failures.append("--clean 误删了 exe")
        else:
            print("  [ok] --clean 只清中间文件")
    except Exception as error:  # noqa: BLE001
        failures.append(f"--clean: {error}")

    # 编译失败：中间文件必须保留。
    exe = WORK / "failing.exe"
    bad_env = dict(os.environ)
    bad_env["CXX"] = "definitely-not-a-compiler"
    try:
        aot(prefix, exe, ["--opt-level=0"], env=bad_env, succeeds=False)
        if not (Path(str(exe) + ".aot.cpp")).exists():
            failures.append("编译失败却没保留生成源码")
        else:
            print("  [ok] 编译失败保留中间文件")
    except Exception as error:  # noqa: BLE001
        failures.append(f"失败保留: {error}")


def check_manifest_safety(failures):
    print("== 清单安全 ==")
    name = "aot_basic"
    prefix = WORK / name
    emit(name, prefix)
    exe = WORK / "safety.exe"
    try:
        aot(prefix, exe, ["--opt-level=0", "--keep-intermediate"])
        user = exe.parent / "user-keep.txt"
        user.write_text("mine\n", encoding="utf-8")
        command([EMITTER, "--target", "cpp-aot", "--clean", "--out", exe])
        if not user.exists():
            failures.append("清理误删了用户文件")
        else:
            print("  [ok] 保留用户文件")
    except Exception as error:  # noqa: BLE001
        failures.append(f"用户文件: {error}")

    # 恶意清单：`..` 越界必须在删除前被拒绝。
    escape_dir = WORK / "escape"
    escape_dir.mkdir(parents=True, exist_ok=True)
    outside = WORK / "outside-keep.txt"
    outside.write_text("outside\n", encoding="utf-8")
    manifest = escape_dir / ".magel-aot-manifest"
    manifest.write_text(
        "# magel-aot generated files v1\n../outside-keep.txt\n", encoding="utf-8")
    result = run(EMITTER, "--target", "cpp-aot", "--clean", "--out",
                 escape_dir / "nothing.exe")
    if result.returncode == 0:
        failures.append("`..` 越界清单被接受了")
    elif not outside.exists():
        failures.append("`..` 越界清单导致目录外文件被删")
    else:
        print("  [ok] 拒绝 `..` 越界清单")


def check_output_suffix(failures):
    """无扩展名的 `--out` 在 Windows 上要落成 `.exe`，否则产物双击或直接调用都跑不起来。"""
    print("== 产物名后缀（Windows 补 .exe）==")
    name = "aot_basic"
    prefix = WORK / name
    emit(name, prefix)
    target = WORK / "suffix"
    expected = Path(str(target) + (".exe" if os.name == "nt" else ""))
    try:
        aot(prefix, target, ["--opt-level=0"])
        if not expected.exists():
            failures.append(f"无扩展名 --out：期望产物 {expected.name}，实际未生成")
            return
        if not close(run(expected).stdout, interpreter(name)):
            failures.append("无扩展名 --out 的产物输出与解释器不一致")
            return
        print(f"  [ok] {expected.name}")
    except Exception as error:  # noqa: BLE001
        failures.append(f"产物名后缀: {error}")


def main():
    global MAGEL
    parser = argparse.ArgumentParser()
    parser.add_argument("--magel", type=Path, default=MAGEL)
    parser.add_argument("--skip", default="",
                        help="逗号分隔的用例名：equivalence,opt,cleanup,suffix,safety")
    args = parser.parse_args()
    MAGEL = args.magel.resolve()
    skip = {item.strip() for item in args.skip.split(",") if item.strip()}

    for tool in (MAGEL, EMITTER):
        if not tool.exists():
            parser.error(f"build required tool first: {tool}")

    shutil.rmtree(WORK, ignore_errors=True)
    WORK.mkdir(parents=True)

    failures = []
    if "equivalence" not in skip:
        check_equivalence(failures)
    if "opt" not in skip:
        check_opt_levels(failures)
    if "cleanup" not in skip:
        check_cleanup(failures)
    if "suffix" not in skip:
        check_output_suffix(failures)
    if "safety" not in skip:
        check_manifest_safety(failures)

    if failures:
        print("\n== 失败 ==")
        for item in failures:
            print(f"  [FAIL] {item}")
        return 1
    print("\n全部通过")
    return 0


if __name__ == "__main__":
    sys.exit(main())
