#!/usr/bin/env python3
"""跨语言微基准驱动：同一份算法在各语言下各跑若干轮，取中位数并核对校验值。

用法（仓库根目录）：

    python benchmarks/perf/run.py             # 全部配置，3 次进程 × 5 轮
    python benchmarks/perf/run.py --runs 5    # 跑 5 次进程
    python benchmarks/perf/run.py --only chali,c,java-jit

每份程序自己用单调时钟包住 `work()`，打印若干行 `rep <纳秒>` 和一行 `sink <校验值>`；
脚本只做构建、调度、中位数统计与校验值比对，不参与计时。
"""

import argparse
import json
import os
import platform
import re
import shutil
import statistics
import subprocess
import sys
import tempfile
from datetime import datetime
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent
CASES = ["int_chain", "float_chain", "call", "array", "list"]
CASE_TITLE = {
    "int_chain": "整数依赖链（Lehmer 乘加）",
    "float_chain": "浮点依赖链（乘加）",
    "call": "小函数调用",
    "array": "固定数组循环读取",
    "list": "动态数组 push + 遍历",
}


def find(name):
    return shutil.which(name)


class Config:
    def __init__(self, key, label, run, build=None, note="", cwd=None):
        self.key = key
        self.label = label
        self.run = run          # (case) -> [cmd...]
        self.build = build      # (case) -> [cmd...] or None
        self.note = note
        self.cwd = cwd          # 单独的工作目录（ChaLi 会把中间产物写到 ./build）


def java_cmd(java, mode):
    if mode == "xint":
        return lambda case: [java, "-Xint", "-cp", str(HERE), case]
    return lambda case: [java, "-Xbatch", "-cp", str(HERE), case]


def build_configs():
    configs = []
    chali = os.environ.get("CHALI") or str(ROOT / ("ChaLi.exe" if os.name == "nt" else "ChaLi"))
    try:
        version = (ROOT / "VERSION").read_text(encoding="utf-8").strip()
    except OSError:
        version = "dev"
    scratch = ROOT / "tmp" / "perf-run"
    scratch.mkdir(parents=True, exist_ok=True)
    configs.append(Config("chali", f"ChaLi {version}（StackVM，默认后端）",
                          lambda case: [chali, "run", str(HERE / f"{case}.chl")],
                          note="字节码解释执行，无即时编译", cwd=str(scratch)))
    if find("gcc"):
        configs.append(Config("c", "C（gcc -O2）",
                              lambda case: [str(HERE / f"{case}-c.exe")],
                              lambda case: [find("gcc"), "-O2", "-DNDEBUG", str(HERE / f"{case}.c"),
                                            "-o", str(HERE / f"{case}-c.exe")],
                              note="原生机器码基线"))
    if find("g++"):
        configs.append(Config("cpp", "C++17（g++ -O2）",
                              lambda case: [str(HERE / f"{case}-cpp.exe")],
                              lambda case: [find("g++"), "-O2", "-DNDEBUG", str(HERE / f"{case}.cpp"),
                                            "-o", str(HERE / f"{case}-cpp.exe")],
                              note="原生机器码"))
    if find("rustc"):
        configs.append(Config("rust", "Rust（rustc -O）",
                              lambda case: [str(HERE / f"{case}-rs.exe")],
                              lambda case: [find("rustc"), "-O", str(HERE / f"{case}.rs"),
                                            "-o", str(HERE / f"{case}-rs.exe")],
                              note="原生机器码"))
    if find("go"):
        configs.append(Config("go", "Go（默认优化）",
                              lambda case: [str(HERE / f"{case}-go.exe")],
                              lambda case: [find("go"), "build", "-o", str(HERE / f"{case}-go.exe"),
                                            str(HERE / f"{case}.go")],
                              note="原生机器码，带 GC"))
    if find("java") and find("javac"):
        configs.append(Config("java-xint", "Java 22 `-Xint`（纯解释）",
                              java_cmd(find("java"), "xint"),
                              lambda case: [find("javac"), "-d", str(HERE), str(HERE / f"{case}.java")],
                              note="HotSpot 解释器，关即时编译"))
        configs.append(Config("java-jit", "Java 22 `-Xbatch`（即时编译）",
                              java_cmd(find("java"), "jit"),
                              note="同一份 class，分层编译，同步编译"))
    if find("node"):
        configs.append(Config("node", "Node 26（V8 即时编译）",
                              lambda case: [find("node"), str(HERE / f"{case}.js")],
                              note="JIT，无预热控制"))
    if find("python"):
        configs.append(Config("python", "CPython 3.12（纯解释）",
                              lambda case: [find("python"), str(HERE / f"{case}.py")],
                              note="默认解释器，无 JIT"))
    return configs


REP_RE = re.compile(r"^rep\s+(\d+)\s*$")
SINK_RE = re.compile(r"^sink\s+(\S+)\s*$")


