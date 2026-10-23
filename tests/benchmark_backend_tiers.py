"""后端分档基准：同一个可执行文件、同一份程序，只换后端开关，量出各档的耗时与规模。

五个档位就是 `tests/vm_differential.py` 守的那五组：

  ① 默认（寄存器执行核 + 槽位复用 + 机器级优化）
  ② `CHALI_NO_REGISTER_CORE=1` —— 通用栈解释核
  ③ `CHALI_NO_SLOT_REUSE=1`    —— 一人一槽（复用前）
  ④ `CHALI_NO_MACHINE_OPT=1`   —— 关掉机器级优化
  ⑤ `CHALI_NO_OPT_INLINE=1`    —— 只关小函数内联（优化层里唯一复制代码的一遍）

耗时取多轮最小值（避免抖动），同时解析 `--stats` 里的规模数字（四元式条数、指令选择覆盖率、
帧槽数），这样"前后数字"既有时间也有体积。基准不进 CI 门禁：机器负载会让毫秒数波动，
它只用来在改后端时给自己一个前后对照。
"""
import argparse
import os
from pathlib import Path
import re
import statistics
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]
EXE = ".exe" if os.name == "nt" else ""

PROGRAMS = {
    "int_loop": """void main() {
    int64 total = 0;
    int64 i = 0;
    while (i < 12000000) {
        total = total + i % 7;
        i = i + 1;
    }
    println("{}", total);
}
""",
    "float_loop": """void main() {
    float64 total = 0.0;
    int64 i = 0;
    while (i < 4000000) {
        total = total + 1.5;
        i = i + 1;
    }
    println("{}", total);
}
""",
    "calls": """int64 step(int64 value) {
    return value + 3;
}

void main() {
    int64 total = 0;
    int64 i = 0;
    while (i < 2000000) {
        total = step(total);
        i = i + 1;
    }
    println("{}", total);
}
""",
}

MODES = [
    ("默认", {}),
    ("栈解释核", {"CHALI_NO_REGISTER_CORE": "1"}),
    ("不复用槽位", {"CHALI_NO_SLOT_REUSE": "1"}),
    ("关优化", {"CHALI_NO_MACHINE_OPT": "1"}),
    ("关内联", {"CHALI_NO_OPT_INLINE": "1"}),
]


def resolve_compiler(name):
    """把 `--chali` 解析成实际可执行文件，兼容 `chali`、`ChaLi.exe` 与绝对路径。"""
    candidate = Path(name)
    if candidate.is_absolute():
        assert candidate.is_file(), f"找不到编译器：{candidate}"
        return candidate
    for option in (ROOT / candidate, ROOT / (candidate.name + EXE)):
        if option.is_file():
            return option
    raise SystemExit(f"找不到编译器：{name}")


def environment(overrides):
    """清掉三组开关再叠加本档设置，避免外部环境污染这次测量。"""
    result = dict(os.environ)
    for name in ("CHALI_NO_REGISTER_CORE", "CHALI_NO_SLOT_REUSE", "CHALI_NO_MACHINE_OPT"):
        result.pop(name, None)
    result.update(overrides)
    result["CHALI_LANG"] = "zh"
    return result


def measure_round(chali, source, overrides):
    """跑一轮，返回 (耗时秒, 输出的最后一行)。"""
    start = time.perf_counter()
    result = subprocess.run([str(chali), str(source)], capture_output=True, text=True,
                            encoding="utf-8", errors="replace", env=environment(overrides), timeout=600)
    elapsed = time.perf_counter() - start
    assert result.returncode == 0, f"{source.name} 在 [{overrides}] 下失败：{result.stderr}"
    last = result.stdout.strip().splitlines()[-1] if result.stdout.strip() else ""
    return elapsed, last


def measure_all(chali, source, repeats):
    """交替跑各档 repeats 轮，返回 {档位: 最小耗时}。

    必须交替：先跑完一档再跑下一档时，第一次运行要付进程与磁盘冷启动的代价，
    那几毫秒到一百多毫秒会全部记到排在最前面的档位头上，得出"优化反而更慢"的假结论。
    """
    best = {label: float("inf") for label, _ in MODES}
    outputs = {}
    for _ in range(repeats):
        for label, overrides in MODES:
            elapsed, last = measure_round(chali, source, overrides)
            best[label] = min(best[label], elapsed)
            outputs[label] = last
    expected = set(outputs.values())
    assert len(expected) == 1, f"{source.name} 各档输出不一致：{outputs}"
    return best


def scale_of(chali, source, overrides):
    """读 `--stats` 的三行后端统计，返回 (四元式条数, 覆盖率, 符号数, 槽数, 内联处数)。"""
    result = subprocess.run([str(chali), "run", str(source), "--stats"], capture_output=True, text=True,
                            encoding="utf-8", errors="replace", env=environment(overrides), timeout=600)
    assert result.returncode == 0, f"读统计失败：{result.stderr}"
    text = result.stderr
    quads = re.search(r"机器级优化：四元式 (\d+) → (\d+)", text)
    covered = re.search(r"覆盖栈指令 (\d+)/(\d+)", text)
    slots = re.search(r"符号 (\d+) 个 → 槽 (\d+) 个", text)
    inlined = re.search(r"内联 (\d+) 处", text)
    return (
        int(quads.group(2)) if quads else 0,
        (int(covered.group(1)), int(covered.group(2))) if covered else (0, 0),
        int(slots.group(1)) if slots else 0,
        int(slots.group(2)) if slots else 0,
        int(inlined.group(1)) if inlined else 0,
    )


def main():
    """跑固定的三段程序，按五档开关输出耗时与规模对照表。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--chali", default="ChaLi.exe")
    parser.add_argument("--repeats", type=int, default=3)
    args = parser.parse_args()
    chali = resolve_compiler(args.chali)

    with tempfile.TemporaryDirectory(prefix="chali-backend-bench-") as directory:
        sources = {}
        for name, text in PROGRAMS.items():
            path = Path(directory) / f"{name}.chl"
            path.write_text(text, encoding="utf-8")
            sources[name] = path

        results = {}
        for name, source in sources.items():
            timings = measure_all(chali, source, args.repeats)
            for label, _ in MODES:
                results[(name, label)] = (timings[label], "", "")

        print(f"{'程序':<12}{'档位':<12}{'最小耗时(ms)':>13}{'相对默认':>10}")
        for name in sources:
            baseline = results[(name, "默认")][0]
            for label, _ in MODES:
                elapsed = results[(name, label)][0]
                ratio = elapsed / baseline if baseline else 1.0
                print(f"{name:<12}{label:<12}{elapsed * 1000:>13.1f}{ratio:>9.2f}x")

        print()
        print("规模（默认档 / 不复用槽位档）：")
        for name, source in sources.items():
            quads, (covered, total), symbols, slots, inlined = scale_of(chali, source, MODES[0][1])
            _, _, symbolsRaw, slotsRaw, _ = scale_of(chali, source, MODES[2][1])
            reused = f"帧槽 {symbols}→{slots}" if symbols else "帧槽 —"
            plain = f"不复用 {symbolsRaw}→{slotsRaw}" if symbolsRaw else "不复用档不经过分配器"
            print(f"  {name:<12} 四元式 {quads}，内联 {inlined} 处，指令选择 {covered}/{total}，{reused}（{plain}）")

        timings = statistics.mean(results[(name, "默认")][0] for name in sources)
        print()
        print(f"默认档三段程序平均最小耗时 {timings * 1000:.1f} ms")
    return 0


if __name__ == "__main__":
    sys.exit(main())
