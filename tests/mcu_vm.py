"""mcu_vm 门禁：板端口径（`-fno-exceptions`）构建 + 异常符号检查 + 样例等价性 + footprint。

判据分三层，任一层失败都返回非零：
  1. **构建**：按 `magel/source/stackvm_mcu/Makefile` 的口径编出 `mcu-host`（这一条同时证明
     VM 源码里没有 `try/catch/throw`——否则 `-fno-exceptions` 直接编译失败）；
  2. **符号**：`mcu-host` 里不许出现 `__cxa_throw` / `__gxx_personality` 这类异常符号；
  3. **等价性**：`MUST_PASS` 里的样例，用工具链编成 `.svmb` 后在板端口径下跑，stdout
     必须与 `example/expected/<名字>.txt` 逐行一致。

其余纯 VM 样例按"元数据 RAM 不够 / 缺原生函数 / 其它"分类报告——那是**已知边界**，
不是失败（见 magel/source/stackvm_mcu/README.md 的实测与下一步）。
"""

from pathlib import Path
import argparse
import os
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / "build/mcu_vm-regress"
EXE = ".exe" if os.name == "nt" else ""

# 必须通过的样例：覆盖你确认的 MCU 子集——整数 / float64 / String / 容器 / 结构体 /
# 枚举 / Result / 闭包 / 接口 / 泛型 / 协程 / 资源清理。
MUST_PASS = [
    "std_read_lines",
    "match_expression", "string_interpolation", "associated_types", "defer_scope", "macro_controls", "const_generics",
    "if_expression", "try_custom", "error_adapters",
    "std_builtin_shadowing",
    "lambda_capture_modes", "foreach_interval",
    "obj", "std_encoding", "std_csv", "std_datetime", "std_utf8",
    "hello",              # String + 打印
    "control",            # 整数与分支循环
    "float",              # float64
    "strings",            # String 拼接/比较
    "struct_container_field",
    "enums",
    "result_match_loop",
    "lambda_auto",
    "interface_protocols",
    "generics",
    "tuple",
    "closeable",          # Closeable 资源清理
    "recursion",          # 递归 + 调用帧
    "array2d",            # 运行时数组 + 下标
    "short_circuit",
    "macro_for",
    # 需要 `.in` / `.args` 夹具的（check.sh 同款约定）
    "bits",
    "crt",
    "gcd",
    "input_text",
    "main_arguments",
    "powmod",
    # 需要 HAL 胶水（`mcu_glue.cpp` 提供 `std::lang::__tryToInt32` / `std::lang::__toInt64`）
    "lang_float_to_int",
]

# 协程单独跑（要固定栈槽，也验证"板端不需要线程"）。
COROUTINE_SAMPLE = "coroutine"

# 已知的原生函数缺口（`library/stackvm` 相对前端产物）。门禁的 sweep 只允许这两种情况：
# 通过，或落在下面的名单里；**出现新缺口就失败**——这样"前后端名字漂移"不会再悄悄发生。
KNOWN_MISSING_NATIVES = {
    "__vm_reference", "__vm_str_ref", "__vm_vec_ref",
    "__vm_closure_new", "__vm_function_copy",
    "__vm_map_new_policy", "__vm_set_new_policy",
    # 这三个是**胶水**（不是 VM 原生函数）：`__tryToInt32`/`__toInt64` 已由
    # `mcu_glue.cpp` 提供并验证；`__float32ToBytes`/`__bytesToFloat64` 要返回/接收
    # `List<uint8>`，卡在"通用 VM 的胶水上下文还没有建容器能力"（见 README 清单 B）。
    "std::lang::__float32ToBytes", "std::lang::__bytesToFloat64", "std::lang::__float64ToBytes",
    "std::lang::__bytesToFloat32",
    # 数组形态的 IEEE 754 胶水（`uint8 const&` / `uint8 mut&`）——要 A 类的引用能力才接得上。
    "std::lang::__arrayToFloat32", "std::lang::__arrayToFloat64",
    "std::lang::__float32ToArray", "std::lang::__float64ToArray",
}

