"""按模块执行门禁，并以可复现随机种子抽样复测已通过的门禁。"""
import argparse
import json
import os
from pathlib import Path
import random
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]
MODULES = {
    "language": ["magel_entry_signature", "magel_function_types", "magel_lambdas"],
    "borrow": ["magel_borrow_flow", "magel_analysis_diagnostics", "magel_reference_values"],
    "memory": ["magel_memory_guard", "magel_tombstone", "magel_watchdog"],
    "generics": ["magel_generic_monomorphization", "magel_language_move", "magel_tuple_returns"],
    "mcu": ["mcu_vm"],
    "samples": [],
    "tools": ["magel_module_compilation", "magel_package", "magel_aot"],
}


def main():
    """运行指定模块，逐项保存日志与结果，失败时返回非零状态。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("modules", nargs="+", choices=[*MODULES, "docs"])
    parser.add_argument("--magel", type=Path, default=ROOT / ("magel.exe" if os.name == "nt" else "magel"))
    parser.add_argument("--random-recheck", type=int, default=0, help="从已通过门禁抽样复测的次数")
    parser.add_argument("--seed", type=int, default=20261013)
    args = parser.parse_args()
    if args.random_recheck < 0:
        parser.error("随机复测次数必须非负")
    base = ROOT / "build" / "module-gates"
    base.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix=time.strftime("%Y%m%d-%H%M%S-"), dir=base))
    commands = {}
    for module in args.modules:
        if module == "samples":
            commands["samples"] = ["sh", "check.sh"]
        elif module == "docs":
            for script, flags in (("check_doc_links", []), ("export_magel_ebnf", ["--check"]),
                                  ("library_reference", ["--check"]), ("sync_vm_execution", ["--check"])):
                commands[script] = [sys.executable, str(ROOT / "tools" / (script + ".py")), *flags]
        else:
            for script in MODULES[module]:
                commands[script] = [sys.executable, str(ROOT / "tests" / (script + ".py")),
                                    "--magel", str(args.magel.resolve())]
    results = []

    def execute(name, command, recheck=False):
        """执行一项门禁，独立日志保留失败上下文。"""
        label = f"{len(results) + 1:02d}-{name}"
        print(f"{'随机复测' if recheck else '门禁'} {label}", flush=True)
        started = time.monotonic()
        with (output / (label + ".log")).open("wb") as log:
            completed = subprocess.run(command, cwd=ROOT, stdout=log, stderr=subprocess.STDOUT,
                                       env=dict(os.environ, DEMO=args.magel.resolve().as_posix(), PYTHONUNBUFFERED="1"))
        results.append({"name": name, "command": command, "recheck": recheck,
                        "exit_code": completed.returncode, "seconds": round(time.monotonic() - started, 2)})
        (output / "results.json").write_text(json.dumps({"seed": args.seed, "results": results},
                                                       ensure_ascii=False, indent=2), encoding="utf-8")
        print(f"  {'通过' if completed.returncode == 0 else '失败'}：{output / (label + '.log')}", flush=True)
        return completed.returncode == 0

    passed = []
    for name, command in commands.items():
        if execute(name, command):
            passed.append(name)
    rng = random.Random(args.seed)
    for _ in range(args.random_recheck):
        if not passed:
            break
        name = rng.choice(passed)
        execute(name, commands[name], True)
    return int(any(result["exit_code"] != 0 for result in results))


if __name__ == "__main__":
    sys.exit(main())
