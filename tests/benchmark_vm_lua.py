"""从官方源码构建 Lua，串行交替测量 Magel VM 与 Lua 的五类相同工作负载。"""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import platform
import statistics
import subprocess
import tarfile
import time
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / "build/vm-lua-benchmark"
URL = "https://www.lua.org/ftp/lua-5.5.1.tar.gz"
SHA256 = "1c4b4068d67061f2a2231ad2b5422e77acea1487ea9890f6320af614f4373dce"


def run(command, **kwargs):
    """执行命令，失败时保留诊断，成功时返回标准输出。"""
    result = subprocess.run(list(map(str, command)), cwd=ROOT, capture_output=True,
                            text=True, encoding="utf-8", errors="replace", **kwargs)
    if result.returncode:
        raise RuntimeError((command, result.returncode, result.stdout, result.stderr))
    return result.stdout


def main():
    """预热后交替测量并保存所有样本、构建信息和校验和。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--magel", type=Path, default=ROOT / "magel.exe")
    parser.add_argument("--runner", type=Path, required=True)
    parser.add_argument("--baseline-magel", type=Path)
    parser.add_argument("--baseline-runner", type=Path)
    parser.add_argument("--repeats", type=int, default=7)
    parser.add_argument("--download", action="store_true", help="重新下载官方源码包")
    parser.add_argument("--output", type=Path, default=WORK / "results.json")
    args = parser.parse_args()
    assert args.repeats > 0
    assert bool(args.baseline_magel) == bool(args.baseline_runner)
    WORK.mkdir(parents=True, exist_ok=True)
    archive = WORK / "lua-5.5.1.tar.gz"
    if args.download or not archive.exists():
        urllib.request.urlretrieve(URL, archive)
    assert hashlib.sha256(archive.read_bytes()).hexdigest() == SHA256
    lua_root = WORK / "lua-5.5.1"
    if not lua_root.exists():
        with tarfile.open(archive) as source:
            source.extractall(WORK, filter="data")
    run(["make", "-C", lua_root / "src", "mingw" if os.name == "nt" else "linux", "-j4"])
    lua = lua_root / "src" / ("lua.exe" if os.name == "nt" else "lua")
    # 整数和浮点分别测试；Lua 只有双精度，float32 另由执行差分验证。
    cases = [
        ("integer_loop", 10000000, "int64", "total+=i;", "total=total+i", 49999995000000),
        ("branches", 1000000, "int64", "if((i&1)==0){total+=i;}else{total-=i;}",
         "if (i & 1)==0 then total=total+i else total=total-i end", -500000),
        ("float64_loop", 2000000, "float64", "total+=0.5;", "total=total+0.5", 1000000),
        ("calls", 1000000, "int64", "total+=mix(i);", "total=total+mix(i)", 17*1000000*999999//2),
        ("array_reads", 1000000, "int64", "total+=values[i%4];", "total=total+values[(i%4)+1]", 2500000),
    ]
    report = {"magel_version": (ROOT / "VERSION").read_text().strip(),
              "measured_at_utc": datetime.now(timezone.utc).isoformat(),
              "base_commit": run(["git", "rev-parse", "HEAD"]).strip(),
              "source_diff_sha256": hashlib.sha256(subprocess.check_output(["git", "diff", "HEAD", "--binary", "--", "library", "source", "tools", "tests", "Makefile", "local.mk", "VERSION", "features.mk"], cwd=ROOT)).hexdigest(),
              "platform": platform.platform(), "machine": platform.processor(),
              "compiler": run(["g++", "--version"]).splitlines()[0],
              "lua_version": run([lua, "-v"]).strip(), "lua_url": URL, "lua_sha256": SHA256,
              "flags": "Lua official mingw/linux Makefile -O2; Magel VM runner -O2 -DNDEBUG MAGEL_CUSTOM_ALLOCATOR=1; all VM optimizations enabled",
              "warmups_per_process": 1, "repeats": args.repeats,
              "timing": "one measured sample per process, alternate configuration order each round; VM steady_clock includes execution preparation and checksum output, excludes source compilation and bytecode loading; Lua os.clock includes loop function only; both exclude process startup",
              "baseline": "v0.18.2 compiler and VM, each compiler generates its own bytecode" if args.baseline_runner else None,
              "results": {}}
    report["binaries"] = {str(path): hashlib.sha256(path.read_bytes()).hexdigest()
                          for path in (args.magel, args.runner, lua)}
    if args.baseline_runner:
        report["baseline_commit"] = run(["git", "rev-parse", "v0.18.2^{commit}"]).strip()
        for path in (args.baseline_magel, args.baseline_runner):
            report["binaries"][str(path)] = hashlib.sha256(path.read_bytes()).hexdigest()
    if os.name == "nt":
        report["cpu"] = json.loads(run(["powershell", "-NoProfile", "-Command",
                                        "Get-CimInstance Win32_Processor | Select-Object Name,NumberOfCores,NumberOfLogicalProcessors | ConvertTo-Json -Compress"]))
    for name, count, value_type, mgl_body, lua_body, expected in cases:
        declaration = "int64 values[4];values[0]=1;values[1]=2;values[2]=3;values[3]=4;" if name == "array_reads" else ""
        source = WORK / (name + ".mgl")
        source.write_text("int64 mix(int64 x){return x*17;}\nint64 main(){int64 n=std::lang::inputInt(),i=0;"
                          + value_type + " total=0;" + declaration + "while(i<n){" + mgl_body
                          + "i+=1;}std::lang::println(\"{}\",total);return 0;}\n", encoding="utf-8")
        lua_file = WORK / (name + ".lua")
        lua_file.write_text("local n=tonumber(arg[1])\nlocal function mix(x) return x*17 end\n"
                            "local function calculate()\nlocal i,total=0," + ("0.0" if value_type == "float64" else "0")
                            + "\nlocal values={1,2,3,4}\nwhile i<n do " + lua_body
                            + ";i=i+1 end\nreturn total end\ncalculate()\nlocal start=os.clock()\n"
                            "local value=calculate()\nprint((os.clock()-start)*1000,value)\n", encoding="utf-8")
        prefix = WORK / name
        started = time.perf_counter()
        run([args.magel.resolve(), "svmb", source, "out", prefix])
        compilation = {"Magel 0.18.3": {"single_compile_ms": (time.perf_counter()-started)*1000,
                                       "bytecode_bytes": prefix.with_suffix(".svmb").stat().st_size}}
        modes = [("Magel 0.18.3", [args.runner.resolve(), prefix, 1, 1, count]),
                 ("Lua 5.5.1", [lua, lua_file, count])]
        if args.baseline_runner:
            old_prefix = WORK / (name + "-baseline")
            started = time.perf_counter()
            run([args.baseline_magel.resolve(), "svmb", source, "out", old_prefix])
            compilation["Magel 0.18.2"] = {"single_compile_ms": (time.perf_counter()-started)*1000,
                                         "bytecode_bytes": old_prefix.with_suffix(".svmb").stat().st_size}
            modes.insert(0, ("Magel 0.18.2", [args.baseline_runner.resolve(), old_prefix, 1, 1, count]))
        samples = {mode: {"samples_ms": [], "checksum": str(expected)} for mode, _ in modes}
        for index in range(args.repeats):
            for mode, command in modes if index % 2 == 0 else reversed(modes):
                fields = run(command).strip().split()
                assert float(fields[-1]) == expected, (name, mode, fields, expected)
                samples[mode]["samples_ms"].append(float(fields[0]))
                if mode.startswith("Magel"):
                    steps = int(fields[1])
                    assert samples[mode].get("logical_steps", steps) == steps
                    samples[mode]["logical_steps"] = steps
        for result in samples.values():
            result["median_ms"] = statistics.median(result["samples_ms"])
        report["results"][name] = {"iterations": count, "configurations": samples,
                                   "compilation_observation": compilation}
        print(name, {mode: result["median_ms"] for mode, result in samples.items()}, flush=True)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, ensure_ascii=False, indent=2)+"\n", encoding="utf-8")


if __name__ == "__main__":
    main()