# 已知的"非缺口"原因（门禁只报告，不判失败）。
KNOWN_OFF_FEATURES = ("没有把大整数编进来", "没有把大浮点数编进来")
KNOWN_HOST_NEEDS = ("没有文件系统", "输入不够")


def run(args, cwd=ROOT, timeout=300):
    """执行命令并保留诊断。"""
    return subprocess.run([str(a) for a in args], cwd=cwd, capture_output=True,
                          encoding="utf-8", errors="replace", timeout=timeout)


def main():
    """构建板端口径并跑等价性回归。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--magel", type=Path, default=ROOT / ("magel" + EXE))
    parser.add_argument("--make", default=shutil.which("mingw32-make") or shutil.which("make"))
    parser.add_argument("--nm", default="nm")
    parser.add_argument("--sweep", action="store_true", help="另外跑全量纯 VM 样例并分类报告")
    args = parser.parse_args()
    compiler = args.magel.resolve()
    if not compiler.exists():
        raise SystemExit(f"找不到 magel 编译器：{compiler}")
    if not args.make:
        raise SystemExit("找不到 make（Windows 上一般是 mingw32-make）")
    WORK.mkdir(parents=True, exist_ok=True)

    # 1. 板端口径构建（宽 arena：先把"语义"和"RAM"分开）。
    budget = ["-DMCU_VM_ARENA_BYTES=524288", "-DMCU_METADATA_BYTES=33554432",
              "-DMCU_RUNTIME_ARENA_BYTES=8388608",
              "-DEMBED_POOL_MAX_BLOCKS=16384"]
    built = run([args.make, "-C", ROOT / "source/stackvm_mcu", "MCU_BUDGET=" + " ".join(budget)])
    if built.returncode != 0:
        raise SystemExit(f"mcu_vm 构建失败：\n{built.stdout}\n{built.stderr}")
    host = ROOT / "build/stackvm_mcu" / ("mcu-host" + EXE)
    assert host.exists(), host
    print(f"[通过] 板端口径构建（-fno-exceptions）：{host}")

    # 2. 异常符号：不能有任何异常机制被链进来。
    symbols = run([args.nm, "-C", host]).stdout
    forbidden = [name for name in ("__cxa_throw", "__cxa_allocate_exception",
                                   "__gxx_personality", "std::terminate")
                 if name in symbols]
    assert not forbidden, f"mcu-host 里出现了异常符号：{forbidden}"
    print("[通过] 无异常符号（__cxa_throw / __gxx_personality / std::terminate）")

    # 3. 等价性：板端口径 stdout 必须与正式期望逐行一致。
    def fixtures(name):
        """按 `check.sh` 的约定带上 `.in`（输入）与 `.args`（main 参数）。"""
        extra = []
        expected_dir = ROOT / "example/expected"
        data = expected_dir / (name + ".in")
        if data.exists():
            extra += ["--input", data.read_text(encoding="utf-8").strip()]
        arguments = expected_dir / (name + ".args")
        if arguments.exists():
            extra.append("--")
            extra += [line.rstrip("\r")
                      for line in arguments.read_text(encoding="utf-8").splitlines()]
        return extra

    def build_and_run(name, extra=None):
        source = ROOT / "example/samples" / (name + ".mgl")
        expected = ROOT / "example/expected" / (name + ".txt")
        assert source.exists() and expected.exists(), name
        prefix = WORK / name
        emitted = run([compiler, "run", source, "--no-reflection", "--emit", "--svmb", "--out", prefix])
        assert emitted.returncode == 0, (name, emitted.stdout, emitted.stderr)
        result = run([host, prefix.with_suffix(".svmb"),
                      Path(str(prefix) + ".data.bin"), Path(str(prefix) + ".layout")]
                     + (extra if extra is not None else fixtures(name)))
        want = expected.read_text(encoding="utf-8").replace("\r\n", "\n").strip()
        got = result.stdout.replace("\r\n", "\n").strip()
        assert result.returncode == 0 and got == want, \
            f"{name}: exit={result.returncode}\n期望：\n{want}\n实际：\n{got}\n{result.stderr}"
        return result.stderr

    failures = []
    for name in MUST_PASS:
        try:
            build_and_run(name)
            print(f"[通过] {name}")
        except AssertionError as error:
            failures.append(str(error))
            print(f"[失败] {name}: {str(error).splitlines()[0]}")
    for name in (COROUTINE_SAMPLE,):
        try:
            build_and_run(name)
            print(f"[通过] {name}（协程：不需要线程）")
        except AssertionError as error:
            failures.append(str(error))
            print(f"[失败] {name}")

    # footprint：把这次实际观测到的峰值打出来（调预算靠它）。
    stderr = build_and_run(COROUTINE_SAMPLE)
    print("[footprint] " + " | ".join(
        line.strip() for line in stderr.splitlines() if "mcu-vm" in line))

    # 4. 文件系统后端打开（`MCU_VM_FILE_BACKEND=1`，C stdio）：文件样例必须等价。
    #    证明 `stackvm::FileApi` 这层抽象在板端口径下真的接得上（换 FatFS 就是换后端）。
    fs_build = run([args.make, "-C", ROOT / "source/stackvm_mcu", "MCU_VM_FILE_BACKEND=1",
                    "MCU_TARGET_SUFFIX=-fs", "MCU_BUDGET=" + " ".join(budget)])
    assert fs_build.returncode == 0, fs_build.stdout + fs_build.stderr
    fs_host = ROOT / "build/stackvm_mcu" / ("mcu-host-fs" + EXE)
    assert fs_host.exists(), fs_host
    fs_prefix = WORK / "std_file_result"
    emitted = run([compiler, "run", ROOT / "example/samples/std_file_result.mgl",
                   "--no-reflection", "--emit", "--svmb", "--out", fs_prefix])
    assert emitted.returncode == 0, emitted.stdout + emitted.stderr
    result = run([fs_host, fs_prefix.with_suffix(".svmb"),
                  Path(str(fs_prefix) + ".data.bin"), Path(str(fs_prefix) + ".layout")])
    want = (ROOT / "example/expected/std_file_result.txt").read_text(
        encoding="utf-8").replace("\r\n", "\n").strip()
    assert result.returncode == 0 and result.stdout.replace("\r\n", "\n").strip() == want, \
        f"std_file_result（文件系统打开）：exit={result.returncode}\n{want}\n{result.stdout}"
    print("[通过] std_file_result（文件系统后端 stdio；换 FatFS 只需换后端/注入 FileApi）")
    bytes_prefix = WORK / "std_file_bytes"
    emitted = run([compiler, "svmb", ROOT / "example/samples/std_file_bytes.mgl", "--no-reflection", "out", bytes_prefix])
    assert emitted.returncode == 0, emitted.stderr
    result = run([fs_host, bytes_prefix.with_suffix(".svmb"),
                  Path(str(bytes_prefix)+".data.bin"), Path(str(bytes_prefix)+".layout")])
    assert result.returncode == 0 and result.stdout.strip() == "Binary FileStream bytes and EOF OK", result
    assert "运行期错误" not in result.stderr and "没释放" not in result.stderr, result.stderr
    assert (ROOT/"build/std-file-bytes.bin").read_bytes() == bytes([0,10,255,13])
    print("[通过] FileStream 字节读写（stdio 后端；零字节、EOF 与关闭错误）")
    for fixture, runtime, expected in (("language_move", host, "move ok"), ("move_file", fs_host, "file move ok")):
        prefix = WORK / fixture
        emitted = run([compiler, "svmb", ROOT / "tests/fixtures" / (fixture+".mgl"), "--no-reflection", "out", prefix])
        assert emitted.returncode == 0, emitted.stderr
        result = run([runtime, prefix.with_suffix(".svmb"), Path(str(prefix)+".data.bin"), Path(str(prefix)+".layout")])
        assert result.returncode == 0 and result.stdout.strip() == expected, result
        assert "没释放" not in result.stderr and "运行期错误" not in result.stderr, result.stderr
        print("[通过] 真正移动所有权：" + fixture)

    # 5. 流式输出：输出下沉写到文件，断言文件内容与期望逐行一致。
    #    板端就是这么接串口的（`keepText=false`，不再累积 `RunResult::text`）。
    streamed = WORK / "streamed.txt"
    stream_prefix = WORK / "stream_print"
    emitted = run([compiler, "run", ROOT / "example/samples/hello.mgl",
                   "--no-reflection", "--emit", "--svmb", "--out", stream_prefix])
    assert emitted.returncode == 0, emitted.stdout + emitted.stderr
    result = run([host, stream_prefix.with_suffix(".svmb"),
                  Path(str(stream_prefix) + ".data.bin"),
                  Path(str(stream_prefix) + ".layout"), "--out", streamed])
    assert result.returncode == 0, result.stderr
    want = (ROOT / "example/expected/hello.txt").read_text(
        encoding="utf-8").replace("\r\n", "\n").strip()
    assert streamed.read_text(encoding="utf-8").replace("\r\n", "\n").strip() == want, \
        "流式输出（输出下沉）与期望不一致"
    assert result.stdout.strip() == "", "设了输出下沉就不该再往 stdout 写文本"
    print("[通过] 流式输出（输出下沉 → 文件；串口/OLED/RTT 同理）")

    # 6. 软重启触发②：`std::watchdog`（显式请求 / 程序侧水位；未 enable 时不产生重启）。
    #    触发点只写"标志 + 原因"，VM 在**下一条指令边界**以可恢复终止结束本轮 —— 所以
    #    请求之后的 `tail` 不该打出来，宿主按原因重置 arena 重跑。
    def watchdog_probe(watermark, request, enable=True):
        """生成一个探针程序：可选 enable(水位)，可选 requestRestart()。"""
        opened = (f'    match (std::watchdog::enable({watermark})) {{\n'
                  '        Result<int64, std::watchdog::WatchdogError>::Ok(w) => { std::lang::println("on {}", w); }\n'
                  '        Result<int64, std::watchdog::WatchdogError>::Err(reason) => {\n'
                  '            match (reason) {\n'
                  '                std::watchdog::WatchdogError::InvalidWatermark => { std::lang::println("bad"); }\n'
                  '                std::watchdog::WatchdogError::InvalidTimeout => { std::lang::println("bad timeout"); }\n'
                  '                std::watchdog::WatchdogError::NotEnabled => { std::lang::println("off"); }\n'
                  '            }\n'
                  '        }\n'
                  '    }\n') if enable else ''
        asked = ('    match (std::watchdog::requestRestart()) {\n'
                 '        Result<bool, std::watchdog::WatchdogError>::Ok(ok) => { std::lang::println("asked {}", ok); }\n'
                 '        Result<bool, std::watchdog::WatchdogError>::Err(reason) => {\n'
                 '            match (reason) {\n'
                 '                std::watchdog::WatchdogError::NotEnabled => { std::lang::println("not enabled"); }\n'
                 '                std::watchdog::WatchdogError::InvalidWatermark => { std::lang::println("bad"); }\n'
                 '                std::watchdog::WatchdogError::InvalidTimeout => { std::lang::println("bad timeout"); }\n'
                 '            }\n'
                 '        }\n'
                 '    }\n') if request else ''
        return ('import std::watchdog;\n\nint64 main() {\n' + opened + asked +
                '    std::lang::println("tail");\n    return 0;\n}\n')

    def run_watchdog(name, source, extra=None):
        """写探针 → 编成 .svmb → 板端口径跑一轮。"""
        path = WORK / (name + ".mgl")
        path.write_text(source, encoding="utf-8")
        prefix = WORK / name
        emitted = run([compiler, "run", path, "--no-reflection", "--emit", "--svmb", "--out", prefix])
        assert emitted.returncode == 0, (name, emitted.stdout, emitted.stderr)
        return run([host, prefix.with_suffix(".svmb"),
                    Path(str(prefix) + ".data.bin"), Path(str(prefix) + ".layout")]
                   + (extra if extra is not None else []))

    # ② a：enable() 之后显式请求 → 可恢复终止 + 软重启（tail 打不出来）。
    result = run_watchdog("watchdog_request", watchdog_probe(0, True), ["--max-restarts", "1"])
    assert "程序显式重启请求" in result.stderr, f"没识别出显式请求：{result.stderr}"
    assert "程序显式请求触发" in result.stderr, f"没按显式请求重启：{result.stderr}"
    assert "tail" not in result.stdout, f"请求之后还在跑：{result.stdout}"
    assert result.returncode != 0, "重启次数用尽时应当报失败"
    print("[通过] 软重启触发②：std::watchdog::requestRestart()（可恢复终止 + 重启）")

    # ② b：没 enable() 过 → 明确报错、不产生重启（程序照常跑完）。
    result = run_watchdog("watchdog_not_enabled", watchdog_probe(0, True, enable=False))
    assert result.returncode == 0, f"未 enable 不该重启：{result.stderr}"
    assert "not enabled" in result.stdout and "tail" in result.stdout, result.stdout
    assert "软重启" not in result.stderr, f"未 enable 却重启了：{result.stderr}"
    print("[通过] 软重启触发②：未 enable() 时明确报错、不重启")

    # ② c：程序侧水位（enable(N)，宿主不带 --watermark）→ 宿主每轮量出来就重启。
    result = run_watchdog("watchdog_watermark", watchdog_probe(1024, False))
    assert "内存水位触发" in result.stderr, f"程序侧水位没生效：{result.stderr}"
    print("[通过] 软重启触发①：程序侧 enable(水位) 由宿主量出并重启")

    if args.sweep:
        buckets = {"pass": [], "ram": [], "native": [], "off_feature": [],
                   "host_need": [], "pending": []}
        samples = sorted((ROOT / "example/samples").glob("*.mgl"))
        for source in samples:
            name = source.stem
            if not (ROOT / "example/expected" / (name + ".txt")).exists():
                continue
            lines = source.read_text(encoding="utf-8", errors="replace")
            if "import std::" in lines:
                continue
            prefix = WORK / "sweep" / name
            prefix.parent.mkdir(parents=True, exist_ok=True)
            emitted = run([compiler, "run", source, "--no-reflection", "--emit", "--svmb", "--out", prefix])
            if emitted.returncode != 0:
                continue
            result = run([host, prefix.with_suffix(".svmb"),
                          Path(str(prefix) + ".data.bin"), Path(str(prefix) + ".layout")]
                         + fixtures(name))
            if result.returncode == 0:
                buckets["pass"].append(name)
            elif "入口不存在（也不在原生函数表里）：" in result.stderr:
                marker = "入口不存在（也不在原生函数表里）："
                at = result.stderr.index(marker) + len(marker)
                missing = result.stderr[at:].split()[0].strip()
                # `std::xxx::` / `gui::` 是**宿主胶水**（板端要自己提供），
                # 不是 VM 原生函数缺口——分开统计，否则会把"没接 HAL"误报成"VM 缺东西"。
                if missing.startswith(("std::", "gui::")):
                    buckets["host_need"].append(name)
                else:
                    buckets["native"].append(missing)
            elif "元数据 arena 字节不够" in result.stderr or \
                    "元数据 块表条目不够" in result.stderr:
                buckets["ram"].append(name)
            elif any(marker in result.stderr for marker in KNOWN_OFF_FEATURES):
                buckets["off_feature"].append(name)
            elif any(marker in result.stderr for marker in KNOWN_HOST_NEEDS):
                buckets["host_need"].append(name)
            else:
                buckets["pending"].append(name)
        for key in ("pass", "native", "ram", "off_feature", "host_need", "pending"):
            print(f"[sweep] {key}: {len(buckets[key])}"
                  + (f" {sorted(buckets[key])}" if key in ("native", "ram", "pending")
                     and len(buckets[key]) < 40 else ""))
        if buckets["native"]:
            print(f"[sweep] native 名字集合：{sorted(set(buckets['native']))}")
        # 只允许"已知缺口"；样例自带的 C++ 胶水（demo_* / glue_*）不算 VM 缺口。
        unexpected = sorted({n for n in buckets["native"]
                             if n not in KNOWN_MISSING_NATIVES and not n.startswith("demo_")
                             and not n.startswith("glue_")})
        assert not unexpected, f"出现新的原生函数缺口：{unexpected}"

    if failures:
        raise SystemExit("\n\n".join(failures))
    print(f"mcu_vm 门禁通过（{len(MUST_PASS) + 1} 个样例等价性）")
    return 0


if __name__ == "__main__":
    sys.exit(main())