def run_once(cmd, cwd):
    proc = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True, encoding="utf-8", errors="replace")
    if proc.returncode != 0:
        raise RuntimeError(f"命令失败（{proc.returncode}）：{' '.join(cmd)}\n{proc.stderr[-2000:]}")
    reps, sink = [], None
    for line in proc.stdout.splitlines():
        line = line.strip()
        m = REP_RE.match(line)
        if m:
            reps.append(int(m.group(1)))
            continue
        m = SINK_RE.match(line)
        if m:
            sink = m.group(1)
    if not reps or sink is None:
        raise RuntimeError(f"输出不符合约定：{' '.join(cmd)}\n{proc.stdout[-2000:]}")
    return reps, sink


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--runs", type=int, default=3, help="每个配置跑几次进程（每次内含 5 轮）")
    parser.add_argument("--only", default="", help="只跑这些配置，逗号分隔")
    parser.add_argument("--out", default="", help="结果目录")
    args = parser.parse_args()

    configs = build_configs()
    if args.only:
        wanted = [item.strip() for item in args.only.split(",") if item.strip()]
        configs = [c for c in configs if c.key in wanted]

    print(f"配置 {len(configs)} 个：{', '.join(c.key for c in configs)}")
    built = set()
    for cfg in configs:
        if cfg.build is None:
            continue
        for case in CASES:
            print(f"  构建 {cfg.key}/{case} …", flush=True)
            proc = subprocess.run(cfg.build(case), capture_output=True, text=True, encoding="utf-8", errors="replace")
            if proc.returncode != 0:
                print(proc.stdout[-1500:])
                print(proc.stderr[-1500:])
                sys.exit(f"构建失败：{cfg.key}/{case}")

    results = {}
    sinks = {}
    for cfg in configs:
        for case in CASES:
            samples, checked = [], None
            for _ in range(args.runs):
                reps, sink = run_once(cfg.run(case), cfg.cwd or str(HERE))
                samples.extend(reps)
                if checked is None:
                    checked = sink
                elif checked != sink:
                    sys.exit(f"校验值不一致：{cfg.key}/{case} {checked} != {sink}")
            results.setdefault(cfg.key, {})[case] = samples
            sinks.setdefault(case, {})[cfg.key] = checked
            print(f"  {cfg.key:<10} {case:<12} 中位 {statistics.median(samples)/1e6:9.3f} ms", flush=True)

    # 校验值必须跨语言一致：这是"各语言确实算了同样的活"的唯一证据。
    for case, table in sinks.items():
        values = set(table.values())
        if len(values) != 1:
            sys.exit(f"校验值跨语言不一致：{case} → {table}")

    stamp = datetime.now().strftime("%Y%m%d-%H%M%S")
    host = platform.node() or "host"
    out_dir = Path(args.out) if args.out else ROOT / "benchmarks" / "results" / f"{host.lower()}-{stamp}-perf"
    out_dir.mkdir(parents=True, exist_ok=True)

    summary = {
        "generated": stamp,
        "host": host,
        "platform": platform.platform(),
        "processor": platform.processor(),
        "python": platform.python_version(),
        "runs": args.runs,
        "reps_per_run": 5,
        "cases": CASES,
        "sinks": sinks,
        "configs": {},
    }
    for cfg in configs:
        summary["configs"][cfg.key] = {
            "label": cfg.label,
            "note": cfg.note,
            "median_ms": {case: round(statistics.median(results[cfg.key][case]) / 1e6, 3) for case in CASES},
            "min_ms": {case: round(min(results[cfg.key][case]) / 1e6, 3) for case in CASES},
            "max_ms": {case: round(max(results[cfg.key][case]) / 1e6, 3) for case in CASES},
            "samples_ns": {case: results[cfg.key][case] for case in CASES},
        }
    (out_dir / "results.json").write_text(json.dumps(summary, ensure_ascii=False, indent=2), encoding="utf-8")

    base = "c" if "c" in summary["configs"] else configs[0].key
    lines = ["| 配置 | " + " | ".join(CASE_TITLE[c] for c in CASES) + " |",
             "| --- | " + " | ".join("---:" for _ in CASES) + " |"]
    for cfg in configs:
        cells = []
        for case in CASES:
            ms = summary["configs"][cfg.key]["median_ms"][case]
            cells.append(f"{ms:.3f}")
        lines.append(f"| {cfg.label} | " + " | ".join(cells) + " |")
    table = "\n".join(lines)

    rel = ["| 配置 | " + " | ".join(CASE_TITLE[c] + " ÷ C" for c in CASES) + " |",
           "| --- | " + " | ".join("---:" for _ in CASES) + " |"]
    for cfg in configs:
        cells = []
        for case in CASES:
            mine = statistics.median(results[cfg.key][case]) / 1e6
            theirs = statistics.median(results[base][case]) / 1e6
            cells.append("1.00" if theirs <= 0 or cfg.key == base else f"{mine / theirs:.1f}")
        rel.append(f"| {cfg.label} | " + " | ".join(cells) + " |")

    (out_dir / "summary.md").write_text(
        f"# 跨语言微基准（{stamp}）\n\n中位数（ms，越小越快），每格来自 {args.runs} 次进程 × 5 轮。\n\n{table}\n\n"
        f"相对 C 的倍数（同一列除以 C 的中位数）：\n\n" + "\n".join(rel) + "\n",
        encoding="utf-8")

    print("\n" + table)
    print("\n相对 C 的倍数：")
    print("\n".join(rel))
    print(f"\n结果目录：{out_dir}")


if __name__ == "__main__":
    main()
