"""用分支、函数调用及结构/图算法比较分派方式，或交替对照运行时服务快速路径与旧提交。"""
import argparse
import json
import os
from pathlib import Path
import statistics
import subprocess
from magel_algorithm_robustness import encode, structures

ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / "build/register-dispatch"


def run(command):
    """执行命令并保留标准输出及计数诊断。"""
    result = subprocess.run(list(map(str, command)), cwd=ROOT, capture_output=True,
                            encoding="utf-8", errors="replace", env=dict(os.environ, MAGEL_LANG="zh"))
    if result.returncode:
        raise RuntimeError((command, result.returncode, result.stdout, result.stderr))
    return result.stdout, result.stderr


def main():
    """编译固定程序，串行测量各执行模式，核对独立期望结果。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--magel", type=Path, default=ROOT / "magel.exe")
    parser.add_argument("--runner", type=Path, default=ROOT / "build/tier-six-benchmark.exe")
    parser.add_argument("--baseline-runner", type=Path)
    parser.add_argument("--baseline-magel", type=Path, help="用旧编译器单独生成基线字节码，比较编译优化")
    parser.add_argument("--services", action="store_true", help="交替测量运行时服务开关，可附加旧提交运行器")
    parser.add_argument("--repeats", type=int, default=7)
    parser.add_argument("--include-loop", action="store_true", help="增加一千万次纯整数累加循环，检查热路径回退")
    parser.add_argument("--grouped", action="store_true", help="连续测完一种模式后再测另一种；默认交替顺序")
    args = parser.parse_args()
    assert args.repeats > 0
    WORK.mkdir(parents=True, exist_ok=True)
    cases = []
    loop_cases = [
        ("branches", "if((i & 1)==0){total+=i;}else{total-=i;}", 1000000, -500000),
        ("calls", "total+=mix(i);", 10000, 17*10000*9999//2),
    ]
    if args.include_loop:
        loop_cases.append(("loop", "total+=i;", 10000000, 10000000*9999999//2))
    for name, body, count, expected in loop_cases:
        source = WORK / (name + ".mgl")
        source.write_text("int64 mix(int64 x){return x*17;}\n"
                          "int64 main(){int64 n=std::lang::inputInt(),total=0;"
                          "for(int64 i=0;i<n;i++){" + body + "}"
                          "std::lang::println(\"{}\",total);return 0;}\n", encoding="utf-8")
        cases.append((name, source, str(count), str(expected)))
    cases.append(("structures", ROOT / "tests/fixtures/borrow_algorithms.mgl", "32 42\n", structures(32,42)))
    n = 128
    cases.append(("transport", ROOT / "tests/fixtures/noip2015_transport.mgl",
                  encode(n, [(v-1,v,1000) for v in range(2,n+1)], [(1,n)]*n), str((n-2)*1000)))
    report = {"warmups_per_process": 1, "repeats": args.repeats,
              "order": "grouped" if args.grouped else "alternate configuration order each round; one measured sample per process",
              "timing": "sequential, loaded bytecode; input file read before timing; includes execution preparation and checksum output",
              "services_comparison": args.services,
              "baseline_compiler": str(args.baseline_magel) if args.baseline_magel else None,
              "results": {}}
    for name, source, data, expected in cases:
        prefix = WORK / name
        run([args.magel.resolve(), "svmb", source, "out", prefix])
        baseline_prefix = WORK / (name + "-baseline")
        if args.baseline_magel:
            run([args.baseline_magel.resolve(), "svmb", source, "out", baseline_prefix])
        input_file = WORK / (name + ".in")
        input_file.write_text(data, encoding="utf-8")
        report["results"][name] = {}
        configurations = [("switch", args.runner, 0, None), ("labels", args.runner, 1, None)]
        if args.services:
            configurations = [("services_off", args.runner, 1, 0), ("services_on", args.runner, 1, 1)]
            if args.baseline_runner:
                configurations.append(("baseline", args.baseline_runner, 1, None))
        if args.baseline_magel:
            configurations = [("current", args.runner, 1, 1),
                              ("baseline", args.baseline_runner or args.runner, 1, 1)]
        collected = {mode: [] for mode, _, _, _ in configurations}
        records = {}
        for round_index in range(1 if args.grouped else args.repeats):
            modes = configurations.copy()
            if round_index % 2:
                modes.reverse()
            for mode, runner, enabled, services in modes:
                program_prefix = baseline_prefix if args.baseline_magel and mode == "baseline" else prefix
                command = [runner.resolve(), program_prefix, args.repeats if args.grouped else 1,
                           1, "@"+str(input_file), 1, enabled, 1, 1, 1]
                if services is not None:
                    command.append(services)
                output, counters = run(command)
                for line in output.splitlines():
                    elapsed, logical_steps, checksum = line.split("\t", 2)
                    assert checksum == expected, (name, mode, checksum, expected)
                    if mode in records:
                        assert int(logical_steps) == records[mode]["logical_steps"]
                    collected[mode].append(float(elapsed))
                    records[mode] = {"logical_steps": int(logical_steps), "checksum": expected,
                                     "counters": counters.strip(), "command": list(map(str, command))}
        if not args.baseline_magel:
            assert len({record["logical_steps"] for record in records.values()}) == 1, (name, records)
        for mode, samples in collected.items():
            assert len(samples) == args.repeats
            report["results"][name][mode] = dict(records[mode], median_ms=statistics.median(samples), samples_ms=samples)
        print(name, {mode: result["median_ms"] for mode,result in report["results"][name].items()}, flush=True)
    (WORK / "results.json").write_text(json.dumps(report, ensure_ascii=False, indent=2)+"\n", encoding="utf-8")


if __name__ == "__main__":
    main()
